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

#ifndef QCP_LAYOUTELEMENT_LEGEND_H
#define QCP_LAYOUTELEMENT_LEGEND_H

#include "../global.h"
#include <QPointer>
#include "../layer.h"
#include "../layout.h"

class QCPPainter;
class QCustomPlot;
class QCPPlottable;
class QCPLegend;

class QCP_LIB_DECL QCPLegendItem : public QCPLayoutElement
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(
        QFont selectedFont READ selectedFont WRITE setSelectedFont NOTIFY selectedFontChanged)
    Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE setSelectedTextColor NOTIFY
                   selectedTextColorChanged)
    Q_PROPERTY(
        QCP::SelectionType selectable READ selectable WRITE setSelectable NOTIFY selectableChanged)
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectionChanged)
    /// \endcond

public:
    explicit QCPLegendItem(QCPLegend* parent);

    // getters:
    QCPHandle<QCPLegend> parentLegend() const;

    QFont font() const { return mFont; }

    QColor textColor() const { return mTextColor; }

    QFont selectedFont() const { return mSelectedFont; }

    QColor selectedTextColor() const { return mSelectedTextColor; }

    QCP::SelectionType selectable() const { return mSelectable ? QCP::stWhole : QCP::stNone; }

    bool selected() const { return mSelected; }

    // setters:
    void setFont(const QFont& font);
    void setTextColor(const QColor& color);
    void setSelectedFont(const QFont& font);
    void setSelectedTextColor(const QColor& color);
    Q_SLOT void setSelectable(QCP::SelectionType selectable);
    Q_SLOT void setSelected(bool selected);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

signals:
    void selectionChanged();
    void selectableChanged();
    void fontChanged(const QFont& font);
    void textColorChanged(const QColor& textColor);
    void selectedFontChanged(const QFont& selectedFont);
    void selectedTextColorChanged(const QColor& selectedTextColor);

protected:
    // property members:
    QFont mFont;
    QColor mTextColor;
    QFont mSelectedFont;
    QColor mSelectedTextColor;
    bool mSelectable, mSelected;

    //! The busy symbol is part of this item, but the plottable's own layer is not the
    //! legend's: repaint the legend layer when the symbol shows or hides. This only marks
    //! the layer dirty; the replot that paints it (and re-lays out the wider legend) is the
    //! one QCPPlottable::onDebounceTimeout queues right after visuallyBusyChanged.
    void repaintWhenBusyChanges(QCPPlottable* plottable);
    //! A row about \a subject has nothing left to show once it is gone: it leaves its legend.
    void leaveLegendWhenDestroyed(QObject* subject);

    //! The legend this row sits in, read from its layout: null in another layout, or in none.
    [[nodiscard]] QCPLegend* legend() const;
    //! Outside a legend a row has no icon: it draws and measures as text only.
    [[nodiscard]] QSize legendIconSize() const;
    [[nodiscard]] int legendIconTextPadding() const;
    //! The icon border for the selection state; no pen outside a legend.
    [[nodiscard]] QPen legendIconBorderPen() const;
    //! Selectable as a row: the row allows it and its legend selects items. Never outside a legend.
    [[nodiscard]] bool selectableAsItem() const;

    // reimplemented virtual methods:
    virtual QCP::Interaction selectionCategory() const override;
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual QRect clipRect() const override;
    virtual void draw(QCPPainter* painter) override = 0;
    // events:
    virtual void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                             bool* selectionStateChanged) override;
    virtual void deselectEvent(bool* selectionStateChanged) override;

private:
    Q_DISABLE_COPY_MOVE(QCPLegendItem)

    friend class QCPLegend;
};

class QCP_LIB_DECL QCPPlottableLegendItem : public QCPLegendItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPPlottableLegendItem)

public:
    QCPPlottableLegendItem(QCPLegend* parent, QCPPlottable* plottable);

    // getters:
    QCPHandle<QCPPlottable> plottable();

protected:
    // property members:
    QPointer<QCPPlottable> mPlottable;

    // reimplemented virtual methods:
    virtual void draw(QCPPainter* painter) override;
    virtual QSize minimumOuterSizeHint() const override;

    // non-virtual methods:
    QColor getTextColor() const;
    QFont getFont() const;

