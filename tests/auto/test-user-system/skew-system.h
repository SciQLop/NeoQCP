#pragma once

// The fixture of milestone 4j: a user's own coordinate system and the rect that hosts it.
// Task 1 drives it with a curve and an item; Tasks 2 and 3 reuse it for the graph matrix.
//
// SkewMapping is the rect's cartesian frame with x shifted by shear * (rect bottom - pixel y): a
// point high in the rect lands far to the right of where a rectangle would put it. The rect's axes
// keep their ranges, dragging and zooming; only createSystem() differs from a QCPAxisRect.

#include "axis/axis.h"
#include "coordinate-system.h"
#include "coords-custom.h"
#include "coords.h"
#include "global.h"
#include "layoutelements/layoutelement-axisrect.h"

#include <QPointF>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>

//! Cartesian, then x shifted by shear * (rect bottom - pixel y): the toy mapping of milestone 4j.
class SkewMapping : public QCPCustomMapping
{
public:
    SkewMapping(const QCPAxis* keyAxis, const QCPAxis* valueAxis, double shear, double bottom)
            : mCartesian(keyAxis, valueAxis), mShear(shear), mBottom(bottom)
    {
    }

    using QCPCustomMapping::map;

    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const override
    {
        const std::size_t count = std::min({ keys.size(), values.size(), out.size() });
        mCartesian.map(keys.first(count), values.first(count), out.first(count));
        for (std::size_t i = 0; i < count; ++i)
            out[i].rx() += shiftOf(out[i].y());
    }

    void map(std::span<const QPointF> data, std::span<QPointF> out) const override
    {
        const std::size_t count = std::min(data.size(), out.size());
        mCartesian.map(data.first(count), out.first(count));
        for (std::size_t i = 0; i < count; ++i)
            out[i].rx() += shiftOf(out[i].y());
    }

    QCP::Pixels mapPoint(QCP::Data data) const override
    {
        const QCP::Pixels pixel = mCartesian.map(data);
        return QCP::Pixels { pixel.x + shiftOf(pixel.y), pixel.y };
    }

    QCP::Data inverse(QPointF pixel) const override
    {
        return mCartesian.inverse(QPointF(pixel.x() - shiftOf(pixel.y()), pixel.y()));
    }

    //! The pixel shift a point at pixel height \a pixelY takes.
    [[nodiscard]] double shiftOf(double pixelY) const { return mShear * (mBottom - pixelY); }

    [[nodiscard]] double shear() const { return mShear; }

private:
    QCPTransform mCartesian;
    double mShear;
    double mBottom;
};

//! The user's system: it maps through SkewMapping and offers the one capability a line needs.
class SkewSystem : public QCPCoordinateSystem
{
public:
    SkewSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
    {
    }

    QCPTransform transform() const override
    {
        // One mapping per revision: the contract says the revision moves whenever the mapping does,
        // a resize included (QCPArea::update bumps its systems when the area's rect changes).
        if (!mMapping || mBuiltRevision != revision())
        {
            mMapping = std::make_shared<const SkewMapping>(
                qobject_cast<const QCPAxis*>(keyAxis()), qobject_cast<const QCPAxis*>(valueAxis()),
                mShear, rect().bottom());
            mBuiltRevision = revision();
        }
        return QCPTransform::custom(mMapping, mBuiltRevision);
    }

    QRect clipRect() const override { return rect(); }

    //! The rect test: what a click may hit is what the system draws in.
    bool contains(QPointF pixel) const override { return rect().contains(pixel.toPoint()); }

    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }

    //! A parameter of the system's own: the revision moves and the drawings repaint.
    void setShear(double shear)
    {
        if (qFuzzyCompare(shear, mShear))
            return;
        mShear = shear;
        parametersChanged();
    }

    [[nodiscard]] double shear() const { return mShear; }

    //! The mapping the current transform holds, for a test that wants the pixel of a data point.
    [[nodiscard]] const SkewMapping& mapping() const
    {
        transform();
        return *mMapping;
    }

    [[nodiscard]] QCP::Pixels pixelOf(double key, double value) const
    {
        return mapping().mapPoint(QCP::Data { key, value });
    }

private:
    [[nodiscard]] QRect rect() const { return static_cast<const QCPAxisRect*>(area())->rect(); }

    mutable std::shared_ptr<const SkewMapping> mMapping;
    mutable quint64 mBuiltRevision = 0;
    double mShear = 1.0;
};

//! An axis rect whose one system is a SkewSystem: the documented J-3 route to a user system.
class SkewRect : public QCPAxisRect
{
public:
    explicit SkewRect(QCustomPlot* plot) : QCPAxisRect(plot) { }

    //! The system of the rect's bottom and left axes, created on first ask.
    [[nodiscard]] SkewSystem* skew()
    {
        return static_cast<SkewSystem*>(system(axis(QCPAxis::atBottom), axis(QCPAxis::atLeft)));
    }

    void setShear(double shear) { skew()->setShear(shear); }

protected:
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override
    {
        return new SkewSystem(this, key, value);
    }
};
