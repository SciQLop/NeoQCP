#include "snippets.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <span>

// [systems_classes]
// The mapping: the rect's own cartesian frame, then x shifted to the right in proportion to the height
// above the rect's bottom edge. A shear keeps straight lines straight.
class ShearMapping : public QCPCustomMapping
{
public:
    ShearMapping(const QCPAxis* keyAxis, const QCPAxis* valueAxis, double shear, double bottom)
            : mCartesian(keyAxis, valueAxis), mShear(shear), mBottom(bottom)
    {
    }

    using QCPCustomMapping::map;   // keep the interleaved overload, built on this one

    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const override
    {
        const std::size_t count = std::min({ keys.size(), values.size(), out.size() });
        mCartesian.map(keys, values, out);
        for (QPointF& pixel : out.first(count))
            pixel.rx() += mShear * (mBottom - pixel.y());
    }

    QCP::Data inverse(QPointF pixel) const override
    {
        return mCartesian.inverse(QPointF(pixel.x() - mShear * (mBottom - pixel.y()), pixel.y()));
    }

private:
    QCPTransform mCartesian;
    double mShear;
    double mBottom;
};

// The system: one mapping per revision, a clip, a hit area and what it can host.
class ShearSystem : public QCPCoordinateSystem
{
public:
    ShearSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
    {
    }

    QCPTransform transform() const override
    {
        if (!mMapping || mBuiltRevision != revision())
        {
            mMapping = std::make_shared<const ShearMapping>(
                static_cast<const QCPAxis*>(keyAxis()), static_cast<const QCPAxis*>(valueAxis()),
                mShear, rect().bottom());
            mBuiltRevision = revision();
        }
        return QCPTransform::custom(mMapping, mBuiltRevision);
    }

    QRect clipRect() const override { return rect(); }
    bool contains(QPointF pixel) const override { return rect().contains(pixel.toPoint()); }
    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }

    void setShear(double shear)
    {
        if (shear == mShear)
            return;            // an unchanged parameter repaints nothing
        mShear = shear;
        parametersChanged();   // new revision, and the drawings repaint
    }

private:
    QRect rect() const { return static_cast<const QCPAxisRect*>(area())->rect(); }

    mutable std::shared_ptr<const ShearMapping> mMapping;
    mutable quint64 mBuiltRevision = 0;
    double mShear = 0.5;
};

// The area: an ordinary axis rect, except that every pair of its axes makes a ShearSystem.
class ShearRect : public QCPAxisRect
{
public:
    using QCPAxisRect::QCPAxisRect;

protected:
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override
    {
        // A shear needs one horizontal and one vertical axis: refuse what it cannot map, as the
        // plain axis rect does, so plot->axisRect()->system(left, right) is a reported misuse.
        if (static_cast<QCPAxis*>(key)->orientation() == static_cast<QCPAxis*>(value)->orientation())
            return QCPAxisRect::createSystem(key, value);
        return new ShearSystem(this, key, value);
    }
};
// [/systems_classes]

DOC_SNIPPET(systems_shear, 600, 420)
{
    // [systems_shear]
    customPlot->plotLayout()->clear();   // drop the default axis rect
    auto* rect = new ShearRect(customPlot);
    customPlot->plotLayout()->addElement(0, 0, rect);
    QCPAxis* x = rect->axis(QCPAxis::atBottom);
    QCPAxis* y = rect->axis(QCPAxis::atLeft);
    x->setRange(0, 10);
    y->setRange(-1.2, 1.2);

    QVector<double> key(101), value(101);
    for (int i = 0; i <= 100; ++i)
    {
        key[i] = i * 0.1;
        value[i] = std::sin(key[i] * std::numbers::pi / 2.5);
    }
    auto* wave = customPlot->addGraph(x, y);   // a QCPGraph: it joins the shear system
    wave->setData(key, value);
    wave->setPen(QPen(QColor(31, 119, 180), 2));
    wave->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));

    auto* steps = customPlot->addGraph(x, y);
    steps->setData({ 1, 3, 5, 7, 9 }, { -0.8, -0.2, -0.6, 0.1, -0.4 });
    steps->setLineStyle(QCPGraph::lsStepLeft);
    steps->setPen(QPen(QColor(214, 39, 40), 2));
    // [/systems_shear]
}
