#include "test-span-rhi.h"
#include <QtWidgets/qtestsupport_widgets.h>
#include "../../../src/qcp.h"
#include "../../../src/painting/span-rhi-layer.h"
#include "../../../src/items/item-vspan.h"

void TestSpanRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mSpan = new QCPItemVSpan(mPlot);
    mSpan->lowerEdge->setCoords(2, 0);
    mSpan->upperEdge->setCoords(4, 0);
    // Shown so that resize() delivers a resizeEvent (hidden widgets defer it),
    // which is what propagates the new viewport into the axis rect layout.
    mPlot->show();
    mPlot->replot();
}

void TestSpanRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    mSpan = nullptr;
}

void TestSpanRhi::firstDetectionReportsChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::unchangedFrameReportsNoChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    QVERIFY(!layer.detectGeometryChanges());
}

void TestSpanRhi::detectsEdgeMove()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mSpan->upperEdge->setCoords(6, 0);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsBrushChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mSpan->setBrush(QBrush(Qt::red));
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsAxisRangeChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mPlot->xAxis->setRange(5, 15);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsAxisRectResize()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mPlot->resize(500, 400);
    mPlot->replot();
    QVERIFY(layer.detectGeometryChanges());
}

namespace {
bool waitForRhi(QCustomPlot* plot)
{
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}

// Fraction of rows, down the vertical line at key inside the axis rect, where the line is drawn.
// A line on a half pixel is antialiased over two grey columns: look at the darkest of three.
double darkFractionDown(const QImage& image, QCustomPlot* plot, double key)
{
    const double dpr = image.devicePixelRatio();
    const int x = qRound(plot->xAxis->coordToPixel(key) * dpr);
    const QRect r = plot->axisRect()->rect();
    int dark = 0, all = 0;
    for (int y = qRound((r.top() + 5) * dpr); y < qRound((r.bottom() - 5) * dpr); ++y, ++all)
    {
        int darkest = 255;
        for (int dx = -1; dx <= 1; ++dx)
            darkest = std::min(darkest, image.pixelColor(x + dx, y).lightness());
        dark += darkest < 160;
    }
    return double(dark) / all;
}
} // namespace

// Spans draw a band and two border lines. Exports also stroked the band's rectangle with the
// item pen (black by default): every exported span got a frame the screen never showed.
void TestSpanRhi::exportedSpanHasNoOutlineTheScreenLacks()
{
    if (!waitForRhi(mPlot))
        QSKIP("No QRhi available — spans are drawn on the GPU only with a real backend");
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(2, 8));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QImage exported = mPlot->toPixmap(mPlot->width(), mPlot->height()).toImage();
    const QRect r = mPlot->axisRect()->rect();
    const int x = qRound(mPlot->xAxis->coordToPixel(5));
    for (int y = r.top() - 1; y <= r.top() + 1; ++y)
    {
        const QColor c = exported.pixelColor(x, y);
        QVERIFY2(c.lightness() > 100, qPrintable(QString("exported span outline at row %1: %2")
                                                      .arg(y - r.top()).arg(c.name())));
    }
}

// The GPU drew span borders solid whatever the pen style; exports dashed them.
void TestSpanRhi::dashedSpanBorderIsDashedOnScreen()
{
    if (!waitForRhi(mPlot))
        QSKIP("No QRhi available — spans are drawn on the GPU only with a real backend");
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->xAxis->grid()->setVisible(false);
    auto* span = new QCPItemVSpan(mPlot); // clear of the fixture's span (2..4)
    span->setRange(QCPRange(5, 8));
    span->setBrush(Qt::NoBrush);
    span->setBorderPen(QPen(Qt::black, 2, Qt::DashLine));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QImage screen = mPlot->grabFramebuffer();
    const QImage exported = mPlot->toPixmap(mPlot->width(), mPlot->height(),
                                            screen.devicePixelRatio()).toImage();
    const double onScreen = darkFractionDown(screen, mPlot, 5);
    const double inExport = darkFractionDown(exported, mPlot, 5);
    QVERIFY2(onScreen < 0.95, qPrintable(QString("solid on screen: %1").arg(onScreen)));
    QVERIFY2(qAbs(onScreen - inExport) < 0.15,
             qPrintable(QString("screen %1, export %2").arg(onScreen).arg(inExport)));
}

// The GPU span fill is one flat colour (brush().color(): black for a gradient).
void TestSpanRhi::gradientSpanBrushLooksTheSameOnScreen()
{
    if (!waitForRhi(mPlot))
        QSKIP("No QRhi available — spans are drawn on the GPU only with a real backend");
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(5, 9));
    QLinearGradient gradient(0, 0, 0, 300);
    gradient.setColorAt(0, Qt::red);
    gradient.setColorAt(1, Qt::blue);
    span->setBrush(gradient);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QImage screen = mPlot->grabFramebuffer();
    const double dpr = screen.devicePixelRatio();
    const QImage exported = mPlot->toPixmap(mPlot->width(), mPlot->height(), dpr).toImage();
    const int x = qRound(mPlot->xAxis->coordToPixel(7) * dpr);
    const int y = qRound(mPlot->yAxis->coordToPixel(5) * dpr);
    const QColor onScreen = screen.pixelColor(x, y), inExport = exported.pixelColor(x, y);
    QVERIFY2(qAbs(onScreen.red() - inExport.red()) < 40 && qAbs(onScreen.blue() - inExport.blue()) < 40,
             qPrintable(QString("screen %1, export %2").arg(onScreen.name(), inExport.name())));
}
