#include "draw-commands.h"

#include "draw-context.h"
#include "painter.h"
#include "raster-fallback.h"

#include "../axis/labelrenderer.h"
#include "../plottables/plottable-draw-utils.h"

#include <QPolygonF>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <type_traits>

namespace
{
// Reused across replays on this thread: a steady-state replay allocates nothing.
struct Scratch
{
    std::vector<QPointF> data; // data-space points of one command, before the batch map
    std::vector<QPointF> pixels; // the same points in pixels, or the resolved positions
    QPolygonF polygon;
};

Scratch& scratch()
{
    thread_local Scratch local;
    return local;
}

//! Where a box of \a size sits relative to the anchor: QCPItemText::getTextDrawPoint, which halves
//! a double and rounds the point - an odd size does not truncate the same way.
QPoint boxTopLeft(const QSize& size, Qt::Alignment alignment)
{
    QPointF topLeft;
    if (alignment & Qt::AlignHCenter)
        topLeft.rx() = -size.width() / 2.0;
    else if (alignment & Qt::AlignRight)
        topLeft.rx() = -size.width();
    if (alignment & Qt::AlignVCenter)
        topLeft.ry() = -size.height() / 2.0;
    else if (alignment & Qt::AlignBottom)
        topLeft.ry() = -size.height();
    return topLeft.toPoint();
}

const QCPLabelRenderer* textRenderer(const QCPTextStyle& style)
{
    return style.mathText ? QCPLabelRenderer::defaultRenderer() : nullptr;
}

//! Counts the measurements, so a test can tell a cached box from a re-measure.
std::atomic<quint64> gTextMeasures { 0 };
} // namespace

QSize qcpMeasureText(const QCPTextStyle& style, const QString& text)
{
    gTextMeasures.fetch_add(1, std::memory_order_relaxed);
    return QCPLabelRenderer::measureRectWith(textRenderer(style), style.font, text, style.textFlags)
        .size();
}

quint64 qcpTextMeasureCount()
{
    return gTextMeasures.load(std::memory_order_relaxed);
}

qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style, const QSize& glyphSize)
{
    QRect glyphs(QPoint(0, 0), glyphSize);
    const int left = int(std::ceil(style.padding.left()));
    const int top = int(std::ceil(style.padding.top()));
    QRect box = glyphs.adjusted(-left, -top, int(std::ceil(style.padding.right())),
                                int(std::ceil(style.padding.bottom())));
    const QPoint topLeft = boxTopLeft(box.size(), style.alignment);
    glyphs.moveTopLeft(topLeft + QPoint(left, top));
    box.moveTopLeft(topLeft);
    return { box, glyphs };
}

qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style, const QString& text)
{
    return qcpTextLayout(style, qcpMeasureText(style, text));
}

namespace
{
//! The box and glyphs, exactly as QCPItemText lays them out relative to the anchor.
void replayText(const qcp::TextCmd& cmd, QCPPainter& painter, const QCPDrawFrame& frame)
{
    const QCPTextStyle& style = *cmd.style;
    painter.save();
    painter.translate(qcpResolvePosition(frame, cmd.at));
    if (style.rotationDegrees != 0.0)
        painter.rotate(style.rotationDegrees);
    const qcp::QCPTextLayout layout = qcpTextLayout(style, cmd.glyphSize);
    if (style.brush.style() != Qt::NoBrush || style.pen.style() != Qt::NoPen)
    {
        painter.setPen(style.pen);
        painter.setBrush(style.brush);
        painter.drawRect(layout.box);
        painter.setBrush(Qt::NoBrush);
    }
    QCPLabelRenderer::drawWith(textRenderer(style), &painter, layout.glyphs, style.font,
                               style.color, cmd.text, style.textFlags);
    painter.restore();
}
} // namespace

namespace
{
//! True only when the command is provably outside \a clip: a recording may cover far more than the
//! frame shows (a bar chart records every bar), and drawing what nobody sees costs the most.
bool outsideClip(const qcp::QCPDrawCommand& command, const QCPDrawFrame& frame)
{
    if (frame.clip.isEmpty())
        return false;
    const qcp::CommandExtent extent = qcp::commandExtent(command, frame);
    return extent.cullable
        && !extent.box.adjusted(-extent.pad, -extent.pad, extent.pad, extent.pad)
                .intersects(frame.clip);
}
} // namespace

