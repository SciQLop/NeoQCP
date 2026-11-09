#pragma once

// Private to the library: what QCPDrawContext records and what qcpReplayWithPainter consumes.
// Not part of the public umbrella header (qcp.h).

#include "draw-context.h"

#include "../scatterstyle.h"
#include "../spatial-index.h"

#include <QBrush>
#include <QImage>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QSizeF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <algorithm>
#include <atomic>
#include <cmath>

#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

class QCPPainter;

namespace qcp
{
//! How many times a vector export upscales an image: enough to reach three times the target
//! rect's resolution, never more than three, never less than one.
[[nodiscard]] inline int vectorExportUpscaleFactor(const QSizeF& target, const QSizeF& source)
{
    if (source.width() <= 0 || source.height() <= 0)
        return 1;
    const double needed = 3.0 * std::max(target.width() / source.width(),
                                         target.height() / source.height());
    return std::clamp(static_cast<int>(std::ceil(needed)), 1, 3);
}

//! A polyline of data-space vertices, each pair of them painted in the colour of its first vertex
//! when \a colours is set: one entry per point, null for the pen's own colour.
struct PolylineDataCmd
{
    std::shared_ptr<const std::vector<QPointF>> points;
    std::shared_ptr<const std::vector<QRgb>> colours;
    QPen pen;
};

//! A polyline whose vertices each carry their own space.
struct PolylinePositionsCmd
{
    std::vector<QCP::Position> points;
    QPen pen;
};

//! A filled and stroked polygon of position-space vertices.
struct PolygonCmd
{
    std::vector<QCP::Position> points;
    QPen pen;
    QBrush brush;
};

//! The area between two data-space series.
struct FillBetweenCmd
{
    std::shared_ptr<const std::vector<QPointF>> upper;
    std::shared_ptr<const std::vector<QPointF>> lower;
    QBrush brush;
};

//! A rectangle between two corners, each in its own space.
struct RectCmd
{
    QCP::Position a;
    QCP::Position b;
    QPen pen;
    QBrush brush;
};

//! An ellipse inside the rectangle between two corners.
struct EllipseCmd
{
    QCP::Position a;
    QCP::Position b;
    QPen pen;
    QBrush brush;
};

/*!
  The arrays of a box batch: four corners and an optional colour per box.

  Filled while the recorder appends, shared read-only once it stops. A recording is copied - the
  item index keeps one, the legend icon another - and behind a pointer a copy costs a refcount
  instead of a megabyte. \a fills is either empty, and every box is the command's \a brush, or one
  colour per box: a solid brush of that colour replaces \a brush for that box (a selected bar paints
  its own colour without breaking the batch).
*/
struct BatchBoxes
{
    std::vector<double> aKey;
    std::vector<double> aValue;
    std::vector<double> bKey;
    std::vector<double> bValue;
    std::vector<QRgb> fills;
};

/*!
  Many rectangles, one command.

  A bar chart records ten thousand rects a frame, and as commands they cost a variant slot, a cache
  key and a visit each. Here they are one command: the recorder appends a rect in constant time,
  the GPU replay builds the same vertices one rect at a time, and the painter replay draws one rect
  at a time, so a PDF keeps one vector rect per bar.

  The corners are data on both sides by construction, which is why the batch moves by one delta on
  a pan. The arrays live behind \a boxes, so a command that is not a batch does not pay for them:
  the variant stays as wide as the one-shape commands. \a bounds is the data box the recorder
  widened as it appended, and \a sortedByKey says whether the keys arrived in ascending order (R5's
  fast paths read it).
*/
struct RectsCmd
{
    //! Never null in a recording; two commands never share one (see appendShape in draw-context).
    std::shared_ptr<const BatchBoxes> boxes;
    QPen pen;
    QBrush brush;
    QCPDataBox bounds;
    bool sortedByKey = true;
};

//! Many ellipses, one command: the same batch, with an ellipse in each box.
struct EllipsesCmd
{
    std::shared_ptr<const BatchBoxes> boxes;
    QPen pen;
    QBrush brush;
    QCPDataBox bounds;
    bool sortedByKey = true;
};

//! One scatter symbol per data-space point, each in its own colour when \a colours is set: one
//! entry per point, null for the style's own pen and brush.
struct MarkersCmd
{
    std::shared_ptr<const std::vector<QPointF>> points;
    std::shared_ptr<const std::vector<QRgb>> colours;
    //! Shared: a style carries a QPen, a QBrush, a QPixmap and a QPainterPath, which would widen
    //! the command past the variant's cache line and a half for a copy nobody ever mutates.
    std::shared_ptr<const QCPScatterStyle> style;
};

//! Error bars around data-space centres. Both forms carry data only, plus pixel widths the replay
//! applies, so a pan shifts the bars instead of remaking them; which one is drawn follows from
//! \a ends being set.
struct ErrorBarsCmd
{
    //! The bar centres, in plot coordinates.
    std::shared_ptr<const std::vector<QPointF>> centres;
    //! Half extents in data units, in keys and values: a symmetric bar, drawn as a cross with its
    //! caps, which is what a multi graph's error bars are.
    std::shared_ptr<const std::vector<QPointF>> spans;
    //! Two end points in plot coordinates per bar, the minus one first: a bar whose two sides may
    //! differ, drawn with a pixel \a whiskerWidth and a pixel \a symbolGap. A side that is not a
    //! number is a side the bar does not have.
    std::shared_ptr<const std::vector<QPointF>> ends;
    //! Whether \a ends run along the keys, or else along the values.
    bool alongKeys = false;
    //! The whisker width and the gap left around the centre, in pixels.
    double whiskerWidth = 0.0;
    double symbolGap = 0.0;
    //! Whether \a centres and \a ends are already pixels: a bar that does not sit at its own key
    //! (grouped or stacked bars) records where the plottable puts it.
    bool inPixels = false;
    QPen pen;
};

//! Text anchored at a position. The style is shared: it carries a QFont and a QColor, which would
//! push the command past a cache line and a half, and text commands are never hot.
//! A text command's two rects, relative to its anchor: the box (glyphs grown by the padding and
//! placed by the style's position alignment) and the glyphs inside it.
struct QCPTextLayout
{
    QRect box;
    QRect glyphs;
};

struct TextCmd
{
    QCP::Position at;
    QString text;
    std::shared_ptr<const QCPTextStyle> style;
    //! Measured once, when the command is recorded: measuring is the expensive half of a text, and
    //! the recording outlives the frame. The layout follows from it (see qcpTextLayout).
    QSize glyphSize;
};

//! A painter path whose origin is a position; the path is in pixels from there.
struct PathCmd
{
    QCP::Position origin;
    QPainterPath path;
    QPen pen;
    QBrush brush;
};

//! What an image command draws: the picture, the texels of it to show, and the sampler it wants.
struct ImagePayload
{
    QImage image;
    QRectF source; //!< in texels; the whole image when empty
    bool smooth = false; //!< a linear sampler, else nearest
};

//! A raster image stretched between two corners: \a a is where the top-left texel lands, \a b where
//! the bottom-right one does. The replay never normalises, so a reversed axis mirrors the image.
struct ImageCmd
{
    QCP::Position a;
    QCP::Position b;
    //! Shared: the picture and those choices do not fit the budget a command is held to.
    std::shared_ptr<const ImagePayload> pixels;
};

//! Arbitrary painter code; the escape hatch for what has no command yet.
struct PainterCmd
{
    std::function<void(QCPPainter&, const QCPTransform&)> callback;
};

//! One draw command. Ordered by the alternatives below, so a replay is a single visit.
using QCPDrawCommand
    = std::variant<PolylineDataCmd, PolylinePositionsCmd, PolygonCmd, FillBetweenCmd, RectCmd,
                   EllipseCmd, RectsCmd, EllipsesCmd, MarkersCmd, ErrorBarsCmd, TextCmd, PathCmd,
                   ImageCmd, PainterCmd>;

//! What a replay asks of a command: one visit answers every question about its kind.
//!
//! \a gpu is what the RHI replay draws (on the screen the painter leaves it to it), \a painterOnly
//! is what keeps no geometry at all (the painter draws it every frame), and the last two name the
//! space its positions are in: \a dataOnly when every one is data, so a kept triangle set moves by
//! one delta when the transform pans, and \a staticOnly when every one is pixels, so it stays where
//! it is. A ratio or an anchor is neither: a recording that keeps one is rebuilt from its commands
//! rather than shifted.
struct CommandKind
{
    bool gpu = false;
    bool painterOnly = false;
    bool dataOnly = false;
    bool staticOnly = false;
};

//! True when every position of \a positions is a \a Space.
template <class Space>
bool allPositionsIn(const std::vector<QCP::Position>& positions)
{
    return std::all_of(positions.begin(), positions.end(),
                       [](const QCP::Position& position)
                       { return std::holds_alternative<Space>(position); });
}

inline CommandKind commandKind(const QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, PolylineDataCmd> || std::is_same_v<T, FillBetweenCmd>
                          || std::is_same_v<T, MarkersCmd> || std::is_same_v<T, ErrorBarsCmd>
                          || std::is_same_v<T, RectsCmd> || std::is_same_v<T, EllipsesCmd>)
                return CommandKind { true, false, true, false }; // data by type, never mapped
            else if constexpr (std::is_same_v<T, TextCmd> || std::is_same_v<T, PathCmd>
                               || std::is_same_v<T, PainterCmd>)
                return CommandKind { false, true, false, false };
            else if constexpr (std::is_same_v<T, ImageCmd>)
                // Two data corners make a textured quad a pan moves; any other corner keeps the
                // picture the painter's, as before.
                return std::holds_alternative<QCP::Data>(cmd.a)
                        && std::holds_alternative<QCP::Data>(cmd.b)
                    ? CommandKind { true, false, true, false }
                    : CommandKind { false, true, false, false };
            else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>)
                return CommandKind { true, false,
                                     std::holds_alternative<QCP::Data>(cmd.a)
                                         && std::holds_alternative<QCP::Data>(cmd.b),
                                     std::holds_alternative<QCP::Pixels>(cmd.a)
                                         && std::holds_alternative<QCP::Pixels>(cmd.b) };
            else
                return CommandKind { true, false, allPositionsIn<QCP::Data>(cmd.points),
                                     allPositionsIn<QCP::Pixels>(cmd.points) };
        },
        command);
}

