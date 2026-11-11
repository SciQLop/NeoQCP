#include "draw-context.h"

#include "draw-commands.h"
#include "raster-fallback.h"

#include "../axis/axis.h"
#include "../coordinate-system.h"
#include "../datasource/graph-resampler.h"
#include "../items/item.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../misuse.h"
#include "../theme.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <variant>

namespace
{
using Limit = std::numeric_limits<double>;

bool finite(double value)
{
    return std::isfinite(value);
}

//! The data box one command contributes, before it is merged into the recording's.
struct Extent
{
    bool keyBounded = true;
    bool valueBounded = true;
    //! True when the command carries no sample at all: it is neutral, it does not touch the box.
    bool empty = false;
    double keyLower = Limit::infinity();
    double keyUpper = -Limit::infinity();
    double valueLower = Limit::infinity();
    double valueUpper = -Limit::infinity();

    void key(double value)
    {
        if (!finite(value))
            return;
        keyLower = std::min(keyLower, value);
        keyUpper = std::max(keyUpper, value);
    }

    void value(double value)
    {
        if (!finite(value))
            return;
        valueLower = std::min(valueLower, value);
        valueUpper = std::max(valueUpper, value);
    }

    //! A side with no usable sample is not bounded by this command: a NaN or a gap says nothing.
    void settle()
    {
        if (!(keyLower <= keyUpper))
            keyBounded = false;
        if (!(valueLower <= valueUpper))
            valueBounded = false;
    }

    void merge(const Extent& other)
    {
        empty = empty && other.empty;
        keyBounded = keyBounded && other.keyBounded;
        valueBounded = valueBounded && other.valueBounded;
        keyLower = std::min(keyLower, other.keyLower);
        keyUpper = std::max(keyUpper, other.keyUpper);
        valueLower = std::min(valueLower, other.valueLower);
        valueUpper = std::max(valueUpper, other.valueUpper);
    }
};

//! Every point is data on both axes, which is what a data-space series gives.
Extent extentOf(const std::vector<QPointF>& data)
{
    Extent extent;
    extent.empty = data.empty();
    for (const QPointF& point : data)
    {
        extent.key(point.x());
        extent.value(point.y());
    }
    extent.settle();
    return extent;
}

Extent extentOf(std::span<const QCP::Position> positions)
{
    Extent extent;
    extent.empty = positions.empty();
    for (const QCP::Position& position : positions)
    {
        if (const auto* data = std::get_if<QCP::Data>(&position))
        {
            extent.key(data->x);
            extent.value(data->y);
        }
        else if (const auto* mixed = std::get_if<QCP::DataXRatioY>(&position))
        {
            extent.valueBounded = false;
            extent.key(mixed->x);
        }
        else if (const auto* mixed = std::get_if<QCP::RatioXDataY>(&position))
        {
            extent.keyBounded = false;
            extent.value(mixed->y);
        }
        else // pixels, an axis-rect ratio or an anchored offset: no data coordinate at all
        {
            extent.keyBounded = false;
            extent.valueBounded = false;
        }
    }
    extent.settle();
    return extent;
}

Extent unboundedExtent()
{
    Extent extent;
    extent.keyBounded = false;
    extent.valueBounded = false;
    extent.settle();
    return extent;
}

//! A command that says nothing about the data box: neutral, it leaves the box as it was.
Extent neutralExtent()
{
    Extent extent;
    extent.empty = true;
    return extent;
}

//! The data box of a single position: its coordinates when both are data, nothing otherwise. A
//! glyph hangs off its anchor by an amount only the painter knows, so the anchor is all it claims.
Extent extentOfPosition(const QCP::Position& position)
{
    Extent extent;
    if (const auto* data = std::get_if<QCP::Data>(&position))
    {
        extent.key(data->x);
        extent.value(data->y);
        extent.settle();
        return extent;
    }
    return neutralExtent();
}

QCPDataBox boxOf(const Extent& extent)
{
    return QCPDataBox { extent.keyBounded ? extent.keyLower : -Limit::infinity(),
                        extent.keyBounded ? extent.keyUpper : Limit::infinity(),
                        extent.valueBounded ? extent.valueLower : -Limit::infinity(),
                        extent.valueBounded ? extent.valueUpper : Limit::infinity() };
}

bool holdsAnchor(const QCP::Position& position)
{
    return std::holds_alternative<QCP::AnchorOffset>(position);
}

bool anyAnchor(std::span<const QCP::Position> positions)
{
    return std::any_of(positions.begin(), positions.end(),
                       [](const QCP::Position& position) { return holdsAnchor(position); });
}

//! True when a command names a QCP::AnchorOffset, which an item owns and may take away.
bool holdsAnchor(const qcp::QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& value) -> bool
        {
            using Command = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Command, qcp::PolylinePositionsCmd>
                          || std::is_same_v<Command, qcp::PolygonCmd>)
                return anyAnchor(value.points);
            else if constexpr (std::is_same_v<Command, qcp::RectCmd>
                               || std::is_same_v<Command, qcp::EllipseCmd>
                               || std::is_same_v<Command, qcp::ImageCmd>)
                return holdsAnchor(value.a) || holdsAnchor(value.b);
            else if constexpr (std::is_same_v<Command, qcp::TextCmd>)
                return holdsAnchor(value.at);
            else if constexpr (std::is_same_v<Command, qcp::PathCmd>)
                return holdsAnchor(value.origin);
            else
                return false;
        },
        command);
}