namespace
{
//! One draw per box of \a batch, rects or ellipses: the painter path of a batch command.
//!
//! A box outside the clip is skipped here, exactly as the command that held it alone used to be.
//! A bar chart records every bar and the frame shows a hundred of them, so this test is what keeps
//! a batch as cheap as the commands it replaced. The stroke's reach is part of the test: a bar half
//! off the edge still draws its half.
template <class Batch>
void paintBoxes(QCPPainter& painter, const Batch& batch, const QCPDrawFrame& frame,
                const qcp::PaintMode& mode, bool ellipse)
{
    painter.setPen(mode.withStroke ? batch.pen : QPen(Qt::NoPen));
    const qcp::BatchBoxes& boxes = *batch.boxes;
    const bool perBoxColour = !boxes.fills.empty();
    if (!perBoxColour)
        painter.setBrush(batch.brush);
    const bool clipKnown = !frame.clip.isEmpty();
    const double pad = qcp::qcpStrokePad(batch.pen);
    const QRectF visible = frame.clip.adjusted(-pad, -pad, pad, pad);
    const std::size_t count = std::min(boxes.aKey.size(), boxes.bKey.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const QRectF box
            = QRectF(qcpResolvePosition(frame, QCP::Data { boxes.aKey[i], boxes.aValue[i] }),
                     qcpResolvePosition(frame, QCP::Data { boxes.bKey[i], boxes.bValue[i] }))
                  .normalized();
        if (clipKnown && !box.intersects(visible))
            continue;
        if (perBoxColour)
            painter.setBrush(QBrush(QColor::fromRgba(boxes.fills[i])));
        if (ellipse)
            painter.drawEllipse(box);
        else
            painter.drawRect(box);
    }
}
} // namespace

namespace qcp
{
void paintCommand(QCPPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                  PaintMode mode)
{
    if (mode.antialiased)
        painter.setAntialiasing(true);
    Scratch& local = scratch();
    std::visit(
        [&](const auto& cmd)
        {
            using Command = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<Command, PolylineDataCmd>)
            {
                const std::span<const QPointF> data(*cmd.points);
                local.pixels.resize(data.size());
                frame.transform.map(data, std::span<QPointF>(local.pixels));
                if (!cmd.colours)
                {
                    painter.setPen(cmd.pen);
                    drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.pixels));
                }
                else
                {
                    // One polyline per colour run: the spans skip the gaps a NaN makes, so a run
                    // never joins across one.
                    const std::vector<ColorSpan> spans
                        = colorSpans(std::span<const QPointF>(local.pixels),
                                     std::span<const QRgb>(*cmd.colours));
                    drawColoredPolylineSpans(&painter, local.pixels, spans, cmd.pen);
                }
            }
            else if constexpr (std::is_same_v<Command, PolylinePositionsCmd>)
            {
                qcpResolveInto(cmd.points, frame, local.pixels);
                painter.setPen(cmd.pen);
                drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.pixels));
            }
            else if constexpr (std::is_same_v<Command, PolygonCmd>)
            {
                qcpResolveInto(cmd.points, frame, local.pixels);
                painter.setPen(mode.withStroke ? cmd.pen : QPen(Qt::NoPen));
                painter.setBrush(cmd.brush);
                qcpPolygonOf(local.pixels, local.polygon);
                painter.drawPolygon(local.polygon);
            }
            else if constexpr (std::is_same_v<Command, FillBetweenCmd>)
            {
                const std::size_t upper = cmd.upper->size();
                local.data.resize(upper + cmd.lower->size());
                std::copy(cmd.upper->cbegin(), cmd.upper->cend(), local.data.begin());
                std::reverse_copy(cmd.lower->cbegin(), cmd.lower->cend(),
                                  local.data.begin() + std::ptrdiff_t(upper));
                local.pixels.resize(local.data.size());
                frame.transform.map(std::span<const QPointF>(local.data),
                                    std::span<QPointF>(local.pixels));
                painter.setPen(Qt::NoPen);
                painter.setBrush(cmd.brush);
                qcpPolygonOf(local.pixels, local.polygon);
                painter.drawPolygon(local.polygon);
            }
            else if constexpr (std::is_same_v<Command, RectCmd>)
            {
                const QRectF rect
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                painter.setPen(mode.withStroke ? cmd.pen : QPen(Qt::NoPen));
                painter.setBrush(cmd.brush);
                painter.drawRect(rect);
            }
            else if constexpr (std::is_same_v<Command, EllipseCmd>)
            {
                const QRectF rect
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                painter.setPen(mode.withStroke ? cmd.pen : QPen(Qt::NoPen));
                painter.setBrush(cmd.brush);
                painter.drawEllipse(rect);
            }
            else if constexpr (std::is_same_v<Command, RectsCmd>)
            {
                paintBoxes(painter, cmd, frame, mode, false);
            }
            else if constexpr (std::is_same_v<Command, EllipsesCmd>)
            {
                paintBoxes(painter, cmd, frame, mode, true);
            }
            else if constexpr (std::is_same_v<Command, MarkersCmd>)
            {
                const std::span<const QPointF> data(*cmd.points);
                local.pixels.resize(data.size());
                frame.transform.map(data, std::span<QPointF>(local.pixels));
                if (!cmd.colours)
                {
                    cmd.style->applyTo(&painter, QPen(Qt::black));
                    for (const QPointF& point : local.pixels)
                        cmd.style->drawShape(&painter, point);
                }
                else
                {
                    // One style per point, its pen and brush replaced by the point's colour: the
                    // style's other properties (shape, size, a pixmap) are the command's.
                    QCPScatterStyle style = *cmd.style;
                    for (std::size_t i = 0; i < local.pixels.size(); ++i)
                    {
                        const QColor colour = QColor::fromRgba((*cmd.colours)[i]);
                        style.setPen(QPen(colour));
                        style.setBrush(colour);
                        style.applyTo(&painter, style.pen());
                        style.drawShape(&painter, local.pixels[i]);
                    }
                }
            }
            else if constexpr (std::is_same_v<Command, ErrorBarsCmd>)
            {
                qcpErrorBarPixels(frame, std::span<const QPointF>(*cmd.centres),
                                  std::span<const QPointF>(*cmd.spans), local.data);
                painter.setPen(cmd.pen);
                drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.data));
            }
            else if constexpr (std::is_same_v<Command, TextCmd>)
            {
                replayText(cmd, painter, frame);
            }
            else if constexpr (std::is_same_v<Command, PathCmd>)
            {
                painter.save(); // the origin translation is this command's own
                painter.translate(qcpResolvePosition(frame, cmd.origin));
                painter.setPen(cmd.pen);
                painter.setBrush(cmd.brush);
                painter.drawPath(cmd.path);
                painter.restore();
            }
            else if constexpr (std::is_same_v<Command, ImageCmd>)
            {
                const QPointF a = qcpResolvePosition(frame, cmd.a);
                const QPointF b = qcpResolvePosition(frame, cmd.b);
                if (cmd.pixels->image.isNull() || cmd.pixels->source.isEmpty())
                    return;
                QImage image = cmd.pixels->image;
                QRectF source = cmd.pixels->source;
                // A vector painter embeds the picture at its own resolution, so a viewer resamples
                // it as it likes. Three times as many texels keep a colour map's cells sharp, but a
                // source already finer than that is left alone.
                if (!cmd.pixels->smooth && painter.modes().testFlag(QCPPainter::pmVectorized))
                {
                    const int factor = qcp::vectorExportUpscaleFactor(
                        QSizeF(std::abs(b.x() - a.x()), std::abs(b.y() - a.y())),
                        QSizeF(source.size()));
                    if (factor > 1)
                    {
                        image = image.scaled(image.width() * factor, image.height() * factor,
                                             Qt::IgnoreAspectRatio, Qt::FastTransformation);
                        source = QRectF(source.topLeft() * factor, source.size() * factor);
                    }
                }
                painter.save();
                painter.setRenderHint(QPainter::SmoothPixmapTransform, cmd.pixels->smooth);
                // a is the top-left texel, so a reversed axis turns into a negative scale and the
                // picture mirrors. A negative-width target rect would instead be empty.
                painter.translate(a);
                painter.scale((b.x() - a.x()) / source.width(), (b.y() - a.y()) / source.height());
                painter.drawImage(QPointF(0, 0), image, source);
                painter.restore();
            }
            else if constexpr (std::is_same_v<Command, PainterCmd>)
            {
                painter.save(); // a callback may leave any state changed
                cmd.callback(painter, frame.transform);
                painter.restore();
            }
        },
        command);
}
} // namespace qcp