//! True when the GPU replay draws \a command. Answered by type, so a polygon or a long position
//! list does not pay the per-point scans of commandKind() on a painter pass. An image is the
//! exception: its two corners decide it, and there are only two.
inline bool isGpuCommand(const QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, ImageCmd>)
                return std::holds_alternative<QCP::Data>(cmd.a)
                    && std::holds_alternative<QCP::Data>(cmd.b);
            else
                return !(std::is_same_v<T, TextCmd> || std::is_same_v<T, PathCmd>
                         || std::is_same_v<T, PainterCmd>);
        },
        command);
}

/*!
  Whether replaying \a command needs a key and a value axis: it holds a position the transform maps,
  a data one or a half data and half ratio one, or it is data by construction. Pixels, axis-rect
  fractions and anchor offsets replay on a plot whose layout holds no axis at all.
*/
inline bool qcpCommandNeedsAxes(const QCPDrawCommand& command)
{
    const auto needsAxes = [](const QCP::Position& position)
    {
        return std::holds_alternative<QCP::Data>(position)
            || std::holds_alternative<QCP::DataXRatioY>(position)
            || std::holds_alternative<QCP::RatioXDataY>(position);
    };
    return std::visit(
        [&needsAxes](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, PolylineDataCmd> || std::is_same_v<T, FillBetweenCmd>
                          || std::is_same_v<T, MarkersCmd> || std::is_same_v<T, RectsCmd>
                          || std::is_same_v<T, EllipsesCmd> || std::is_same_v<T, PainterCmd>)
                return true; // data by type, or a callback that maps with the transform itself
            else if constexpr (std::is_same_v<T, ErrorBarsCmd>)
                return !cmd.inPixels; // a grouped or stacked bar records where it sits, in pixels
            else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>
                               || std::is_same_v<T, ImageCmd>)
                return needsAxes(cmd.a) || needsAxes(cmd.b);
            else if constexpr (std::is_same_v<T, TextCmd>)
                return needsAxes(cmd.at);
            else if constexpr (std::is_same_v<T, PathCmd>)
                return needsAxes(cmd.origin);
            else
                return std::any_of(cmd.points.begin(), cmd.points.end(), needsAxes);
        },
        command);
}

} // namespace qcp

