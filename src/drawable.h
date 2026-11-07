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
struct QCPDrawList;
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
    //! Whether the painter is an export: the object may resolve what a screen frame waits for.
    bool exporting = false;
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
    //! Not called dataRange(): a colour-mapped plottable already exposes that for its colour scale.
    virtual std::optional<QCPDataBox> drawnDataRange() const = 0;
    //! The pixel distance to \a at, or none when it missed.
    virtual std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const = 0;
    //! Draws the object into a legend icon's rect; the adapter replays the recording, fitted.
    virtual void drawLegendIcon(QCPDrawContext& icon) const = 0;

    //! The data the recording was made from, nullptr for none: a new pointer is a stale recording.
    [[nodiscard]] virtual const void* dataSourceIdentity() const { return nullptr; }

    /*!
      Everything else the recording depends on that the generation does not cover: other data (a
      bar's stack, its group), a counter that only ever grows. A change makes the recording stale.
    */
    [[nodiscard]] virtual quint64 dependencyRevision() const { return 0; }

    /*!
      Whether an export resolves what a screen frame waits for, so the two need separate
      recordings. Most objects draw the same either way and answer false.
    */
    [[nodiscard]] virtual bool resolvesForTheExport() const { return false; }
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
    //! With \a applyLogRule false, a log axis does not drop a side that reaches zero: a plottable's
    //! range is data, not an item index.
    std::optional<QCPDataBox> dataRange(const QCPDrawable& object, const QCPDrawnView& view,
                                        bool applyLogRule = true);

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

    //! The object's content generation: a setter bumped it this many times.
    [[nodiscard]] quint64 generation() const;

    //! Test hook: how many times the object was asked to record.
    [[nodiscard]] int recordingCount() const;

    //! Whether a recording exists: the object has drawn at least once.
    [[nodiscard]] bool hasRecording() const;

    //! Test hook: how many commands the recording holds.
    [[nodiscard]] int recordedCommandCount() const;

    //! Test hook: the recording itself, empty before the object has drawn.
    [[nodiscard]] const QCPDrawList& recordedList() const;

    //! Test hook: the legend icon recording, empty before the icon was painted.
    [[nodiscard]] const QCPDrawList& iconRecordedList() const;

    //! Test hook: how many vertices the recording's polylines hold.
    [[nodiscard]] int recordedPolylinePointCount() const;

    //! Test hook: how many of those vertices are NaNs, i.e. where the line is broken.
    [[nodiscard]] int recordedPolylineNaNs() const;

    //! Test hook: how many times the resolve cache mapped a recording again.
    [[nodiscard]] quint64 resolveRemapCount() const;

    //! Test hook: how many rasterised images the object's replay keeps.
    [[nodiscard]] int rasterCount() const;

    /*!
      The pixels a pan has to shift this object's layer texture by, or a null point when the layer
      must repaint it instead.

      A drawn object's pixels are its recording replayed at a transform: a pan moves all of them by
      one delta exactly when the recording is one piece in data space and needs no remake. The layer
      then shifts the texture it has, which is the whole cost of the frame.
    */
    [[nodiscard]] QPointF stallOffset(const QCPDrawable& object, const QCPDrawnView& view) const;

private:
    bool ensureRecording(const QCPDrawable& object, const QCPDrawnView& view);

    //! True when the recording would be remade for \a view: the test ensureRecording() takes.
    [[nodiscard]] bool recordingIsStale(const QCPDrawable& object, const QCPDrawnView& view) const;

    //! notes the ranges the layer now holds this object drawn at: a pan may shift them.
    void notePainted(const QCPDrawnView& view);

    struct State;
    //! Shared with the deferred painter callbacks: they check the object is still there.
    std::shared_ptr<State> mState;
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

//! False when \a pos is outside \a plottable's axis rect and the plot did not opt in.
[[nodiscard]] QCP_LIB_DECL bool qcpDrawnClickInsideAxisRect(const QCPAbstractPlottable& plottable,
                                                            const QPointF& pos);

/*!
  \brief The non-template half of an adapter: the contract's methods, answered from the recording.

  Only \a view() waits for the base, because only the base knows where its axes, its clip and its
  selection rule are.
*/
class QCP_LIB_DECL QCPDrawnAdapter : public QCPDrawable
{
public:
    std::optional<QCPDataBox> drawnDataRange() const override
    {
        return mCore.dataRange(*this, view());
    }

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

    //! The base's public virtual, answered from the object's own hit test.
    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override
    {
        return mCore.selectTest(*this, pos, onlySelectable, details, view());
    }

protected:
    void draw(QCPPainter* painter) final { mCore.paint(*this, *painter, view()); }

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

    //! The data container the recording read, when the base has one.
    [[nodiscard]] const void* dataSourceIdentity() const override
    {
        if constexpr (requires { this->mDataContainer.get(); })
            return this->mDataContainer.get();
        else
            return nullptr;
    }

    //! The container's revision: a setter that only fills the container still makes a stale record.
    [[nodiscard]] quint64 dependencyRevision() const override
    {
        if constexpr (requires { this->mDataContainer->revision(); })
            return this->mDataContainer->revision();
        else
            return 0;
    }

public:
    // The bases declare these public; re-declaring them under another access would hide them from
    // name lookup, so a user's `bars->selectTest(...)` and `band->getKeyRange(...)` stop compiling.
    //! A hit hands selectEvent a selection covering the whole recording.
    //!
    //! The recording has no per-point index to hand out: selectEvent only needs a non-empty
    //! QCPDataSelection, and without one it sets an empty selection and the click selects nothing.
    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override
    {
        // Built-in plottables miss outside the axis rect unless the plot opts in: their geometry
        // may still be painted there (an unclipped plottable), but a click does not grab it.
        if (!qcpDrawnClickInsideAxisRect(*this, pos))
            return -1;

        const double hit = QCPDrawnBase<Base>::selectTest(pos, onlySelectable, details);
        if (hit < 0 || !details)
            return hit;
        if (details->isValid() && !details->value<QCPDataSelection>().isEmpty())
            return hit;
        int count = 1;
        if constexpr (requires { this->dataCount(); })
            count = this->dataCount();
        *details = QVariant::fromValue(QCPDataSelection(QCPDataRange(0, count)));
        return hit;
    }

    //! A recording that is one piece in data space lets its layer shift instead of repaint.
    QPointF stallPixelOffset() const override
    {
        return this->mCore.stallOffset(*this, this->view());
    }

public:
    QCPRange getKeyRange(bool& foundRange,
                         QCP::SignDomain inSignDomain = QCP::sdBoth) const override
    {
        return qcpDrawnRange(this->mCore.dataRange(*this, this->view(), false), QCPDrawnSide::Key,
                             foundRange, inSignDomain);
    }

    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override
    {
        Q_UNUSED(inKeyRange) // simplify: a box keeps no per-key values, so it cannot be narrowed
        return qcpDrawnRange(this->mCore.dataRange(*this, this->view(), false), QCPDrawnSide::Value,
                             foundRange, inSignDomain);
    }

protected:
    using QCPDrawnAdapter::drawLegendIcon; // the contract's icon stays reachable

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
