#ifndef QCP_ITEM_SPANBASE_H
#define QCP_ITEM_SPANBASE_H

#include "../global.h"
#include "item.h"
#include <QScopedValueRollback>

class QCustomPlot;

//! What every span has: the pens, the brush and the movable flag, and the edge drag they share.
class QCP_LIB_DECL QCPAbstractSpanItem : public QCPAbstractItem
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
    Q_PROPERTY(
        QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush NOTIFY selectedBrushChanged)
    Q_PROPERTY(QPen borderPen READ borderPen WRITE setBorderPen NOTIFY borderPenChanged)
    Q_PROPERTY(QPen selectedBorderPen READ selectedBorderPen WRITE setSelectedBorderPen NOTIFY
                   selectedBorderPenChanged)
    Q_PROPERTY(bool movable READ movable WRITE setMovable NOTIFY movableChanged)
    /// \endcond

public:
    using QCPAbstractItem::QCPAbstractItem;
    virtual ~QCPAbstractSpanItem() override;

    QBrush brush() const { return mBrush; }

    QBrush selectedBrush() const { return mSelectedBrush; }

    QPen borderPen() const { return mBorderPen; }

    QPen selectedBorderPen() const { return mSelectedBorderPen; }

    bool movable() const { return mMovable; }

    // A span is drawn as a band (brush) and its edge lines (border pen).
    void setBrush(const QBrush& brush);
    void setSelectedBrush(const QBrush& brush);
    void setBorderPen(const QPen& pen);
    void setSelectedBorderPen(const QPen& pen);

    void setMovable(bool movable);

signals:
    void brushChanged(const QBrush& brush);
    void selectedBrushChanged(const QBrush& selectedBrush);
    void borderPenChanged(const QPen& borderPen);
    void selectedBorderPenChanged(const QPen& selectedBorderPen);
    void movableChanged(bool movable);

protected:
    QBrush mBrush, mSelectedBrush;
    QPen mBorderPen, mSelectedBorderPen;
    bool mMovable = true;
    // True while the span moves its own edges: the writer signals the range once, when it is done.
    bool mEdgesQuiet = false;

    QBrush mainBrush() const { return mSelected ? mSelectedBrush : mBrush; }

    QPen mainBorderPen() const { return mSelected ? mSelectedBorderPen : mBorderPen; }

    // Moves one coordinate of an edge to a pixel position, whatever the edge's position type is;
    // the other coordinate is left untouched.
    static void setEdgePixel(QCPItemPosition* edge, Qt::Orientation orientation, double pixel);
};

#endif // QCP_ITEM_SPANBASE_H