//! An unbounded side of \a extent says nothing about \a box: boxOf() leaves it at ±inf, so merging
//! keeps the recording's side unbounded too. A neutral extent never touches the box.
void mergeExtent(QCPDrawList::Box& box, const Extent& extent)
{
    if (!extent.empty)
        box.merge(boxOf(extent), extent.keyBounded, extent.valueBounded);
}

//! Records \a command with its \a data box (ranges, legend icon) and its \a index box (item index,
//! culling). A side is bounded only while every command so far is.
void record(QCPDrawList& list, qcp::QCPDrawCommand command, const Extent& data,
            const Extent& index)
{
    if (holdsAnchor(command))
        list.anchorDependent = true;
    if (list.commands.empty())
        list.id = qcpNextDrawListId(); // a re-recorded list is a new recording
    mergeExtent(list.data, data);
    mergeExtent(list.index, index);
    if (qcp::needsRaster(command))
        ++list.rasterCommands;
    list.commands.push_back(std::move(command));
}

void record(QCPDrawList& list, qcp::QCPDrawCommand command, const Extent& extent)
{
    record(list, std::move(command), extent, extent);
}

/*!
  True when \a brush can join a batch whose brush is \a batchBrush.

  A batch keeps one brush, so a pattern, a texture or a gradient has to be the very same one: two
  different gradients with the same style would silently paint as one. A solid colour may differ,
  though - it becomes a per-box fill, which is how a selected bar keeps its colour in the batch.

  A brush the GPU cannot fill needs the raster fallback, and a batch that needs one is a single image
  over the whole run: its fill would land under the strokes of every box beside it, where a command
  per box interleaves fill and stroke. Such a box keeps its own command.
*/
bool joinsBatch(const QBrush& batchBrush, const QBrush& brush)
{
    if (qcp::fillNeedsRaster(brush) || qcp::fillNeedsRaster(batchBrush))
        return false;
    return brush == batchBrush
        || (brush.style() == Qt::SolidPattern && batchBrush.style() == Qt::SolidPattern);
}

/*!
  The payload of \a batch, mutable.

  The command holds it as const - a recording is read-only once it is made - while the recorder is
  the one writer. It hands out a mutable reference only when it owns the payload alone; a copy of the
  recording that shares it gets a copy of its own first, so a recording already taken never changes
  under the one that keeps appending.
*/
template <class Batch>
qcp::BatchBoxes& editableBoxes(Batch& batch)
{
    if (batch.boxes.use_count() > 1)
        batch.boxes = std::make_shared<qcp::BatchBoxes>(*batch.boxes);
    return const_cast<qcp::BatchBoxes&>(*batch.boxes);
}

