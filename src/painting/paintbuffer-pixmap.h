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
#pragma once

#include "paintbuffer.h"

class QCP_LIB_DECL QCPPaintBufferPixmap : public QCPAbstractPaintBuffer
{
public:
    explicit QCPPaintBufferPixmap(const QSize& size, double devicePixelRatio,
                                  const QString& layerName);
    virtual ~QCPPaintBufferPixmap() Q_DECL_OVERRIDE;

    // reimplemented virtual methods:
    virtual QCPPainter* startPainting() Q_DECL_OVERRIDE;
    virtual void draw(QCPPainter* painter) const Q_DECL_OVERRIDE;
    void clear(const QColor& color) Q_DECL_OVERRIDE;

protected:
    // non-property members:
    QPixmap mBuffer;

    // reimplemented virtual methods:
    virtual void reallocateBuffer() Q_DECL_OVERRIDE;
};
