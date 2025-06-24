/***************************************************************************
**                                                                        **
**  NeoQCP, a fork of QCustomPlot, an easy to use, modern plotting widget **
**  for Qt.                                                               **
**  Copyright (C) 2011-2022 Emanuel Eichhammer (QCustomPlot)              **
**  Copyright (C) 2025 The NeoQCP Authors                                 **
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
**           Authors: Emanuel Eichhammer, The NeoQCP Authors              **
**  Website/Contact: https://www.qcustomplot.com/ (original)              **
**                   https://github.com/SciQLop/NeoQCP (fork)             **
** Original version: 2.1.1 (Date: 06.11.22)                               **
****************************************************************************/

#include "paintbuffer-pixmap.h"
#include "painter.h"
#include "Profiling.hpp"



////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPaintBufferPixmap
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPPaintBufferPixmap
  \brief A paint buffer based on QPixmap, using software raster rendering

  This paint buffer is the default and fall-back paint buffer which uses software rendering and
  QPixmap as internal buffer. It is used if \ref QCustomPlot::setOpenGl is false.
*/

/*!
  Creates a pixmap paint buffer instancen with the specified \a size and \a devicePixelRatio, if
  applicable.
*/
QCPPaintBufferPixmap::QCPPaintBufferPixmap(const QSize& size, double devicePixelRatio,
                                           const QString& layerName)
        : QCPAbstractPaintBuffer(size, devicePixelRatio, layerName)
{
    QCPPaintBufferPixmap::reallocateBuffer();
}

QCPPaintBufferPixmap::~QCPPaintBufferPixmap() { }

/* inherits documentation from base class */
QCPPainter* QCPPaintBufferPixmap::startPainting()
{
    QCPPainter* result = new QCPPainter(&mBuffer);
    return result;
}

/* inherits documentation from base class */
void QCPPaintBufferPixmap::draw(QCPPainter* painter) const
{
    PROFILE_HERE_N("QCPPaintBufferPixmap::draw");
    if (painter && painter->isActive())
        painter->drawPixmap(0, 0, mBuffer);
    else
        qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
}

/* inherits documentation from base class */
void QCPPaintBufferPixmap::clear(const QColor& color)
{
    mBuffer.fill(color);
}

/* inherits documentation from base class */
void QCPPaintBufferPixmap::reallocateBuffer()
{
    setInvalidated();
    if (!qFuzzyCompare(1.0, mDevicePixelRatio))
    {
        mBuffer = QPixmap(mSize * mDevicePixelRatio);
        mBuffer.setDevicePixelRatio(mDevicePixelRatio);
    }
    else
    {
        mBuffer = QPixmap(mSize);
    }
}

