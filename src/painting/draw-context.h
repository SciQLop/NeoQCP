#pragma once

#include "../coords.h"
#include "../global.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QMargins>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>

#include <functional>
#include <memory>
#include <span>
#include <vector>

class QCPAxis;
class QCPAxisRect;
struct QCPDrawList;
class QCPPainter;
class QCPTheme;
class QCPScatterStyle;

/*!
  \brief How a piece of text is drawn by \ref QCPDrawContext::text.

  What a text command carries besides its position and its string: the font, the colour (the pen its
  renderer draws with), the border pen and the background brush of its box, the alignment that
  places the box relative to the anchor, the flags the glyphs are measured and drawn with, and the
  rotation around the anchor in degrees. \a padding grows the box on every side; the glyphs keep
  their own rect inside it, as QCPItemText places them.

  \a mathText measures and draws through \ref QCPLabelRenderer::defaultRenderer, which is what
  QCPItemText does (the LaTeX typesetter when the library is built with it). It is off by default
  because the renderer measures a plain string one pixel wider and taller than plain font metrics,
  so a recording that never promised math keeps the plain look.
*/
struct QCP_LIB_DECL QCPTextStyle
{
    QFont font;
    QColor color = Qt::black;
    QPen pen = Qt::NoPen; //!< the box's border
    QBrush brush = Qt::NoBrush; //!< the box's background
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
    int textFlags = Qt::AlignLeft; //!< how the glyphs are measured and drawn inside the box
    double rotationDegrees = 0.0;
    QMarginsF padding;
    bool mathText = false;
};

/*!
  \brief Everything a replay needs from the outside world: the two transforms and the clip.

  The transforms snapshot the axes and the axis rect when the frame is built, so a recorder can hand
  out its frame once per frame and the replay stays independent of what happens to the plot later.
  \a clip is in pixels; an empty clip means no clipping.
*/
struct QCP_LIB_DECL QCPDrawFrame
{
    QCPTransform transform;
    QCPRatioTransform ratio;
    QRectF clip;
};

/*!
  \brief The frame of one axis pair and one clip rect, built without recording anything.

  A recording usually does not depend on the transform, only its replay does, so a cached recording
  can be replayed into a fresh frame without the object drawing itself again. A recording that did
  read the transform is remade when the new one is not a translation of it.
*/
[[nodiscard]] QCP_LIB_DECL QCPDrawFrame qcpMakeFrame(const QCPAxis* keyAxis,
                                                     const QCPAxis* valueAxis,
                                                     const QCPAxisRect* axisRect,
                                                     const QRectF& clip);

//! A frame that fits \a box into \a rect: a replay without axes, as a legend icon does.
[[nodiscard]] QCP_LIB_DECL QCPDrawFrame qcpMakeFittedFrame(const QCPDataBox& box,
                                                           const QRectF& rect);

/*!
  \brief The recorder a drawing object receives: typed helpers that append to a \ref QCPDrawList.

  Every helper takes its geometry in the space that fits it -- data coordinates for series, \ref
  QCP::Position for anything anchored to an axis, a pixel offset or a fraction of the axis rect --
  and the commands keep it that way. A replay resolves the positions through the frame's transforms,
  which is what lets the same recording be drawn again after a pan or a zoom.

  The context is built by the library only (its constructor is private): a drawing object is handed
  one together with the list it records into, and never owns either.
*/
class QCP_LIB_DECL QCPDrawContext
{
public:
    QCPDrawContext(const QCPDrawContext&) = delete;
    QCPDrawContext& operator=(const QCPDrawContext&) = delete;
    ~QCPDrawContext();

    /*!
      The axes the object draws on; positions in \ref QCP::Data go through it.

      A recording is reused across pans and zooms: draw() runs once and the replay maps the same
      data positions through this frame's transform. Draw code must therefore not bake any
      pixel-dependent decision (a line width in pixels, a label placed at a pixel coordinate, a
      decision based on the current range) into the commands. Record what moves with the data as a
      position; ask \ref painter() for anything that needs this frame's pixels.

      Reading the transform is allowed for geometry that is genuinely pixels: the recording then
      carries \ref QCPDrawList::transformDependent and is remade on every change of the transform,
      a pan included, since every baked pixel moved. That is the price QCPBars pays for a width or
      a stacking offset in pixels. It is not free, so a recording that can be expressed in data
      must not read it: QCPBars with a width in plot coordinates never does, and keeps its
      recording across a pan and a zoom.
    */
    [[nodiscard]] const QCPTransform& transform() const;