private:
    // Raw, for the walks over every row (K8: no handle per row).
    [[nodiscard]] QCPPlottable* plottableRaw() const { return mPlottable.data(); }

    friend class QCPLegend;
    friend class QCPPlottable;
};

template <typename T, typename Self = void>
class QCPLegendOps;

class QCP_LIB_DECL QCPLegend : public QCPLayoutGrid
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPen borderPen READ borderPen WRITE setBorderPen NOTIFY borderPenChanged)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(int iconTextPadding READ iconTextPadding WRITE setIconTextPadding NOTIFY
                   iconTextPaddingChanged)
    Q_PROPERTY(
        QPen iconBorderPen READ iconBorderPen WRITE setIconBorderPen NOTIFY iconBorderPenChanged)
    Q_PROPERTY(SelectableParts selectableParts READ selectableParts WRITE setSelectableParts NOTIFY
                   selectableChanged)
    Q_PROPERTY(SelectableParts selectedParts READ selectedParts WRITE setSelectedParts NOTIFY
                   selectionChanged)
    Q_PROPERTY(QPen selectedBorderPen READ selectedBorderPen WRITE setSelectedBorderPen NOTIFY
                   selectedBorderPenChanged)
    Q_PROPERTY(QPen selectedIconBorderPen READ selectedIconBorderPen WRITE setSelectedIconBorderPen
                   NOTIFY selectedIconBorderPenChanged)
    Q_PROPERTY(
        QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY selectedBrushChanged)
    Q_PROPERTY(
        QFont selectedFont READ selectedFont WRITE setSelectedFont NOTIFY selectedFontChanged)
    Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE setSelectedTextColor NOTIFY
                   selectedTextColorChanged)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPLegendOps<T>;

    /*!
      Defines the selectable parts of a legend

      \see setSelectedParts, setSelectableParts
    */
    enum SelectablePart
    {
        spNone = 0x000 ///< <tt>0x000</tt> None
        ,
        spLegendBox = 0x001 ///< <tt>0x001</tt> The legend box (frame)
        ,
        spItems = 0x002 ///< <tt>0x002</tt> Legend items individually (see \ref selectedItems)
    };
    Q_ENUM(SelectablePart)
    Q_DECLARE_FLAGS(SelectableParts, SelectablePart)
    Q_FLAG(SelectableParts)

    explicit QCPLegend();
    virtual ~QCPLegend() override;

    // getters:
    QPen borderPen() const { return mBorderPen; }

    QBrush brush() const { return mBrush; }

    QFont font() const { return mFont; }

    QColor textColor() const { return mTextColor; }

    QSize iconSize() const { return mIconSize; }

    int iconTextPadding() const { return mIconTextPadding; }

    QPen iconBorderPen() const { return mIconBorderPen; }

    SelectableParts selectableParts() const { return mSelectableParts; }

    SelectableParts selectedParts() const;

    bool selected() const { return selectedParts() != spNone; }

    QCP::SelectionType selectable() const
    {
        return mSelectableParts != spNone ? QCP::stWhole : QCP::stNone;
    }

    QPen selectedBorderPen() const { return mSelectedBorderPen; }

    QPen selectedIconBorderPen() const { return mSelectedIconBorderPen; }

    QBrush selectedBrush() const { return mSelectedBrush; }

    QFont selectedFont() const { return mSelectedFont; }

    QColor selectedTextColor() const { return mSelectedTextColor; }

    // setters:
    void setBorderPen(const QPen& pen);
    void setBrush(const QBrush& brush);
    void setFont(const QFont& font);
    void setTextColor(const QColor& color);
    void setIconSize(const QSize& size);
    void setIconSize(int width, int height);
    void setIconTextPadding(int padding);
    void setIconBorderPen(const QPen& pen);
    Q_SLOT void setSelectableParts(const SelectableParts& selectableParts);
    Q_SLOT void setSelectedParts(const SelectableParts& selectedParts);
    void setSelectedBorderPen(const QPen& pen);
    void setSelectedIconBorderPen(const QPen& pen);
    void setSelectedBrush(const QBrush& brush);
    void setSelectedFont(const QFont& font);
    void setSelectedTextColor(const QColor& color);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    // non-virtual methods:
    QCPHandle<QCPLegendItem> item(int index) const;
    QCPHandle<QCPPlottableLegendItem> itemWithPlottable(const QCPPlottable* plottable) const;
    int itemCount() const;
    bool hasItem(QCPLegendItem* item) const;
    bool hasItemWithPlottable(const QCPPlottable* plottable) const;
    Q_INVOKABLE bool addItem(QCPLegendItem* item);
    // An item leaving can take the last selected one with it: selectedParts may move.
    QCPLayoutElement* takeAt(int index) override;
    Q_INVOKABLE bool removeItem(int index);
    bool removeItem(QCPLegendItem* item);
    Q_INVOKABLE void clearItems();
    QList<QCPHandle<QCPLegendItem>> selectedItems() const;

