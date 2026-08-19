#include "labelrenderer.h"

#include "../painting/painter.h"

#if NEOQCP_WITH_LATEX
#  include "latexlabelrenderer.h"
#endif

#include <QFontMetrics>

namespace
{
QCPLabelRenderer* gDefaultRenderer =
#if NEOQCP_WITH_LATEX
    QCPLatexLabelRenderer::instance();
#else
    nullptr;
#endif
}

QCPLabelRenderer* QCPLabelRenderer::defaultRenderer()
{
    return gDefaultRenderer;
}

void QCPLabelRenderer::setDefaultRenderer(QCPLabelRenderer* renderer)
{
    gDefaultRenderer = renderer;
}

QSize QCPLabelRenderer::measureWith(const QCPLabelRenderer* renderer, const QFont& font,
                                    const QString& text)
{
    if (renderer)
        return renderer->measure(font, text);
    return QFontMetrics(font).boundingRect(0, 0, 0, 0, Qt::TextDontClip, text).size();
}

QRect QCPLabelRenderer::measureRectWith(const QCPLabelRenderer* renderer, const QFont& font,
                                        const QString& text, int flags)
{
    if (!renderer)
        return QFontMetrics(font).boundingRect(0, 0, 0, 0, flags, text);

    const QSize size = renderer->measure(font, text);
    QRect rect(QPoint(0, 0), size);
    if (flags & Qt::AlignHCenter)
        rect.moveLeft(-size.width() / 2);
    else if (flags & Qt::AlignRight)
        rect.moveLeft(-size.width());
    if (flags & Qt::AlignVCenter)
        rect.moveTop(-size.height() / 2);
    else if (flags & Qt::AlignBottom)
        rect.moveTop(-size.height());
    return rect;
}

void QCPLabelRenderer::drawWith(const QCPLabelRenderer* renderer, QCPPainter* painter,
                                const QRect& rect, const QFont& font, const QColor& color,
                                const QString& text, int flags)
{
    if (renderer)
    {
        renderer->draw(painter, rect, font, color, text, flags);
        return;
    }
    painter->setFont(font);
    painter->setPen(QPen(color));
    painter->drawText(rect, flags, text);
}
