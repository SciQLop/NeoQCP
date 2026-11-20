#include "test-zoom-indicator.h"

#include "misuse-recorder.h"

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace
{
double bivariateNormal(double x, double y, double sx, double sy, double mx, double my)
{
    const double u = (x - mx) / sx, v = (y - my) / sy;
    return std::exp(-0.5 * (u * u + v * v)) / (2 * std::numbers::pi * sx * sy);
}

// The inset-zoom target program's data: matplotlib's sample axes_grid/bivariate_normal.npy (15 x 15,
// step 0.5 from (-3, -4)), pasted at row 30, column 30 of a 150 x 150 zero image.
double image(int column, int row)
{
    const int i = column - 30, j = row - 30;
    if (i < 0 || i >= 15 || j < 0 || j >= 15)
        return 0;
    const double x = -3 + 0.5 * i, y = -4 + 0.5 * j;
    return 10 * (bivariateNormal(x, y, 1.0, 1.0, 0, 0) - bivariateNormal(x, y, 1.5, 0.5, 1, 1));
}

QImage rendered(QCustomPlot& plot)
{
    return plot.toPixmap(plot.width(), plot.height())
        .toImage()
        .convertToFormat(QImage::Format_ARGB32);
}

// The box and the connectors draw a black pen, and the colour map's lowest viridis colour keeps a
// red channel of 68: ink is a pixel dark in every channel, a background pixel is not.
bool isInk(const QColor& colour)
{
    return colour.red() < 40 && colour.green() < 40 && colour.blue() < 40;
}

bool hasInkNear(const QImage& image, const QPoint& at, int radius = 2)
{
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (isInk(image.pixelColor(at.x() + dx, at.y() + dy)))
                return true;
    return false;
}

QPoint pointIn(const QCPAxis& keyAxis, const QCPAxis& valueAxis, double key, double value)
{
    return QPoint(qRound(keyAxis.coordToPixel(key)), qRound(valueAxis.coordToPixel(value)));
}

// A probe on the connector that joins \a insetCorner to \a boxCorner, \a distance pixels out of the
// inset along it: far enough from the inset's own axes to see the connector and nothing else.
QPoint onConnectorNear(const QPointF& insetCorner, const QPointF& boxCorner, double distance = 4.0)
{
    const QPointF direction = boxCorner - insetCorner;
    const double length = std::hypot(direction.x(), direction.y());
    return (insetCorner + direction / length * distance).toPoint();
}

// The image with the indicator hidden: what the scene draws without it. A pixel that differs from
// it by more than \a drop grey levels is drawn by the indicator, antialiasing included.
bool differsNear(const QImage& image, const QImage& control, const QPoint& at, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const QPoint there(at.x() + dx, at.y() + dy);
            if (qGray(image.pixel(there)) + 30 < qGray(control.pixel(there)))
                return true;
        }
    return false;
}

QImage withoutTheIndicator(QCustomPlot& plot, const QCPHandle<QCPItemZoomIndicator>& indicator)
{
    indicator->setVisible(false);
    plot.replot();
    const QImage image = rendered(plot);
    indicator->setVisible(true);
    plot.replot();
    return image;
}

//! A probe \a distance pixels along the segment that joins \a from to \a to.
QPoint onSegment(const QPointF& from, const QPointF& to, double distance)
{
    const QPointF direction = to - from;
    const double length = std::hypot(direction.x(), direction.y());
    return (from + direction / length * distance).toPoint();
}

//! The box of the test scene's inset ranges in the parent's pixels, whichever way its axes run.
QRectF targetBoxPixels(const QCPAxis& keyAxis, const QCPAxis& valueAxis)
{
    return QRectF(QPointF(keyAxis.coordToPixel(-1.5), valueAxis.coordToPixel(-1.9)),
                  QPointF(keyAxis.coordToPixel(-0.9), valueAxis.coordToPixel(-2.5)))
        .normalized();
}