//! The size \a text needs in \a style: the measurement a text command keeps.
[[nodiscard]] QCP_LIB_DECL QSize qcpMeasureText(const QCPTextStyle& style, const QString& text);

/*!
  The layout of glyphs of \a glyphSize in \a style, as QCPItemText has always drawn it: one place
  computes it, so the painter replay, the hit test and an item's anchors cannot disagree.
*/
[[nodiscard]] QCP_LIB_DECL qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style,
                                                            const QSize& glyphSize);

//! The layout of \a text in \a style: measure, then lay out.
[[nodiscard]] QCP_LIB_DECL qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style,
                                                            const QString& text);

//! Test hook: how many texts have been measured since the process started.
[[nodiscard]] QCP_LIB_DECL quint64 qcpTextMeasureCount();

//! A new identity per recording; copies and re-records take a fresh one.
inline quint64 qcpNextDrawListId()
{
    static std::atomic<quint64> next { 1 };
    return next.fetch_add(1, std::memory_order_relaxed);
}

/*!
  \brief What a QCPDrawContext recorded: the commands in draw order and their data box.

  Each box follows the same rule as an item's data extent: a side is meaningful only while its
  keyBounded / valueBounded flag is set, which it is only when every command recorded so far is
  data on that side. One pixel, ratio or anchored command makes that side unbounded for the rest of
  the recording, and a side that stayed bounded holds the min/max of the data coordinates.
  A recording keeps two of them: \a data, which feeds its ranges and its legend icon, and \a index,
  the pixel-padded box the item index and the culling use. They differ on the commands whose glyphs
  live at an anchor - a text at a data position is data, but its box never shrinks the index.

  A log axis makes a non-positive lower bound unmappable, exactly as it does for an item's extent
  (QCPAbstractItem::positionsExtent drops that side). QCPDrawnCore::dataRange applies that rule when
  the box describes an item; a plottable's range is data, so its getters ask for the bounds without
  the rule and let the sign domain decide.
*/
struct QCP_LIB_DECL QCPDrawList
{
    QCPDrawList() = default;

