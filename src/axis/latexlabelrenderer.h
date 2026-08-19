#ifndef QCP_AXIS_LATEXLABELRENDERER_H
#define QCP_AXIS_LATEXLABELRENDERER_H

#include "labelrenderer.h"

/*!
  \brief Typesets axis labels containing LaTeX, via JKQTMathText.

  Only text carrying a `$...$` span is typeset; anything else is drawn as plain
  text. So an ordinary label costs nothing, and a lone `$` -- a currency sign in
  "Cost [$/kg]" -- cannot turn a label into an equation on its own.

  Axes install this by default when NeoQCP is built with `with_latex`, which is
  the only reason \ref QCPAxis::setLabelRenderer needs to be called explicitly:
  to replace it, or to pass nullptr and get plain text back.

  One instance serves every axis -- it keeps no per-label state between calls --
  so use \ref instance rather than constructing your own.
*/
class QCP_LIB_DECL QCPLatexLabelRenderer : public QCPLabelRenderer
{
public:
    static QCPLatexLabelRenderer* instance();

    //! True when \a text has a `$...$` span, i.e. is worth handing to the parser.
    static bool containsMath(const QString& text) noexcept;

    QSize measure(const QFont& font, const QString& text) const override;
    void draw(QCPPainter* painter, const QRect& rect, const QFont& font, const QColor& color,
              const QString& text, int flags) const override;

private:
    QCPLatexLabelRenderer() = default;
};

#endif // QCP_AXIS_LATEXLABELRENDERER_H
