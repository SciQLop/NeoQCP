#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <coords.h>
#include <datasource/resample.h>
#include <datasource/soa-datasource-2d.h>
#include <drawable.h>
#include <functional>
#include <memory>
#include <painting/draw-commands.h>
#include <painting/draw-context.h>
#include <qcustomplot.h>
#include <random>
#include <span>
#include <spatial-index.h>
#include <vector>

class Benchmark : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();


    void QCPColorMap_Standard();
    void QCPColorMap_ColorizeMap();

    void QCPColorMap2_ResampleDouble();
    void QCPColorMap2_ResampleFloat();
    void QCPColorMapData_ConstructLarge();
    void QCPColorMap_SetCell1M();
    void QCPColorMap2_ResamplePanSequence();
    void QCPColorMap2_ResamplePanSequenceSmall();
    void QCPColorMap2_ResampleWideSerial();
    void QCPColorMap2_ResampleWideParallel();
    void QCPColorScale_Rescale8();


    void QCPAxis_TickLabels();
    void QCPAxis_TickLabelsCached();
    void QCPAxis_SetRange10k();
    void QCPAxis_SetTickLength10kItems();

    void QCPItems_AddRemove10k();
    void QCPItems_AddRemove20k();
    void QCPItems_Replot10k();
    void QCPItems_Add10k();
    void QCPItems_Clear10k();
    void QCPItems_DeleteEach10k();
    void QCPItems_RemoveEach10k();
    void QObject_DeleteEachChild10k();
    void QCPItems_DeletePlot10k();
    void QCPItems_Zoom10k();
    void QCPItems_Pan10k();
    void QCPItems_Zoom100();
    void QCPItems_ZoomEmpty();
    void QCPItems_Click10k();
    void QCPItems_ClickEmpty10k();
    void QCPItems_Click100();
    void QCPItems_FirstFrame10k();
    void QCPItems_DragOne10k();
    void QCPItems_RestyleAll10k();
    void QCPAxisRects_Stack8_Replot();
    void QCPPlots_Stack8_Replot();
    void QCPPlots_Stack8_Aligned_Replot();
    void QCPInput_MouseMoveStorm();

    void QCPSpatialIndex_Build10k();
    void QCPSpatialIndex_Query10k();

    void QCPGpu_LineSolid100k();
    void QCPGpu_LineDashed100k();
    void QCPGpu_Pan100k();
    void QCPMultiGraph_SetData1M();
    void QCPMultiGraph_Standard();
    void QCPMultiGraph_ManyPoints();
    void QCPMultiGraph_ManyLines();
    void QCPMultiGraph_ManyOffScreenLines();
    void QCPMultiGraph_SetDataDouble();
    void QCPMultiGraph_SetDataFloat();
    void QCPMultiGraph_RenderDouble();
    void QCPMultiGraph_RenderFloat();
    void QCPMultiGraph_RenderManyLines();
    void QCPMultiGraph_ScatterDisc1M();
    void QCPMultiGraph_ScatterSquare1M();
    void QCPMultiGraph_ScatterWithLine1M();
    void QCPInput_WheelOverMultiGraph_1M();
    void QCPMultiGraph_UserSystem100k();
    void QCPMultiGraph_AddDataAtEndSorted();
    void QCPMultiGraph_AddDataAtBeginSorted();
    void QCPMultiGraph_AddDataMixedSorted();
    void QCPMultiGraph_AddDataAtEndUnsorted();
    void QCPMultiGraph_AddDataAtBeginUnsorted();
    void QCPMultiGraph_AddDataMixedUnsorted();
    void QCPMultiGraph_AddDataSingleAtEnd();
    void QCPMultiGraph_AddDataSingleAtBegin();
    void QCPMultiGraph_AddDataSingleRandom();
    void QCPMultiGraph_RemoveDataBetween();
    void QCPMultiGraph_RemoveDataAfter();
    void QCPMultiGraph_RemoveDataBefore();
    void QCPMultiGraph_FillToGraphPan10k();
    void QCPGpu_MultiGraphPan1M();
    void QCPGpu_MultiGraphRender1M();
    void QCPGpu_MultiGraphZoom1M();
    void QCPGpu_MultiGraphColored100kPan();
    void QCPGpu_CurvePan100k();
    void QCPGpu_Bars10k();
    void QCPGpu_BarsPan10k();
    void QCPGpu_Text1k();
    void QCPGpu_MixedLayer();
    void QCPGpu_ColorMapPan();
    void QCPGpu_ColorMap2Pan();
    void QCPGpu_ColorMap2Zoom();
    void QCPGpu_Histogram2DPan();
    void QCPGpu_HistogramPan();

    void QCPCoords_CoordToPixel1M();
    void QCPTransform_Map1M();
    void QCPDrawContext_ReplayData1M();
    void QCPDrawContext_ReplayMarkers100k();

    void QCPBars_Zoom10k();
    void QCPBars_Pan10k();
    void QCPItemText_Zoom1k();

    void QCPDrawn_Zoom10k();
    void QCPDrawn_Pan10k();
    void QCPDrawn_Pan10kOneRecording();

    // M4e: the classes the next ports take over.
    void QCPErrorBars_Pan10k();
    void QCPFinancial_Pan10k();
    void QCPStatisticalBox_Pan1k();
    void QCPIntervals_Pan10k();
    void QCPIntervals_Pan10kLabelled();
    void QCPItems_MixedKinds_Pan1k();
    void QCPItems_Tracers_Pan1k();
    void QCPSpans_Pan100();

    // M4f: polar pictures and the plottable-list rows.
    void QCPPolar_Draw10k();
    void QCPPolar_Rotate10k();
    void QCPPolar_RadialZoom10k();
    void QCPPolar_Export10k();
    void QCPPlottables_Pan200();
    void QCPPlottables_Zoom200();

private:
    QCustomPlot* mPlot;
    QWidget mContainerWidget;
};

QTEST_MAIN(Benchmark)
#include "benchmark.moc"

////////////////////////////////////////////////////////////////////
/////// Benchmark Implementation
////////////////////////////////////////////////////////////////////

void Benchmark::init()
{
    if (!mContainerWidget.isVisible())
    {
        mContainerWidget.setGeometry(0, 0, 640, 360);
        mContainerWidget.show();
    }
    mPlot = new QCustomPlot(&mContainerWidget);
    mPlot->setGeometry(0, 0, 640, 360);
    mPlot->show();
    qApp->processEvents();
}

void Benchmark::cleanup()
{
    delete mPlot;
}

void Benchmark::QCPColorMap_Standard()
{
    QCPColorMap* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    const int n = 200;
    map->data()->setSize(n, n);
    map->data()->setRange(QCPRange(0, 5), QCPRange(0, 5));
    std::srand(0);
    for (int x = 0; x < map->data()->keySize(); ++x)
    {
        for (int y = 0; y < map->data()->valueSize(); ++y)
        {
            map->data()->setCell(x, y, std::rand() / double(RAND_MAX));
        }
    }

    const int nanCount = 50;
    for (int i = 0; i < nanCount * nanCount; ++i)
        map->data()->setCell(i % nanCount + n / 2 - nanCount / 2,
                             i / nanCount + n / 2 - nanCount / 2,
                             qQNaN()); // insert NaNs in reproducible pseudorandom locations

    QCPColorGradient gradient(QCPColorGradient::gpHot);
    map->setGradient(gradient);
    map->setInterpolate(false);
    map->rescaleDataRange();
    mPlot->rescaleAxes();

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPColorMap_ColorizeMap()
{
    QCPColorMap* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    const int n = 200;
    map->data()->setSize(n, n);
    map->data()->setRange(QCPRange(0, 5), QCPRange(0, 5));
    std::srand(0);
    for (int x = 0; x < map->data()->keySize(); ++x)
    {
        for (int y = 0; y < map->data()->valueSize(); ++y)
        {
            map->data()->setCell(x, y, std::rand() / double(RAND_MAX));
        }
    }

    const int nanCount = 50;
    for (int i = 0; i < nanCount * nanCount; ++i)
        map->data()->setCell(i % nanCount + n / 2 - nanCount / 2,
                             i / nanCount + n / 2 - nanCount / 2,
                             qQNaN()); // insert NaNs in reproducible pseudorandom locations

    QCPColorGradient gradient(QCPColorGradient::gpHot);
    map->setGradient(gradient);
    map->setInterpolate(false);
    map->rescaleDataRange();
    mPlot->rescaleAxes();

    QBENCHMARK
    {
        map->data()->setCell(0, 0, 0); // to invalidate the currently cached map image
        mPlot->replot();
    }
}

void Benchmark::QCPColorScale_Rescale8()
{
    // Offscreen row: one colour scale over 8 classic maps of 500x500. The rescale reads each
    // map's cached bounds (no cell scan: dataBounds is kept by setCell) and sets the scale range.
    auto* scale = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, scale);
    const int n = 500;
    for (int m = 0; m < 8; ++m)
    {
        auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
        map->setColorScale(scale);
        map->data()->setSize(n, n);
        map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
        for (int x = 0; x < n; ++x)
            for (int y = 0; y < n; ++y)
                map->data()->setCell(x, y, double((x * 31 + y * 17 + m) % 1000));
    }
    QBENCHMARK
    {
        scale->rescaleDataRange(true);
    }
    // The row times the loop over the plot's plottables: if realVisibility ever hid the maps, the
    // loop would walk past all 8 and time nothing.
    QVERIFY2(scale->dataRange().upper > 900, "the rescale saw the maps");
}

namespace
{
// Mirrors a realistic streamed spectrogram: nx columns (time) x ys rows
// (frequency), resampled to a ~1080p viewport. Y is 1D (one frequency axis
// shared by all columns) so only X/Z dtype matters for the accessor choice.
template <typename ZT>
QCPColorMapData* benchmarkResample(int nx, int ys)
{
    std::vector<double> x(nx);
    for (int i = 0; i < nx; ++i)
        x[i] = static_cast<double>(i);

    std::vector<double> y(ys);
    for (int j = 0; j < ys; ++j)
        y[j] = static_cast<double>(j);

    std::vector<ZT> z(static_cast<std::size_t>(nx) * ys);
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = static_cast<ZT>(i % 997);

    QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const ZT>> src {
        std::span<const double>(x), std::span<const double>(y), std::span<const ZT>(z)
    };

    return qcp::algo2d::resample(src, 0, nx, QCPRange(0, nx - 1), QCPRange(0, ys - 1), 1500, 800,
                                 false, false, 0.0);
}
}

