#include "draw-context.h"

#include "draw-commands.h"

#include "../axis/axis.h"
#include "../items/item.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../theme.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

QCPDataBox boxOf(const Extent& extent)
{
    return QCPDataBox { extent.keyBounded ? extent.keyLower : -Limit::infinity(),
                        extent.keyBounded ? extent.keyUpper : Limit::infinity(),
                        extent.valueBounded ? extent.valueLower : -Limit::infinity(),
                        extent.valueBounded ? extent.valueUpper : Limit::infinity() };
}

//! A side is bounded only while every command so far is: one pixel or ratio command ends it.
void record(QCPDrawList& list, qcp::QCPDrawCommand command, const Extent& extent)
{
    if (list.commands.empty())
    {
        list.keyBounded = extent.keyBounded;
        list.valueBounded = extent.valueBounded;
        list.bounds = boxOf(extent);
    }
    else
    {
        list.keyBounded = list.keyBounded && extent.keyBounded;
        list.valueBounded = list.valueBounded && extent.valueBounded;
        if (list.keyBounded)
        {
            list.bounds.keyLower = std::max(list.bounds.keyLower, extent.keyLower);
            list.bounds.keyUpper = std::min(list.bounds.keyUpper, extent.keyUpper);
        }
        if (list.valueBounded)
        {
            list.bounds.valueLower = std::max(list.bounds.valueLower, extent.valueLower);
            list.bounds.valueUpper = std::min(list.bounds.valueUpper, extent.valueUpper);
        }
    }
    if (!list.keyBounded)
    {
        list.bounds.keyLower = -Limit::infinity();
        list.bounds.keyUpper = Limit::infinity();
    }
    if (!list.valueBounded)
    {
        list.bounds.valueLower = -Limit::infinity();
        list.bounds.valueUpper = Limit::infinity();
    }
    list.commands.push_back(std::move(command));
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
    if (const auto* mixed = std::get_if<QCP::DataXRatioY>(&position))
    {
        // One axis of the transform: the key pixel does not depend on the value coordinate, so any
        // value serves and only its key half is used.
        const QCP::Pixels keyPixels = frame.transform.map(QCP::Data { mixed->x, 0.0 });
        const QCP::Pixels ratioPixels = frame.ratio.map(QCP::AxisRatio { 0.0, mixed->y });
        return QPointF(keyPixels.x, ratioPixels.y);
    }
    const auto& mixed = std::get<QCP::RatioXDataY>(position);
    const QCP::Pixels ratioPixels = frame.ratio.map(QCP::AxisRatio { mixed.x, 0.0 });
    const QCP::Pixels valuePixels = frame.transform.map(QCP::Data { 0.0, mixed.y });
    return QPointF(ratioPixels.x, valuePixels.y);
}

QCPDrawContext::QCPDrawContext(const QCPAxis* keyAxis, const QCPAxis* valueAxis,
                               const QCPAxisRect* axisRect, const QCPTheme* theme)
    : mList(std::make_unique<QCPDrawList>())
    , mTransform(keyAxis, valueAxis)
    , mRatio(axisRect)
    , mClip(axisRect ? QRectF(axisRect->rect()) : QRectF())
    , mTheme(theme)
{
}

QCPDrawContext::~QCPDrawContext() = default;

const QCPTheme& QCPDrawContext::theme() const
{
    static const QCPTheme fallback;
    return mTheme ? *mTheme : fallback;
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
    record(*mList, qcp::PolylinePositionsCmd { std::vector<QCP::Position>(points.begin(), points.end()), pen },
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
    // The glyphs extend beyond the anchor by an amount only the painter knows, so no data box can
    // describe this command: it leaves the recording unbounded.
    record(*mList, qcp::TextCmd { at, text, std::make_shared<const QCPTextStyle>(style) },
           unboundedExtent());
}

void QCPDrawContext::path(QCP::Position origin, const QPainterPath& path, const QPen& pen,
                          const QBrush& brush)
{
    // The path is pixels from the origin: like text, it has no data-space extent.
    record(*mList, qcp::PathCmd { origin, path, pen, brush }, unboundedExtent());
}

void QCPDrawContext::image(QCP::Position a, QCP::Position b, const QImage& image)
{
    const QCP::Position corners[] = { a, b };
    const Extent extent = extentOf(corners);
    record(*mList, qcp::ImageCmd { a, b, image }, extent);
}

void QCPDrawContext::painter(std::function<void(QCPPainter&, const QCPTransform&)> callback)
{
    record(*mList, qcp::PainterCmd { std::move(callback) }, unboundedExtent());
}