//! \a box's four corners in pixels, in the visibility rule's order: bottom-left, top-left,
//! bottom-right, top-right.
std::array<QPointF, 4> cornersInPixels(const QRectF& box)
{
    return { box.bottomLeft(), box.topLeft(), box.bottomRight(), box.topRight() };
}

//! Where the test scene's four data corners of the box land in pixels, in the same order.
std::array<QPointF, 4> dataCornersInPixels(const QCPAxis& keyAxis, const QCPAxis& valueAxis)
{
    const auto at = [&](double key, double value)
    { return QPointF(keyAxis.coordToPixel(key), valueAxis.coordToPixel(value)); };
    return { at(-1.5, -2.5), at(-1.5, -1.9), at(-0.9, -2.5), at(-0.9, -1.9) };
}

//! The index of \a inset in \a layout, or -1: the legend may be element 0.
int indexIn(const QCPHandle<QCPLayoutInset>& layout, const QCPAxisRect* inset)
{
    const QList<QCPHandle<QCPLayoutElement>> elements = layout->elements(false);
    for (int i = 0; i < elements.size(); ++i)
        if (elements.at(i).get() == inset)
            return i;
    return -1;
}
} // namespace

void TestZoomIndicator::init()
{
    mPlot = new QCustomPlot;
    mPlot->resize(560, 540);
    std::vector<double> x, y, z;
    for (int i = 0; i < 150; ++i)
    {
        x.push_back(-3 + (i + 0.5) * 7.0 / 150);
        y.push_back(-4 + (i + 0.5) * 7.0 / 150);
    }
    for (int column = 0; column < 150; ++column)
        for (int row = 0; row < 150; ++row)
            z.push_back(image(column, row));
    const QCPColorMapOptions look { .gradient = QCPColorGradient::gpViridis, .colorBar = false };
    mAx = mPlot->axisRect().get();
    mAx->colorMap(x, y, z, look);
    mAx->xAxis()->setRange(-3, 4);
    mAx->yAxis()->setRange(-4, 3);
    mInset = mAx->addInset(QRectF(0.5, 0.03, 0.47, 0.47)).get();
    mInsetMap = mInset->colorMap(x, y, z, look);
    mInset->xAxis()->setRange(-1.5, -0.9);
    mInset->yAxis()->setRange(-2.5, -1.9);
    mInset->xAxis()->setTickLabels(false);
    mInset->yAxis()->setTickLabels(false);
    mIndicator = mAx->zoomIndicator(QCPHandle<QCPAxisRect>(mInset));
}

void TestZoomIndicator::cleanup()
{
    mInsetMap = QCPHandle<QCPColorMap2>();
    mIndicator = QCPHandle<QCPItemZoomIndicator>();
    delete mPlot;
    mPlot = nullptr;
    mAx = nullptr;
    mInset = nullptr;
}

void TestZoomIndicator::connectorsForAnInsetUpRight()
{
    // The target's geometry: the box sits left of and below the inset, so the two connectors that
    // reach it are the ones leaving the box's top-left and bottom-right corners.
    const QRectF box(10, 80, 10, 10), inset(50, 10, 40, 40);
    const std::array<bool, 4> shown = qcp::zoomConnectorVisibility(box, inset);
    const std::array<bool, 4> expected { false, true, true, false };
    QVERIFY2(shown == expected, "an inset up and to the right of the box took the wrong connectors");
}

void TestZoomIndicator::connectorsForAnInsetDownRight()
{
    const QRectF box(10, 10, 10, 10), inset(50, 50, 40, 40);
    const std::array<bool, 4> shown = qcp::zoomConnectorVisibility(box, inset);
    const std::array<bool, 4> expected { true, false, false, true };
    QVERIFY2(shown == expected,
             "an inset down and to the right of the box took the wrong connectors");
}

