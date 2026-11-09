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

#ifndef QCP_ITEM_PIXMAP_H
#define QCP_ITEM_PIXMAP_H

#include "../global.h"
#include "item.h"
#include <QImage>

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemPixmapOps;

class QCP_LIB_DECL QCPItemPixmap : public QCPAbstractItem
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)
    Q_PROPERTY(bool scaled READ scaled WRITE setScaled)
    Q_PROPERTY(Qt::AspectRatioMode aspectRatioMode READ aspectRatioMode)
    Q_PROPERTY(Qt::TransformationMode transformationMode READ transformationMode)
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemPixmapOps<T>;

    explicit QCPItemPixmap(QCustomPlot* parentPlot);
    virtual ~QCPItemPixmap() override;

    // getters:
    QPixmap pixmap() const { return mPixmap; }

    bool scaled() const { return mScaled; }

    Qt::AspectRatioMode aspectRatioMode() const { return mAspectRatioMode; }

    Qt::TransformationMode transformationMode() const { return mTransformationMode; }

    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    // setters;
    void setPixmap(const QPixmap& pixmap);
    void setScaled(bool scaled, Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio,
                   Qt::TransformationMode transformationMode = Qt::SmoothTransformation);
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);

    //! Test hook: the recordings that converted the picture to an image since the process started.
    //! A recording that finds the scale it already holds converts nothing.
    [[nodiscard]] static quint64 imageConversionCount();

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;

    QCPItemPosition* const topLeft;
    QCPItemPosition* const bottomRight;
    QCPItemAnchor* const top;
    QCPItemAnchor* const topRight;
    QCPItemAnchor* const right;
    QCPItemAnchor* const bottom;
    QCPItemAnchor* const bottomLeft;
    QCPItemAnchor* const left;

protected:
    enum AnchorIndex
    {
        aiTop,
        aiTopRight,
        aiRight,
        aiBottom,
        aiBottomLeft,
        aiLeft
    };

    // property members:
    QPixmap mPixmap;
    QPixmap mScaledPixmap;
    QImage mImage; //!< the pixmap as drawn, converted once per scale: what a recording embeds
    bool mScaled;
    bool mScaledPixmapInvalidated;
    Qt::AspectRatioMode mAspectRatioMode;
    Qt::TransformationMode mTransformationMode;
    QPen mPen, mSelectedPen;

    // reimplemented virtual methods:
    virtual QPointF anchorPixelPosition(int anchorId) const override;

    // non-virtual methods:
    void updateScaledPixmap(QRect finalRect = QRect(), bool flipHorz = false,
                            bool flipVert = false);
    QRectF getFinalRect(bool* flippedHorz = nullptr, bool* flippedVert = nullptr) const;
    QPen mainPen() const;
};

// The fluent handle of a QCPItemPixmap; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemPixmapOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemPixmapOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemPixmapOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setTopLeft(double key, double value)
    {
        return this->apply([&](T& i) { i.topLeft->setCoords(key, value); });
    }

    Ret setBottomRight(double key, double value)
    {
        return this->apply([&](T& i) { i.bottomRight->setCoords(key, value); });
    }

    Ret setPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setPen(pen); });
    }

    Ret setSelectedPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setSelectedPen(pen); });
    }
    Ret setPixmap(const QPixmap& pixmap)
    {
        return this->apply([&](T& i) { i.setPixmap(pixmap); });
    }

    Ret setScaled(bool scaled, Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio,
                  Qt::TransformationMode transformationMode = Qt::SmoothTransformation)
    {
        return this->apply(
            [&](T& i) { i.setScaled(scaled, aspectRatioMode, transformationMode); });
    }

};

#endif // QCP_ITEM_PIXMAP_H
