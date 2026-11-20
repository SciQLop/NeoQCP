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

#ifndef QCP_LAYOUT_H
#define QCP_LAYOUT_H

#include <array>

#include "global.h"
#include "layer.h"

class QCPLayout;
class QCPLayoutElement;
class QCustomPlot;

class QCPAlignmentGroup;

class QCP_LIB_DECL QCPLayoutElement : public QCPLayerable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QRect outerRect READ outerRect WRITE setOuterRect NOTIFY outerRectChanged)
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins NOTIFY marginsChanged)
    Q_PROPERTY(QMargins minimumMargins READ minimumMargins WRITE setMinimumMargins NOTIFY
                   minimumMarginsChanged)
    Q_PROPERTY(QSize minimumSize READ minimumSize WRITE setMinimumSize NOTIFY minimumSizeChanged)
    Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize NOTIFY maximumSizeChanged)
    Q_PROPERTY(SizeConstraintRect sizeConstraintRect READ sizeConstraintRect WRITE
                   setSizeConstraintRect NOTIFY sizeConstraintRectChanged)
    Q_PROPERTY(QCP::MarginSides autoMargins READ autoMargins WRITE setAutoMargins NOTIFY
                   autoMarginsChanged)
    /// \endcond

public:
    /*!
      Defines the phases of the update process, that happens just before a replot. At each phase,
      \ref updateLayout is called with the according UpdatePhase value.
    */
    enum UpdatePhase
    {
        upPreparation ///< Phase used for any type of preparation that needs to be done before
                      ///< margin calculation and layout
        ,
        upMargins ///< Phase in which the margins are calculated and set
        ,
        upLayout ///< Final phase in which the layout system places the rects of the elements
    };
    Q_ENUM(UpdatePhase)

    /*!
      Defines to which rect of a layout element the size constraints that can be set via \ref
      setMinimumSize and \ref setMaximumSize apply. The outer rect (\ref outerRect) includes the
      margins (e.g. in the case of a QCPAxisRect the axis labels), whereas the inner rect (\ref
      rect) does not.

      \see setSizeConstraintRect
    */
    enum SizeConstraintRect
    {
        scrInnerRect ///< Minimum/Maximum size constraints apply to inner rect
        ,
        scrOuterRect ///< Minimum/Maximum size constraints apply to outer rect, thus include layout
                     ///< element margins
    };
    Q_ENUM(SizeConstraintRect)

    explicit QCPLayoutElement(QCustomPlot* parentPlot = nullptr);
    virtual ~QCPLayoutElement() override;

    // getters:
    // Out of line: the handle of the parent layout needs the complete QCPLayout (K1).
    QCPHandle<QCPLayout> layout() const;

    QRect rect() const { return mRect; }

    QRect outerRect() const { return mOuterRect; }

    QMargins margins() const { return mMargins; }

    QMargins minimumMargins() const { return mMinimumMargins; }

    QCP::MarginSides autoMargins() const { return mAutoMargins; }

    QSize minimumSize() const { return mMinimumSize; }

    QSize maximumSize() const { return mMaximumSize; }

    SizeConstraintRect sizeConstraintRect() const { return mSizeConstraintRect; }

    //! The group this element's \a side aligns with, if any: an element joins one group per side.
    QPointer<QCPAlignmentGroup> alignmentGroup(QCP::MarginSide side) const;

    //! Whether this element's destructor has started. A slot that runs there must not delete it again.
    [[nodiscard]] bool isBeingDestroyed() const { return mBeingDestroyed; }

    // setters:
    void setOuterRect(const QRect& rect);
    void setMargins(const QMargins& margins);
    void setMinimumMargins(const QMargins& margins);
    void setAutoMargins(QCP::MarginSides sides);
    void setMinimumSize(const QSize& size);
    void setMinimumSize(int width, int height);
    void setMaximumSize(const QSize& size);
    void setMaximumSize(int width, int height);
    void setSizeConstraintRect(SizeConstraintRect constraintRect);

    // introduced virtual methods:
    virtual void updateLayout(UpdatePhase phase);
    // A layout setter (margins, spacing, stretch, insets) must call this first: it invalidates what
    // this element draws and, for a layout, everything it arranges.
    virtual void markLayoutAppearanceDirty();
    virtual QSize minimumOuterSizeHint() const;
    virtual QSize maximumOuterSizeHint() const;
    virtual QList<QCPHandle<QCPLayoutElement>> elements(bool recursive) const;

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

