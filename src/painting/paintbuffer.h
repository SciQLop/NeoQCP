/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2022 Emanuel Eichhammer                            **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: https://www.qcustomplot.com/                         **
**             Date: 06.11.22                                             **
**          Version: 2.1.1                                                **
****************************************************************************/

#ifndef QCP_PAINTBUFFER_H
#define QCP_PAINTBUFFER_H

#include "global.h"
class QCPPainter;

class QCP_LIB_DECL QCPAbstractPaintBuffer
{
public:
    explicit QCPAbstractPaintBuffer(const QSize& size, double devicePixelRatio,
                                    const QString& layerName);
    virtual ~QCPAbstractPaintBuffer();

    // getters:
    QSize size() const { return mSize; }

    bool invalidated() const { return mInvalidated; }

    double devicePixelRatio() const { return mDevicePixelRatio; }

    QString layerName() const { return mLayerName; }

    // setters:
    void setSize(const QSize& size);
    void setInvalidated(bool invalidated = true);
    void setDevicePixelRatio(double ratio);

    // introduced virtual methods:
    virtual QCPPainter* startPainting() = 0;

    virtual void donePainting() { }

    virtual void draw(QCPPainter* painter) const = 0;
    virtual void clear(const QColor& color) = 0;

protected:
    // property members:
    QSize mSize;
    double mDevicePixelRatio;
    QString mLayerName; // the name of the layer this paint buffer belongs to, if applicable

    // non-property members:
    bool mInvalidated;

    // introduced virtual methods:
    virtual void reallocateBuffer() = 0;
};


#endif // QCP_PAINTBUFFER_H