void Benchmark::QCPColorMap2_ResampleDouble()
{
    QBENCHMARK
    {
        delete benchmarkResample<double>(50000, 1024);
    }
}

void Benchmark::QCPColorMap2_ResampleFloat()
{
    QBENCHMARK
    {
        delete benchmarkResample<float>(50000, 1024);
    }
}

namespace
{
// LOFAR/e-Callisto-scale dynamic spectrum: a wide, dense pan window (100K
// visible time samples) x 3000 frequency channels -- the scale that measured
// ~1.1-1.5s single-threaded (see docs/backlog, colormap perf investigation).
//
// Data is built once by the caller and passed in by reference -- building the
// 300M-cell (2.4GB) dataset *inside* the QBENCHMARK block instead makes the
// allocation+fill dominate the reported time and masks the actual resample()
// parallel speedup (measured 1.3x instead of the real ~2.3-2.5x the first
// time this mistake was made here).
QCPColorMapData*
benchmarkResampleWide(const QCPSoADataSource2D<std::span<const double>, std::span<const double>,
                                               std::span<const double>>& src,
                      int nx, int ys, bool forceSerial)
{
    // Target grid matches the un-reduced-Y case (ys stays under the 4x
    // supersampling cap relative to a ~800px-tall viewport, so h == ys; width
    // clamps to 4x a ~1458px-wide viewport) -- the exact regime measured at
    // ~1.1-1.5s single-threaded during the original investigation.
    return qcp::algo2d::resample(src, 0, nx, QCPRange(0, nx - 1), QCPRange(0, ys - 1), 5832, ys,
                                 false, false, 0.0, nullptr, forceSerial);
}
}

void Benchmark::QCPColorMap2_ResampleWideSerial()
{
    const int nx = 100000, ys = 3000;
    std::vector<double> x(nx);
    for (int i = 0; i < nx; ++i)
        x[i] = static_cast<double>(i);
    std::vector<double> y(ys);
    for (int j = 0; j < ys; ++j)
        y[j] = static_cast<double>(j);
    std::vector<double> z(static_cast<std::size_t>(nx) * ys);
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = static_cast<double>(i % 997);
    QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
        src { std::span<const double>(x), std::span<const double>(y), std::span<const double>(z) };

    QBENCHMARK
    {
        delete benchmarkResampleWide(src, nx, ys, /*forceSerial=*/true);
    }
}

void Benchmark::QCPColorMap2_ResampleWideParallel()
{
    const int nx = 100000, ys = 3000;
    std::vector<double> x(nx);
    for (int i = 0; i < nx; ++i)
        x[i] = static_cast<double>(i);
    std::vector<double> y(ys);
    for (int j = 0; j < ys; ++j)
        y[j] = static_cast<double>(j);
    std::vector<double> z(static_cast<std::size_t>(nx) * ys);
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = static_cast<double>(i % 997);
    QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
        src { std::span<const double>(x), std::span<const double>(y), std::span<const double>(z) };

    QBENCHMARK
    {
        delete benchmarkResampleWide(src, nx, ys, /*forceSerial=*/false);
    }
}

void Benchmark::QCPColorMapData_ConstructLarge()
{
    // Isolates the ctor's own cost (setSize's fill(0) + the ctor's redundant
    // second fill(0)) from any resample work.
    QBENCHMARK
    {
        delete new QCPColorMapData(2000, 2000, QCPRange(0, 1), QCPRange(0, 1));
    }
}

// Filling a map cell by cell on a live plot: every setCell reaches the map's repaint mark.
void Benchmark::QCPColorMap_SetCell1M()
{
    QCPColorMap* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    const int n = 1000;
    map->data()->setSize(n, n);
    mPlot->replot();
    QBENCHMARK
    {
        for (int x = 0; x < n; ++x)
            for (int y = 0; y < n; ++y)
                map->data()->setCell(x, y, x + y);
    }
}

void Benchmark::QCPColorMap2_ResamplePanSequence()
{
    // Mirrors a pan: repeated resample() calls at the same target size, only
    // xBegin/xEnd (viewport) moves. Isolates the per-job scratch allocation
    // (accum/counts/gapBetween) cost from the one-off Y axis generation that
    // ResampleCache already caches.
    const int nx = 50000, ys = 256;
    std::vector<double> x(nx), y(ys);
    for (int i = 0; i < nx; ++i)
        x[i] = static_cast<double>(i);
    for (int j = 0; j < ys; ++j)
        y[j] = static_cast<double>(j);
    std::vector<double> z(static_cast<std::size_t>(nx) * ys);
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = static_cast<double>(i % 997);

    QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
        src { std::span<const double>(x), std::span<const double>(y), std::span<const double>(z) };

    QBENCHMARK
    {
        qcp::algo2d::ResampleCache cache;
        for (int step = 0; step < 20; ++step)
        {
            int xBegin = step * 100;
            int xEnd = nx - 20 * 100 + step * 100;
            delete qcp::algo2d::resample(src, xBegin, xEnd, QCPRange(xBegin, xEnd - 1),
                                         QCPRange(0, ys - 1), 1500, 800, false, false, 0.0, &cache);
        }
    }
}

void Benchmark::QCPColorMap2_ResamplePanSequenceSmall()
{
    // Same pattern as QCPColorMap2_ResamplePanSequence but with a small,
    // typical-widget-sized target and a modest windowed source column count —
    // the regime where per-job scratch-buffer allocation could plausibly be a
    // larger fraction of the total cost than in the multi-million-cell case.
    const int nx = 4000, ys = 64;
    std::vector<double> x(nx), y(ys);
    for (int i = 0; i < nx; ++i)
        x[i] = static_cast<double>(i);
    for (int j = 0; j < ys; ++j)
        y[j] = static_cast<double>(j);
    std::vector<double> z(static_cast<std::size_t>(nx) * ys);
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = static_cast<double>(i % 997);

    QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
        src { std::span<const double>(x), std::span<const double>(y), std::span<const double>(z) };

    QBENCHMARK
    {
        qcp::algo2d::ResampleCache cache;
        for (int step = 0; step < 100; ++step)
        {
            int xBegin = step;
            int xEnd = nx - 100 + step;
            delete qcp::algo2d::resample(src, xBegin, xEnd, QCPRange(xBegin, xEnd - 1),
                                         QCPRange(0, ys - 1), 800, 400, false, false, 0.0, &cache);
        }
    }
}

void Benchmark::QCPAxis_TickLabels()
{
    mPlot->setPlottingHint(QCP::phCacheLabels, false);
    mPlot->axisRect()->setupFullAxesBox();
    mPlot->xAxis2->setTickLabels(true);
    mPlot->yAxis2->setTickLabels(true);
    mPlot->xAxis->setRange(-10, 10);
    mPlot->yAxis->setRange(0.001, 0.002);
    mPlot->xAxis2->setRange(1e6, 1e8);
    mPlot->yAxis2->setRange(-1e100, 1e100);
    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPAxis_TickLabelsCached()
{
    mPlot->setPlottingHint(QCP::phCacheLabels, true);
    mPlot->axisRect()->setupFullAxesBox();
    mPlot->xAxis2->setTickLabels(true);
    mPlot->yAxis2->setTickLabels(true);
    mPlot->xAxis->setRange(-10, 10);
    mPlot->yAxis->setRange(0.001, 0.002);
    mPlot->xAxis2->setRange(1e6, 1e8);
    mPlot->yAxis2->setRange(-1e100, 1e100);
    QBENCHMARK
    {
        mPlot->replot();
    }
}

namespace
{

QVector<double> sineKeys(int n)
{
    QVector<double> x(n);
    for (int i = 0; i < n; ++i)
        x[i] = i / static_cast<double>(n);
    return x;
}

QVector<double> sineValues(const QVector<double>& x)
{
    QVector<double> y(x.size());
    for (int i = 0; i < x.size(); ++i)
        y[i] = qSin(x[i] * 10 * M_PI);
    return y;
}

void addLineItems(QCustomPlot* plot, int count, double xMax)
{
    for (int i = 0; i < count; ++i)
    {
        auto* item = new QCPItemLine(plot);
        const double x = xMax * i / count;
        item->start->setCoords(x, 0);
        item->end->setCoords(x + 0.01, 1);
    }
}

}

void Benchmark::QCPAxis_SetRange10k()
{
    const auto x = sineKeys(10000);
    mPlot->addGraph()->setData(x, sineValues(x));
    QBENCHMARK
    {
        for (int i = 0; i < 10000; ++i)
            mPlot->xAxis->setRange(i * 1e-5, 1 + i * 1e-5);
    }
}

void Benchmark::QCPAxis_SetTickLength10kItems()
{
    addLineItems(mPlot, 10000, 100);
    int step = 0;
    QBENCHMARK
    {
        for (int i = 0; i < 100; ++i)
            mPlot->xAxis->setTickLengthIn(step++ % 2 ? 8 : 10);
    }
}

void Benchmark::QCPItems_AddRemove10k()
{
    QBENCHMARK
    {
        addLineItems(mPlot, 10000, 100);
        mPlot->clearItems();
    }
}

void Benchmark::QCPItems_AddRemove20k()
{
    QBENCHMARK
    {
        addLineItems(mPlot, 20000, 100);
        mPlot->clearItems();
    }
}

void Benchmark::QCPItems_Replot10k()
{
    addLineItems(mPlot, 10000, 100);
    mPlot->xAxis->setRange(0, 1);
    mPlot->yAxis->setRange(0, 1);
    int step = 0;
    QBENCHMARK
    {
        mPlot->xAxis->setRange(0.001 * ++step, 1 + 0.001 * step);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

namespace
{
// A shuffled order, so neither "delete the newest first" nor "delete the oldest first" can hide in
// an index pattern. Seeded, so two runs measure the same sequence.
QVector<QCPAbstractItem*> itemsInOrder(QCustomPlot* plot)
{
    QVector<QCPAbstractItem*> items;
    items.reserve(plot->itemCount());
    for (int i = 0; i < plot->itemCount(); ++i)
        items.append(plot->item(i));
    return items;
}

QVector<int> shuffledIndices(int count)
{
    QVector<int> indices(count);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), std::mt19937 { 42 });
    return indices;
}
} // namespace

void Benchmark::QCPItems_Add10k()
{
    // The clear happens in cleanup() with the plot, outside the measurement.
    QBENCHMARK_ONCE
    {
        addLineItems(mPlot, 10000, 100);
    }
}

void Benchmark::QCPItems_Clear10k()
{
    addLineItems(mPlot, 10000, 100);
    QBENCHMARK_ONCE
    {
        mPlot->clearItems();
    }
}

void Benchmark::QCPItems_DeleteEach10k()
{
    // The user path: `delete item`, or a handle's remove(), in a shuffled order.
    addLineItems(mPlot, 10000, 100);
    QVector<QCPAbstractItem*> items = itemsInOrder(mPlot);
    const QVector<int> order = shuffledIndices(items.size());
    QBENCHMARK_ONCE
    {
        for (int index : order)
            delete items[index];
    }
}

void Benchmark::QCPItems_RemoveEach10k()
{
    // The same shuffled order through the plot API, which looks the item up before deleting it.
    addLineItems(mPlot, 10000, 100);
    QVector<QCPAbstractItem*> items = itemsInOrder(mPlot);
    const QVector<int> order = shuffledIndices(items.size());
    QBENCHMARK_ONCE
    {
        for (int index : order)
            mPlot->removeItem(items[index]);
    }
}

void Benchmark::QObject_DeleteEachChild10k()
{
    // The control row: Qt's own children bookkeeping, without any QCP list.
    auto* root = new QObject;
    QList<QObject*> children;
    for (int i = 0; i < 10000; ++i)
        children.append(new QObject(root));
    const QVector<int> order = shuffledIndices(children.size());
    QBENCHMARK_ONCE
    {
        for (int index : order)
            delete children[index];
    }
    delete root;
}

void Benchmark::QCPItems_DeletePlot10k()
{
    QBENCHMARK_ONCE
    {
        auto* plot = new QCustomPlot;
        addLineItems(plot, 10000, 100);
        delete plot;
    }
}

namespace
{
// A zoom around a fixed centre: the range width changes, so the layer cannot be translated and
// every visible item is drawn again on each replot.
void zoomInPlace(QCustomPlot* plot, double centre, double width, int& step)
{
    const double w = (step++ % 2) ? width : width * 0.98;
    plot->xAxis->setRange(centre - w / 2, centre + w / 2);
    plot->replot(QCustomPlot::rpImmediateRefresh);
}
} // namespace

void Benchmark::QCPItems_Zoom10k()
{
    // Items every 0.01 over 0..100; a width of 1 keeps 100 of them in view, as in Zoom100.
    addLineItems(mPlot, 10000, 100);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(mPlot, 50.5, 1.0, step);
    }
}