signals:
    void outerRectChanged(const QRect& outerRect);
    void marginsChanged(const QMargins& margins);
    void minimumMarginsChanged(const QMargins& minimumMargins);
    void minimumSizeChanged(const QSize& minimumSize);
    void maximumSizeChanged(const QSize& maximumSize);
    void sizeConstraintRectChanged(QCPLayoutElement::SizeConstraintRect sizeConstraintRect);
    void autoMarginsChanged(QCP::MarginSides autoMargins);

protected:
    // property members:
    QCPLayout* mParentLayout;
    QSize mMinimumSize, mMaximumSize;
    SizeConstraintRect mSizeConstraintRect;
    QRect mRect, mOuterRect;
    QMargins mMargins, mMinimumMargins;
    QCP::MarginSides mAutoMargins;
    // One alignment group per side, in the order of alignmentSlot(): left, right, top, bottom.
    std::array<QPointer<QCPAlignmentGroup>, 4> mAlignmentGroups;
    //! Set by a destructor that runs user slots, so a slot that deletes this element is refused.
    bool mBeingDestroyed = false;

    // introduced virtual methods:
    virtual int calculateAutoMargin(QCP::MarginSide side);
    virtual void layoutChanged();
    //! The child elements raw, for the layout's own passes (K8). The default converts elements().
    virtual QList<QCPLayoutElement*> elementsRaw(bool recursive) const;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint([[maybe_unused]] QCPPainter* painter) const override
    {
    }

    virtual void draw([[maybe_unused]] QCPPainter* painter) override { }

    virtual void parentPlotInitialized(QCustomPlot* parentPlot) override;

private:
    Q_DISABLE_COPY_MOVE(QCPLayoutElement)

    //! The index of \a side in mAlignmentGroups, or -1 for anything but one of the four sides.
    static int alignmentSlot(QCP::MarginSide side);
    bool alignedEdgeMoved() const;

    friend class QCustomPlot;
    friend class QCPLayout;
    friend class QCPAlignmentGroup;
    friend class QCPPlottable; // walks the layout for the legend rows a name change touches
};

class QCP_LIB_DECL QCPLayout : public QCPLayoutElement
{
    Q_OBJECT

public:
    explicit QCPLayout();

    // reimplemented virtual methods:
    virtual void updateLayout(UpdatePhase phase) override;
    virtual QList<QCPHandle<QCPLayoutElement>> elements(bool recursive) const override;

    // introduced virtual methods:
    virtual int elementCount() const = 0;
    virtual QCPHandle<QCPLayoutElement> elementAt(int index) const = 0;
    //! take() and takeAt() release the element but do not delete it: it is re-parented to the plot, which
    //! keeps it alive until the plot dies or the element is added again (so a script cannot orphan it).
    Q_INVOKABLE virtual QCPLayoutElement* takeAt(int index) = 0;
    Q_INVOKABLE virtual bool take(QCPLayoutElement* element) = 0;
    Q_INVOKABLE virtual void simplify();

    // non-virtual methods:
    Q_INVOKABLE bool removeAt(int index);
    Q_INVOKABLE bool remove(QCPLayoutElement* element);
    Q_INVOKABLE void clear();

    // introduced virtual methods:
    void markLayoutAppearanceDirty() override;

protected:
    virtual void layoutChildren();
    //! The child elements raw, for the layout's own passes (K8).
    virtual QList<QCPLayoutElement*> elementsRaw(bool recursive) const override;
    //! elementAt() raw, for the layout's own passes (K8). The default converts elementAt().
    virtual QCPLayoutElement* elementAtRaw(int index) const;

