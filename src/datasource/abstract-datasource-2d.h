#pragma once
#include "abstract-datasource.h" // for IndexableNumericRange concept, QCPRange
#include "global.h"              // for QCP::SignDomain

class QCPAbstractDataSource2D
{
public:
    virtual ~QCPAbstractDataSource2D() = default;

    virtual int xSize() const = 0;
    virtual int ySize() const = 0;
    virtual bool yIs2D() const = 0;

    virtual double xAt(int i) const = 0;
    virtual double yAt(int i, int j) const = 0;
    virtual double zAt(int i, int j) const = 0;

    virtual QCPRange xRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual QCPRange yRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual QCPRange zRange(bool& found, int xBegin = 0, int xEnd = -1) const = 0;

    virtual int findXBegin(double sortKey) const = 0;
    virtual int findXEnd(double sortKey) const = 0;

    // Raw array access for hot loops (avoids virtual dispatch per element).
    // Empty by default; concrete sources override to expose raw arrays.
    virtual const double* rawX() const { return nullptr; }
    virtual QCPRawColumn rawY() const { return {}; }
    virtual QCPRawColumn rawZ() const { return {}; }
};
