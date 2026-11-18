#ifndef QCP_ITEM_MOUSE_EVENT_H
#define QCP_ITEM_MOUSE_EVENT_H

#include "coords.h"

#include <QPointer>
#include <QVariant>

class QCPItem;
class QMouseEvent;

/*!
  A mouse event, in the terms an item works in.

  An item that wants the mouse overrides mousePressEvent() and its three siblings on QCPItem, and
  those handlers receive this event. The pixels are the plot's, the data positions are the item's own
  (the axes of its area, log and reversed axes included), and \a part is what the press hit in the
  item's recording: the part of the tagged command under the mouse, -1 when it hit none.

  The event arrives accepted, as Qt's do, and an item that does not want it calls ignore(). The four
  handlers on QCPItem all ignore by default.
*/
class QCP_LIB_DECL QCPMouseEvent
{
public:
    /// \internal Built by QCPItem's handler overrides, never by a user.
    QCPMouseEvent(const QCPItem* item, const QMouseEvent* source, QCP::Pixels pixels,
                  QCP::Pixels lastPixels, int part, const QVariant& details);
    ~QCPMouseEvent();

    //! Where the mouse is now, in plot pixels.
    [[nodiscard]] QCP::Pixels pixels() const { return mPixels; }
    //! Where the previous event of this drag was: the press for the first move, the last move
    //! afterwards.
    [[nodiscard]] QCP::Pixels lastPixels() const { return mLastPixels; }
    //! How much the mouse moved since the previous event, in plot pixels.
    [[nodiscard]] QCP::PixelOffset moved() const { return mPixels - mLastPixels; }
    //! \a pixels in the item's own frame: its area's axes, log and reversed axes included.
    [[nodiscard]] QCP::Data data() const;
    //! \a lastPixels in the item's own frame.
    [[nodiscard]] QCP::Data lastData() const;
    //! The part the press hit, -1 when it hit no tagged command. The whole drag keeps it, wherever
    //! the mouse then goes.
    [[nodiscard]] int part() const { return mPart; }

    [[nodiscard]] Qt::MouseButton button() const { return mButton; }
    [[nodiscard]] Qt::MouseButtons buttons() const { return mButtons; }
    [[nodiscard]] Qt::KeyboardModifiers modifiers() const { return mModifiers; }

    void accept() { mAccepted = true; }
    void ignore() { mAccepted = false; }
    [[nodiscard]] bool isAccepted() const { return mAccepted; }

    /*! \internal What the item's own hitTest answered. An item whose hitTest reports details of its
      own (a span's edge, say) reads its part here: part() only knows the tagged form. */
    [[nodiscard]] const QVariant& details() const { return mDetails; }

private:
    QPointer<const QCPItem> mItem;
    QCP::Pixels mPixels {};
    QCP::Pixels mLastPixels {};
    int mPart;
    QVariant mDetails;
    Qt::MouseButton mButton = Qt::NoButton;
    Qt::MouseButtons mButtons = Qt::NoButton;
    Qt::KeyboardModifiers mModifiers = Qt::NoModifier;
    bool mAccepted = true;
};

#endif