    //! The box of a recording on one side: bounds are meaningful on a side only while its flag is
    //! set, and \a hasSamples goes true as soon as a command with a sample set them.
    struct Box
    {
        QCPDataBox bounds { -std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::infinity() };
        bool keyBounded = false;
        bool valueBounded = false;
        bool hasSamples = false;

        //! Merges one command's box: a side it does not bound leaves the recording's unbounded too.
        void merge(const QCPDataBox& commandBox, bool commandHasKey, bool commandHasValue)
        {
            if (!hasSamples)
                bounds = commandBox;
            else
            {
                bounds.keyLower = std::min(bounds.keyLower, commandBox.keyLower);
                bounds.keyUpper = std::max(bounds.keyUpper, commandBox.keyUpper);
                bounds.valueLower = std::min(bounds.valueLower, commandBox.valueLower);
                bounds.valueUpper = std::max(bounds.valueUpper, commandBox.valueUpper);
            }
            keyBounded = hasSamples ? keyBounded && commandHasKey : commandHasKey;
            valueBounded = hasSamples ? valueBounded && commandHasValue : commandHasValue;
            hasSamples = true;
        }
    };

    //! A copy is a new recording: it can never be matched by an image of the one it came from.
    QCPDrawList(const QCPDrawList& other) { *this = other; }

    QCPDrawList& operator=(const QCPDrawList& other)
    {
        commands = other.commands;
        data = other.data;
        index = other.index;
        viewDependent = other.viewDependent;
        anchorDependent = other.anchorDependent;
        transformDependent = other.transformDependent;
        everyFrame = other.everyFrame;
        keyWindow = other.keyWindow;
        rasterCommands = other.rasterCommands;
        id = qcpNextDrawListId();
        return *this;
    }

    std::vector<qcp::QCPDrawCommand> commands;
    //! What the recording covers in data: its key/value ranges and its legend-icon fit.
    Box data;
    //! The box for the item index and for culling, as it always was: a text, a path or a painter
    //! callback leaves it unbounded, whatever position its anchor names.
    Box index;
    //! A command read the theme (or another view fact the commands do not carry): re-record when
    //! the view epoch moves, even if the command list itself looks view-independent.
    bool viewDependent = false;
    //! A command holds a QCP::AnchorOffset: an item owns that anchor, and removing the item
    //! invalidates this recording even though its generation did not move.
    bool anchorDependent = false;
    //! A command read the frame's transform and baked a pixel out of it (a bar width in pixels):
    //! the recording is still good after a pan, but not after a zoom or a resize.
    bool transformDependent = false;
    //! A command holds a position recorded as pixels because its own space cannot express it (see
    //! QCPItemPosition::asPosition): a snapshot, which goes stale without the object's generation
    //! moving. Such a recording is remade every frame.
    bool everyFrame = false;
    //! The key range and scale the recording drew for, when it asked for one (see
    //! QCPDrawContext::keyWindow): the recording is reused while the view stays inside that window
    //! at the same scale, and translated rather than remade for a pan inside it.
    std::optional<QCPKeyWindow> keyWindow;
    //! How many commands the replay has to rasterise (see needsRaster): a raster image is a
    //! scarce resource, and counting here keeps the query off every frame's replay.
    int rasterCommands = 0;
    quint64 id = qcpNextDrawListId(); //!< unique: with a command's index it is a cache key
};