void TestZoomIndicator::connectorsKeepTheirPixelCornersOnAReversedKeyAxis()
{
    mAx->xAxis()->setRangeReversed(true);
    mPlot->replot();
    const QImage image = rendered(*mPlot);
    const QImage control = withoutTheIndicator(*mPlot, mIndicator);
    const QRectF boxPixels = targetBoxPixels(*mAx->xAxis(), *mAx->yAxis());
    const QRectF insetPixels(mInset->rect());
    const std::array<QPointF, 4> box = cornersInPixels(boxPixels);
    const std::array<QPointF, 4> inset = cornersInPixels(insetPixels);
    const std::array<QPointF, 4> data = dataCornersInPixels(*mAx->xAxis(), *mAx->yAxis());
    const std::array<bool, 4> shown = qcp::zoomConnectorVisibility(boxPixels, insetPixels);

    // The rule picks corners of the box as it is drawn, whatever the key axis says, and the
    // connectors leave them. A connector taken from a data corner instead leaves another one.
    bool aDataCornerMoved = false;
    for (std::size_t i = 0; i < box.size(); ++i)
    {
        if (!shown[i])
            continue;
        QVERIFY2(differsNear(image, control, onSegment(box[i], inset[i], 12), 2),
                 "a connector does not leave the box corner the rule picked");
        if (data[i] != box[i])
        {
            aDataCornerMoved = true;
            QVERIFY2(!differsNear(image, control, onSegment(data[i], inset[i], 12), 1),
                     "a connector leaves a data corner of the box, not its pixel corner");
        }
    }
    QVERIFY2(aDataCornerMoved, "a reversed key axis did not turn the data corners round");
}

void TestZoomIndicator::connectorsKeepTheirPixelCornersOnAReversedValueAxis()
{
    mAx->yAxis()->setRangeReversed(true);
    mPlot->replot();
    const QImage image = rendered(*mPlot);
    const QImage control = withoutTheIndicator(*mPlot, mIndicator);
    const QRectF boxPixels = targetBoxPixels(*mAx->xAxis(), *mAx->yAxis());
    const QRectF insetPixels(mInset->rect());
    const std::array<QPointF, 4> box = cornersInPixels(boxPixels);
    const std::array<QPointF, 4> inset = cornersInPixels(insetPixels);
    const std::array<QPointF, 4> data = dataCornersInPixels(*mAx->xAxis(), *mAx->yAxis());
    const std::array<bool, 4> shown = qcp::zoomConnectorVisibility(boxPixels, insetPixels);

    bool aDataCornerMoved = false;
    for (std::size_t i = 0; i < box.size(); ++i)
    {
        if (!shown[i])
            continue;
        QVERIFY2(differsNear(image, control, onSegment(box[i], inset[i], 12), 2),
                 "a connector does not leave the box corner the rule picked");
        if (data[i] != box[i])
        {
            aDataCornerMoved = true;
            QVERIFY2(!differsNear(image, control, onSegment(data[i], inset[i], 12), 1),
                     "a connector leaves a data corner of the box, not its pixel corner");
        }
    }
    QVERIFY2(aDataCornerMoved, "a reversed value axis did not turn the data corners round");
}

void TestZoomIndicator::theBoxCoversTheInsetRanges()
{
    mPlot->replot();
    const QImage image = rendered(*mPlot);
    QVERIFY2(hasInkNear(image, pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2)),
             "the box has no left edge at the inset's key range");
    QVERIFY2(hasInkNear(image, pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.2, -1.9)),
             "the box has no top edge at the inset's value range");
    QVERIFY2(!hasInkNear(image, pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.2, -2.2)),
             "the box is filled: its inside is inked");
}

