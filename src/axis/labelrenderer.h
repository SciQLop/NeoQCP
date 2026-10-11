#ifndef QCP_AXIS_LABELRENDERER_H
#define QCP_AXIS_LABELRENDERER_H

#include "../global.h"

#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QFont>

class QCPPainter;

/*!
  \brief Sets a piece of user-supplied text, so callers can substitute a richer
  typesetter.

  QCustomPlot paints axis labels, legend entries and titles with a single
  QPainter::drawText, which is all plain text needs and all a plotting library
  should presume. Anything more -- markup, mathematical typesetting -- is a
  domain concern, and this is the seam where that domain plugs in.

  \ref defaultRenderer is what every text-bearing element consults, and is
  what QCPAxis starts out with. There is no default subclass to inherit from,
  because a null renderer *is* the plain-text default: \ref measureWith and
  \ref drawWith fall back to QPainter::drawText when handed one, so a call site
  never has to branch.

  \a measure is answered without a QPainter because sizes are needed while the
  layout computes margins, before any painting begins. An implementation that
  needs one is expected to make its own throwaway.
*/
class QCP_LIB_DECL QCPLabelRenderer
{
public:
    virtual ~QCPLabelRenderer() = default;

    //! Size \a text occupies when set in \a font, unrotated.
    virtual QSize measure(const QFont& font, const QString& text) const = 0;

    /*!
      Draw \a text inside \a rect in the painter's current coordinates, which
      for a left/right axis are already rotated. \a flags are Qt's text flags,
      as QPainter::drawText takes them.
    */
    virtual void draw(QCPPainter* painter, const QRect& rect, const QFont& font,
                      const QColor& color, const QString& text, int flags) const = 0;

    /*!
      \a text shortened to fit \a width, or \a text itself when it cannot be shortened
      as text -- the default, since markup cut in the middle no longer parses.
    */
    virtual QString elide(const QFont& font, const QString& text, int width) const
    {
        Q_UNUSED(font);
        Q_UNUSED(width);
        return text;
    }

    /*!
      The renderer text-bearing elements use unless told otherwise. This is the
      LaTeX typesetter when NeoQCP is built with \c with_latex, otherwise null
      -- and null simply means plain text.
    */
    static QCPLabelRenderer* defaultRenderer();
    //! Replace \ref defaultRenderer; null restores plain QPainter::drawText.
    static void setDefaultRenderer(QCPLabelRenderer* renderer);

    //! \ref measure through \a renderer, or plain font metrics when it is null.
    static QSize measureWith(const QCPLabelRenderer* renderer, const QFont& font,
                             const QString& text);
    /*!
      Rect \a text occupies relative to a zero-size origin, exactly as
      QFontMetrics::boundingRect(0,0,0,0,flags,text) gives it -- so a centred
      label comes back with a negative topLeft. Callers that *place* text from
      this need that offset; callers that only need a size can use \ref
      measureWith.
    */
    static QRect measureRectWith(const QCPLabelRenderer* renderer, const QFont& font,
                                 const QString& text, int flags);
    //! \ref elide through \a renderer, or a plain middle elision when it is null.
    static QString elideWith(const QCPLabelRenderer* renderer, const QFont& font,
                             const QString& text, int width);
    //! \ref draw through \a renderer, or plain QPainter::drawText when it is null.
    static void drawWith(const QCPLabelRenderer* renderer, QCPPainter* painter,
                         const QRect& rect, const QFont& font, const QColor& color,
                         const QString& text, int flags);

protected:
    //! What a plain-text renderer's \ref elide does.
    static QString plainElide(const QFont& font, const QString& text, int width);
};

#endif // QCP_AXIS_LABELRENDERER_H
