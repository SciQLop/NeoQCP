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

#ifndef QCP_LAYOUTELEMENT_COLORSCALE_H
#define QCP_LAYOUTELEMENT_COLORSCALE_H

#include "../axis/axis.h"
#include "../colorgradient.h"
#include "../global.h"
#include "../layout.h"
#include "../layoutelements/layoutelement-axisrect.h"

class QCPPainter;
class QCustomPlot;
class QCPColorMap;
class QCPColorScale;
class QCPAbstractColorMapped;

class QCPColorScaleAxisRectPrivate : public QCPAxisRect
{
    Q_OBJECT

public:
    explicit QCPColorScaleAxisRectPrivate(QCPColorScale* parentColorScale);

protected:
    QCPColorScale* mParentColorScale;
    QImage mGradientImage;
    bool mGradientImageInvalidated;
    // The scale inside a scale must not make a scale of its own.
    bool mayOwnColorScale() const override { return false; }
    // re-using some methods of QCPAxisRect to make them available to friend class QCPColorScale
    using QCPAxisRect::calculateAutoMargin;
    using QCPAxisRect::mouseMoveEvent;
    using QCPAxisRect::mousePressEvent;
    using QCPAxisRect::mouseReleaseEvent;
    using QCPAxisRect::update;
    using QCPAxisRect::wheelEvent;
    virtual void draw(QCPPainter* painter) override;
    void updateGradientImage();
    Q_SLOT void axisSelectionChanged();
    Q_SLOT void axisSelectableChanged();
    friend class QCPColorScale;
};

template <typename T, typename Self>
class QCPColorScaleOps;

class QCP_LIB_DECL QCPColorScale : public QCPLayoutElement
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QCPAxis::AxisType type READ type WRITE setType)
    Q_PROPERTY(QCPRange dataRange READ dataRange WRITE setDataRange NOTIFY dataRangeChanged)
    Q_PROPERTY(QCPAxis::ScaleType dataScaleType READ dataScaleType WRITE setDataScaleType NOTIFY
                   dataScaleTypeChanged)
    Q_PROPERTY(QCPColorGradient gradient READ gradient WRITE setGradient NOTIFY gradientChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel)
    Q_PROPERTY(int barWidth READ barWidth WRITE setBarWidth)
    Q_PROPERTY(bool rangeDrag READ rangeDrag WRITE setRangeDrag)
    Q_PROPERTY(bool rangeZoom READ rangeZoom WRITE setRangeZoom)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPColorScaleOps<T>;

    explicit QCPColorScale(QCustomPlot* parentPlot);
    virtual ~QCPColorScale() override;

    // getters:
    QCPAxis* axis() const { return mColorAxis.data(); }

    QCPAxis::AxisType type() const { return mType; }

    QCPRange dataRange() const { return mDataRange; }

    QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }

    QCPColorGradient gradient() const { return mGradient; }

    QString label() const;

    int barWidth() const { return mBarWidth; }

    bool rangeDrag() const;
    bool rangeZoom() const;

    // setters:
    void setType(QCPAxis::AxisType type);
    Q_SLOT void setDataRange(const QCPRange& dataRange);
    Q_SLOT void setDataScaleType(QCPAxis::ScaleType scaleType);
    Q_SLOT void setGradient(const QCPColorGradient& gradient);
    void setLabel(const QString& str);
    void setBarWidth(int width);
    void setRangeDrag(bool enabled);
    void setRangeZoom(bool enabled);

    // non-property methods:
    QList<QCPColorMap*> colorMaps() const;

    //! Every plottable that takes its gradient, range and scale type from this scale.
    QList<QCPAbstractColorMapped*> users() const { return mUsers; }

    void rescaleDataRange(bool onlyVisibleMaps);

    // reimplemented virtual methods:
    virtual void update(UpdatePhase phase) override;

signals:
    void dataRangeChanged(const QCPRange& newRange);
    void dataScaleTypeChanged(QCPAxis::ScaleType scaleType);
    void gradientChanged(const QCPColorGradient& newGradient);

protected:
    // property members:
    QCPAxis::AxisType mType;
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType;
    QCPColorGradient mGradient;
    int mBarWidth;

    // non-property members:
    QPointer<QCPColorScaleAxisRectPrivate> mAxisRect;
    QPointer<QCPAxis> mColorAxis;
    QList<QCPAbstractColorMapped*> mUsers;
    // The rect that owns and places this scale, null for a scale the user put in a layout.
    QPointer<QCPAxisRect> mOwnerRect;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual int calculateAutoMargin(QCP::MarginSide side) override;
    // events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void wheelEvent(QWheelEvent* event) override;

private:
    Q_DISABLE_COPY(QCPColorScale)

    // The owner reports a user or size change, which moves the margin the scale sits in.
    void usersChanged();

    friend class QCPAxisRect;
    friend class QCPColorScaleAxisRectPrivate;
    friend class QCPAbstractColorMapped;
};

// The handle of a colour scale: created in the layout by QCustomPlot::add<QCPColorScale>(row,
// column).
template <typename T, typename Self>
class QCPColorScaleOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPColorScaleOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPColorScaleOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    Ret setType(QCPAxis::AxisType type)
    {
        return this->apply([&](T& p) { p.setType(type); });
    }

    Ret setLabel(const QString& label)
    {
        return this->apply([&](T& p) { p.setLabel(label); });
    }

    Ret setBarWidth(int width)
    {
        return this->apply([&](T& p) { p.setBarWidth(width); });
    }

    Ret setGradient(const QCPColorGradient& gradient)
    {
        return this->apply([&](T& p) { p.setGradient(gradient); });
    }

    Ret setDataRange(const QCPRange& range)
    {
        return this->apply([&](T& p) { p.setDataRange(range); });
    }

    Ret setDataScaleType(QCPAxis::ScaleType type)
    {
        return this->apply([&](T& p) { p.setDataScaleType(type); });
    }
};

#endif // QCP_LAYOUTELEMENT_COLORSCALE_H