    // non-virtual methods:
    void sizeConstraintsChanged() const;
    void adoptElement(QCPLayoutElement* el);
    void releaseElement(QCPLayoutElement* el);
    //! True when \a el already belongs to a plot other than this layout's: an area lives in the
    //! plot it was built in, and moving one means building a new one there.
    bool elementBelongsToAnotherPlot(const QCPLayoutElement* el) const;
    QVector<int> getSectionSizes(QVector<int> maxSizes, QVector<int> minSizes,
                                 QVector<double> stretchFactors, int totalSize) const;
    static QSize getFinalMinimumOuterSize(const QCPLayoutElement* el);
    static QSize getFinalMaximumOuterSize(const QCPLayoutElement* el);

private:
    Q_DISABLE_COPY_MOVE(QCPLayout)
    friend class QCPLayoutElement;
    friend class QCustomPlot; // its own walks read the children raw (K8)
};

class QCP_LIB_DECL QCPLayoutGrid : public QCPLayout
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QList<double> columnStretchFactors READ columnStretchFactors WRITE
                   setColumnStretchFactors NOTIFY columnStretchFactorsChanged)
    Q_PROPERTY(QList<double> rowStretchFactors READ rowStretchFactors WRITE setRowStretchFactors
                   NOTIFY rowStretchFactorsChanged)
    Q_PROPERTY(
        int columnSpacing READ columnSpacing WRITE setColumnSpacing NOTIFY columnSpacingChanged)
    Q_PROPERTY(int rowSpacing READ rowSpacing WRITE setRowSpacing NOTIFY rowSpacingChanged)
    Q_PROPERTY(FillOrder fillOrder READ fillOrder WRITE setFillOrder NOTIFY fillOrderChanged)
    Q_PROPERTY(int wrap READ wrap WRITE setWrap NOTIFY wrapChanged)
    /// \endcond

public:
    /*!
      Defines in which direction the grid is filled when using \ref addElement(QCPLayoutElement*).
      The column/row at which wrapping into the next row/column occurs can be specified with \ref
      setWrap.

      \see setFillOrder
    */
    enum FillOrder
    {
        foRowsFirst ///< Rows are filled first, and a new element is wrapped to the next column if
                    ///< the row count would exceed \ref setWrap.
        ,
        foColumnsFirst ///< Columns are filled first, and a new element is wrapped to the next row
                       ///< if the column count would exceed \ref setWrap.
    };
    Q_ENUM(FillOrder)

    explicit QCPLayoutGrid();
    virtual ~QCPLayoutGrid() override;

    // getters:
    int rowCount() const { return mElements.size(); }

    int columnCount() const { return mElements.size() > 0 ? mElements.first().size() : 0; }

    QList<double> columnStretchFactors() const { return mColumnStretchFactors; }

    QList<double> rowStretchFactors() const { return mRowStretchFactors; }

    int columnSpacing() const { return mColumnSpacing; }

    int rowSpacing() const { return mRowSpacing; }

    int wrap() const { return mWrap; }

    FillOrder fillOrder() const { return mFillOrder; }

    // setters:
    void setColumnStretchFactor(int column, double factor);
    void setColumnStretchFactors(const QList<double>& factors);
    void setRowStretchFactor(int row, double factor);
    void setRowStretchFactors(const QList<double>& factors);
    void setColumnSpacing(int pixels);
    void setRowSpacing(int pixels);
    void setWrap(int count);
    void setFillOrder(FillOrder order, bool rearrange = true);

    // reimplemented virtual methods:
    virtual void layoutChildren() override;

    virtual int elementCount() const override { return rowCount() * columnCount(); }

    virtual QCPHandle<QCPLayoutElement> elementAt(int index) const override;
    virtual QCPLayoutElement* takeAt(int index) override;
    virtual bool take(QCPLayoutElement* element) override;
    virtual void simplify() override;
    virtual QSize minimumOuterSizeHint() const override;
    virtual QSize maximumOuterSizeHint() const override;

    // non-virtual methods:
    QCPHandle<QCPLayoutElement> element(int row, int column) const;
    Q_INVOKABLE bool addElement(int row, int column, QCPLayoutElement* element);
    bool addElement(QCPLayoutElement* element);
    bool hasElement(int row, int column);
    Q_INVOKABLE void expandTo(int newRowCount, int newColumnCount);
    Q_INVOKABLE void insertRow(int newIndex);
    Q_INVOKABLE void insertColumn(int newIndex);
    int rowColToIndex(int row, int column) const;
    void indexToRowCol(int index, int& row, int& column) const;

