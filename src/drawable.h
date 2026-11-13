#pragma once

// The drawing contract (QCPDrawable) and the core that keeps one recording per object.
// QCPAbstractPlottable and QCPAbstractItem derive from the contract; their painter entries are final
// and only replay the recording. Library-private.

#include "axis/range.h"
#include "coords.h"
#include "global.h"
#include "spatial-index.h"

#include <QHashFunctions>
#include <QRect>
#include <memory>
#include <optional>

class QCPAxis;
class QCPAxisRect;
class QCPAbstractItem;
class QCPAbstractPlottable;
class QCPDrawContext;
struct QCPDrawFrame;
struct QCPDrawList;
class QCPLayer;
class QCPArea;
class QCPCoordinateSystem;
class QCPPainter;
class QCPTheme;
class QCustomPlot;
class QVariant;

/*!
  \brief What the adapter reads from its object once per frame.

  The system, the axes it projects to, the axis rect and the clip come from the object itself: a
  recording does not depend on the transform, so only its replay reads these.
*/
struct QCP_LIB_DECL QCPDrawnView
{
    const QCPAxis* keyAxis = nullptr; //!< the cartesian projections of \a system, when it has some
    const QCPAxis* valueAxis = nullptr;
    const QCPAxisRect* axisRect = nullptr; //!< the rect's axes projection, when it has one
    //! The area whose ratio this object draws in: a plottable's system area, an item's position
    //! area.
    const QCPArea* area = nullptr;
    //! The area whose shape clips this object: a plottable's system area; an item's clip area, and
    //! only while the item clips (an unclipped item draws anywhere, as on an axis rect).
    const QCPArea* clipArea = nullptr;
    //! The frame the object draws in: a plottable's system, or the system of the item's first
    //! plot-coordinate position. Null only when the object names no frame at all.
    const QCPCoordinateSystem* system = nullptr;
    const QCPTheme* theme = nullptr;
    QCustomPlot* plot = nullptr;
    QCPLayer* layer = nullptr;
    QRect clip; //!< what the layer clips this object with
    bool selectable = false; //!< whether the base lets the object be selected at all
    //! Whether the painter is an export: the object may resolve what a screen frame waits for.
    bool exporting = false;
    //! Whether the recording is measured against \a clip: plottables are, and so are items whose
    //! clip area is a path (a polar disc); an item clipped to an axis rect keeps the painter's clip.
    bool clippedToFrame = false;

    //! Whether the view names a frame at all: a system, or a pair of axes for the items.
    [[nodiscard]] bool framed() const { return system || (keyAxis && valueAxis); }
};

//! What an item occupies, for the layer's spatial index: the data box of its recording on one axis
//! pair, plus the pixels its drawing may add around it (pens, line endings, a bracket's arms).
struct QCP_LIB_DECL QCPItemExtent
{
    QCPAxis* keyAxis = nullptr;
    QCPAxis* valueAxis = nullptr;
    QCPDataBox box { };
    double pixelPad = 0.0;
    //! The system the box is measured in, when the item has one: the index groups by it, and only
    //! a cartesian system's box is worth an axis-pair query.
    QCPCoordinateSystem* system = nullptr;
};

/*!
  \brief The drawing contract: \a draw records, the three others default to the recording.

  A recording is transform independent; a replay is not. The core keeps one recording per object
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
  \internal

  The key a recording that read \a source is stale on: the identity of the data it read and that
  data's revision, hashed in order. A revision alone misses a container swapped for another one that
  happens to be at the same revision, as two refilled buffers are.
*/
inline quint64 qcpDependencyKey(const QCPDrawable* source)
{
    if (!source)
        return 0;
    return qHashMulti(0, source->dataSourceIdentity(), source->dependencyRevision());
}

/*!
  \brief Everything a plottable or an item keeps between frames: the recording, its keys and the
  caches.

  One per object, non-template so the bases stay thin: the recording itself, the (generation, view
  epoch) pair it was made under, the resolve cache, the raster fallback, the per-command extruded
  vertices, and the guard that catches a setter called from inside draw().
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
    //! \a known is the frame the caller already built when it has one: the paint path hands the
    //! frame it replays with here, so an object that records nothing new builds no transform.
    bool ensureRecording(const QCPDrawable& object, const QCPDrawnView& view,
                         const QCPDrawFrame* known = nullptr);

    //! True when the recording would be remade for \a view: the test ensureRecording() takes. \a known
    //! is the frame the caller built, when it has one, so the paint path builds no second transform.
    [[nodiscard]] bool recordingIsStale(const QCPDrawable& object, const QCPDrawnView& view,
                                       const QCPDrawFrame* known = nullptr) const;

    //! True when \a recorded is the transform of the frame \a view describes, without building it when
    //! the caller hands over the frame it already has.
    static bool frameTransformMatches(const QCPDrawnView& view, const QCPDrawFrame* known,
                                      const QCPTransform& recorded);

    //! notes the ranges the layer now holds this object drawn at: a pan may shift them.
    void notePainted(const QCPDrawnView& view);

    struct State;

    //! Allocates the state on first use: an object created, restyled and removed without ever being
    //! drawn allocates none, and the generation keeps counting without it.
    void ensureState();

    //! Shared with the deferred painter callbacks: they check the object is still there.
    std::shared_ptr<State> mState;
    //! Beside the state, not in it: an object that never draws still counts its changes.
    quint64 mGeneration = 0;
    //! Whether the object is inside a recording, where a setter is a misuse.
    bool mInsideDraw = false;
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
