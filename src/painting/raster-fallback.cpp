#include "raster-fallback.h"

#include "../plottables/plottable-draw-utils.h"
#include "painter.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace qcp
{
namespace
{
    // The raster must keep the stroke's outer half and its antialiasing; the fill alone needs one
    // pixel.
    double marginOf(const QPen& pen, double dpr, bool withStroke)
    {
        if (!withStroke || pen.style() == Qt::NoPen)
            return 1.0;
        return double(qcp::extrusionPenWidth(pen, dpr)) / 2.0 + 1.0;
    }

    //! A brush that paints nothing: no brush at all, or a solid one whose colour is transparent.
    bool brushPaintsNothing(const QBrush& brush)
    {
        return brush.style() == Qt::NoBrush
            || (brush.style() == Qt::SolidPattern && brush.color().alpha() == 0);
    }
} // namespace

bool strokeNeedsRaster(const QPen& pen)
{
    return pen.style() != Qt::NoPen && !gpuCanFill(pen.brush());
}

bool strokeIsDashed(const QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            if constexpr (requires { cmd.pen; })
                return isDashed(cmd.pen);
            else
                return false;
        },
        command);
}

namespace
{
    //! Which halves of a command the raster has to paint: its fill, its stroke, or both.
    struct RasterNeeds
    {
        bool fill = false;
        bool stroke = false;
    };

    //! One dispatch answers both questions: every caller asks about both.
    RasterNeeds rasterNeedsOf(const QCPDrawCommand& command)
    {
        return std::visit(
            [](const auto& cmd)
            {
                using T = std::decay_t<decltype(cmd)>;
                RasterNeeds needs;
                if constexpr (requires { cmd.pen; })
                    needs.stroke = strokeNeedsRaster(cmd.pen);
                if constexpr (std::is_same_v<T, PolygonCmd>)
                    needs.fill = !brushPaintsNothing(cmd.brush);
                else if constexpr (std::is_same_v<T, FillBetweenCmd>)
                    needs.fill = !brushPaintsNothing(cmd.brush)
                        && (!gpuCanFill(cmd.brush) || cmd.upper->size() != cmd.lower->size());
                else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>)
                    needs.fill = !brushPaintsNothing(cmd.brush) && !gpuCanFill(cmd.brush);
                return needs;
            },
            command);
    }
} // namespace

bool strokeNeedsRaster(const QCPDrawCommand& command)
{
    return rasterNeedsOf(command).stroke;
}

bool fillNeedsRaster(const QCPDrawCommand& command)
{
    return rasterNeedsOf(command).fill;
}

bool needsRaster(const QCPDrawCommand& command)
{
    const RasterNeeds needs = rasterNeedsOf(command);
    return needs.fill || needs.stroke;
}

QRectF fallbackPixelBounds(const QCPDrawCommand& command, const QCPDrawFrame& frame, double dpr,
                           bool withStroke)
{
    const CommandExtent extent = commandExtent(command, frame, true);
    if (extent.box.isEmpty())
        return extent.box;
    const double margin = marginOf(extent.pen, dpr, withStroke);
    return extent.box.adjusted(-margin, -margin, margin, margin);
}

void paintFallback(QCPPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                   bool withStroke)
{
    // Two points of a fill enclose nothing to raster, and the painter would draw the outline as a
    // line where the replay draws none.
    if (const auto* polygon = std::get_if<qcp::PolygonCmd>(&command);
        polygon && polygon->points.size() < 3)
        return;
    qcp::paintCommand(painter, command, frame, { withStroke, true });
}

QImage rasterizeFallback(const QCPDrawCommand& command, const QRectF& bounds,
                         const QCPDrawFrame& frame, double dpr, bool withStroke)
{
    const QSize device(int(std::ceil(bounds.width() * dpr)), int(std::ceil(bounds.height() * dpr)));
    if (device.isEmpty())
        return { };
    QImage image(device, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QCPPainter painter(&image);
    painter.scale(dpr, dpr);
    painter.translate(-bounds.topLeft());
    paintFallback(painter, command, frame, withStroke);
    painter.end();
    return image;
}

void QCPRasterFallback::beginReplay()
{
    mPainterHandled.clear();
    mPainterLists.clear();
    sweepUnused(mEntries, [](const Entry&) { return false; });
}

const QImage& QCPRasterFallback::image(quint64 list, quint64 index, const QCPDrawCommand& command,
                                       const QCPDrawFrame& frame, const QRectF& bounds, double dpr,
                                       quint64 generation, PanBehaviour behaviour, QPointF* delta)
{
    for (Entry& entry : mEntries)
    {
        // Not keyed on used: a recording replayed through its kept geometry matches the very image
        // its last build made, and that image was already used by the frame that built it.
        if (entry.list != list || entry.index != index || entry.generation != generation
            || entry.dpr != dpr || entry.bounds != bounds || entry.behaviour != behaviour)
            continue;
        QPointF shift;
        if (entry.transform == frame.transform)
            shift = QPointF();
        else if (behaviour == PanBehaviour::Exact
                 || !frame.transform.isTranslationOf(entry.transform, &shift))
            continue;
        else if (behaviour == PanBehaviour::Stays)
            shift = QPointF();
        entry.used = true;
        if (delta)
            *delta = shift;
        return entry.image;
    }

    Entry& entry = mEntries.emplace_back();
    entry.list = list;
    entry.index = index;
    entry.generation = generation;
    entry.transform = frame.transform;
    entry.bounds = bounds;
    entry.dpr = dpr;
    entry.behaviour = behaviour;
    entry.used = true;
    if (delta)
        *delta = QPointF();
    entry.image = rasterizeFallback(command, bounds, frame, dpr, strokeNeedsRaster(command));
    return entry.image;
}

void QCPRasterFallback::painterHandles(quint64 list, quint64 index)
{
    mPainterHandled.insert({ list, index });
}

bool QCPRasterFallback::painterHandled(quint64 list, quint64 index) const
{
    return mPainterLists.contains(list) || mPainterHandled.contains({ list, index });
}

void QCPRasterFallback::paintWholeRecordingWithPainter(quint64 list)
{
    mPainterLists.insert(list);
}

bool QCPRasterFallback::painterDrawsAll(quint64 list) const
{
    return mPainterLists.contains(list);
}

void QCPRasterFallback::clear()
{
    mEntries.clear();
    mPainterHandled.clear();
    mPainterLists.clear();
}
} // namespace qcp
