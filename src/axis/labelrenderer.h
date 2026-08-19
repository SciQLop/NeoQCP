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
  \brief Draws an axis label, so callers can substitute a richer typesetter.

  QCustomPlot paints axis labels with a single QPainter::drawText, which is all
  plain text needs and all a plotting library should presume. Anything more --
  markup, mathematical typesetting -- is a domain concern, and belongs to
  whoever has that domain, not here.

  Install one with \ref QCPAxis::setLabelRenderer. With none installed the axis
  paints text exactly as it always has; there is no default subclass to inherit
  from, because "no renderer" is that default.

  \a measure is answered without a QPainter because it is also needed while the
  axis rect computes its margins, before any painting begins. An implementation
  that needs one is expected to make its own throwaway.
*/
class QCP_LIB_DECL QCPLabelRenderer
{
public:
    virtual ~QCPLabelRenderer() = default;

    /*!
      Size \a text occupies when set in \a font, unrotated. Only the height is
      consulted for left/right axes, which are drawn rotated by 90 degrees.
    */
    virtual QSize measure(const QFont& font, const QString& text) const = 0;

    /*!
      Draw \a text centered in \a rect in the painter's current coordinates,
      which for a left/right axis are already rotated.
    */
    virtual void draw(QCPPainter* painter, const QRect& rect, const QFont& font,
                      const QColor& color, const QString& text) const = 0;
};

#endif // QCP_AXIS_LABELRENDERER_H
