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

#ifndef QCP_ITEM_TEXT_H
#define QCP_ITEM_TEXT_H

#include "../drawable.h"
#include "../global.h"
#include "../painting/draw-context.h"
#include "item.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemTextOps;

class QCP_LIB_DECL QCPItemText : public QCPItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPItemText)
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(
        QColor selectedColor READ selectedColor WRITE setSelectedColor NOTIFY selectedColorChanged)
    Q_PROPERTY(QPen pen READ pen WRITE setPen NOTIFY penChanged)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen NOTIFY selectedPenChanged)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
    Q_PROPERTY(
        QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY selectedBrushChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(
        QFont selectedFont READ selectedFont WRITE setSelectedFont NOTIFY selectedFontChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(Qt::Alignment positionAlignment READ positionAlignment WRITE setPositionAlignment
                   NOTIFY positionAlignmentChanged)
    Q_PROPERTY(Qt::Alignment textAlignment READ textAlignment WRITE setTextAlignment NOTIFY
                   textAlignmentChanged)
    Q_PROPERTY(double rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(QMargins padding READ padding WRITE setPadding NOTIFY paddingChanged)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemTextOps<T>;

    QCPItemText();
    virtual ~QCPItemText() override;

    // getters:
    QColor color() const { return mColor; }

    QColor selectedColor() const { return mSelectedColor; }

    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    QBrush brush() const { return mBrush; }

    QBrush selectedBrush() const { return mSelectedBrush; }

    QFont font() const { return mFont; }

    QFont selectedFont() const { return mSelectedFont; }

    QString text() const { return mText; }

    Qt::Alignment positionAlignment() const { return mPositionAlignment; }

    Qt::Alignment textAlignment() const { return mTextAlignment; }

    double rotation() const { return mRotation; }

    QMargins padding() const { return mPadding; }

    //! Fills the colours a caller never set: the plot's theme, applied when the item is placed.
    void applyThemeColors(const QColor& color, const QColor& selectedColor);

    // setters;
    void setColor(const QColor& color);
    void setSelectedColor(const QColor& color);
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);
    void setBrush(const QBrush& brush);
    void setSelectedBrush(const QBrush& brush);
    void setFont(const QFont& font);
    void setSelectedFont(const QFont& font);
    void setText(const QString& text);
    void setPositionAlignment(Qt::Alignment alignment);
    void setTextAlignment(Qt::Alignment alignment);
    void setRotation(double degrees);
    void setPadding(const QMargins& padding);

    //! Places the item, the way QCPItemPosition::set does: a QCP::Data position follows a pan.
    void setPosition(const QCP::Position& position);

    QCPItemPosition* const position;
    QCPItemAnchor* const topLeft;
    QCPItemAnchor* const top;
    QCPItemAnchor* const topRight;
    QCPItemAnchor* const right;
    QCPItemAnchor* const bottomRight;
    QCPItemAnchor* const bottom;
    QCPItemAnchor* const bottomLeft;
    QCPItemAnchor* const left;

signals:
    void colorChanged(const QColor& color);
    void selectedColorChanged(const QColor& selectedColor);
    void penChanged(const QPen& pen);
    void selectedPenChanged(const QPen& selectedPen);
    void brushChanged(const QBrush& brush);
    void selectedBrushChanged(const QBrush& selectedBrush);
    void fontChanged(const QFont& font);
    void selectedFontChanged(const QFont& selectedFont);
    void textChanged(const QString& text);
    void positionAlignmentChanged(Qt::Alignment positionAlignment);
    void textAlignmentChanged(Qt::Alignment textAlignment);
    void rotationChanged(double rotation);
    void paddingChanged(const QMargins& padding);

protected:
    enum AnchorIndex
    {
        aiTopLeft,
        aiTop,
        aiTopRight,
        aiRight,
        aiBottomRight,
        aiBottom,
        aiBottomLeft,
        aiLeft
    };

    // property members:
    QColor mColor, mSelectedColor;
    //! Whether each colour is still the constructor's default, and so the theme's to fill.
    bool mColorFromTheme = true;
    bool mSelectedColorFromTheme = true;
    QPen mPen, mSelectedPen;
    QBrush mBrush, mSelectedBrush;
    QFont mFont, mSelectedFont;
    QString mText;
    Qt::Alignment mPositionAlignment;
    Qt::Alignment mTextAlignment;
    double mRotation;
    QMargins mPadding;
    //! The measurement the box queries reuse; text and font are the only inputs it depends on.
    mutable QSize mGlyphSize;
    mutable QString mGlyphText;
    mutable QFont mGlyphFont;
    mutable bool mGlyphMath = false;
    mutable bool mGlyphValid = false;

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;
    std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const override;
    virtual QPointF anchorPixelPosition(int anchorId) const override;

    // non-virtual methods:
    //! What the item draws, hit-tests and anchors with: the selected variants when selected.
    QCPTextStyle textStyle() const;
    //! The box around the text before rotation, placed as if the position were the origin.
    QRectF textBoxAtOrigin() const;
    //! The size of the text in \a style, measured once per text and font.
    QSize glyphSize(const QCPTextStyle& style) const;
    QPointF getTextDrawPoint(const QPointF& pos, const QRectF& rect,
                             Qt::Alignment positionAlignment) const;
};

// The fluent handle of a QCPItemText; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemTextOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemTextOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemTextOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setPosition(double key, double value)
    {
        return this->apply([&](T& i) { i.position->setCoords(key, value); });
    }

    Ret setColor(const QColor& color)
    {
        return this->apply([&](T& o) { o.setColor(color); });
    }

    Ret setSelectedColor(const QColor& color)
    {
        return this->apply([&](T& o) { o.setSelectedColor(color); });
    }

    Ret setPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setPen(pen); });
    }

    Ret setSelectedPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setSelectedPen(pen); });
    }

    Ret setBrush(const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setBrush(brush); });
    }

    Ret setSelectedBrush(const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setSelectedBrush(brush); });
    }

    Ret setFont(const QFont& font)
    {
        return this->apply([&](T& o) { o.setFont(font); });
    }

    Ret setSelectedFont(const QFont& font)
    {
        return this->apply([&](T& o) { o.setSelectedFont(font); });
    }

    Ret setText(const QString& text)
    {
        return this->apply([&](T& o) { o.setText(text); });
    }

    Ret setPositionAlignment(Qt::Alignment alignment)
    {
        return this->apply([&](T& o) { o.setPositionAlignment(alignment); });
    }

    Ret setTextAlignment(Qt::Alignment alignment)
    {
        return this->apply([&](T& o) { o.setTextAlignment(alignment); });
    }

    Ret setRotation(double degrees)
    {
        return this->apply([&](T& o) { o.setRotation(degrees); });
    }
    Ret setPadding(const QMargins& padding)
    {
        return this->apply([&](T& i) { i.setPadding(padding); });
    }

};

#endif // QCP_ITEM_TEXT_H