signals:
    void columnStretchFactorsChanged(const QList<double>& columnStretchFactors);
    void rowStretchFactorsChanged(const QList<double>& rowStretchFactors);
    void columnSpacingChanged(int columnSpacing);
    void rowSpacingChanged(int rowSpacing);
    void fillOrderChanged(QCPLayoutGrid::FillOrder fillOrder);
    void wrapChanged(int wrap);

protected:
    virtual QCPLayoutElement* elementAtRaw(int index) const override;

    // property members:
    QList<QList<QCPLayoutElement*>> mElements;
    QList<double> mColumnStretchFactors;
    QList<double> mRowStretchFactors;
    int mColumnSpacing, mRowSpacing;
    int mWrap;
    FillOrder mFillOrder;

    // non-virtual methods:
    void getMinimumRowColSizes(QVector<int>* minColWidths, QVector<int>* minRowHeights) const;
    void getMaximumRowColSizes(QVector<int>* maxColWidths, QVector<int>* maxRowHeights) const;

private:
    void stretchFactorsMoved(const QList<double>& columns, const QList<double>& rows);
    //! setFillOrder() takes every element out and back: it signals the lists once, at the end.
    bool mRearranging = false;

    Q_DISABLE_COPY_MOVE(QCPLayoutGrid)
};

class QCP_LIB_DECL QCPLayoutInset : public QCPLayout
{
    Q_OBJECT

public:
    /*!
      Defines how the placement and sizing is handled for a certain element in a QCPLayoutInset.
    */
    enum InsetPlacement
    {
        ipFree ///< The element may be positioned/sized arbitrarily, see \ref setInsetRect
        ,
        ipBorderAligned ///< The element is aligned to one of the layout sides, see \ref
                        ///< setInsetAlignment
        ,
        ipInnerRect ///< The element's inner rect (its outer rect less its margins) takes the inset
                    ///< rect, measured in the rect of the area the layout belongs to, not in this
                    ///< layout's own (margin-reduced) rect: see QCPAxisRect::addInset
    };
    Q_ENUM(InsetPlacement)

    explicit QCPLayoutInset();
    virtual ~QCPLayoutInset() override;

    // getters:
    InsetPlacement insetPlacement(int index) const;
    Qt::Alignment insetAlignment(int index) const;
    QRectF insetRect(int index) const;

    // setters:
    void setInsetPlacement(int index, InsetPlacement placement);
    void setInsetAlignment(int index, Qt::Alignment alignment);
    void setInsetRect(int index, const QRectF& rect);

    // reimplemented virtual methods:
    virtual void layoutChildren() override;
    virtual int elementCount() const override;
    virtual QCPHandle<QCPLayoutElement> elementAt(int index) const override;
    virtual QCPLayoutElement* takeAt(int index) override;
    virtual bool take(QCPLayoutElement* element) override;

    virtual void simplify() override { }

    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    // non-virtual methods:
    Q_INVOKABLE void addElement(QCPLayoutElement* element, Qt::Alignment alignment);
    void addElement(QCPLayoutElement* element, const QRectF& rect);

protected:
    virtual QCPLayoutElement* elementAtRaw(int index) const override;

    // property members:
    QList<QCPLayoutElement*> mElements;
    QList<InsetPlacement> mInsetPlacement;
    QList<Qt::Alignment> mInsetAlignment;
    QList<QRectF> mInsetRect;

private:
    Q_DISABLE_COPY_MOVE(QCPLayoutInset)
};

#endif // QCP_LAYOUT_H