//! True when any command of \a list needs a key and a value axis (see qcpCommandNeedsAxes): the
//! answer a recording made without axes has to give before it may be replayed.
inline bool qcpNeedsAxes(const QCPDrawList& list)
{
    return std::any_of(list.commands.begin(), list.commands.end(), qcp::qcpCommandNeedsAxes);
}

//! The one place a QCP::Position becomes pixels: every command and every caller goes through it.
[[nodiscard]] QPointF qcpResolvePosition(const QCPDrawFrame& frame, const QCP::Position& position);

//! \a positions mapped through \a frame into \a out, in pixels.
inline void qcpResolveInto(std::span<const QCP::Position> positions, const QCPDrawFrame& frame,
                           std::vector<QPointF>& out)
{
    out.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
        out[i] = qcpResolvePosition(frame, positions[i]);
}

//! \a points as a polygon in \a out: no allocation once its storage has grown.
inline void qcpPolygonOf(std::span<const QPointF> points, QPolygonF& out)
{
    out.resize(int(points.size()));
    std::copy(points.begin(), points.end(), out.begin());
}

//! \a open with its first point appended: what a closed stroke extrudes.
inline std::vector<QPointF> qcpRingOf(std::span<const QPointF> open)
{
    std::vector<QPointF> ring(open.begin(), open.end());
    if (!ring.empty())
        ring.push_back(ring.front());
    return ring;
}

//! \a command's place in \a list: with the list's id it names the command across replays.
inline quint64 qcpCommandIndex(const QCPDrawList& list, const qcp::QCPDrawCommand& command)
{
    return quint64(&command - list.commands.data());
}

//! Draws \a list into \a painter through \a frame: the QPainter path (screen fallback and export).
//! What a replay is for: All paints everything (no GPU pass), PainterOnly is the other half of a
//! screen frame and leaves the GPU kinds to the RHI replay, GpuOnly is the export's second pass:
//! the QPainter form of the GPU kinds, which an export draws over the layer's painter content.
enum class QCPReplayKinds
{
    All,
    PainterOnly,
    GpuOnly
};

//! The four corners of each error bar in data space: above, below, left and right of the centre.
//! Both replays start here, so a span is never mistaken for a position.
inline void qcpErrorBarCorners(std::span<const QPointF> centres, std::span<const QPointF> spans,
                               std::vector<QPointF>& out)
{
    const std::size_t count = std::min(centres.size(), spans.size());
    out.resize(4 * count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = centres[i];
        const QPointF span = spans[i];
        out[4 * i + 0] = QPointF(centre.x(), centre.y() + span.y());
        out[4 * i + 1] = QPointF(centre.x(), centre.y() - span.y());
        out[4 * i + 2] = QPointF(centre.x() - span.x(), centre.y());
        out[4 * i + 3] = QPointF(centre.x() + span.x(), centre.y());
    }
}

//! An ellipse inscribed in \a box: \a segments points on the outline, open (close it with
//! qcpRingOf).
inline std::vector<QPointF> qcpEllipseRing(const QRectF& box, int segments = 96)
{
    const QPointF centre = box.center();
    const double rx = box.width() / 2.0;
    const double ry = box.height() / 2.0;
    std::vector<QPointF> ring;
    ring.reserve(std::size_t(segments));
    for (int i = 0; i < segments; ++i)
    {
        const double angle = 2.0 * M_PI * i / segments;
        ring.emplace_back(centre.x() + rx * std::cos(angle), centre.y() + ry * std::sin(angle));
    }
    return ring;
}

//! Data-space \a points through \a transform into \a out: a series is positions like any other.
inline void qcpMapDataInto(std::span<const QPointF> points, const QCPTransform& transform,
                           std::vector<QPointF>& out)
{
    out.resize(points.size());
    transform.map(points, std::span<QPointF>(out));
}