//! Appends \a a, \a b and \a brush to \a batch, keeping its fills, its sort flag and its box.
template <class Batch>
void appendBox(Batch& batch, const QPointF& a, const QPointF& b, const QBrush& brush)
{
    qcp::BatchBoxes& boxes = editableBoxes(batch);
    // The batch's own brush needs no entry; the first colour that differs gives every box one.
    if (boxes.fills.empty() && brush.color().rgba() != batch.brush.color().rgba())
        boxes.fills.assign(boxes.aKey.size(), batch.brush.color().rgba());
    if (!boxes.fills.empty())
        boxes.fills.push_back(brush.color().rgba());
    batch.sortedByKey = batch.sortedByKey && (boxes.aKey.empty() || a.x() >= boxes.aKey.back());
    boxes.aKey.push_back(a.x());
    boxes.aValue.push_back(a.y());
    boxes.bKey.push_back(b.x());
    boxes.bValue.push_back(b.y());
    batch.bounds.keyLower = std::min(batch.bounds.keyLower, std::min(a.x(), b.x()));
    batch.bounds.keyUpper = std::max(batch.bounds.keyUpper, std::max(a.x(), b.x()));
    batch.bounds.valueLower = std::min(batch.bounds.valueLower, std::min(a.y(), b.y()));
    batch.bounds.valueUpper = std::max(batch.bounds.valueUpper, std::max(a.y(), b.y()));
}

/*!
  Appends one more box to the \a Batch the previous command holds, or turns the single \a Single it
  holds into one: the second rect of a run is what makes the batch, so a recording of one rect stays
  the command it always was.

  False when the new box cannot join it - another shape, another pen, a brush of another kind - and
  the caller records a single command instead.
*/
template <class Batch, class Single>
bool appendShape(QCPDrawList& list, const QPointF& a, const QPointF& b, const QPen& pen,
                 const QBrush& brush, const Extent& extent)
{
    if (list.commands.empty())
        return false;
    qcp::QCPDrawCommand& previous = list.commands.back();
    if (auto* batch = std::get_if<Batch>(&previous))
    {
        if (!(batch->pen == pen) || !joinsBatch(batch->brush, brush))
            return false;
        appendBox(*batch, a, b, brush);
    }
    else if (const auto* single = std::get_if<Single>(&previous))
    {
        const auto* firstA = std::get_if<QCP::Data>(&single->a);
        const auto* firstB = std::get_if<QCP::Data>(&single->b);
        if (!firstA || !firstB || !(single->pen == pen) || !joinsBatch(single->brush, brush))
            return false;
        Batch batch;
        batch.pen = single->pen;
        batch.brush = single->brush;
        batch.boxes = std::make_shared<qcp::BatchBoxes>();
        batch.bounds
            = QCPDataBox { std::min(firstA->x, firstB->x), std::max(firstA->x, firstB->x),
                           std::min(firstA->y, firstB->y), std::max(firstA->y, firstB->y) };
        appendBox(batch, QPointF(firstA->x, firstA->y), QPointF(firstB->x, firstB->y),
                  single->brush);
        appendBox(batch, a, b, brush);
        // The pen and the brush style did not change, so what the replay asks of the raster cannot
        // either: the count record() took for the single command stays right.
        previous = std::move(batch);
    }
    else
        return false;
    // The box is kept as the recorder appends, so a batch is never re-scanned to answer its range.
    mergeExtent(list.data, extent);
    mergeExtent(list.index, extent);
    return true;
}
} // namespace

namespace
{
//! An axis-ratio position has nowhere to land in an area without an axis rect (a polar disc): the
//! command is dropped, and the misuse is told once rather than once per frame.
void reportRatioWithoutRect()
{
    static const bool once = []
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "a position measured in axis ratios needs an area that maps them: it is not drawn");
        return true;
    }();
    Q_UNUSED(once);
}

//! A mixed position is a key and a value, one mapped by the frame and one by the ratio: a polar
//! ratio maps the two together (an angle and a radius), so there is no key/value side to split.
void reportMixedOnANonSeparableRatio()
{
    static const bool once = []
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "a position mixing plot coordinates and axis ratios needs a separable "
                          "ratio: it is not drawn");
        return true;
    }();
    Q_UNUSED(once);
}
} // namespace

