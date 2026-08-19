#include "latexlabelrenderer.h"

#include "../painting/painter.h"

#include <QFontMetrics>
#include <QPicture>
#include <QRegularExpression>

#include <jkqtmathtext/jkqtmathtext.h>

namespace
{
/*!
  JKQTMathText is neither cheap to construct nor reentrant, and a repaint asks
  for the same label over and over. One per thread, reconfigured per call.
*/
JKQTMathText& parserFor(const QFont& font, const QColor& color, const QString& text)
{
    static thread_local JKQTMathText parser;
    parser.useXITS();
    parser.setFontSize(font.pointSizeF() > 0 ? font.pointSizeF() : font.pixelSize());
    parser.setFontColor(color);
    parser.parse(text);
    return parser;
}
}

QCPLatexLabelRenderer* QCPLatexLabelRenderer::instance()
{
    static QCPLatexLabelRenderer renderer;
    return &renderer;
}

bool QCPLatexLabelRenderer::containsMath(const QString& text) noexcept
{
    static const QRegularExpression mathSpan(QStringLiteral(R"(\$[^$]+\$)"));
    return mathSpan.match(text).hasMatch();
}

QSize QCPLatexLabelRenderer::measure(const QFont& font, const QString& text) const
{
    if (!containsMath(text))
        return QFontMetrics(font)
            .boundingRect(0, 0, 0, 0, Qt::TextDontClip | Qt::AlignCenter, text)
            .size();

    // getSize needs a painter, and the axis asks for a size while computing its
    // margins, before any painting starts -- so make a throwaway. QPicture only
    // records, so nothing is rasterised here.
    QPicture scratch;
    QPainter painter(&scratch);
    const QSizeF size = parserFor(font, Qt::black, text).getSize(painter);
    painter.end();
    return size.toSize();
}

void QCPLatexLabelRenderer::draw(QCPPainter* painter, const QRect& rect, const QFont& font,
                                 const QColor& color, const QString& text, int flags) const
{
    if (!containsMath(text))
    {
        painter->setFont(font);
        painter->setPen(QPen(color));
        painter->drawText(rect, flags, text);
        return;
    }

    auto& parser = parserFor(font, color, text);
    // JKQTMathText draws from a baseline-left origin while we are handed a box
    // and Qt alignment flags; the ascent is what converts between the two.
    double width = 0., ascent = 0., descent = 0., strikeout = 0.;
    parser.getSizeDetail(*painter, width, ascent, descent, strikeout);
    const double height = ascent + descent;

    double x = rect.x();
    if (flags & Qt::AlignHCenter)
        x += (rect.width() - width) / 2.;
    else if (flags & Qt::AlignRight)
        x += rect.width() - width;

    double y = rect.y() + ascent;   // Qt::AlignTop, and the default
    if (flags & Qt::AlignVCenter)
        y = rect.y() + (rect.height() - height) / 2. + ascent;
    else if (flags & Qt::AlignBottom)
        y = rect.y() + rect.height() - descent;

    parser.draw(*painter, x, y);
}
