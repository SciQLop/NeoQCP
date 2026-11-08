#ifndef QCP_ITEM_SPANBASE_H
#define QCP_ITEM_SPANBASE_H

#include "../global.h"
#include "item.h"

class QCPPainter;
class QCustomPlot;

class QCP_LIB_DECL QCPAbstractSpanItem : public QCPAbstractItem
{
    Q_OBJECT

public:
    using QCPAbstractItem::QCPAbstractItem;
    virtual ~QCPAbstractSpanItem() override;

    QPen pen() const { return mPen; }
    QPen selectedPen() const { return mSelectedPen; }
    QBrush brush() const { return mBrush; }
    QBrush selectedBrush() const { return mSelectedBrush; }
    QPen borderPen() const { return mBorderPen; }
    QPen selectedBorderPen() const { return mSelectedBorderPen; }
    bool movable() const { return mMovable; }

    // A span is drawn as a band (brush) and its edge lines (border pen). The item pen is kept
    // for API compatibility and not drawn: use setBorderPen() for the lines.
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);
    void setBrush(const QBrush& brush);
    void setSelectedBrush(const QBrush& brush);
    void setBorderPen(const QPen& pen);
    void setSelectedBorderPen(const QPen& pen);
    void setMovable(bool movable) { mMovable = movable; }

protected:
    QPen mPen, mSelectedPen;
    QBrush mBrush, mSelectedBrush;
    QPen mBorderPen, mSelectedBorderPen;
    bool mMovable = true;

    QPen mainPen() const { return mSelected ? mSelectedPen : mPen; }
    QBrush mainBrush() const { return mSelected ? mSelectedBrush : mBrush; }
    QPen mainBorderPen() const { return mSelected ? mSelectedBorderPen : mBorderPen; }

    void markRhiDirty();

    // Moves one coordinate of an edge to a pixel position, whatever the edge's position type is;
    // the other coordinate is left untouched.
    static void setEdgePixel(QCPItemPosition* edge, Qt::Orientation orientation, double pixel);

    // Returns true if draw() should return early (RHI layer handles rendering)
    bool tryRhiDraw(QCPPainter* painter);

protected:
    //! Whether a position of this span sits on a linked axis, which the layer cannot place.
    virtual bool usesLinkedAxis() const { return false; }
};

#endif // QCP_ITEM_SPANBASE_H
