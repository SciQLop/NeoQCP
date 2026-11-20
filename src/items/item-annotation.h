#ifndef QCP_ITEM_ANNOTATION_H
#define QCP_ITEM_ANNOTATION_H

#include "../drawable.h"
#include "../global.h"
#include "../lineending.h"
#include "../painting/draw-context.h"
#include "connector.h"
#include "item.h"

#include <QFont>
#include <QSize>

class QCustomPlot;

/*!
  \brief A text and the line from it to a target: matplotlib's `annotate`.

  The item draws one QCPConnector from its text to its target, with an optional head at the target,
  and the text on top. \ref position is the text, \ref target is what it points at, and each names
  its own space: a label pinned in pixels with a tip that follows the data is one annotation.

  The connector starts at the centre of the text box and leaves it where the path crosses the box's
  edge, so the text never has a line drawn through it; a text that is empty has no box and the
  connector starts at \ref position. \ref shrink cuts the connector that many pixels short of each
  of those two ends. A target inside the box leaves no connector, and the text still draws.

  QCPAxisRect::annotation() makes one in a single call.
*/
class QCP_LIB_DECL QCPItemAnnotation : public QCPItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPItemAnnotation)
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QCPTextStyle textStyle READ textStyle WRITE setTextStyle NOTIFY textStyleChanged)
    Q_PROPERTY(QCPConnector connector READ connector WRITE setConnector NOTIFY connectorChanged)
    Q_PROPERTY(QPen pen READ pen WRITE setPen NOTIFY penChanged)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen NOTIFY selectedPenChanged)
    Q_PROPERTY(QCPLineEnding head READ head WRITE setHead NOTIFY headChanged)
    Q_PROPERTY(QCP::PixelLength shrink READ shrink WRITE setShrink NOTIFY shrinkChanged)
    /// \endcond

public:
    QCPItemAnnotation();
    virtual ~QCPItemAnnotation() override;

    // getters:
    QString text() const { return mText; }

    QCPTextStyle textStyle() const { return mTextStyle; }

    QCPConnector connector() const { return mConnector; }

    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    QCPLineEnding head() const { return mHead; }

    QCP::PixelLength shrink() const { return mShrink; }

    // setters:
    void setText(const QString& text);
    void setTextStyle(const QCPTextStyle& textStyle);
    void setConnector(const QCPConnector& connector);
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& selectedPen);
    void setHead(const QCPLineEnding& head);
    void setShrink(QCP::PixelLength shrink);

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;

    //! The target the connector points at, in whatever space it names.
    QCPItemPosition* const target;
    //! Where the text sits, in whatever space it names.
    QCPItemPosition* const position;

signals:
    void textChanged(const QString& text);
    void textStyleChanged(const QCPTextStyle& textStyle);
    void connectorChanged(const QCPConnector& connector);
    void penChanged(const QPen& pen);
    void selectedPenChanged(const QPen& selectedPen);
    void headChanged(const QCPLineEnding& head);
    void shrinkChanged(QCP::PixelLength shrink);

protected:
    //! The text's measured glyphs, re-measured only when the text, the font or mathText changes.
    QSize glyphSize(const QCPTextStyle& style) const;
    bool hiddenForItsTarget() const;
    //! The pen the connector is drawn with: the selected one while the item is selected.
    QPen mainPen() const;

    // property members:
    QString mText;
    QCPTextStyle mTextStyle;
    QCPConnector mConnector;
    QPen mPen, mSelectedPen;
    QCPLineEnding mHead;
    QCP::PixelLength mShrink;

    mutable QSize mGlyphSize;
    mutable QString mGlyphText;
    mutable QFont mGlyphFont;
    mutable bool mGlyphMath = false;
    mutable bool mGlyphValid = false;

    friend class TestAnnotation; // reads the recording's count
    friend struct AnnotationRecording; // reads the recording's commands
};

#endif // QCP_ITEM_ANNOTATION_H