void Benchmark::QCPItems_Zoom100()
{
    addLineItems(mPlot, 100, 1);
    mPlot->xAxis->setRange(0, 1);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(mPlot, 0.5, 1.0, step);
    }
}

void Benchmark::QCPItems_ZoomEmpty()
{
    // The floor: the same ranges as Zoom100 with no item at all.
    mPlot->xAxis->setRange(0, 1);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(mPlot, 0.5, 1.0, step);
    }
}

namespace
{
// A click is the only pointer event that hit-tests the items: press selects, release resolves the
// click. Alternating between a point on an item and an empty point keeps both directions measured.
void clickStorm(QCustomPlot* plot, const QPointF& onItem, const QPointF& empty)
{
    plot->setInteractions(QCP::iSelectItems);
    int step = 0;
    QBENCHMARK
    {
        const QPointF pos = (step++ % 2) ? onItem : empty;
        const QPointF global = plot->mapToGlobal(pos);
        QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(plot, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QCoreApplication::sendEvent(plot, &release);
    }
}
} // namespace

void Benchmark::QCPItems_ClickEmpty10k()
{
    // Clicks that hit nothing and change no selection: the index answers for the items away from
    // the click, and the deselect pass visits only selected items.
    addLineItems(mPlot, 10000, 100);
    // The items span y 0..1; y = 1.5 is in view and hits none of them.
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 2);
    mPlot->replot();
    const QPointF empty(mPlot->xAxis->coordToPixel(50.5), mPlot->yAxis->coordToPixel(1.5));
    clickStorm(mPlot, empty, empty);
}

void Benchmark::QCPItems_Click10k()
{
    // The same layout as Zoom10k: 100 of the 10 000 items are in view.
    addLineItems(mPlot, 10000, 100);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    const QPointF onItem(mPlot->xAxis->coordToPixel(50.5), mPlot->yAxis->coordToPixel(0.5));
    const QPointF empty(mPlot->xAxis->coordToPixel(50.9), mPlot->yAxis->coordToPixel(0.5));
    clickStorm(mPlot, onItem, empty);
}

void Benchmark::QCPItems_Click100()
{
    addLineItems(mPlot, 100, 1);
    mPlot->xAxis->setRange(0, 1);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    const QPointF onItem(mPlot->xAxis->coordToPixel(0.5), mPlot->yAxis->coordToPixel(0.5));
    const QPointF empty(mPlot->xAxis->coordToPixel(0.9), mPlot->yAxis->coordToPixel(0.5));
    clickStorm(mPlot, onItem, empty);
}

