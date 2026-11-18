#include "item-mouse-event.h"

#include "item.h"

#include <QMouseEvent>

QCPMouseEvent::QCPMouseEvent(const QCPItem* item, const QMouseEvent* source, QCP::Pixels pixels,
                             QCP::Pixels lastPixels, int part, const QVariant& details)
    : mItem(item)
    , mPixels(pixels)
    , mLastPixels(lastPixels)
    , mPart(part)
    , mDetails(details)
    , mButton(source ? source->button() : Qt::NoButton)
    , mButtons(source ? source->buttons() : Qt::NoButton)
    , mModifiers(source ? source->modifiers() : Qt::NoModifier)
{
}

QCPMouseEvent::~QCPMouseEvent() = default;

/*! \internal

  The item's own frame, the one its recording replays in: the axes of its area, never the plot's
  default pair.
*/
QCP::Data QCPMouseEvent::data() const
{
    return mItem ? mItem->mCore.dataAt(mPixels, mItem->view()) : QCP::Data {};
}

QCP::Data QCPMouseEvent::lastData() const
{
    return mItem ? mItem->mCore.dataAt(mLastPixels, mItem->view()) : QCP::Data {};
}
