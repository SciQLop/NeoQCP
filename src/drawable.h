#pragma once

// The drawing contract (QCPDrawable) and its adapter QCPDrawn<Base>: a ported class keeps its old
// base (data, groups, legend, selection) and the adapter answers the old drawing virtuals from the
// new ones. Library-private; milestone 4 folds the adapter into the bases.

#include "coords.h"
#include "core.h"
#include "global.h"
#include "items/item.h"
#include "plottables/plottable.h"
#include "spatial-index.h"

#include <QRect>
#include <memory>
#include <optional>
#include <type_traits>

class QCPAxis;
class QCPAxisRect;
class QCPDrawContext;
class QCPLayer;
class QCPPainter;
class QCPTheme;
class QCustomPlot;
class QVariant;

/*!
  \brief What the adapter reads from its object once per frame.

  The axes, the axis rect and the clip come from the object itself: a recording does not depend on
  the transform, so only its replay reads these.
*/
struct QCP_LIB_DECL QCPDrawnView
{
    const QCPAxis* keyAxis = nullptr;
    const QCPAxis* valueAxis = nullptr;
    const QCPAxisRect* axisRect = nullptr; //!< the rect the ratio transform maps
    const QCPTheme* theme = nullptr;
    QCustomPlot* plot = nullptr;
    QCPLayer* layer = nullptr;
    QRect clip; //!< what the layer clips this object with
    bool selectable = false; //!< whether the base lets the object be selected at all
};

/*!
  \brief The drawing contract: \a draw records, the three others default to the recording.

  A recording is transform independent; a replay is not. The adapter keeps one recording per object
  and re-makes it only when the object says it changed (contentChanged) or when the view changed
  and the recording contains view-dependent commands.
*/
class QCP_LIB_DECL QCPDrawable
{
public:
    virtual ~QCPDrawable() = default;

    //! Appends what this object draws to \a context. Called only when the recording is stale.
    virtual void draw(QCPDrawContext& context) const = 0;
    //! What the object occupies in data coordinates; the adapter answers it from the recording.
    virtual std::optional<QCPDataBox> dataRange() const = 0;
    //! The pixel distance to \a at, or none when it missed.
    virtual std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const = 0;
    //! Draws the object into a legend icon's rect; the adapter replays the recording, fitted.
    virtual void drawLegendIcon(QCPDrawContext& icon) const = 0;
};

/*!
  \brief Everything a \ref QCPDrawn keeps between frames: the recording, its keys and the caches.

  One per object, non-template so the template above it stays thin: the recording itself, the
  (generation, view epoch) pair it was made under, the resolve cache, the raster fallback, the
  per-command extruded vertices, and the guard that catches a setter called from inside draw().
*/
class QCP_LIB_DECL QCPDrawnCore
{
public:
    QCPDrawnCore();
    ~QCPDrawnCore();
    QCPDrawnCore(const QCPDrawnCore&) = delete;
    QCPDrawnCore& operator=(const QCPDrawnCore&) = delete;

    //! A new generation. False, and one misuse report, when called from inside draw().
    bool contentChanged();

    //! What the object occupies, from the recording: QCPAbstractItem::dataExtent's two rules.
    std::optional<QCPDataBox> dataRange(const QCPDrawable& object, const QCPDrawnView& view);

    //! The pixels the recording's widest stroke reaches around its geometry.
    [[nodiscard]] double pixelPad() const;

    //! The recording's box as an item extent: QCPAbstractItem::dataExtent's two rules.
    std::optional<QCPItemExtent> dataExtent(const QCPDrawable& object, const QCPDrawnView& view);

    //! The pixel distance from \a at to the recording, or none when it missed.
    std::optional<double> hitTest(const QCPDrawable& object, QCP::Pixels at, QVariant* details,
                                  const QCPDrawnView& view);

    //! hitTest() as the bases' selectTest wants it: -1 when \a onlySelectable and not selectable.
    double selectTest(const QCPDrawable& object, const QPointF& pos, bool onlySelectable,
                      QVariant* details, const QCPDrawnView& view);

    //! Draws \a object's recording through \a painter: an export, or a screen without a GPU layer.
    void paint(const QCPDrawable& object, QCPPainter& painter, const QCPDrawnView& view);

    //! Draws \a object's recording into \a rect, fitted to its data box: the default legend icon.
    void paintIcon(const QCPDrawable& object, QCPPainter& painter, const QRectF& rect,
                   const QCPDrawnView& view);

    //! Test hook: how many times the object was asked to record.
    [[nodiscard]] int recordingCount() const;

    //! Test hook: how many times the resolve cache mapped a recording again.
    [[nodiscard]] quint64 resolveRemapCount() const;

private:
    bool ensureRecording(const QCPDrawable& object, const QCPDrawnView& view);