void Benchmark::QCPAxisRects_Stack8_Replot()
{
    const auto x = sineKeys(100000);
    const auto y = sineValues(x);
    QList<QCPAxisRect*> rects;
    mPlot->plotLayout()->clear();
    for (int i = 0; i < 8; ++i)
    {
        auto* rect = new QCPAxisRect(mPlot);
        mPlot->plotLayout()->addElement(i, 0, rect);
        mPlot->addGraph(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft))->setData(x, y);
        rects.append(rect);
    }
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    int step = 0;
    QBENCHMARK
    {
        rects[3]->axis(QCPAxis::atBottom)->setRange(0.001 * ++step, 0.7 + 0.001 * step);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPPlots_Stack8_Replot()
{
    const auto x = sineKeys(100000);
    const auto y = sineValues(x);
    std::vector<std::unique_ptr<QCustomPlot>> plots;
    for (int i = 0; i < 8; ++i)
    {
        auto plot = std::make_unique<QCustomPlot>(&mContainerWidget);
        plot->setGeometry(0, 0, 640, 45);
        plot->addGraph()->setData(x, y);
        plot->rescaleAxes();
        plot->show();
        plots.push_back(std::move(plot));
    }
    qApp->processEvents();
    QCustomPlot* changing = plots[3].get();
    changing->replot(QCustomPlot::rpImmediateRefresh);
    int step = 0;
    QBENCHMARK
    {
        changing->xAxis->setRange(0.001 * ++step, 0.7 + 0.001 * step);
        changing->replot(QCustomPlot::rpImmediateRefresh);
    }
}

// The same stack as QCPPlots_Stack8_Replot, its eight rects in one alignment group: the gate
// covers the margin arithmetic two layout passes add, not eight separate plots.
void Benchmark::QCPPlots_Stack8_Aligned_Replot()
{
    const auto x = sineKeys(100000);
    const auto y = sineValues(x);
    std::vector<std::unique_ptr<QCustomPlot>> plots;
    QList<QCPLayoutElement*> members;
    for (int i = 0; i < 8; ++i)
    {
        auto plot = std::make_unique<QCustomPlot>(&mContainerWidget);
        plot->setGeometry(0, 45 * i, 640, 45);
        plot->addGraph()->setData(x, y);
        plot->rescaleAxes();
        plot->show();
        members.append(plot->axisRect());
        plots.push_back(std::move(plot));
    }
    QCPAlignmentGroup::create(members);
    qApp->processEvents();
    // Two passes per plot settle the shared edges before the timed replots.
    for (int round = 0; round < 2; ++round)
        for (const auto& plot : plots)
            plot->replot(QCustomPlot::rpImmediateRefresh);
    qApp->processEvents();
    QCustomPlot* changing = plots[3].get();
    changing->replot(QCustomPlot::rpImmediateRefresh);
    int step = 0;
    QBENCHMARK
    {
        changing->xAxis->setRange(0.001 * ++step, 0.7 + 0.001 * step);
        changing->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPInput_MouseMoveStorm()
{
    mPlot->addGraph()->setData(sineKeys(1000), sineValues(sineKeys(1000)));
    mPlot->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    int moves = 0;
    connect(mPlot, &QCustomPlot::mouseMove, mPlot, [&moves] { ++moves; });
    const int kMoves = 10000;
    QBENCHMARK
    {
        moves = 0;
        for (int i = 0; i < kMoves; ++i)
        {
            const QPointF pos(10 + i % 600, 10 + i % 300);
            QMouseEvent event(QEvent::MouseMove, pos, mPlot->mapToGlobal(pos), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(mPlot, &event);
        }
    }
    QCOMPARE(moves, kMoves);
}

// One wheel event over a 1M-point one-column graph with the cursor inside the axis rect but away from the
// curve: the hit test must not walk the data. Target: <= 0.1 ms.
namespace
{
std::vector<std::pair<QCPDataBox, int>> spatialIndexEntries10k()
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> position(0.0, 1000.0);
    std::vector<std::pair<QCPDataBox, int>> entries;
    entries.reserve(10000);
    for (int i = 0; i < 10000; ++i)
    {
        const double key = position(rng);
        const double value = position(rng);
        entries.push_back({ { key, key + 1.0, value, value + 1.0 }, i });
    }
    return entries;
}
} // namespace

void Benchmark::QCPSpatialIndex_Build10k()
{
    const std::vector<std::pair<QCPDataBox, int>> entries = spatialIndexEntries10k();
    QCPSpatialIndex index;
    QBENCHMARK
    {
        index.build(entries);
    }
    QCOMPARE(index.size(), 10000);
}

void Benchmark::QCPSpatialIndex_Query10k()
{
    QCPSpatialIndex index;
    index.build(spatialIndexEntries10k());
    // 10 000 items of one unit square over 1000x1000: a 100x100 window hits about a hundred.
    const QCPDataBox query { 450.0, 550.0, 450.0, 550.0 };
    int hits = 0;
    QBENCHMARK
    {
        hits = 0;
        index.query(query, [&hits](int) { ++hits; });
    }
    QVERIFY(hits > 50 && hits < 200);
}

void Benchmark::QCPItems_FirstFrame10k()
{
    QBENCHMARK
    {
        addLineItems(mPlot, 10000, 100);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
        mPlot->clearItems();
    }
}

void Benchmark::QCPItems_DragOne10k()
{
    addLineItems(mPlot, 10000, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    auto* dragged = qobject_cast<QCPItemLine*>(mPlot->item(5000));
    QVERIFY(dragged);
    double x = 50.0;
    QBENCHMARK
    {
        x += 0.01;
        dragged->start->setCoords(x, 0.2);
        dragged->end->setCoords(x + 0.01, 0.8);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
    mPlot->clearItems();
}

namespace
{
std::vector<QCPItemLine*> linesOf(const QCustomPlot& plot)
{
    std::vector<QCPItemLine*> lines;
    for (int i = 0; i < plot.itemCount(); ++i)
        if (auto* line = qobject_cast<QCPItemLine*>(plot.item(i)))
            lines.push_back(line);
    return lines;
}
} // namespace

void Benchmark::QCPItems_RestyleAll10k()
{
    addLineItems(mPlot, 10000, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const std::vector<QCPItemLine*> lines = linesOf(*mPlot);
    QCOMPARE(int(lines.size()), 10000);
    int round = 0;
    QBENCHMARK_ONCE
    {
        const QPen pen(QColor(30 * (round++ % 8), 120, 200), 1 + round % 4);
        for (QCPItemLine* line : lines)
            line->setPen(pen);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
    mPlot->clearItems();
}

void Benchmark::QCPItems_Pan10k()
{
    // Same layout as Zoom10k: 10 000 items every 0.01 over 0..100, 100 of them in view.
    addLineItems(mPlot, 10000, 100);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // a pan step: the width never changes, only the window slides
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

namespace
{
// A GPU row builds its own shown window: mPlot lives in a container an earlier row may have kept
// unexposed, and only a shown widget creates a QRhi. Returns false when the platform has none
// (qoffscreen), which the row turns into QSKIP.
bool makeGpuPlot(QWidget& host, QCustomPlot*& plot)
{
    host.resize(1000, 600);
    plot = new QCustomPlot(&host);
    plot->setGeometry(0, 0, 1000, 600);
    host.show();
    if (!QTest::qWaitForWindowExposed(&host))
        return false;
    // QRhiWidget creates its QRhi on the first render: pump the loop until that has happened.
    QElapsedTimer waited;
    waited.start();
    while (!plot->rhi() && waited.elapsed() < 5000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
    }
    return plot->rhi() != nullptr;
}
} // namespace

namespace
{
void fillSine(QVector<double>& keys, QVector<double>& values, double span)
{
    const int n = int(keys.size());
    for (int i = 0; i < n; ++i)
    {
        keys[i] = span * i / n;
        values[i] = qSin(6 * M_PI * i / n);
    }
}
} // namespace

namespace
{
// The multigraph rows: 3 sine columns a third of a period apart, so the three lines differ.
void fillSineMulti(int n, double span, int columns, std::vector<double>& keys,
                   std::vector<std::vector<double>>& values)
{
    keys.resize(n);
    values.assign(columns, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        const double phase = 6 * M_PI * i / n;
        keys[i] = span * i / n;
        for (int c = 0; c < columns; ++c)
            values[c][i] = qSin(phase + c * (2 * M_PI / 3));
    }
}

// The minimal user system for QCPMultiGraph_UserSystem100k: the 4j skew fixture without the
// test-only inspection helpers.
class BenchSkewMapping : public QCPCustomMapping
{
public:
    BenchSkewMapping(const QCPAxis* keyAxis, const QCPAxis* valueAxis, double shear, double bottom)
            : mCartesian(keyAxis, valueAxis), mShear(shear), mBottom(bottom)
    {
    }

    using QCPCustomMapping::map;

    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const override
    {
        const std::size_t count = std::min({ keys.size(), values.size(), out.size() });
        mCartesian.map(keys.first(count), values.first(count), out.first(count));
        for (std::size_t i = 0; i < count; ++i)
            out[i].rx() += shiftOf(out[i].y());
    }

    void map(std::span<const QPointF> data, std::span<QPointF> out) const override
    {
        const std::size_t count = std::min(data.size(), out.size());
        mCartesian.map(data.first(count), out.first(count));
        for (std::size_t i = 0; i < count; ++i)
            out[i].rx() += shiftOf(out[i].y());
    }

    QCP::Pixels mapPoint(QCP::Data data) const override
    {
        const QCP::Pixels pixel = mCartesian.map(data);
        return QCP::Pixels { pixel.x + shiftOf(pixel.y), pixel.y };
    }

    QCP::Data inverse(QPointF pixel) const override
    {
        return mCartesian.inverse(QPointF(pixel.x() - shiftOf(pixel.y()), pixel.y()));
    }

private:
    double shiftOf(double pixelY) const { return mShear * (mBottom - pixelY); }

    QCPTransform mCartesian;
    double mShear;
    double mBottom;
};

class BenchSkewSystem : public QCPCoordinateSystem
{
public:
    BenchSkewSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
    {
    }

    QCPTransform transform() const override
    {
        if (!mMapping || mBuiltRevision != revision())
        {
            mMapping = std::make_shared<const BenchSkewMapping>(
                qobject_cast<const QCPAxis*>(keyAxis().get()),
                qobject_cast<const QCPAxis*>(valueAxis().get()),
                0.5, static_cast<const QCPAxisRect*>(area().get())->rect().bottom());
            mBuiltRevision = revision();
        }
        return QCPTransform::custom(mMapping, mBuiltRevision);
    }

    QRect clipRect() const override { return area()->rect(); }
    bool contains(QPointF pixel) const override { return area()->rect().contains(pixel.toPoint()); }
    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }

private:
    mutable std::shared_ptr<const BenchSkewMapping> mMapping;
    mutable quint64 mBuiltRevision = 0;
};

class BenchSkewRect : public QCPAxisRect
{
public:
    explicit BenchSkewRect(QCustomPlot* plot) : QCPAxisRect(plot) { }

protected:
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override
    {
        if (!key || !value || static_cast<QCPAxis*>(key)->orientation()
            == static_cast<QCPAxis*>(value)->orientation())
            return nullptr;
        return new BenchSkewSystem(this, key, value);
    }
};
} // namespace

namespace
{
// A freshly shown plot has to paint once before the loop so texture/pipeline setup stays out of
// the measurement.
void settleGpuPlot(QCustomPlot* plot)
{
    plot->replot();
    QCoreApplication::processEvents();
}
} // namespace

namespace
{
// A multigraph above kResampleThreshold builds its L1 off-thread: paint, pump until that first
// resample has landed, then paint again. Otherwise a pan row would time the L1 build, not the pan.
bool settleMultiGraph(QCustomPlot* plot, QCPMultiGraph* graph)
{
    plot->replot();
    QElapsedTimer waited;
    waited.start();
    while (graph->pipeline().isBusy() && waited.elapsed() < 10000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
    }
    if (graph->pipeline().isBusy())
        return false;
    plot->replot();
    QCoreApplication::processEvents();
    return true;
}
} // namespace

void Benchmark::QCPGpu_LineSolid100k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    QCPMultiGraph* graph = plot->addGraph();
    graph->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    QVector<double> keys(100000), values(100000);
    fillSine(keys, values, 1.0);
    graph->setData(keys, values);
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(-1.2, 1.2);
    settleGpuPlot(plot);
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 0.5, 1.0, step);
    }
}

void Benchmark::QCPGpu_LineDashed100k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    QCPMultiGraph* graph = plot->addGraph();
    graph->setPen(QPen(Qt::black, 1, Qt::DashLine)); // the QPainter branch, on a GPU surface
    QVector<double> keys(100000), values(100000);
    fillSine(keys, values, 1.0);
    graph->setData(keys, values);
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(-1.2, 1.2);
    settleGpuPlot(plot);
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 0.5, 1.0, step);
    }
}

void Benchmark::QCPGpu_Pan100k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    QCPMultiGraph* graph = plot->addGraph();
    graph->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    QVector<double> keys(100000), values(100000);
    fillSine(keys, values, 100.0);
    graph->setData(keys, values);
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(-1.2, 1.2);
    settleGpuPlot(plot);
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // 1 % of the window, the pan-fast-path candidate
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPMultiGraph_SetData1M()
{
    // Offscreen row: the multigraph never had a rendered range, so setData installs at once and
    // the loop times the copy into the source.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 1000000;
    QBENCHMARK
    {
        std::vector<double> keys;
        std::vector<std::vector<double>> values;
        fillSineMulti(n, 1.0, 3, keys, values);
        mg->setData(std::move(keys), std::move(values));
    }
}

// The 2.x QCPGraph_RemoveData* workloads: 1M points set again before every timed removal, so
// the timing is ours and averaged over enough iterations.
static void timeRemoval(QCustomPlot* plot, const std::function<void(QCPMultiGraph*)>& remove)
{
    auto* graph = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    int n = 1000000;
    QVector<double> x1(n), y1(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = 2.0 * i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
    }

    QElapsedTimer timer;
    bool done = false;
    qint64 elapsed = 0;
    int iterations = 0;
    while (!done)
    {
        ++iterations;
        graph->setData(x1, y1);
        timer.restart();
        remove(graph);
        elapsed += timer.nsecsElapsed();
        done = elapsed > 25e3 && iterations > 3; // have 25us and done some iterations
    }
    QTest::setBenchmarkResult(elapsed / 1e6 / (double)iterations, QTest::WalltimeMilliseconds);
}

void Benchmark::QCPMultiGraph_RemoveDataBetween()
{
    timeRemoval(mPlot, [](QCPMultiGraph* graph) { graph->removeData(0.5, 1.5); }); // 50% of total data in center
}

void Benchmark::QCPMultiGraph_RemoveDataAfter()
{
    timeRemoval(mPlot, [](QCPMultiGraph* graph) { graph->removeDataAfter(1.0); }); // last 50% of total data
}

void Benchmark::QCPMultiGraph_RemoveDataBefore()
{
    timeRemoval(mPlot, [](QCPMultiGraph* graph) { graph->removeDataBefore(1.0); }); // first 50% of total data
}

void Benchmark::QCPMultiGraph_AddDataAtEndSorted()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500000;
    QVector<double> x1(n), y1(n), x2(n), y2(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
        x2[i] = (i + n) / (double)n;
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QBENCHMARK_ONCE
    {
        graph->addData(x2, y2, true);
    }
}

void Benchmark::QCPMultiGraph_AddDataAtBeginSorted()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500000;
    QVector<double> x1(n), y1(n), x2(n), y2(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = (i + n) / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
        x2[i] = i / (double)n;
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QBENCHMARK_ONCE
    {
        graph->addData(x2, y2, true);
    }
}

void Benchmark::QCPMultiGraph_AddDataMixedSorted()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500000;
    QVector<double> x1(n), y1(n), x2(n), y2(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
        x2[i] = (i + 0.5) / (double)(3 * n);
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QBENCHMARK_ONCE
    {
        graph->addData(x2, y2, true);
    }
}

void Benchmark::QCPMultiGraph_AddDataAtEndUnsorted()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500000;
    QVector<double> x1(n), y1(n), x2(n), y2(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
        x2[i] = (i + n + 0.1 * (std::rand() % 500))
            / (double)n; // not completely randomized, but local disorder, global ascending order
                         // (typical use-case for data acquisition)
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QBENCHMARK_ONCE
    {
        graph->addData(x2, y2, false);
    }
}

void Benchmark::QCPMultiGraph_AddDataAtBeginUnsorted()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500000;
    QVector<double> x1(n), y1(n), x2(n), y2(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = (i + n) / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
        x2[i] = (i - 0.1 * (std::rand() % 500))
            / (double)n; // not completely randomized, but local disorder, global ascending order
                         // (typical use-case for data acquisition)
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QBENCHMARK_ONCE
    {
        graph->addData(x2, y2, false);
    }
}

void Benchmark::QCPMultiGraph_AddDataMixedUnsorted()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500000;
    QVector<double> x1(n), y1(n), x2(n), y2(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
        x2[i] = (i + 0.1 * (std::rand() % 500))
            / (double)(3 * n); // not completely randomized, but local disorder, global ascending
                               // order (typical use-case for data acquisition)
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QBENCHMARK_ONCE
    {
        graph->addData(x2, y2, false);
    }
}

void Benchmark::QCPMultiGraph_AddDataSingleAtEnd()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 50000;
    QVector<double> x1(n), y1(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
    }
    int n2 = 5000;
    QVector<double> y2(n2);
    for (int i = 0; i < n2; ++i)
        y2[i] = qSin(i / (double)n * 10 * M_PI);

    graph->setData(x1, y1);
    double key = x1.last() + 0.1;
    QElapsedTimer timer;
    qint64 elapsed = 0;
    int iteration = 0;
    while (iteration < n2)
    {
        timer.restart();
        graph->addData(key, y2[iteration]);
        elapsed += timer.nsecsElapsed();
        key += 0.1;
        ++iteration;
    }
    QTest::setBenchmarkResult(elapsed / 1e3 / (double)iteration,
                              QTest::WalltimeMilliseconds);
}