signals:
    void selectionChanged();
    void selectableChanged();
    void borderPenChanged(const QPen& borderPen);
    void brushChanged(const QBrush& brush);
    void fontChanged(const QFont& font);
    void textColorChanged(const QColor& textColor);
    void iconSizeChanged(const QSize& iconSize);
    void iconTextPaddingChanged(int iconTextPadding);
    void iconBorderPenChanged(const QPen& iconBorderPen);
    void selectedBorderPenChanged(const QPen& selectedBorderPen);
    void selectedIconBorderPenChanged(const QPen& selectedIconBorderPen);
    void selectedBrushChanged(const QBrush& selectedBrush);
    void selectedFontChanged(const QFont& selectedFont);
    void selectedTextColorChanged(const QColor& selectedTextColor);

protected:
    // property members:
    QPen mBorderPen, mIconBorderPen;
    QBrush mBrush;
    QFont mFont;
    QColor mTextColor;
    QSize mIconSize;
    int mIconTextPadding;
    SelectableParts mSelectedParts, mSelectableParts;
    QPen mSelectedBorderPen, mSelectedIconBorderPen;
    QBrush mSelectedBrush;
    QFont mSelectedFont;
    QColor mSelectedTextColor;

    // reimplemented virtual methods:
    virtual void parentPlotInitialized(QCustomPlot* parentPlot) override;
    virtual QCP::Interaction selectionCategory() const override;
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;
    // events:
    virtual void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                             bool* selectionStateChanged) override;
    virtual void deselectEvent(bool* selectionStateChanged) override;

    // non-virtual methods:
    QPen getBorderPen() const;
    QBrush getBrush() const;

private:
    Q_DISABLE_COPY_MOVE(QCPLegend)

    //! Signals selectionChanged when an item's selection, or an item leaving, moved selectedParts.
    void noteItemSelection();
    bool mApplyingSelection = false; // setSelectedParts deselects the items and signals once

    // The rows raw: the legend's own setters and selection passes loop over them (K8).
    QCPLegendItem* itemRaw(int index) const;

    friend class QCustomPlot;
    friend class QCPLegendItem;
    friend class QCPPlottable; // walks the rows raw when a name change marks them
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPLegend::SelectableParts)

// The handle of a legend; the plot's default one is wrapped with qcp::makeHandle<QCPLegendOps<QCPLegend>>(plot.legend).
template <typename T, typename Self>
class QCPLegendOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPLegendOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPLegendOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setBorderPen(const QPen& pen)
    {
        return this->apply([&](T& p) { p.setBorderPen(pen); });
    }
    Ret setBrush(const QBrush& brush)
    {
        return this->apply([&](T& p) { p.setBrush(brush); });
    }
    Ret setFont(const QFont& font)
    {
        return this->apply([&](T& p) { p.setFont(font); });
    }
    Ret setTextColor(const QColor& color)
    {
        return this->apply([&](T& p) { p.setTextColor(color); });
    }
    Ret setIconSize(const QSize& size)
    {
        return this->apply([&](T& p) { p.setIconSize(size); });
    }
    Ret setIconTextPadding(int padding)
    {
        return this->apply([&](T& p) { p.setIconTextPadding(padding); });
    }
};

#endif // QCP_LAYOUTELEMENT_LEGEND_H