//! The pixel polyline of error bars from their four mapped corners: bar plus two caps, with a gap
//! between two bars so a replay never joins them.
inline void qcpErrorBarPolyline(std::span<const QPointF> corners, std::vector<QPointF>& out)
{
    const QPointF gap(std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN());
    const std::size_t count = corners.size() / 4;
    out.clear();
    out.reserve(count * 9);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF& above = corners[4 * i + 0];
        const QPointF& below = corners[4 * i + 1];
        const QPointF& left = corners[4 * i + 2];
        const QPointF& right = corners[4 * i + 3];
        out.push_back(above);
        out.push_back(below);
        out.push_back(gap);
        out.push_back(QPointF(left.x(), above.y()));
        out.push_back(QPointF(right.x(), above.y()));
        out.push_back(gap);
        out.push_back(QPointF(left.x(), below.y()));
        out.push_back(QPointF(right.x(), below.y()));
        out.push_back(gap);
    }
}

//! The pixel polyline of \a spans around \a centres, through \a frame: what both replays stroke.
inline void qcpErrorBarPixels(const QCPDrawFrame& frame, std::span<const QPointF> centres,
                              std::span<const QPointF> spans, std::vector<QPointF>& out)
{
    static thread_local std::vector<QPointF> data;
    static thread_local std::vector<QPointF> corners;
    qcpErrorBarCorners(centres, spans, data);
    corners.resize(data.size());
    frame.transform.map(std::span<const QPointF>(data), std::span<QPointF>(corners));
    qcpErrorBarPolyline(corners, out);
}

//! The pixel polyline of \a cmd's bars, through \a frame: what every replay strokes, what the hit
//! test measures and what the index boxes. A bar whose \a ends are given is two backbones, each
//! starting at the gap around the centre, with a whisker across each end.
inline void qcpErrorBarPixels(const QCPDrawFrame& frame, const qcp::ErrorBarsCmd& cmd,
                              std::vector<QPointF>& out)
{
    if (!cmd.ends)
    {
        qcpErrorBarPixels(frame, std::span<const QPointF>(*cmd.centres),
                          std::span<const QPointF>(*cmd.spans), out);
        return;
    }
    static thread_local std::vector<QPointF> centres;
    static thread_local std::vector<QPointF> ends;
    const std::size_t count = std::min(cmd.centres->size(), cmd.ends->size() / 2);
    centres.resize(count);
    ends.resize(2 * count);
    if (cmd.inPixels)
    {
        std::copy(cmd.centres->begin(), cmd.centres->end(), centres.begin());
        std::copy(cmd.ends->begin(), cmd.ends->end(), ends.begin());
    }
    else
    {
        frame.transform.map(std::span<const QPointF>(*cmd.centres), std::span<QPointF>(centres));
        frame.transform.map(std::span<const QPointF>(*cmd.ends), std::span<QPointF>(ends));
    }

    // The error axis's pixel direction, taken from one bar: a reversed axis, a vertical key axis
    // and a log axis all come out of the transform, so none of them needs a case of its own. Bars
    // that come in pixels already carry their direction in their own ends.
    QPointF axisDir(cmd.alongKeys ? 1.0 : 0.0, cmd.alongKeys ? 0.0 : 1.0);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(centres[i].x()) || !std::isfinite(centres[i].y()))
            continue;
        QPointF direction;
        if (cmd.inPixels)
        {
            const QPointF end = ends[2 * i + 1];
            direction = QPointF(end.x() - centres[i].x(), end.y() - centres[i].y());
        }
        else
        {
            const QPointF data = cmd.centres->at(i);
            const QCP::Pixels base = frame.transform.map(QCP::Data { data.x(), data.y() });
            const QCP::Pixels ahead = frame.transform.map(
                QCP::Data { data.x() + (cmd.alongKeys ? 1.0 : 0.0),
                            data.y() + (cmd.alongKeys ? 0.0 : 1.0) });
            direction = QPointF(ahead.x - base.x, ahead.y - base.y);
        }
        const double length = std::hypot(direction.x(), direction.y());
        if (std::isfinite(length) && length > 0.0)
            axisDir = direction / length;
        break;
    }
    const QPointF across(-axisDir.y(), axisDir.x());
    const QPointF gap(std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN());
    const double halfWhisker = cmd.whiskerWidth * 0.5;
    out.clear();
    out.reserve(12 * count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = centres[i];
        if (!std::isfinite(centre.x()) || !std::isfinite(centre.y()))
            continue;
        for (int side = 0; side < 2; ++side)
        {
            const QPointF end = ends[2 * i + std::size_t(side)];
            if (!std::isfinite(end.x()) || !std::isfinite(end.y()))
                continue;
            const double sign = side == 0 ? -1.0 : 1.0;
            const QPointF start = centre + sign * cmd.symbolGap * 0.5 * axisDir;
            const double along = (end.x() - centre.x()) * axisDir.x()
                + (end.y() - centre.y()) * axisDir.y();
            // The backbone starts at the end of the gap: an error that does not reach past it has
            // no backbone at all, rather than a stub running backwards towards the centre.
            if (along * sign > cmd.symbolGap * 0.5)
            {
                out.push_back(start);
                out.push_back(end);
                out.push_back(gap);
            }
            out.push_back(end - halfWhisker * across);
            out.push_back(end + halfWhisker * across);
            out.push_back(gap);
        }
    }
}