void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame,
                          QCPReplayKinds kinds, const qcp::QCPRasterFallback* fallback,
                          bool dashedAvailable, bool antialiasStrokes, QCPReplayState state)
{
    // PainterOnly leaves the GPU kinds to another pass: on screen the RHI replay, in an export the
    // layer's deferred GpuOnly pass. \a fallback, when there is one, draws the kinds the layer
    // refused; GpuOnly draws exactly the GPU kinds, so it never needs one.
    const bool owned = state == QCPReplayState::Save;
    if (owned)
        painter.save();
    if (owned && !frame.clip.isEmpty())
        painter.setClipRect(frame.clip);
    // A recording the layer could not take whole is painted by this pass like any other recording:
    // the call below draws every one of its commands.
    const bool painterDrawsAll = fallback && fallback->painterDrawsAll(list.id);
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        const bool gpu = qcp::isGpuCommand(command);
        if (kinds == QCPReplayKinds::GpuOnly && !gpu)
            continue;
        if (kinds == QCPReplayKinds::PainterOnly && gpu && !painterDrawsAll)
        {
            // The layer could not take this command's image: paint it here, in the buffer the GPU
            // geometry draws over. A stroke the GPU cannot express is part of it.
            if (fallback && fallback->painterHandled(list.id, qcpCommandIndex(list, command)))
                qcp::paintFallback(painter, command, frame,
                                   qcp::strokeGoesToRaster(command, dashedAvailable));
            continue;
        }
        if (outsideClip(command, frame))
            continue;
        // A stroke is antialiased as its object drew it; an image is not, as the GPU draws its quad
        // with hard edges, and the export has to agree with the frame.
        const bool stroke = !std::holds_alternative<qcp::ImageCmd>(command);
        qcp::paintCommand(painter, command, frame, { true, antialiasStrokes && stroke });
    }
    if (owned)
        painter.restore();
}
