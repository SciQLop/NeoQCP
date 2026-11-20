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
    // A rect leaves its plottables behind, so the inset's own colour map goes first.
    mInsetMap.remove();
    QVERIFY2(mAx->insetLayout()->remove(mInset), "the inset is not in its rect's inset layout");
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
}