void TestZoomIndicator::theBoxFollowsThePannedInset()
{
    mPlot->replot();
    const QPoint oldEdge = pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2);
    QVERIFY2(hasInkNear(rendered(*mPlot), oldEdge), "the box is not there before the pan");

    const int recorded = mIndicator->mCore.recordingCount();
    mInset->xAxis()->setRange(0, 0.6);
    mPlot->replot();
    QVERIFY2(mIndicator->mCore.recordingCount() > recorded,
             "a panned inset did not remake the box");

    const QImage image = rendered(*mPlot);
    QVERIFY2(!hasInkNear(image, oldEdge), "the box stayed over the inset's old key range");
    QVERIFY2(hasInkNear(image, pointIn(*mAx->xAxis(), *mAx->yAxis(), 0.0, -2.2)),
             "the box did not move to the inset's new key range");
}

void TestZoomIndicator::aPlainReplotKeepsTheRecording()
{
    // The first replot settles the layout: the inset's margins are computed there and its rect moves
    // once, which is a reason to remake the recording.
    mPlot->replot();
    mPlot->replot();
    const int recorded = mIndicator->mCore.recordingCount();
    mPlot->replot();
    QCOMPARE(mIndicator->mCore.recordingCount(), recorded);
    QVERIFY2(hasInkNear(rendered(*mPlot), pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2)),
             "the box is gone after a second replot");
}

void TestZoomIndicator::connectorsEndAtTheInsetCorners()
{
    mPlot->replot();
    const QRectF insetPixels(mInset->rect());
    const QPointF boxTopLeft(pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -1.9));
    const QPointF boxBottomRight(pointIn(*mAx->xAxis(), *mAx->yAxis(), -0.9, -2.5));
    const QImage image = rendered(*mPlot);
    QVERIFY2(hasInkNear(image, onConnectorNear(insetPixels.topLeft(), boxTopLeft), 1),
             "no connector joins the box's top-left corner to the inset's top-left corner");
    QVERIFY2(hasInkNear(image, onConnectorNear(insetPixels.bottomRight(), boxBottomRight), 1),
             "no connector joins the box's bottom-right corner to the inset's bottom-right corner");

    const int recorded = mIndicator->mCore.recordingCount();
    mPlot->resize(700, 600);
    mPlot->replot();
    QVERIFY2(mIndicator->mCore.recordingCount() > recorded, "a resize did not remake the connectors");

    const QRectF moved(mInset->rect());
    QVERIFY2(moved != insetPixels, "the inset's pixel rect did not follow the resize");
    const QImage resized = rendered(*mPlot);
    const QPointF movedBoxTopLeft(pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -1.9));
    const QPointF movedBoxBottomRight(pointIn(*mAx->xAxis(), *mAx->yAxis(), -0.9, -2.5));
    QVERIFY2(hasInkNear(resized, onConnectorNear(moved.topLeft(), movedBoxTopLeft), 1),
             "the top-left connector did not follow the resize");
    QVERIFY2(hasInkNear(resized, onConnectorNear(moved.bottomRight(), movedBoxBottomRight), 1),
             "the bottom-right connector did not follow the resize");
}

void TestZoomIndicator::theBoxFollowsAnInsetThatMovesInItsLayout()
{
    // The settle first, as aPlainReplotKeepsTheRecording does: the inset's auto margins and outer rect
    // are computed in the first replots, and that is a reason of its own to remake the recording.
    mPlot->replot();
    mPlot->replot();
    const QRectF before(mInset->rect());
    const int index = indexIn(mAx->insetLayout(), mInset);
    QVERIFY2(index >= 0, "the inset is not in its rect's inset layout");
    const int recorded = mIndicator->mCore.recordingCount();

    mAx->insetLayout()->setInsetRect(index, QRectF(0.6, 0.03, 0.35, 0.35));
    // Two replots: the layout places the inset during the first, and the geometry signal a rect emits
    // is deferred to the end of that replot, so the repaint it asks for is the second one. This is
    // the one queued replot a move costs on screen; an export reads the new rect at once.
    mPlot->replot();
    mPlot->replot();

    const QRectF after(mInset->rect());
    QVERIFY2(after != before, "the inset did not move in its layout");
    QVERIFY2(mIndicator->mCore.recordingCount() > recorded,
             "a moved inset did not remake the connectors");

    const QImage image = rendered(*mPlot);
    const QRectF box = targetBoxPixels(*mAx->xAxis(), *mAx->yAxis());
    QVERIFY2(hasInkNear(image, onSegment(box.topLeft(), QRectF(after).topLeft(), 4), 1),
             "the top-left connector did not follow the inset's new corner");
    QVERIFY2(hasInkNear(image, onSegment(box.bottomRight(), QRectF(after).bottomRight(), 4), 1),
             "the bottom-right connector did not follow the inset's new corner");
}

