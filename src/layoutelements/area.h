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

#ifndef QCP_LAYOUTELEMENTS_AREA_H
#define QCP_LAYOUTELEMENTS_AREA_H

#include "../global.h"
#include "../layout.h"

class QCPPainter;
class QCustomPlot;
class QCPAxisBase;

/*!
  The parts every area of a plot has: a background, an inset layout for elements that sit in the
  area, and a place for a colour scale (which a plain area does not have).
*/
class QCP_LIB_DECL QCPArea : public QCPLayoutElement
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPixmap background READ background WRITE setBackground)
    Q_PROPERTY(bool backgroundScaled READ backgroundScaled WRITE setBackgroundScaled)
    Q_PROPERTY(Qt::AspectRatioMode backgroundScaledMode READ backgroundScaledMode WRITE
                   setBackgroundScaledMode)
    /// \endcond

public:
    explicit QCPArea(QCustomPlot* parentPlot);
    virtual ~QCPArea() override;

    // getters:
    QPixmap background() const { return mBackgroundPixmap; }

    QBrush backgroundBrush() const { return mBackgroundBrush; }

    bool backgroundScaled() const { return mBackgroundScaled; }

    Qt::AspectRatioMode backgroundScaledMode() const { return mBackgroundScaledMode; }

    QCPLayoutInset* insetLayout() const { return mInsetLayout; }

    // setters:
    void setBackground(const QPixmap& pm);
    void setBackground(const QPixmap& pm, bool scaled,
                       Qt::AspectRatioMode mode = Qt::KeepAspectRatioByExpanding);
    void setBackground(const QBrush& brush);
    void setBackgroundScaled(bool scaled);
    void setBackgroundScaledMode(Qt::AspectRatioMode mode);

    // reimplemented virtual methods:
    virtual void update(UpdatePhase phase) override;
    virtual QList<QCPLayoutElement*> elements(bool recursive) const override;

    //! The axes of the area, as the base type the range model lives in.
    virtual QList<QCPAxisBase*> allAxes() const = 0;

protected:
    // property members:
    QBrush mBackgroundBrush;
    QPixmap mBackgroundPixmap;
    QPixmap mScaledBackgroundPixmap;
    bool mBackgroundScaled;
    Qt::AspectRatioMode mBackgroundScaledMode;
    QCPLayoutInset* mInsetLayout;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;

    // non-property methods:
    //! Draws the background inside the area's shape: the whole rect here, a subclass may narrow it
    //! (a polar area keeps it inside its disc).
    virtual void drawBackground(QCPPainter* painter);
    //! The two parts of the background, so a subclass can give either one its own shape.
    virtual void drawBackgroundBrush(QCPPainter* painter);
    virtual void drawBackgroundPixmap(QCPPainter* painter);
    //! Deletes the inset layout now. A subclass calls it before it drops what its plot still has to
    //! deliver: the elements leaving the layout emit. The destructor calls it if nobody did.
    void destroyInsetLayout();

private:
    Q_DISABLE_COPY(QCPArea)
};

#endif // QCP_LAYOUTELEMENTS_AREA_H