namespace qcp
{
//! How far the stroke of \a pen reaches beyond the geometry it strokes, in pixels.
inline double qcpStrokePad(const QPen& pen)
{
    return pen.style() == Qt::NoPen ? 1.0 : std::max(1.0, pen.widthF()) + 1.0;
}

//! The box \a pixels cover; a pixel that is not finite carries nothing, so a gap in a series or
//! between two error bars never widens the box to infinity. The corners are tracked by hand: a box
//! of points has no extent, and QRectF::united() returns the other rect when either is null, so it
//! would keep the last point alone.
inline QRectF qcpPixelBox(std::span<const QPointF> pixels)
{
    QPointF low;
    QPointF high;
    bool empty = true;
    for (const QPointF& pixel : pixels)
    {
        if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y()))
            continue;
        low = empty ? pixel : QPointF(std::min(low.x(), pixel.x()), std::min(low.y(), pixel.y()));
        high = empty ? pixel : QPointF(std::max(high.x(), pixel.x()), std::max(high.y(), pixel.y()));
        empty = false;
    }
    return empty ? QRectF() : QRectF(low, high);
}

//! \a positions resolved through \a frame and boxed; \a points mapped through \a transform and
//! boxed: the extent of one command's geometry either way.
inline QRectF qcpPositionBox(std::span<const QCP::Position> positions, const QCPDrawFrame& frame)
{
    static thread_local std::vector<QPointF> pixels;
    qcpResolveInto(positions, frame, pixels);
    return qcpPixelBox(pixels);
}

inline QRectF qcpDataBox(std::span<const QPointF> points, const QCPTransform& transform)
{
    static thread_local std::vector<QPointF> pixels;
    qcpMapDataInto(points, transform, pixels);
    return qcpPixelBox(pixels);
}

/*!
  What the paint of \a command covers in pixels: the box its geometry spans, the pen that strokes
  it, and how far that paint reaches beyond the box. The clip test asks whether a command is
  visible, the raster asks how much room the image it needs takes: one visit answers both, and the
  caller adds only the inflation its own question takes.
*/
struct CommandExtent
{
    QRectF box; //!< the geometry's box, empty when the command spans nothing
    QPen pen = Qt::NoPen; //!< the stroke, where the command has one
    double pad = 1.0; //!< how far the paint reaches beyond \a box
    //! False where boxing would cost what drawing costs: a data series (unless \a mapSeries asked
    //! for it) and the kinds whose geometry is a path or a callback.
    bool cullable = false;
};

