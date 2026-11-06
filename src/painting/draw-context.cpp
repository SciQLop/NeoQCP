#include "draw-context.h"

#include "draw-commands.h"
#include "raster-fallback.h"

#include "../axis/axis.h"
#include "../items/item.h"
#include "../layoutelements/layoutelement-axisrect.h"
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
} // namespace

QPointF qcpResolvePosition(const QCPDrawFrame& frame, const QCP::Position& position)
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
        const QCP::Pixels pixels = frame.ratio.map(*ratio);
        return QPointF(pixels.x, pixels.y);
    }
    if (const auto* anchored = std::get_if<QCP::AnchorOffset>(&position))
    {
        const QPointF anchor = anchored->anchor ? anchored->anchor->pixelPosition() : QPointF();
        return anchor + QPointF(anchored->offset.dx, anchored->offset.dy);
    }
    // A mixed position's two numbers are not the two screen axes: on a vertical key axis the key
    // coordinate is on screen y, so a plot-coordinate slot reads the axis of the other orientation.
    // The 2.x pixelPosition() resolves it the same way, and both paths must land on one pixel.
    const bool keyVertical = frame.transform.keyIsVertical();
    if (const auto* mixed = std::get_if<QCP::DataXRatioY>(&position))
    {
        // One axis of the transform: the key pixel does not depend on the value coordinate, so any
        // value serves and only its key half is used.
        const QCP::Pixels keyPixels = frame.transform.map(QCP::Data { mixed->x, 0.0 });
        const QCP::Pixels ratioPixels = frame.ratio.map(QCP::AxisRatio { 0.0, mixed->y });
        if (!keyVertical)
            return QPointF(keyPixels.x, ratioPixels.y);
        // The plot-coordinate slot of a mixed position is its key, which a vertical key axis puts on
        // screen y; the ratio stays where the ratio transform puts it.
        const QCP::Pixels valuePixels = frame.transform.map(QCP::Data { 0.0, mixed->y });
        return QPointF(valuePixels.x, ratioPixels.y);
    }
    const auto& mixed = std::get<QCP::RatioXDataY>(position);
    const QCP::Pixels ratioPixels = frame.ratio.map(QCP::AxisRatio { mixed.x, 0.0 });
    const QCP::Pixels valuePixels = frame.transform.map(QCP::Data { 0.0, mixed.y });
    if (!keyVertical)
        return QPointF(ratioPixels.x, valuePixels.y);
    const QCP::Pixels keyPixels = frame.transform.map(QCP::Data { mixed.x, 0.0 });
    return QPointF(ratioPixels.x, keyPixels.y);
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

QCPDrawContext::QCPDrawContext(const QCPAxis* keyAxis, const QCPAxis* valueAxis,
                               const QCPAxisRect* axisRect, const QCPTheme* theme)
        : mKeyAxis(keyAxis)
        , mValueAxis(valueAxis)
        , mAxisRect(axisRect)
        , mList(std::make_unique<QCPDrawList>())
        , mTransform(keyAxis, valueAxis)
        , mRatio(axisRect)
        , mClip(axisRect ? QRectF(axisRect->rect()) : QRectF())
        , mTheme(theme)
{
}

QCPDrawContext::~QCPDrawContext() = default;

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
    const Extent extent = extentOf(*data);
    record(*mList, qcp::PolylineDataCmd { std::move(data), pen }, extent);
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
    record(*mList, qcp::RectCmd { a, b, pen, brush }, extent);
}

void QCPDrawContext::ellipse(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush)
{
    const QCP::Position corners[] = { a, b };
    const Extent extent = extentOf(corners);
    record(*mList, qcp::EllipseCmd { a, b, pen, brush }, extent);
}

void QCPDrawContext::markers(std::shared_ptr<const std::vector<QPointF>> data,
                             const QCPScatterStyle& style)
{
    const Extent extent = extentOf(*data);
    record(*mList, qcp::MarkersCmd { std::move(data), style }, extent);
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
    record(*mList, qcp::ErrorBarsCmd { std::move(centres), std::move(spans), pen }, extent);
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

void QCPDrawContext::image(QCP::Position a, QCP::Position b, const QImage& image)
{
    const QCP::Position corners[] = { a, b };
    const Extent extent = extentOf(corners);
    record(*mList, qcp::ImageCmd { a, b, image }, extent);
}

void QCPDrawContext::painter(std::function<void(QCPPainter&, const QCPTransform&)> callback)
{
    record(*mList, qcp::PainterCmd { std::move(callback) }, neutralExtent(), unboundedExtent());
}