void TestZoomIndicator::aHiddenInsetHidesTheIndicator()
{
    mPlot->replot();
    const QPoint edge = pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2);
    QVERIFY2(hasInkNear(rendered(*mPlot), edge), "the box is not there to begin with");

    mInset->setVisible(false);
    mPlot->replot();
    QVERIFY2(!hasInkNear(rendered(*mPlot), edge), "a hidden inset still shows its indicator");

    mInset->setVisible(true);
    mPlot->replot();
    QVERIFY2(hasInkNear(rendered(*mPlot), edge), "the indicator did not come back with its inset");
}

void TestZoomIndicator::anIndicatorOutlivesItsInset()
{
    mPlot->replot();
    const QPoint edge = pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2);
    QVERIFY2(hasInkNear(rendered(*mPlot), edge), "the box is not there to begin with");

    // The indicator goes first: the box and its connectors leave with it.
    mIndicator.remove();
    QVERIFY2(!mIndicator, "a removed indicator kept its handle");
    mPlot->replot();
    QVERIFY2(!hasInkNear(rendered(*mPlot), edge), "a removed indicator still draws");

    // And the inset goes first: the indicator stays, draws nothing, and survives a replot.
    mIndicator = mAx->zoomIndicator(QCPHandle<QCPAxisRect>(mInset));
    mPlot->replot();
    QVERIFY2(hasInkNear(rendered(*mPlot), edge), "the remade indicator does not draw");
    QVERIFY2(mAx->insetLayout()->remove(mInset), "the inset is not in its rect's inset layout");
    QVERIFY2(!mInsetMap, "the inset's colour map outlived the inset");
    mInset = nullptr;
    mPlot->replot();
    QVERIFY2(!hasInkNear(rendered(*mPlot), edge), "the indicator outlived its inset's ink");
    QVERIFY2(!mIndicator->inset(), "the indicator still points at the deleted inset");
    mPlot->replot();
}

void TestZoomIndicator::aRectThatIsNotAnInsetIsRefused()
{
    QCustomPlot other;
    other.resize(400, 300);
    other.setAxisRectGrid(1, 2);
    const QCPHandle<QCPAxisRect> parent = other.axisRect(0, 0);
    const QCPHandle<QCPAxisRect> cell = other.axisRect(0, 1);
    QVERIFY(parent && cell);

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPItemZoomIndicator> onACell = parent->zoomIndicator(cell);
    qcp::setMisuseHandler(previous);
    QVERIFY2(!onACell, "a grid cell was taken for an inset of another rect");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(test_misuse::calls.front().what.find("inset") != std::string::npos,
             test_misuse::calls.front().what.c_str());

    const QCPHandle<QCPAxisRect> elsewhere = cell->addInset(QRectF(0.1, 0.1, 0.3, 0.3));
    QVERIFY(elsewhere);
    const auto again = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPItemZoomIndicator> onAnotherRectsInset = parent->zoomIndicator(elsewhere);
    qcp::setMisuseHandler(again);
    QVERIFY2(!onAnotherRectsInset, "an inset of another rect was accepted");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));

    // A rect of another plot: its pixels are another widget's.
    QCustomPlot foreignPlot;
    foreignPlot.resize(300, 200);
    const QCPHandle<QCPAxisRect> foreignParent = foreignPlot.axisRect();
    const QCPHandle<QCPAxisRect> foreignInset = foreignParent->addInset(QRectF(0.1, 0.1, 0.2, 0.2));
    QVERIFY(foreignInset);
    const auto third = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPItemZoomIndicator> onAnotherPlot = parent->zoomIndicator(foreignInset);
    qcp::setMisuseHandler(third);
    QVERIFY2(!onAnotherPlot, "an inset of another plot was accepted");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));

    // setInset() is the check, so a caller that goes round the one-call helper is refused as well,
    // and the inset the item had stays.
    const QCPHandle<QCPAxisRect> ownInset = parent->addInset(QRectF(0.5, 0.05, 0.4, 0.4));
    const QCPHandle<QCPItemZoomIndicator> indicator = parent->zoomIndicator(ownInset);
    QVERIFY(indicator);
    const auto fourth = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    indicator->setInset(QCPHandle<QCPAxisRect>(cell.get()));
    qcp::setMisuseHandler(fourth);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(indicator->inset().get(), ownInset.get());
}