    /*!
      The pixels the object may draw in; an empty rect means unclipped.

      This is the clip of the frame in which draw() ran, not necessarily the frame replaying the
      recording: the axis rect can move without the data moving. Reading it therefore counts as
      reading the transform, and the recording carries \ref QCPDrawList::transformDependent: it is
      remade on every change of the transform, so its commands and the clip they were cut for
      always come from the same frame.
    */
    [[nodiscard]] QRectF clip() const;

    //! The colours the plot is themed with, so a recorder can pick its default pen and brush.
    [[nodiscard]] const QCPTheme& theme() const;

    //! A polyline whose vertices are data coordinates (keys, values).
    void polyline(std::shared_ptr<const std::vector<QPointF>> data, const QPen& pen);
    //! A polyline whose vertices each carry their own space.
    void polyline(std::span<const QCP::Position> points, const QPen& pen);
    //! A filled and stroked polygon; its vertices each carry their own space.
    void polygon(std::span<const QCP::Position> points, const QPen& pen, const QBrush& brush);
    //! The area between two data-space series: \a upper forward, \a lower backward.
    void fillBetween(std::shared_ptr<const std::vector<QPointF>> upper,
                     std::shared_ptr<const std::vector<QPointF>> lower, const QBrush& brush);
    //! An axis-aligned rectangle between two corners, each in its own space.
    void rect(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush);
    //! An axis-aligned ellipse inside the rectangle between two corners.
    void ellipse(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush);
    //! One scatter symbol per data-space point.
    void markers(std::shared_ptr<const std::vector<QPointF>> data, const QCPScatterStyle& style);
    //! Vertical error bars: \a spans holds the half extents in data units, in keys and values.
    void errorBars(std::shared_ptr<const std::vector<QPointF>> centres,
                   std::shared_ptr<const std::vector<QPointF>> spans, const QPen& pen);
    //! Text anchored at \a at, measured and placed relative to that anchor.
    void text(QCP::Position at, const QString& text, const QCPTextStyle& style);
    //! The same, for a recorder that already measured \a glyphSize and will ask for it again.
    void text(QCP::Position at, const QString& text, const QCPTextStyle& style,
              const QSize& glyphSize);
    /*!
      Marks the recording as holding a position only available in pixels: the recorder fell back on
      the position's current pixel (see \ref QCPItemPosition::asPosition), so that command is a
      snapshot of this frame and nothing in the recording moves it. The recording is then remade on
      every frame, which is the only way such a position can follow what it is anchored to.
    */
    void recordEveryFrame();

    //! A painter path whose origin is \a origin; the path itself is in pixels from there.
    void path(QCP::Position origin, const QPainterPath& path, const QPen& pen, const QBrush& brush);
    //! A raster image stretched into the rectangle between two corners.
    void image(QCP::Position a, QCP::Position b, const QImage& image);
    //! The escape hatch: arbitrary painter code, in pixels, with the frame's transform to map with.
    void painter(std::function<void(QCPPainter&, const QCPTransform&)> callback);

    //! The commands recorded so far, in draw order, with their data box.
    [[nodiscard]] const QCPDrawList& drawList() const;

    //! The same list, moved out: a recorder that keeps it pays no copy for a whole bar chart.
    [[nodiscard]] QCPDrawList takeDrawList();

    //! What a replay needs; build it once per frame.
    [[nodiscard]] QCPDrawFrame frame() const
    {
        return qcpMakeFrame(mKeyAxis, mValueAxis, mAxisRect, mClip);
    }

private:
    QCPDrawContext(const QCPAxis* keyAxis, const QCPAxis* valueAxis, const QCPAxisRect* axisRect,
                   const QCPTheme* theme);
    friend class TestDrawContext;
    friend class TestRepaint; //!< the GPU replay tests record through the same door
    friend class Benchmark; //!< records through the same door to time the replay
    friend class QCPDrawnCore; //!< the adapter records an object's draw() into its cache

    const QCPAxis* mKeyAxis = nullptr;
    const QCPAxis* mValueAxis = nullptr;
    const QCPAxisRect* mAxisRect = nullptr;
    std::unique_ptr<QCPDrawList> mList;
    QCPTransform mTransform;
    QCPRatioTransform mRatio;
    QRectF mClip;
    const QCPTheme* mTheme = nullptr;
};
