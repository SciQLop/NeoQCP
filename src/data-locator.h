#ifndef QCP_DATA_LOCATOR_H
#define QCP_DATA_LOCATOR_H

#include "global.h"
#include "handle.h"
#include <QVector>
#include <limits>

class QCPAbstractPlottable;

class QCP_LIB_DECL QCPDataLocator
{
public:
    QCPDataLocator() = default;

    void setPlottable(QCPAbstractPlottable* plottable);
    bool locate(const QPointF& pixelPos);
    bool locateAtKey(QCPAbstractPlottable* plottable, double key,
                     double value = std::numeric_limits<double>::quiet_NaN());

    bool isValid() const { return mValid; }
    double key() const { return mKey; }
    double value() const { return mValue; }
    double data() const { return mData; }
    int dataIndex() const { return mDataIndex; }
    const QVector<double>& componentValues() const { return mComponentValues; }
    //! Out of line: a handle of the forward-declared QCPAbstractPlottable needs its complete type.
    QCPHandle<QCPAbstractPlottable> plottable() const;
    QCPHandle<QCPAbstractPlottable> hitPlottable() const;

private:
    QCPAbstractPlottable* mPlottable = nullptr;
    QCPAbstractPlottable* mHitPlottable = nullptr;
    bool mValid = false;
    double mKey = 0;
    double mValue = 0;
    double mData = std::numeric_limits<double>::quiet_NaN();
    int mDataIndex = -1;
    QVector<double> mComponentValues;

    bool locateCurve(const QPointF& pixelPos);
    bool locateColorMap(const QPointF& pixelPos);
    bool locateColorMap2(const QPointF& pixelPos);
    bool locateHistogram2D(const QPointF& pixelPos);
    bool locateMultiGraph(const QPointF& pixelPos);

    bool locateMultiGraphAtKey(double key);
    bool locateColorMap2AtKey(double key, double value);
    bool locateColorMapAtKey(double key, double value);
    bool locateHistogram2DAtKey(double key, double value);
};

#endif // QCP_DATA_LOCATOR_H
