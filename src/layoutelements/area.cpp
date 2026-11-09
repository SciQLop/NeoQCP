#include "area.h"
#include "../painting/painter.h"

QCPArea::QCPArea(QCustomPlot* parentPlot)
        : QCPLayoutElement(parentPlot)
        , mBackgroundBrush(Qt::NoBrush)
        , mBackgroundScaled(true)
        , mBackgroundScaledMode(Qt::KeepAspectRatioByExpanding)
        , mInsetLayout(new QCPLayoutInset)
{
    mInsetLayout->initializeParentPlot(mParentPlot);
    mInsetLayout->setParentLayerable(this);
    mInsetLayout->setParent(this);
}

QCPArea::~QCPArea()
{
    destroyInsetLayout();
}

void QCPArea::destroyInsetLayout()
{
    delete mInsetLayout;
    mInsetLayout = nullptr;
}

/*! \internal

  Runs the margin pass, then hands the area's rect to the inset layout and updates it: an area
  draws its inset elements inside itself, so its subclass need not know about the inset layout.
*/
void QCPArea::update(UpdatePhase phase)
{
    QCPLayoutElement::update(phase);

    if (phase == upLayout)
        mInsetLayout->setOuterRect(rect());

    mInsetLayout->update(phase);
}

/* inherits documentation from base class */
QList<QCPLayoutElement*> QCPArea::elements(bool recursive) const
{
    QList<QCPLayoutElement*> result;
    if (mInsetLayout)
    {
        result << mInsetLayout;
        if (recursive)
            result << mInsetLayout->elements(recursive);
    }
    return result;
}

/* inherits documentation from base class */
void QCPArea::applyDefaultAntialiasingHint(QCPPainter* painter) const
{
    painter->setAntialiasing(false);
}

/* inherits documentation from base class */
void QCPArea::draw(QCPPainter* painter)
{
    drawBackground(painter);
}

/*!
  Sets \a pm as the area background pixmap. The area background pixmap will be drawn inside the
  area.

  For cases where the provided pixmap doesn't have the same size as the area, scaling can be
  enabled with \ref setBackgroundScaled and the scaling mode (i.e. whether and how the aspect ratio
  is preserved) can be set with \ref setBackgroundScaledMode. To set all these options in one call,
  consider using the overloaded version of this function.

  Below the pixmap, the area may be optionally filled with a brush, if specified with \ref
  setBackground(const QBrush &brush).

  \see setBackgroundScaled, setBackgroundScaledMode, setBackground(const QBrush &brush)
*/
void QCPArea::setBackground(const QPixmap& pm)
{
    if (pm.cacheKey() == mBackgroundPixmap.cacheKey())
        return;
    mBackgroundPixmap = pm;
    mScaledBackgroundPixmap = QPixmap();
    markDirty();
}

/*! \overload

  Sets \a brush as the background brush. The area background will be filled with this brush.

  The brush will be drawn before (under) any background pixmap, which may be specified with \ref
  setBackground(const QPixmap &pm).

  To disable drawing of a background brush, set \a brush to Qt::NoBrush.

  \see setBackground(const QPixmap &pm)
*/
void QCPArea::setBackground(const QBrush& brush)
{
    if (brush == mBackgroundBrush)
        return;
    mBackgroundBrush = brush;
    markDirty();
}

/*! \overload

  Allows setting the background pixmap of the area, whether it shall be scaled and how it shall be
  scaled in one call.

  \see setBackground(const QPixmap &pm), setBackgroundScaled, setBackgroundScaledMode
*/
void QCPArea::setBackground(const QPixmap& pm, bool scaled, Qt::AspectRatioMode mode)
{
    if (pm.cacheKey() == mBackgroundPixmap.cacheKey() && scaled == mBackgroundScaled
        && mode == mBackgroundScaledMode)
        return;
    mBackgroundPixmap = pm;
    mScaledBackgroundPixmap = QPixmap();
    mBackgroundScaled = scaled;
    mBackgroundScaledMode = mode;
    markDirty();
}

/*!
  Sets whether the background pixmap shall be scaled to fit the area or not. If \a scaled is set to
  true, you may control whether and how the aspect ratio of the original pixmap is preserved with
  \ref setBackgroundScaledMode.

  Note that the scaled version of the original pixmap is buffered, so there is no performance
  penalty on replots. (Except when the area dimensions are changed continuously.)

  \see setBackground, setBackgroundScaledMode
*/
void QCPArea::setBackgroundScaled(bool scaled)
{
    if (scaled == mBackgroundScaled)
        return;
    mBackgroundScaled = scaled;
    markDirty();
}

/*!
  If scaling of the background pixmap is enabled (\ref setBackgroundScaled), use this function to
  define whether and how the aspect ratio of the original pixmap passed to \ref setBackground is
  preserved.

  \see setBackground, setBackgroundScaled
*/
void QCPArea::setBackgroundScaledMode(Qt::AspectRatioMode mode)
{
    if (mode == mBackgroundScaledMode)
        return;
    mBackgroundScaledMode = mode;
    markDirty();
}

/*! \internal

  Draws the background of this area. It may consist of a background fill (a QBrush) and a pixmap.

  If a brush was given via \ref setBackground(const QBrush &brush), this function first draws an
  according filling inside the area with the provided \a painter.

  Then, if a pixmap was provided via \ref setBackground, this function buffers the scaled version
  depending on \ref setBackgroundScaled and \ref setBackgroundScaledMode and then draws it inside
  the area with the provided \a painter. The scaled version is buffered in
  mScaledBackgroundPixmap to prevent expensive rescaling at every redraw. It is only updated, when
  the area has changed in a way that requires a rescale of the background pixmap (this is
  dependent on the \ref setBackgroundScaledMode), or when a different background pixmap was set.

  \see setBackground, setBackgroundScaled, setBackgroundScaledMode
*/
void QCPArea::drawBackground(QCPPainter* painter)
{
    if (mBackgroundBrush != Qt::NoBrush)
        drawBackgroundBrush(painter);
    if (!mBackgroundPixmap.isNull())
        drawBackgroundPixmap(painter);
}

/*! \internal
  Fills the area's whole rect with \ref backgroundBrush.
*/
void QCPArea::drawBackgroundBrush(QCPPainter* painter)
{
    painter->fillRect(mRect, mBackgroundBrush);
}

/*! \internal
  Draws \ref background, scaled as \ref backgroundScaled says, over the whole rect.
*/
void QCPArea::drawBackgroundPixmap(QCPPainter* painter)
{
    if (mBackgroundScaled)
    {
        // check whether mScaledBackground needs to be updated:
        QSize scaledSize(mBackgroundPixmap.size());
        scaledSize.scale(mRect.size(), mBackgroundScaledMode);
        if (mScaledBackgroundPixmap.size() != scaledSize)
            mScaledBackgroundPixmap = mBackgroundPixmap.scaled(
                mRect.size(), mBackgroundScaledMode, Qt::SmoothTransformation);
        painter->drawPixmap(mRect.topLeft() + QPoint(0, -1), mScaledBackgroundPixmap,
                            QRect(0, 0, mRect.width(), mRect.height())
                                & mScaledBackgroundPixmap.rect());
    }
    else
    {
        painter->drawPixmap(mRect.topLeft() + QPoint(0, -1), mBackgroundPixmap,
                            QRect(0, 0, mRect.width(), mRect.height()));
    }
}