void Benchmark::QCPMultiGraph_AddDataSingleAtBegin()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 50000;
    QVector<double> x1(n), y1(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
    }
    int n2 = 5000;
    QVector<double> y2(n2);
    for (int i = 0; i < n2; ++i)
        y2[i] = qSin(i / (double)n * 10 * M_PI);

    graph->setData(x1, y1);
    double key = x1.first() - 0.1;
    QElapsedTimer timer;
    qint64 elapsed = 0;
    int iteration = 0;
    while (iteration < n2)
    {
        timer.restart();
        graph->addData(key, y2[iteration]);
        elapsed += timer.nsecsElapsed();
        key -= 0.1;
        ++iteration;
    }
    QTest::setBenchmarkResult(elapsed / 1e3 / (double)iteration,
                              QTest::WalltimeMilliseconds);
}

void Benchmark::QCPMultiGraph_AddDataSingleRandom()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 50000;
    QVector<double> x1(n), y1(n);
    for (int i = 0; i < n; ++i)
    {
        x1[i] = i / (double)n;
        y1[i] = qSin(x1[i] * 10 * M_PI);
    }
    int n2 = 5000;
    QVector<double> x2(n2), y2(n2);
    for (int i = 0; i < n2; ++i)
    {
        x2[i] = (std::rand() % n + 0.5) / (double)n;
        y2[i] = qSin(x2[i] * 10 * M_PI);
    }

    graph->setData(x1, y1);
    QElapsedTimer timer;
    qint64 elapsed = 0;
    int iteration = 0;
    while (iteration < n2)
    {
        timer.restart();
        graph->addData(x2[iteration], y2[iteration]);
        elapsed += timer.nsecsElapsed();
        ++iteration;
    }
    QTest::setBenchmarkResult(elapsed / 1e3 / (double)iteration,
                              QTest::WalltimeMilliseconds);
}

void Benchmark::QCPMultiGraph_FillToGraphPan10k()
{
    auto* owner = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto* partner = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 10000;
    QVector<double> keys(n), ownerValues(n), partnerValues(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = 100.0 * i / n;
        ownerValues[i] = 0.4 + 0.2 * qSin(i * 0.01);
        partnerValues[i] = 0.7 + 0.2 * qCos(i * 0.013);
    }
    owner->setData(keys, ownerValues);
    partner->setData(keys, partnerValues);
    owner->setComponentFillToGraph(0, partner, 0, QBrush(QColor(0, 0, 255, 80)));
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.01;
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPMultiGraph_Standard()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    int n = 500;
    QVector<double> x(n), y1(n), y2(n), y3(n);
    for (int i = 0; i < n; ++i)
    {
        x[i] = i / (double)n;
        y1[i] = qSin(x[i] * 10 * M_PI);
        y2[i] = qCos(x[i] * 40 * M_PI);
        y3[i] = x[i];
    }
    mg->setData(std::move(x), std::vector<std::vector<double>>{
                                  std::vector<double>(y1.begin(), y1.end()),
                                  std::vector<double>(y2.begin(), y2.end()),
                                  std::vector<double>(y3.begin(), y3.end())});
    mg->setComponentFill(0, 0.0, QBrush(QColor(100, 0, 0, 100)));
    mPlot->rescaleAxes();
    mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_ManyPoints()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsNone);
    mg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle));
    int n = 50000;
    std::vector<double> x(n);
    std::vector<std::vector<double>> columns(3, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        x[i] = i / (double)n;
        columns[0][i] = qSin(x[i] * 10 * M_PI);
        columns[1][i] = qCos(x[i] * 40 * M_PI);
        columns[2][i] = x[i];
    }
    mg->setData(std::move(x), std::move(columns));
    mPlot->rescaleAxes();
    mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_ManyLines()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsLine);
    mg->setScatterStyle(QCPScatterStyle());
    int n = 50000;
    std::vector<double> x(n);
    std::vector<std::vector<double>> columns(3, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        x[i] = i / (double)n;
        columns[0][i] = qSin(x[i] * 10 * M_PI);
        columns[1][i] = qCos(x[i] * 40 * M_PI);
        columns[2][i] = x[i];
    }
    mg->setData(std::move(x), std::move(columns));
    mPlot->rescaleAxes();
    mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_ManyOffScreenLines()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsLine);
    mg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));
    int n = 50000;
    std::vector<double> x(n);
    std::vector<std::vector<double>> columns(3, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        x[i] = i / (double)n;
        columns[0][i] = qSin(x[i] * 10 * M_PI);
        columns[1][i] = qCos(x[i] * 40 * M_PI);
        columns[2][i] = x[i];
    }
    mg->setData(std::move(x), std::move(columns));
    mPlot->rescaleAxes();
    mPlot->xAxis->setRange(1.1, 2.1);

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_SetDataDouble()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 1000000;
    QBENCHMARK
    {
        std::vector<double> keys(n), values(n);
        for (int i = 0; i < n; ++i)
        {
            keys[i] = i / static_cast<double>(n);
            values[i] = qSin(keys[i] * 10 * M_PI);
        }
        mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(values)});
    }
}

void Benchmark::QCPMultiGraph_SetDataFloat()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 1000000;
    QBENCHMARK
    {
        std::vector<double> keys(n);
        std::vector<float> values(n);
        for (int i = 0; i < n; ++i)
        {
            keys[i] = i / static_cast<double>(n);
            values[i] = static_cast<float>(qSin(keys[i] * 10 * M_PI));
        }
        mg->setData(std::move(keys), std::vector<std::vector<float>>{std::move(values)});
    }
}

void Benchmark::QCPMultiGraph_RenderDouble()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(1000000, 1.0, 1, keys, values);
    mg->setData(std::move(keys), std::move(values));
    mg->rescaleAxes();
    mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_RenderFloat()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 1000000;
    std::vector<double> keys(n);
    std::vector<float> values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i / static_cast<double>(n);
        values[i] = static_cast<float>(qSin(keys[i] * 10 * M_PI));
    }
    mg->setData(std::move(keys), std::vector<std::vector<float>>{std::move(values)});
    mg->rescaleAxes();
    mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_RenderManyLines()
{
    const int n = 50000;
    for (int g = 0; g < 3; ++g)
    {
        auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
        std::vector<double> keys(n), values(n);
        for (int i = 0; i < n; ++i)
        {
            keys[i] = i / static_cast<double>(n);
            values[i] = qSin(keys[i] * (10 + 30 * g) * M_PI);
        }
        mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(values)});
    }
    mPlot->rescaleAxes();
    mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_ScatterDisc1M()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsNone);
    mg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
    const int n = 1000000;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i / static_cast<double>(n);
        values[i] = qSin(keys[i] * 10 * M_PI) + 0.1 * (std::rand() / double(RAND_MAX));
    }
    mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(values)});
    mg->rescaleAxes();

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_ScatterSquare1M()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsNone);
    mg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, 4));
    const int n = 1000000;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i / static_cast<double>(n);
        values[i] = qSin(keys[i] * 10 * M_PI) + 0.1 * (std::rand() / double(RAND_MAX));
    }
    mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(values)});
    mg->rescaleAxes();

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPMultiGraph_ScatterWithLine1M()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsLine);
    mg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
    const int n = 1000000;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i / static_cast<double>(n);
        values[i] = qSin(keys[i] * 10 * M_PI);
    }
    mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(values)});
    mg->rescaleAxes();

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPInput_WheelOverMultiGraph_1M()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(1000000, 1.0, 1, keys, values);
    mg->setData(std::move(keys), std::move(values));
    mPlot->xAxis->setRange(0, 1);
    mPlot->yAxis->setRange(-1.2, 1.2);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QRect axisRect = mPlot->axisRect()->rect();
    const QPointF pos(axisRect.left() + 10, axisRect.top() + 10);
    QWheelEvent event(pos, mPlot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QBENCHMARK
    {
        QCoreApplication::sendEvent(mPlot, &event);
    }
}