    struct State;
    std::unique_ptr<State> mState;
};

//! A box side, for the one range helper both plottable ranges answer through.
enum class QCPDrawnSide
{
    Key,
    Value
};

/*!
  The view \a item or \a plottable is drawn through; \a clip is what its layer clips it with (the
  bases keep clipRect() protected, so the object hands it over).
*/
[[nodiscard]] QCP_LIB_DECL QCPDrawnView qcpDrawnViewOf(const QCPAbstractItem& item, QRect clip);
[[nodiscard]] QCP_LIB_DECL QCPDrawnView qcpDrawnViewOf(const QCPAbstractPlottable& plottable,
                                                       QRect clip);

//! \a box's \a side as a QCPRange, keeping only the requested sign domain.
[[nodiscard]] QCP_LIB_DECL QCPRange qcpDrawnRange(const std::optional<QCPDataBox>& box,
                                                  QCPDrawnSide side, bool& foundRange,
                                                  QCP::SignDomain inSignDomain);

/*!
  \brief The non-template half of an adapter: the contract's methods, answered from the recording.

  Only \a view() waits for the base, because only the base knows where its axes, its clip and its
  selection rule are.
*/
class QCP_LIB_DECL QCPDrawnAdapter : public QCPDrawable
{
public:
    std::optional<QCPDataBox> dataRange() const override { return mCore.dataRange(*this, view()); }

    std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const override
    {
        return mCore.hitTest(*this, at, details, view());
    }

    //! The default icon is the object's own drawing; an override records a summary instead.
    void drawLegendIcon(QCPDrawContext& icon) const override { draw(icon); }

protected:
    //! What the core reads from the object this frame.
    [[nodiscard]] virtual QCPDrawnView view() const = 0;

    mutable QCPDrawnCore mCore;
};

/*!
  \brief The common part of an adapter: what needs \a Base, which is the old virtuals and the
  setter.
*/
template <typename Base>
class QCPDrawnBase : public Base, public QCPDrawnAdapter
{
public:
    using Base::Base;

    //! The setter rule: a new generation, then the layer mark the base needs.
    //!
    //! This is the layerable markDirty the old bases call from their setters (setPen, setCoords,
    //! setSelected, ...). A pan or a zoom never reaches it, so a range change still reuses the
    //! recording.
    void markDirty() override
    {
        if (mCore.contentChanged())
            Base::markDirty();
    }

    //! The adapter's own name for the same thing.
    void contentChanged() { markDirty(); }

    using QCPDrawable::draw; // keep draw(QCPDrawContext&) reachable from this class

protected:
    void draw(QCPPainter* painter) final { mCore.paint(*this, *painter, view()); }

    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const final
    {
        return mCore.selectTest(*this, pos, onlySelectable, details, view());
    }

    //! This frame's view: the clip comes from here, since the bases keep clipRect() protected.
    [[nodiscard]] QCPDrawnView view() const override
    {
        return qcpDrawnViewOf(*this, this->clipRect());
    }
};

//! A drawn plottable: the recording answers its two ranges and its legend icon.
template <typename Base>
class QCPDrawnPlottable : public QCPDrawnBase<Base>
{
public:
    using QCPDrawnBase<Base>::QCPDrawnBase;

protected:
    using QCPDrawnAdapter::drawLegendIcon; // the contract's icon stays reachable

    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const override
    {
        return qcpDrawnRange(this->dataRange(), QCPDrawnSide::Key, foundRange, inSignDomain);
    }

    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                           const QCPRange& inKeyRange = QCPRange()) const override
    {
        Q_UNUSED(inKeyRange) // simplify: a box keeps no per-key values, so it cannot be narrowed
        return qcpDrawnRange(this->dataRange(), QCPDrawnSide::Value, foundRange, inSignDomain);
    }

    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override
    {
        this->mCore.paintIcon(*this, *painter, rect, this->view());
    }
};

//! A drawn item: the recording answers its extent.
template <typename Base>
class QCPDrawnItem : public QCPDrawnBase<Base>
{
public:
    using QCPDrawnBase<Base>::QCPDrawnBase;

protected:
    std::optional<QCPItemExtent> dataExtent() const override
    {
        return this->mCore.dataExtent(*this, this->view());
    }
};

/*!
  \brief What a ported class derives from: \a Base is QCPAbstractPlottable, a
  QCPAbstractPlottable1D<T> or QCPAbstractItem, and the adapter adds the virtuals that base actually
  declares.
*/
template <typename Base>
using QCPDrawn = std::conditional_t<std::is_base_of_v<QCPAbstractItem, Base>, QCPDrawnItem<Base>,
                                    QCPDrawnPlottable<Base>>;