QPointF qcpResolvePosition(const QCPDrawFrame& frame, const QCP::Position& position,
                             bool report)
{
    if (const auto* data = std::get_if<QCP::Data>(&position))
    {
        const QCP::Pixels pixels = frame.transform.map(*data);
        return QPointF(pixels.x, pixels.y);
    }
    if (const auto* absolute = std::get_if<QCP::Pixels>(&position))
        return QPointF(absolute->x, absolute->y);
    if (const auto* ratio = std::get_if<QCP::AxisRatio>(&position))
    {
        if (report && !frame.ratio.valid())
            reportRatioWithoutRect();
        const QCP::Pixels pixels = frame.ratio.map(*ratio);
        return QPointF(pixels.x, pixels.y);
    }
    if (const auto* anchored = std::get_if<QCP::AnchorOffset>(&position))
    {
        const QPointF anchor = anchored->anchor ? anchored->anchor->pixelPosition() : QPointF();
        return anchor + QPointF(anchored->offset.dx, anchored->offset.dy);
    }
    // A mixed position is a key and a value, like QCP::Data, one of them a fraction of the axis
    // rect. A vertical key axis runs along screen y, so each number takes the screen axis of its
    // own side.
    const bool keyVertical = frame.transform.keyIsVertical();
    const auto keyAndValue = [keyVertical](QCP::Pixels keySide, QCP::Pixels valueSide)
    { return keyVertical ? QPointF(valueSide.x, keySide.y) : QPointF(keySide.x, valueSide.y); };
    const bool mixedKinds
        = std::get_if<QCP::DataXRatioY>(&position) || std::get_if<QCP::RatioXDataY>(&position);
    if (report && mixedKinds && !frame.ratio.valid())
        reportRatioWithoutRect();
    if (report && mixedKinds && frame.ratio.valid() && !frame.ratio.separable())
        reportMixedOnANonSeparableRatio();
    if (mixedKinds && !frame.ratio.separable())
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return QPointF(nan, nan); // not drawn
    }
    if (const auto* mixed = std::get_if<QCP::DataXRatioY>(&position))
        return keyAndValue(frame.transform.map(QCP::Data { mixed->x, 0.0 }),
                           frame.ratio.map(QCP::AxisRatio { mixed->y, mixed->y }));
    const auto& mixed = std::get<QCP::RatioXDataY>(position);
    return keyAndValue(frame.ratio.map(QCP::AxisRatio { mixed.x, mixed.x }),
                       frame.transform.map(QCP::Data { 0.0, mixed.y }));
}

QCPDrawFrame qcpMakeFrame(const QCPAxis* keyAxis, const QCPAxis* valueAxis,
                          const QCPAxisRect* axisRect, const QRectF& clip)
{
    return QCPDrawFrame { QCPTransform(keyAxis, valueAxis), QCPRatioTransform(axisRect), clip };
}

QCPDrawFrame qcpMakeFittedFrame(const QCPDataBox& box, const QRectF& rect)
{
    return QCPDrawFrame { QCPTransform::fitted(box, rect), QCPRatioTransform::fitted(rect), rect };
}

double qcpKeyPixelsPerUnit(const QCPAxis* keyAxis)
{
    if (!keyAxis)
        return 0.0;
    const QCPRange range = keyAxis->range();
    if (!(range.size() > 0.0))
        return 0.0;
    // A magnitude: a reversed or vertical axis reads the same as a plain horizontal one.
    return std::abs(keyAxis->coordToPixel(range.upper) - keyAxis->coordToPixel(range.lower))
        / range.size();
}

QCPDrawContext::QCPDrawContext(const QCPAxis* keyAxis, const QCPAxis* valueAxis,
                               const QCPArea* area, const QCPTheme* theme, bool exporting,
                               const QRectF& clip, const QCPCoordinateSystem* system)
        : mKeyAxis(keyAxis)
        , mValueAxis(valueAxis)
        , mSystem(system)
        , mList(std::make_unique<QCPDrawList>())
        , mTransform(system ? system->transform() : QCPTransform(keyAxis, valueAxis))
        , mRatio(area ? area->ratioTransform() : QCPRatioTransform())
        , mClip(clip.isNull()
                    ? (system ? system->clipRect() : (area ? QRectF(area->rect()) : QRectF()))
                    : clip)
        , mTheme(theme)
        , mExporting(exporting)
{
}

QCPDrawContext::~QCPDrawContext() = default;

QCPDrawFrame QCPDrawContext::frame() const
{
    return { mTransform, mRatio, mClip };
}

const QCPTransform& QCPDrawContext::transform() const
{
    // A recorder that reads the transform bakes pixels out of it: the recording is only good while
    // the transform keeps translating (a pan), not across a zoom or a resize.
    mList->transformDependent = true;
    return mTransform;
}

QRectF QCPDrawContext::clip() const
{
    mList->transformDependent = true; // the clip is this frame's: see the doc in the header
    return mClip;
}