void Benchmark::QCPMultiGraph_UserSystem100k()
{
    mPlot->plotLayout()->clear();
    auto* rect = new BenchSkewRect(mPlot);
    mPlot->plotLayout()->addElement(0, 0, rect);
    rect->axis(QCPAxis::atBottom)->setRange(0, 100000);
    rect->axis(QCPAxis::atLeft)->setRange(-1.5, 1.5);

    auto* mg = new QCPMultiGraph(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(100000, 100000.0, 1, keys, values);
    mg->setData(std::move(keys), std::move(values));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QBENCHMARK
    {
        mPlot->replot();
    }
}

void Benchmark::QCPGpu_MultiGraphRender1M()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(1000000, 1.0, 1, keys, values);
    mg->setData(std::move(keys), std::move(values));
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(-1.2, 1.2);
    QVERIFY2(settleMultiGraph(plot, mg), "the first multigraph resample did not land");
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 0.5, 1.0, step);
    }
}

void Benchmark::QCPGpu_MultiGraphPan1M()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(1000000, 100.0, 3, keys, values);
    mg->setData(std::move(keys), std::move(values));
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(-1.2, 1.2);
    QVERIFY2(settleMultiGraph(plot, mg), "the first multigraph resample did not land");
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // 1 % of the window
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_MultiGraphZoom1M()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(1000000, 100.0, 3, keys, values);
    mg->setData(std::move(keys), std::move(values));
    plot->xAxis->setRange(49.5, 50.5);
    plot->yAxis->setRange(-1.2, 1.2);
    QVERIFY2(settleMultiGraph(plot, mg), "the first multigraph resample did not land");
    int step = 0;
    QBENCHMARK
    {
        const double width = (step++ % 2) ? 1.0 : 0.99; // zoom in place by 1 %
        plot->xAxis->setRange(50 - width / 2, 50 + width / 2);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_MultiGraphColored100kPan()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    const int n = 100000;
    std::vector<double> keys;
    std::vector<std::vector<double>> values;
    fillSineMulti(n, 100.0, 1, keys, values);
    mg->setData(std::move(keys), std::move(values));
    std::vector<double> scalar(n);
    for (int i = 0; i < n; ++i)
        scalar[i] = 50 + 50 * qSin(6 * M_PI * i / n); // colour by another scalar
    mg->setColorValues(std::move(scalar));
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(-1.2, 1.2);
    QVERIFY2(settleMultiGraph(plot, mg), "the first multigraph resample did not land");
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // 1 % of the window
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_CurvePan100k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* curve = new QCPCurve(plot->xAxis, plot->yAxis);
    const int n = 100000;
    QVector<double> t(n), keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        t[i] = i; // a Lissajous: three loops of the key against two of the value
        keys[i] = 50 + 50 * qSin(6 * M_PI * i / n);
        values[i] = qSin(4 * M_PI * i / n);
    }
    curve->setData(t, keys, values);
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(-1.2, 1.2);
    settleGpuPlot(plot);
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // 1 % of the window
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_Bars10k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* bars = new QCPBars(plot->xAxis, plot->yAxis);
    QVector<double> keys(10000), values(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = 100.0 * i / 10000;
        values[i] = 0.5 + 0.5 * qSin(i * 0.01);
    }
    bars->setData(keys, values);
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(0, 1);
    settleGpuPlot(plot);
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 50.5, 1.0, step);
    }
}

void Benchmark::QCPGpu_BarsPan10k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* bars = new QCPBars(plot->xAxis, plot->yAxis);
    QVector<double> keys(10000), values(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = 100.0 * i / 10000;
        values[i] = 0.5 + 0.5 * qSin(i * 0.01);
    }
    bars->setData(keys, values);
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(0, 1);
    settleGpuPlot(plot);
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // a pan: a plot-width bar's pixels all move by one delta
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_Text1k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    for (int i = 0; i < 1000; ++i)
    {
        auto* text = new QCPItemText(plot);
        text->setText(QString::number(i));
        text->position->setCoords(100.0 * i / 1000, 0.5);
    }
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(0, 1);
    settleGpuPlot(plot);
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 50.5, 1.0, step);
    }
}

void Benchmark::QCPGpu_MixedLayer()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    // The stacking question of the drawing API: GPU-drawn graphs and QPainter text interleaved in
    // one layer, which is what a default plot does today.
    for (int i = 0; i < 100; ++i)
    {
        QCPMultiGraph* graph = plot->addGraph();
        graph->setPen(QPen(QColor::fromHsv((i * 7) % 360, 200, 160), 1, Qt::SolidLine));
        QVector<double> keys(1000), values(1000);
        for (int j = 0; j < 1000; ++j)
        {
            keys[j] = j / 999.0;
            values[j] = 0.9 * qSin(6 * M_PI * keys[j] + i * 0.1);
        }
        graph->setData(keys, values);
        auto* text = new QCPItemText(plot);
        text->setText(QStringLiteral("g%1").arg(i));
        text->position->setCoords(0.01 * i, 0.9);
    }
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(-1.1, 1.1);
    settleGpuPlot(plot);
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 0.5, 1.0, step);
    }
}

namespace
{
// A 4000 x 1000 spectrogram: x and y index the cells, z a repeating ramp, as the resample rows.
void fillSpectrogram(int nx, int ny, std::vector<double>& x, std::vector<double>& y,
                     std::vector<double>& z)
{
    x.resize(nx);
    for (int i = 0; i < nx; ++i)
        x[i] = double(i);
    y.resize(ny);
    for (int j = 0; j < ny; ++j)
        y[j] = double(j);
    z.resize(std::size_t(nx) * ny);
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = double(i % 997);
}

// One million deterministic samples over 0..100 in both coordinates (irrational strides).
void fillScatter2D(int n, std::vector<double>& keys, std::vector<double>& values)
{
    keys.resize(n);
    values.resize(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = 100 * std::fmod(i * 0.7548776662466927, 1.0);
        values[i] = 100 * std::fmod(i * 0.5698402909980532, 1.0);
    }
}

// A 2-D plottable above the async threshold bins or resamples off-thread: paint, pump until that
// first result has landed, then paint again. Otherwise a pan row would time the first build.
template <typename Plottable>
bool settlePipeline(QCustomPlot* plot, Plottable* plottable)
{
    plot->replot();
    QElapsedTimer waited;
    waited.start();
    while (plottable->pipeline().isBusy() && waited.elapsed() < 30000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
    }
    if (plottable->pipeline().isBusy())
        return false;
    plot->replot();
    QCoreApplication::processEvents();
    return true;
}
} // namespace