//! \a command's extent at \a frame; \a mapSeries boxes a data series too.
inline CommandExtent commandExtent(const QCPDrawCommand& command, const QCPDrawFrame& frame,
                                   bool mapSeries = false)
{
    return std::visit(
        [&](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            CommandExtent extent;
            if constexpr (requires { cmd.pen; })
            {
                extent.pen = cmd.pen;
                extent.pad = qcpStrokePad(cmd.pen);
            }
            if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>
                          || std::is_same_v<T, ImageCmd>)
            {
                extent.box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                extent.cullable = true;
            }
            else if constexpr (std::is_same_v<T, RectsCmd> || std::is_same_v<T, EllipsesCmd>)
            {
                // A batch is one command over many boxes: the union is what culling and the raster
                // ask for. Built from both corners, so a rect whose corners arrive reversed still
                // widens the box it occupies.
                const BatchBoxes& boxes = *cmd.boxes;
                const std::size_t count = std::min(boxes.aKey.size(), boxes.bKey.size());
                double left = 0.0;
                double top = 0.0;
                double right = 0.0;
                double bottom = 0.0;
                for (std::size_t i = 0; i < count; ++i)
                {
                    const QPointF a
                        = qcpResolvePosition(frame, QCP::Data { boxes.aKey[i], boxes.aValue[i] });
                    const QPointF b
                        = qcpResolvePosition(frame, QCP::Data { boxes.bKey[i], boxes.bValue[i] });
                    const double x0 = std::min(a.x(), b.x());
                    const double x1 = std::max(a.x(), b.x());
                    const double y0 = std::min(a.y(), b.y());
                    const double y1 = std::max(a.y(), b.y());
                    if (i == 0)
                    {
                        left = x0;
                        top = y0;
                        right = x1;
                        bottom = y1;
                    }
                    else
                    {
                        left = std::min(left, x0);
                        top = std::min(top, y0);
                        right = std::max(right, x1);
                        bottom = std::max(bottom, y1);
                    }
                }
                if (count > 0)
                    extent.box = QRectF(QPointF(left, top), QPointF(right, bottom));
                extent.cullable = true;
            }
            else if constexpr (std::is_same_v<T, PolygonCmd>
                               || std::is_same_v<T, PolylinePositionsCmd>)
            {
                extent.box = qcpPositionBox(cmd.points, frame);
                extent.cullable = true;
            }
            else if constexpr (std::is_same_v<T, MarkersCmd>)
            {
                extent.box = qcpDataBox(*cmd.points, frame.transform);
                extent.pad = cmd.style->size() + 1.0;
                extent.cullable = true;
            }
            else if constexpr (std::is_same_v<T, TextCmd>)
            {
                QTransform rotation;
                const QPointF at = qcpResolvePosition(frame, cmd.at);
                rotation.translate(at.x(), at.y());
                rotation.rotate(cmd.style->rotationDegrees);
                extent.box = rotation.mapRect(QRectF(qcpTextLayout(*cmd.style, cmd.glyphSize).box));
                extent.pad = 0.0;
                extent.cullable = true;
            }
            else if constexpr (std::is_same_v<T, PolylineDataCmd>)
            {
                if (mapSeries)
                    extent.box = qcpDataBox(*cmd.points, frame.transform);
            }
            else if constexpr (std::is_same_v<T, FillBetweenCmd>)
            {
                if (mapSeries)
                    extent.box = qcpDataBox(*cmd.upper, frame.transform)
                                     .united(qcpDataBox(*cmd.lower, frame.transform));
            }
            else if constexpr (std::is_same_v<T, ErrorBarsCmd>)
            {
                if (mapSeries)
                {
                    static thread_local std::vector<QPointF> bars;
                    qcpErrorBarPixels(frame, cmd, bars);
                    extent.box = qcpPixelBox(std::span<const QPointF>(bars));
                }
            }
            return extent;
        },
        command);
}
} // namespace qcp

namespace qcp
{
class QCPRasterFallback;

/*!
  How \a command is painted: \a withStroke leaves out the outline of a filled shape when the
  caller wants its fill alone, and \a antialiased turns antialiasing on, since a command
  rasterised on its own has no plot to inherit the setting from.
*/
struct PaintMode
{
    bool withStroke = true;
    bool antialiased = false;
};

//! Paints \a command into \a painter at \a frame: the one painter path of a command, used by the
//! painter replay and by the raster fallback.
void paintCommand(QCPPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                  PaintMode mode = { });
} // namespace qcp

//! Whether the replay has to bracket its work with save()/restore() and set the frame's clip: the
//! layer already saves and clips at clipRect().translated(0, -1) for every child it draws
//! (QCPLayer::draw), so a layerable's own replay is AlreadySaved and does neither.
enum class QCPReplayState
{
    Save,
    AlreadySaved
};

//! The commands a full frame could not draw on the GPU (no QRhi, export, or the layer's image cap)
//! are painted here: in PainterOnly a GPU kind is drawn only when \a fallback says the layer left
//! it to this pass.
void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame,
                          QCPReplayKinds kinds = QCPReplayKinds::All,
                          const qcp::QCPRasterFallback* fallback = nullptr,
                          bool dashedAvailable = true, bool antialiasStrokes = false,
                          QCPReplayState state = QCPReplayState::Save);
