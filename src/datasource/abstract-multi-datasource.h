#pragma once
#include "abstract-datasource.h"

class QCPAbstractMultiDataSource {
public:
    virtual ~QCPAbstractMultiDataSource() = default;

    virtual int columnCount() const = 0;
    virtual int size() const = 0;
    virtual bool empty() const { return size() == 0; }

    virtual double keyAt(int i) const = 0;
    virtual QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual int findBegin(double sortKey, bool expandedRange = true) const = 0;
    virtual int findEnd(double sortKey, bool expandedRange = true) const = 0;

    virtual double valueAt(int column, int i) const = 0;
    virtual QCPRange valueRange(int column, bool& found,
                                QCP::SignDomain sd = QCP::sdBoth,
                                const QCPRange& inKeyRange = QCPRange()) const = 0;

    virtual QVector<QPointF> getOptimizedLineData(
        int column, int begin, int end, int pixelWidth,
        QCPAxis* keyAxis, QCPAxis* valueAxis) const = 0;

    virtual QVector<QPointF> getLines(
        int column, int begin, int end,
        QCPAxis* keyAxis, QCPAxis* valueAxis) const = 0;

    virtual const double* rawKeyData() const { return nullptr; }
    virtual const double* rawColumnData(int /*column*/) const { return nullptr; }
};
