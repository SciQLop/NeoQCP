#pragma once

#include "../coords.h"
#include "../global.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QImage>
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

  The four things a text command carries besides its position and its string: the font, the colour
  (the pen its renderer draws with), the alignment that places the text relative to the anchor, and
  the rotation around that anchor in degrees. \a padding grows the measured rectangle on every side,
  which only matters for alignment and for a background drawn by a renderer.
*/
struct QCP_LIB_DECL QCPTextStyle
{
    QFont font;
    QColor color = Qt::black;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
    double rotationDegrees = 0.0;
    double padding = 0.0;
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

  A recording does not depend on the transform, only its replay does, so a cached recording can be
  replayed into a fresh frame without the object drawing itself again.
*/
[[nodiscard]] QCP_LIB_DECL QCPDrawFrame qcpMakeFrame(const QCPAxis* keyAxis,
                                                     const QCPAxis* valueAxis,
                                                     const QCPAxisRect* axisRect,
                                                     const QRectF& clip);

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

    //! The axes the object draws on; positions in \ref QCP::Data go through it.
    [[nodiscard]] const QCPTransform& transform() const { return mTransform; }

    //! The pixels the object may draw in; an empty rect means unclipped.
    [[nodiscard]] QRectF clip() const { return mClip; }

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
    //! A painter path whose origin is \a origin; the path itself is in pixels from there.
    void path(QCP::Position origin, const QPainterPath& path, const QPen& pen, const QBrush& brush);
    //! A raster image stretched into the rectangle between two corners.
    void image(QCP::Position a, QCP::Position b, const QImage& image);
    //! The escape hatch: arbitrary painter code, in pixels, with the frame's transform to map with.
    void painter(std::function<void(QCPPainter&, const QCPTransform&)> callback);

    //! The commands recorded so far, in draw order, with their data box.
    [[nodiscard]] const QCPDrawList& drawList() const;

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