const QCPTheme& QCPDrawContext::theme() const
{
    // Theme colours are a view fact: the commands that read them must not be reused across a
    // theme change (the epoch rule picks this flag up).
    mList->viewDependent = true;
    static const QCPTheme fallback;
    return mTheme ? *mTheme : fallback;
}

QCPKeyWindow QCPDrawContext::keyWindow() const
{
    if (!mKeyAxis)
        return { };
    const bool logarithmic = mKeyAxis->scaleType() == QCPAxis::stLogarithmic;
    const QCPKeyWindow window { qcp::algo::lineCacheKeyRange(mKeyAxis->range(), logarithmic),
                                qcpKeyPixelsPerUnit(mKeyAxis) };
    if (logarithmic)
    {
        // simplify: pixels per key are not constant on a log axis, so no window in data units can
        // promise them; the recording is remade on every transform change, a log-key pan included.
        // Upgrade: a window in log units with the scale checked in those.
        mList->transformDependent = true;
        return window;
    }
    mList->keyWindow = window;
    return window;
}

QCPDrawList QCPDrawContext::takeDrawList()
{
    return std::move(*mList);
}

const QCPDrawList& QCPDrawContext::drawList() const
{
    return *mList;
}

void QCPDrawContext::polyline(std::shared_ptr<const std::vector<QPointF>> data, const QPen& pen)
{
    polyline(std::move(data), nullptr, pen);
}

void QCPDrawContext::polyline(std::shared_ptr<const std::vector<QPointF>> data,
                              std::shared_ptr<const std::vector<QRgb>> colours, const QPen& pen)
{
    QCP_CHECK_OR_RETURN(!colours || colours->size() == data->size(),
                        "a polyline needs one colour per point");
    const Extent extent = extentOf(*data);
    record(*mList, qcp::PolylineDataCmd { std::move(data), std::move(colours), pen }, extent);
}

void QCPDrawContext::polyline(std::span<const QCP::Position> points, const QPen& pen)
{
    const Extent extent = extentOf(points);
    record(
        *mList,
        qcp::PolylinePositionsCmd { std::vector<QCP::Position>(points.begin(), points.end()), pen },
        extent);
}

void QCPDrawContext::polygon(std::span<const QCP::Position> points, const QPen& pen,
                             const QBrush& brush)
{
    const Extent extent = extentOf(points);
    record(*mList,
           qcp::PolygonCmd { std::vector<QCP::Position>(points.begin(), points.end()), pen, brush },
           extent);
}

void QCPDrawContext::fillBetween(std::shared_ptr<const std::vector<QPointF>> upper,
                                 std::shared_ptr<const std::vector<QPointF>> lower,
                                 const QBrush& brush)
{
    Extent extent = extentOf(*upper);
    extent.merge(extentOf(*lower));
    extent.settle();
    record(*mList, qcp::FillBetweenCmd { std::move(upper), std::move(lower), brush }, extent);
}

void QCPDrawContext::rect(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush)
{
    const QCP::Position corners[] = { a, b };
    const Extent extent = extentOf(corners);
    // A run of data-space rects is one batch: a bar chart records ten thousand a frame, and joining
    // it is a constant-time push. A pixel, ratio or anchored corner has no data box to widen.
    const auto* first = std::get_if<QCP::Data>(&a);
    const auto* second = std::get_if<QCP::Data>(&b);
    if (first && second
        && appendShape<qcp::RectsCmd, qcp::RectCmd>(
            *mList, QPointF(first->x, first->y), QPointF(second->x, second->y), pen, brush, extent))
        return;
    record(*mList, qcp::RectCmd { a, b, pen, brush }, extent);
}

void QCPDrawContext::ellipse(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush)
{
    const QCP::Position corners[] = { a, b };
    const Extent extent = extentOf(corners);
    const auto* first = std::get_if<QCP::Data>(&a);
    const auto* second = std::get_if<QCP::Data>(&b);
    if (first && second
        && appendShape<qcp::EllipsesCmd, qcp::EllipseCmd>(
            *mList, QPointF(first->x, first->y), QPointF(second->x, second->y), pen, brush, extent))
        return;
    record(*mList, qcp::EllipseCmd { a, b, pen, brush }, extent);
}