void Benchmark::QCPGpu_ColorMapPan()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    const int n = 1000;
    auto* map = new QCPColorMap(plot->xAxis, plot->yAxis);
    map->data()->setSize(n, n);
    map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
    for (int x = 0; x < n; ++x)
        for (int y = 0; y < n; ++y)
            map->data()->setCell(x, y, double((x * 31 + y * 17) % 1000));
    map->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    map->rescaleDataRange();
    plot->xAxis->setRange(0, 2);
    plot->yAxis->setRange(0, 10);
    settleGpuPlot(plot);
    double left = 0;
    QBENCHMARK
    {
        left += 0.02; // 1 % of the window
        if (left + 2 > 10)
            left = 0; // wrap: one measurement runs far more steps than the data is wide
        plot->xAxis->setRange(left, left + 2);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_ColorMap2Pan()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    std::vector<double> x, y, z;
    fillSpectrogram(4000, 1000, x, y, z);
    cm->setData(std::move(x), std::move(y), std::move(z));
    plot->xAxis->setRange(0, 400);
    plot->yAxis->setRange(0, 999);
    QVERIFY2(settlePipeline(plot, cm), "the first colour-map resample did not land");
    double left = 0;
    QBENCHMARK
    {
        left += 4; // 1 % of the 400-wide window
        if (left + 400 > 3999)
            left = 0; // wrap: one measurement runs far more steps than the source is wide
        plot->xAxis->setRange(left, left + 400);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_ColorMap2Zoom()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    std::vector<double> x, y, z;
    fillSpectrogram(4000, 1000, x, y, z);
    cm->setData(std::move(x), std::move(y), std::move(z));
    plot->xAxis->setRange(1800, 2200);
    plot->yAxis->setRange(0, 999);
    QVERIFY2(settlePipeline(plot, cm), "the first colour-map resample did not land");
    int step = 0;
    QBENCHMARK
    {
        const double width = (step++ % 2) ? 400.0 : 396.0; // zoom in place by 1 %
        plot->xAxis->setRange(2000 - width / 2, 2000 + width / 2);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_Histogram2DPan()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    hist->setBins(200, 200);
    std::vector<double> keys, values;
    fillScatter2D(1000000, keys, values);
    hist->setData(std::move(keys), std::move(values));
    plot->xAxis->setRange(40, 60);
    plot->yAxis->setRange(40, 60);
    QVERIFY2(settlePipeline(plot, hist), "the first 2-D binning did not land");
    double left = 40;
    QBENCHMARK
    {
        left += 0.2; // 1 % of the window
        if (left + 20 > 100)
            left = 0; // wrap: one measurement runs far more steps than the data is wide
        plot->xAxis->setRange(left, left + 20);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPGpu_HistogramPan()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    auto* hist = new QCPHistogram(plot->xAxis, plot->yAxis);
    hist->setBins(100);
    const int n = 1000000;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = double(i);
        values[i] = 100 * std::fmod(i * 0.7548776662466927, 1.0);
    }
    using Src = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
    hist->setDataSource(std::make_shared<Src>(
        std::move(keys), std::vector<std::vector<double>> { std::move(values) }));
    plot->xAxis->setRange(40, 60);
    plot->yAxis->setRange(0, 15000);
    QVERIFY2(settlePipeline(plot, hist), "the first 1-D binning did not land");
    double left = 40;
    QBENCHMARK
    {
        left += 0.2; // 1 % of the window
        if (left + 20 > 100)
            left = 0; // wrap: one measurement runs far more steps than the data is wide
        plot->xAxis->setRange(left, left + 20);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPBars_Zoom10k()
{
    auto* bars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(10000), values(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = 100.0 * i / 10000;
        values[i] = 0.5 + 0.5 * qSin(i * 0.01);
    }
    bars->setData(keys, values);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(mPlot, 50.5, 1.0, step);
    }
}

void Benchmark::QCPBars_Pan10k()
{
    auto* bars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(10000), values(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = 100.0 * i / 10000;
        values[i] = 0.5 + 0.5 * qSin(i * 0.01);
    }
    bars->setData(keys, values);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.01;
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPItemText_Zoom1k()
{
    for (int i = 0; i < 1000; ++i)
    {
        auto* text = new QCPItemText(mPlot);
        text->setText(QString::number(i));
        text->position->setCoords(100.0 * i / 1000, 0.5);
    }
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(mPlot, 50.5, 1.0, step);
    }
}

namespace
{
// One million points over 0..1000 / -1..1, the same data for the scalar and the batch row.
void millionPoints(std::vector<double>& keys, std::vector<double>& values)
{
    const int n = 1000000;
    keys.resize(n);
    values.resize(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = 1000.0 * i / n;
        values[i] = qSin(6 * M_PI * i / n);
    }
}
} // namespace

void Benchmark::QCPCoords_CoordToPixel1M()
{
    // The baseline the batch has to beat: two virtual calls per point, through the axis painter.
    std::vector<double> keys, values;
    millionPoints(keys, values);
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot();
    std::vector<QPointF> out(keys.size());
    QBENCHMARK
    {
        const QCPAxis* key = mPlot->xAxis;
        const QCPAxis* value = mPlot->yAxis;
        for (std::size_t i = 0; i < keys.size(); ++i)
            out[i] = QPointF(key->coordToPixel(keys[i]), value->coordToPixel(values[i]));
    }
    QCOMPARE(int(out.size()), 1000000);
}

void Benchmark::QCPTransform_Map1M()
{
    std::vector<double> keys, values;
    millionPoints(keys, values);
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot();
    const QCPTransform transform(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(transform.valid());
    std::vector<QPointF> out(keys.size());
    QBENCHMARK
    {
        transform.map(keys, values, out);
    }
    QCOMPARE(int(out.size()), 1000000);
}

namespace
{
// One shared 1M-point set for the replay rows: data space, so the replay maps every point.
std::shared_ptr<const std::vector<QPointF>> sharedMillionPoints()
{
    static const std::shared_ptr<const std::vector<QPointF>> points = []
    {
        auto made = std::make_shared<std::vector<QPointF>>(1000000);
        for (std::size_t i = 0; i < made->size(); ++i)
            (*made)[i] = QPointF(1000.0 * i / made->size(), qSin(6 * M_PI * i / made->size()));
        return std::shared_ptr<const std::vector<QPointF>>(made);
    }();
    return points;
}

std::shared_ptr<const std::vector<QPointF>> sharedPoints(int count)
{
    auto made = std::make_shared<std::vector<QPointF>>(count);
    for (int i = 0; i < count; ++i)
        (*made)[i] = QPointF(1000.0 * i / count, qSin(6 * M_PI * i / count));
    return std::shared_ptr<const std::vector<QPointF>>(made);
}
} // namespace

void Benchmark::QCPDrawContext_ReplayData1M()
{
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot();
    QCPDrawContext context(mPlot->xAxis, mPlot->yAxis, mPlot->axisRect(), nullptr);
    // NoPen: the timed part is the mapping and the NaN split, not the rasterisation.
    context.polyline(sharedMillionPoints(), QPen(Qt::NoPen));
    const QCPDrawFrame frame = context.frame();
    const QCPDrawList& list = context.drawList();
    QImage image(1000, 600, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QBENCHMARK
    {
        QCPPainter painter(&image);
        qcpReplayWithPainter(list, painter, frame);
    }
}

void Benchmark::QCPDrawContext_ReplayMarkers100k()
{
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot();
    QCPDrawContext context(mPlot->xAxis, mPlot->yAxis, mPlot->axisRect(), nullptr);
    QCPScatterStyle style(QCPScatterStyle::ssDot, QPen(Qt::black), QBrush(Qt::black), 1.0);
    context.markers(sharedPoints(100000), style);
    const QCPDrawFrame frame = context.frame();
    const QCPDrawList& list = context.drawList();
    QImage image(1000, 600, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QBENCHMARK
    {
        QCPPainter painter(&image);
        qcpReplayWithPainter(list, painter, frame);
    }
}

namespace
{
//! A drawn item the benchmarks own: one bar, recorded through the new contract only.
class BenchDrawnBar : public QCPAbstractItem
{
public:
    BenchDrawnBar(QCustomPlot* plot, double key, double width) : QCPAbstractItem(plot)
    {
        mAnchor = createPosition(QStringLiteral("anchor")); // the axes the replay maps through
        mAnchor->setCoords(key, 0);
        setClipAxisRect(plot->axisRect());
        mA = QCP::Data { key, 0.0 };
        mB = QCP::Data { key + width, 1.0 };
    }

    void draw(QCPDrawContext& context) const override
    {
        context.rect(mA, mB, mPen, QBrush(Qt::NoBrush));
    }

    QCPItemPosition* mAnchor = nullptr;
    QCP::Data mA;
    QCP::Data mB;
    QPen mPen = QPen(Qt::black);
};

//! The layout of addLineItems(), expressed only with the drawing contract.
void addDrawnBars(QCustomPlot* plot, int count, double xMax)
{
    for (int i = 0; i < count; ++i)
        new BenchDrawnBar(plot, xMax * i / count, 0.01);
}

//! The same bars, but all of them recorded by ONE object: one recording, \a count commands.
class BenchDrawnBarSet : public QCPAbstractItem
{
public:
    BenchDrawnBarSet(QCustomPlot* plot, int count, double xMax) : QCPAbstractItem(plot)
    {
        auto* anchor = createPosition(QStringLiteral("anchor"));
        anchor->setCoords(xMax / 2, 0);
        setClipAxisRect(plot->axisRect());
        mBars.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i)
        {
            const double key = xMax * i / count;
            mBars.emplace_back(QCP::Data { key, 0.0 }, QCP::Data { key + 0.01, 1.0 });
        }
    }

    void draw(QCPDrawContext& context) const override
    {
        for (const auto& [lower, upper] : mBars)
            context.rect(lower, upper, mPen, QBrush(Qt::NoBrush));
    }

    std::vector<std::pair<QCP::Data, QCP::Data>> mBars;
    QPen mPen = QPen(Qt::black);
};
} // namespace

void Benchmark::QCPDrawn_Zoom10k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    addDrawnBars(plot, 10000, 100);
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(0, 1);
    settleGpuPlot(plot);
    int step = 0;
    QBENCHMARK
    {
        zoomInPlace(plot, 50.5, 1.0, step);
    }
}

void Benchmark::QCPDrawn_Pan10k()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    addDrawnBars(plot, 10000, 100);
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(0, 1);
    settleGpuPlot(plot);
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // a pan step: the kept triangles only need the delta
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPDrawn_Pan10kOneRecording()
{
    QWidget host;
    QCustomPlot* plot = nullptr;
    if (!makeGpuPlot(host, plot))
        QSKIP("no QRhi on this platform (qoffscreen)");
    new BenchDrawnBarSet(plot, 10000, 100); // one object: the extrusion cache has 10 000 keys
    plot->xAxis->setRange(50, 51);
    plot->yAxis->setRange(0, 1);
    settleGpuPlot(plot);
    double left = 50;
    QBENCHMARK
    {
        left += 0.01; // a pan step: the kept triangles only need the delta
        plot->xAxis->setRange(left, left + 1);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

////////////////////////////////////////////////////////////////////
/////// Milestone 4e rows: the classes the next ports take over

// Every row below pans the same window over the same span, so the numbers compare with the
// QCPItems_Pan10k / QCPBars_Pan10k rows above.
void Benchmark::QCPErrorBars_Pan10k()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(10000);
    QVector<double> values(10000);
    QVector<double> errors(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = 100.0 * i / 10000;
        values[i] = 0.5 + 0.5 * qSin(i * 0.01);
        errors[i] = 0.05;
    }
    graph->setData(keys, values);
    auto* bars = new QCPErrorBars(mPlot->xAxis, mPlot->yAxis);
    bars->setDataPlottable(graph);
    bars->setData(errors);
    bars->setErrorType(QCPErrorBars::etValueError);
    bars->setWhiskerWidth(4);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.01;
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPFinancial_Pan10k()
{
    auto* financial = new QCPFinancial(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(10000);
    QVector<double> open(10000);
    QVector<double> high(10000);
    QVector<double> low(10000);
    QVector<double> close(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = 100.0 * i / 10000;
        const double mid = 0.5 + 0.3 * qSin(i * 0.01);
        open[i] = mid - 0.05;
        close[i] = mid + 0.05;
        high[i] = mid + 0.15;
        low[i] = mid - 0.15;
    }
    financial->setData(keys, open, high, low, close);
    financial->setChartStyle(QCPFinancial::csCandlestick);
    financial->setWidth(0.008);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.01;
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPStatisticalBox_Pan1k()
{
    auto* box = new QCPStatisticalBox(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1000);
    QVector<double> minimum(1000);
    QVector<double> lowerQuartile(1000);
    QVector<double> median(1000);
    QVector<double> upperQuartile(1000);
    QVector<double> maximum(1000);
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = 100.0 * i / 1000;
        const double mid = 0.5 + 0.3 * qSin(i * 0.03);
        minimum[i] = mid - 0.2;
        lowerQuartile[i] = mid - 0.08;
        median[i] = mid;
        upperQuartile[i] = mid + 0.08;
        maximum[i] = mid + 0.2;
    }
    box->setData(keys, minimum, lowerQuartile, median, upperQuartile, maximum);
    box->setWidth(0.06);
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.01;
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPIntervals_Pan10k()
{
    // 10 000 intervals in two lanes, every one of them inside the one-key window below.
    auto* layout = new QCPLaneLayout();
    layout->setParent(mPlot);
    const int laneA = layout->laneIndex(QStringLiteral("A"));
    const int laneB = layout->laneIndex(QStringLiteral("B"));
    auto* intervals = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, layout);
    qcp::intervals::Columns columns;
    columns.start.resize(10000);
    columns.stop.resize(10000);
    columns.lane.resize(10000);
    columns.category.assign(10000, 0);
    columns.ids.resize(10000);
    for (int i = 0; i < 10000; ++i)
    {
        columns.start[i] = 50.0 + 0.9 * i / 10000.0;
        columns.stop[i] = columns.start[i] + 0.0004;
        columns.lane[i] = i % 2 ? laneB : laneA;
        columns.ids[i] = i;
    }
    intervals->setData(columns);
    intervals->setCategoryNames(QStringList { QStringLiteral("A") });
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 2);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.001; // a pan step: the width never changes, only the window slides
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPIntervals_Pan10kLabelled()
{
    // QCPIntervals_Pan10k with a short text on every interval: the real timeline of SciQLop,
    // where the labels are made from the same merged bars the recording draws.
    auto* layout = new QCPLaneLayout();
    layout->setParent(mPlot);
    const int laneA = layout->laneIndex(QStringLiteral("A"));
    const int laneB = layout->laneIndex(QStringLiteral("B"));
    auto* intervals = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, layout);
    qcp::intervals::Columns columns;
    columns.start.resize(10000);
    columns.stop.resize(10000);
    columns.lane.resize(10000);
    columns.category.assign(10000, 0);
    columns.ids.resize(10000);
    columns.labels.reserve(10000);
    for (int i = 0; i < 10000; ++i)
    {
        columns.start[i] = 50.0 + 0.9 * i / 10000.0;
        columns.stop[i] = columns.start[i] + 0.0004;
        columns.lane[i] = i % 2 ? laneB : laneA;
        columns.ids[i] = i;
        columns.labels << QStringLiteral("e%1").arg(i);
    }
    intervals->setData(columns);
    intervals->setCategoryNames(QStringList { QStringLiteral("A") });
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 2);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.001; // a pan step: the width never changes, only the window slides
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPItems_MixedKinds_Pan1k()
{
    // 100 of every item kind plus 50 spans of each orientation, all inside 50..51: the pan
    // below draws each of them on every frame instead of culling almost all of them.
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(101);
    QVector<double> values(101);
    for (int i = 0; i < 101; ++i)
    {
        keys[i] = 50.0 + 0.9 * i / 100.0;
        values[i] = 0.5 + 0.3 * qSin(i * 0.2);
    }
    graph->setData(keys, values);

    QPixmap pixmap(48, 36);
    pixmap.fill(QColor(40, 120, 200));

    for (int i = 0; i < 100; ++i)
    {
        const double x = 50.0 + i * 0.009;
        const double y = 0.5 + 0.3 * qSin(i * 0.2);
        auto* line = new QCPItemLine(mPlot);
        line->start->setCoords(x, y - 0.05);
        line->end->setCoords(x + 0.006, y + 0.05);
        auto* straight = new QCPItemStraightLine(mPlot);
        straight->point1->setCoords(x, y - 0.04);
        straight->point2->setCoords(x + 0.006, y + 0.04);
        auto* curve = new QCPItemCurve(mPlot);
        curve->start->setCoords(x, y - 0.04);
        curve->startDir->setCoords(x + 0.002, y - 0.01);
        curve->endDir->setCoords(x + 0.004, y + 0.01);
        curve->end->setCoords(x + 0.006, y + 0.04);
        auto* bracket = new QCPItemBracket(mPlot);
        bracket->left->setCoords(x, y);
        bracket->right->setCoords(x + 0.006, y);
        bracket->setLength(6);
        auto* rect = new QCPItemRect(mPlot);
        rect->topLeft->setCoords(x, y + 0.02);
        rect->bottomRight->setCoords(x + 0.006, y - 0.02);
        auto* ellipse = new QCPItemEllipse(mPlot);
        ellipse->topLeft->setCoords(x, y + 0.02);
        ellipse->bottomRight->setCoords(x + 0.006, y - 0.02);
        auto* image = new QCPItemPixmap(mPlot);
        image->topLeft->setCoords(x, y + 0.02);
        image->bottomRight->setCoords(x + 0.006, y - 0.02);
        image->setPixmap(pixmap);
        auto* tracer = new QCPItemTracer(mPlot);
        tracer->setGraph(graph);
        tracer->setGraphKey(x);
        tracer->setStyle(QCPItemTracer::tsCircle);
        auto* rspan = new QCPItemRSpan(mPlot);
        rspan->leftEdge->setCoords(x, y);
        rspan->rightEdge->setCoords(x + 0.004, y);
        rspan->topEdge->setCoords(x, y + 0.03);
        rspan->bottomEdge->setCoords(x, y - 0.03);
        if (i % 2 == 0)
        {
            auto* hspan = new QCPItemSpan(mPlot, Qt::Horizontal);
            hspan->lowerEdge->setCoords(x, y - 0.02);
            hspan->upperEdge->setCoords(x, y + 0.02);
        }
        else
        {
            auto* vspan = new QCPItemSpan(mPlot, Qt::Vertical);
            vspan->lowerEdge->setCoords(x, y);
            vspan->upperEdge->setCoords(x + 0.004, y);
        }
    }
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.001; // a pan step: the width never changes, only the window slides
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPItems_Tracers_Pan1k()
{
    // A thousand tracers on one graph, each at its own key inside the one-key window: every one of
    // them draws on each frame, and each asks its graph where it is.
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1001);
    QVector<double> values(1001);
    for (int i = 0; i < 1001; ++i)
    {
        keys[i] = 50.0 + 0.9 * i / 1000.0;
        values[i] = 0.5 + 0.3 * qSin(i * 0.2);
    }
    graph->setData(keys, values);
    for (int i = 0; i < 1000; ++i)
    {
        auto* tracer = new QCPItemTracer(mPlot);
        tracer->setGraph(graph);
        tracer->setGraphKey(50.0 + 0.9 * i / 1000.0);
        tracer->setStyle(QCPItemTracer::tsCircle);
        tracer->setSize(6);
    }
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.001; // a pan step: the width never changes, only the window slides
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPSpans_Pan100()
{
    // 100 spans, all inside the one-key window, so every one of them draws on each frame.
    for (int i = 0; i < 100; ++i)
    {
        const double x = 50.0 + i * 0.009;
        const double y = 0.5 + 0.3 * qSin(i * 0.2);
        if (i % 2 == 0)
        {
            auto* hspan = new QCPItemSpan(mPlot, Qt::Horizontal);
            hspan->lowerEdge->setCoords(x, y - 0.05);
            hspan->upperEdge->setCoords(x, y + 0.05);
        }
        else
        {
            auto* vspan = new QCPItemSpan(mPlot, Qt::Vertical);
            vspan->lowerEdge->setCoords(x, y);
            vspan->upperEdge->setCoords(x + 0.005, y);
        }
    }
    mPlot->xAxis->setRange(50, 51);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 50;
    QBENCHMARK
    {
        left += 0.001; // a pan step: the width never changes, only the window slides
        mPlot->xAxis->setRange(left, left + 1);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

////////////////////////////////////////////////////////////////////
/////// Milestone 4f rows: polar pictures and the plottable list

namespace
{
// The polar area replaces the default axis rect, so every polar row starts from a cleared layout.
QCPPolarArea* benchPolarPlot(QCustomPlot* plot)
{
    plot->plotLayout()->clear();
    auto* area = new QCPPolarArea(plot);
    plot->plotLayout()->addElement(0, 0, area);
    QCPPolarAxisAngular* angular = area->angularAxis();
    angular->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    QVector<double> angle(10000), radius(10000);
    for (int i = 0; i < 10000; ++i)
    {
        angle[i] = 360.0 * i / 10000;
        radius[i] = 0.5 + 0.35 * qSin(6 * M_PI * i / 10000); // stays inside [0.15, 0.85]
    }
    auto* graph = new QCPPolarGraph(angular, area->radialAxis());
    graph->setData(angle, radius);
    return area;
}
} // namespace

void Benchmark::QCPPolar_Draw10k()
{
    benchPolarPlot(mPlot);
    mPlot->replot(); // build once, so the row measures drawing only
    QBENCHMARK
    {
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPPolar_Rotate10k()
{
    // No polar recording exists before 4h: this is a full repaint after an angular range step.
    QCPPolarAxisAngular* angular = benchPolarPlot(mPlot)->angularAxis();
    mPlot->replot();
    double lower = 0;
    QBENCHMARK
    {
        lower += 0.5; // a rotation step: the angular window slides, its width never changes
        angular->setRange(lower, lower + 360);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPPolar_RadialZoom10k()
{
    QCPPolarArea* area = benchPolarPlot(mPlot);
    QCPPolarAxisRadial* radial = area->radialAxis();
    mPlot->replot();
    double factor = 1.01;
    QBENCHMARK
    {
        radial->scaleRange(factor); // alternating, so the data stays inside the visible disc
        factor = 1.0 / factor;
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPPolar_Export10k()
{
    benchPolarPlot(mPlot);
    mPlot->replot();
    QBENCHMARK
    {
        Q_UNUSED(mPlot->toPixmap(400, 300));
    }
}

void Benchmark::QCPPlottables_Pan200()
{
    // 200 graphs of 1 000 points, all inside the window: a one-pixel pan step keeps them there.
    QVector<double> keys(1000);
    for (int i = 0; i < 1000; ++i)
        keys[i] = 1000.0 * i / 1000;
    for (int g = 0; g < 200; ++g)
    {
        QVector<double> values(1000);
        for (int i = 0; i < 1000; ++i)
            values[i] = 0.5 + 0.4 * qSin(0.01 * i + 0.03 * g);
        auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
        graph->setData(keys, values);
    }
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double left = 0;
    QBENCHMARK
    {
        left += 0.002; // about a pixel: the window slides, its width never changes
        mPlot->xAxis->setRange(left, left + 1000);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void Benchmark::QCPPlottables_Zoom200()
{
    QVector<double> keys(1000);
    for (int i = 0; i < 1000; ++i)
        keys[i] = 1000.0 * i / 1000;
    for (int g = 0; g < 200; ++g)
    {
        QVector<double> values(1000);
        for (int i = 0; i < 1000; ++i)
            values[i] = 0.5 + 0.4 * qSin(0.01 * i + 0.03 * g);
        auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
        graph->setData(keys, values);
    }
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot();
    double factor = 1.01;
    QBENCHMARK
    {
        mPlot->xAxis->scaleRange(factor); // alternating about the centre keeps every graph in view
        factor = 1.0 / factor;
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}