void TestZoomIndicator::aNullInsetIsNoInset()
{
    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    mIndicator->setInset(nullptr);
    qcp::setMisuseHandler(previous);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QVERIFY(!mIndicator->inset());
    mPlot->replot();
    QVERIFY2(!hasInkNear(rendered(*mPlot), pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2)),
             "an indicator with no inset drew a box");

    // The one-call helper insists on one: an empty handle is misuse, one report, no item.
    const auto again = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPItemZoomIndicator> empty = mAx->zoomIndicator(QCPHandle<QCPAxisRect>());
    qcp::setMisuseHandler(again);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(!empty, "the helper made an indicator with no inset");
}

void TestZoomIndicator::aRectGoesWithWhatIsDrawnInIt()
{
    // The scene's inset holds a colour map, as the target program's does: the inset takes it.
    QVERIFY(mInsetMap);
    QVERIFY2(mAx->insetLayout()->remove(mInset), "the inset is not in its rect's inset layout");
    mInset = nullptr;
    QVERIFY2(!mInsetMap, "the inset's colour map outlived the inset");

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    mPlot->replot();
    qcp::setMisuseHandler(previous);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QVERIFY(!hasInkNear(rendered(*mPlot), pointIn(*mAx->xAxis(), *mAx->yAxis(), -1.5, -2.2)));

    // And a grid cell goes with its line.
    QCustomPlot grid;
    grid.resize(400, 300);
    grid.setAxisRectGrid(1, 2);
    const QCPHandle<QCPAxisRect> cell = grid.axisRect(0, 1);
    const QCPHandle<QCPGraph> line = cell->line(std::vector<double> { 0, 1, 2 },
                                                std::vector<double> { 0, 1, 0 });
    QVERIFY(cell && line);
    const auto again = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    QVERIFY(cell->layout()->remove(cell.get()));
    QVERIFY2(!line, "the cell's line outlived its rect");
    grid.replot();
    qcp::setMisuseHandler(again);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestZoomIndicator::theIndicatorGoesWithItsRect()
{
    // The item lives in its rect's coordinate system, so the rect takes it, both ways round.
    QCustomPlot grid;
    grid.resize(400, 300);
    grid.setAxisRectGrid(1, 2);
    const QCPHandle<QCPAxisRect> cell = grid.axisRect(0, 0);
    const QCPHandle<QCPAxisRect> inset = cell->addInset(QRectF(0.2, 0.2, 0.4, 0.4));
    const QCPHandle<QCPItemZoomIndicator> indicator = cell->zoomIndicator(inset);
    QVERIFY(cell && inset && indicator);

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    QVERIFY(cell->layout()->remove(cell.get()));
    QVERIFY2(!indicator, "the indicator outlived the rect it is drawn in");
    grid.replot();
    qcp::setMisuseHandler(previous);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}