void QCPDrawContext::markers(std::shared_ptr<const std::vector<QPointF>> data,
                             const QCPScatterStyle& style)
{
    markers(std::move(data), nullptr, style);
}

void QCPDrawContext::markers(std::shared_ptr<const std::vector<QPointF>> data,
                             std::shared_ptr<const std::vector<QRgb>> colours,
                             const QCPScatterStyle& style)
{
    QCP_CHECK_OR_RETURN(!colours || colours->size() == data->size(),
                        "markers need one colour per point");
    const Extent extent = extentOf(*data);
    record(*mList,
           qcp::MarkersCmd { std::move(data), std::move(colours),
                             std::make_shared<const QCPScatterStyle>(style) },
           extent);
}

void QCPDrawContext::errorBars(std::shared_ptr<const std::vector<QPointF>> centres,
                               std::shared_ptr<const std::vector<QPointF>> spans, const QPen& pen)
{
    Extent extent;
    const std::size_t count = std::min(centres->size(), spans->size());
    extent.empty = count == 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = centres->at(i);
        const QPointF span = spans->at(i);
        extent.key(centre.x() - span.x());
        extent.key(centre.x() + span.x());
        extent.value(centre.y() - span.y());
        extent.value(centre.y() + span.y());
    }
    extent.settle();
    record(*mList,
           qcp::ErrorBarsCmd { std::move(centres), std::move(spans), nullptr, false, 0.0, 0.0,
                               false, pen },
           extent);
}

void QCPDrawContext::errorBars(std::shared_ptr<const std::vector<QPointF>> centres,
                               std::shared_ptr<const std::vector<QPointF>> ends, bool alongKeys,
                               double whiskerWidth, double symbolGap, const QPen& pen,
                               bool inPixels)
{
    Extent extent;
    const std::size_t count = std::min(centres->size(), ends->size() / 2);
    extent.empty = count == 0 || inPixels; // pixels describe no data box
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = centres->at(i);
        extent.key(centre.x());
        extent.value(centre.y());
        for (std::size_t side = 0; side < 2; ++side)
        {
            const QPointF end = ends->at(2 * i + side);
            extent.key(end.x());
            extent.value(end.y());
        }
    }
    extent.settle();
    // The whiskers reach across the bar by a pixel width the box cannot know; the index pads a
    // command by its pen, which is all a bar needs for a hit test on its own backbone.
    record(*mList,
           qcp::ErrorBarsCmd { std::move(centres), nullptr, std::move(ends), alongKeys,
                               whiskerWidth, symbolGap, inPixels, pen },
           extent);
}

void QCPDrawContext::text(QCP::Position at, const QString& text, const QCPTextStyle& style)
{
    this->text(at, text, style, qcpMeasureText(style, text));
}

void QCPDrawContext::text(QCP::Position at, const QString& text, const QCPTextStyle& style,
                          const QSize& glyphSize)
{
    // The anchor is the only data this command names; its glyphs hang off it by an amount only the
    // painter knows. So it feeds the data box and leaves the index box unbounded, as it always did.
    record(*mList,
           qcp::TextCmd { at, text, std::make_shared<const QCPTextStyle>(style), glyphSize },
           extentOfPosition(at), unboundedExtent());
}

void QCPDrawContext::recordEveryFrame()
{
    mList->everyFrame = true;
}

void QCPDrawContext::path(QCP::Position origin, const QPainterPath& path, const QPen& pen,
                          const QBrush& brush)
{
    // The path is pixels from the origin: like text, only the origin can be data.
    record(*mList, qcp::PathCmd { origin, path, pen, brush }, extentOfPosition(origin),
           unboundedExtent());
}

void QCPDrawContext::image(QCP::Position a, QCP::Position b, const QImage& image,
                           const QRectF& source, bool smooth)
{
    const QCP::Position corners[] = { a, b };
    const Extent extent = extentOf(corners);
    const QRectF texels = source.isNull() ? QRectF(QPointF(0, 0), QSizeF(image.size())) : source;
    record(*mList,
           qcp::ImageCmd {
               a, b,
               std::make_shared<qcp::ImagePayload>(qcp::ImagePayload { image, texels, smooth }) },
           extent);
}

void QCPDrawContext::painter(std::function<void(QCPPainter&, const QCPTransform&)> callback)
{
    record(*mList, qcp::PainterCmd { std::move(callback) }, neutralExtent(), unboundedExtent());
}
