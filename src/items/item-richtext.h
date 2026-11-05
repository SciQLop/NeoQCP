#ifndef QCP_ITEM_RICHTEXT_H
#define QCP_ITEM_RICHTEXT_H

#include "item-text.h"
#include <QTextDocument>

class QCPPainter;

template <typename T, typename Self = void>
class QCPItemRichTextOps;

class QCP_LIB_DECL QCPItemRichText : public QCPItemText
{
    Q_OBJECT
    Q_PROPERTY(QString html READ html WRITE setHtml)

public:
    template <typename T>
    using OpsFor = QCPItemRichTextOps<T>;

    explicit QCPItemRichText(QCustomPlot* parentPlot);
    virtual ~QCPItemRichText() override;

    QString html() const { return mHtml; }
    void setHtml(const QString& html);
    void clearHtml();

    double cornerRadius() const { return mCornerRadius; }
    void setCornerRadius(double radius) { mCornerRadius = radius; }

    QSizeF contentSize() const { return mBoundingRect.size().grownBy(mPadding); }

protected:
    void draw(QCPDrawContext& context) const override;
    std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const override;
    virtual QPointF anchorPixelPosition(int anchorId) const override;

private:
    void drawHtml(QCPPainter& painter) const;
    QRectF computeDrawRect(const QPointF& pos) const;

    QString mHtml;
    mutable QTextDocument mDoc;
    mutable QRectF mBoundingRect;
    bool mUseHtml = false;
    double mCornerRadius = 0;
};

// The fluent handle of a QCPItemRichText; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemRichTextOps : public QCPItemTextOps<T, qcp::OpsRet<Self, QCPItemRichTextOps<T>>>
{
    // A rich text item is a text item: it keeps the text setters and the position.
    using Base = QCPItemTextOps<T, qcp::OpsRet<Self, QCPItemRichTextOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setHtml(const QString& html)
    {
        return this->apply([&](T& o) { o.setHtml(html); });
    }
};

#endif // QCP_ITEM_RICHTEXT_H
