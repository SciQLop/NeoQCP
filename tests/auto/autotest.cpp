#include "test-alignment-group/test-alignment-group.h"
#include "test-axis-ticker-log/test-axis-ticker-log.h"
#include "test-builder/test-builder.h"
#include "test-busy-indicator/test-busy-indicator.h"
#include "test-color-by-scalar/test-color-by-scalar.h"
#include "test-colormap/test-colormap.h"
#include "test-colorscale/test-colorscale.h"
#include "test-coords/test-coords.h"
#include "test-creation-mode/test-creation-mode.h"
#include "test-data-locator/test-data-locator.h"
#include "test-data-swap/test-data-swap.h"
#include "test-datacontainer/test-datacontainer.h"
#include "test-datasource/test-datasource.h"
#include "test-datasource2d/test-datasource2d.h"
#include "test-draw-context/test-draw-context.h"
#include "test-drawable/test-drawable.h"
#include "test-errorbar-geometry/test-errorbar-geometry.h"
#include "test-fill-geometry/test-fill-geometry.h"
#include "test-gestures/test-gestures.h"
#include "test-gradient-presets/test-gradient-presets.h"
#include "test-grid-rhi/test-grid-rhi.h"
#include "test-handle/test-handle.h"
#include "test-hidden-replot/test-hidden-replot.h"
#include "test-histogram1d/test-histogram-plottable.h"
#include "test-histogram1d/test-histogram1d.h"
#include "test-hit-rule/test-hit-rule.h"
#include "test-intervals/test-intervals.h"
#include "test-item-index/test-item-index.h"
#include "test-item-position/test-item-position.h"
#include "test-item-recording/test-item-recording.h"
#include "test-layer-removal/test-layer-removal.h"
#include "test-line-extruder/test-line-extruder.h"
#include "test-linked-axes/test-linked-axes.h"
#include "test-misuse/test-misuse.h"
#include "test-multi-datasource/test-multi-datasource.h"
#include "test-multigraph/test-multigraph.h"
#include "test-overlay/test-overlay.h"
#include "test-ownership/test-ownership.h"
#include "test-paintbuffer/test-paintbuffer.h"
#include "test-pipeline/test-pipeline.h"
#include "test-plottable-group/test-plottable-group.h"
#include "test-polar/test-polar.h"
#include "test-qcpaxisrect/test-qcpaxisrect.h"
#include "test-qcpbars/test-qcpbars.h"
#include "test-qcpcurve/test-qcpcurve.h"
#include "test-qcpfinancial/test-qcpfinancial.h"
#include "test-qcpgraph/test-qcpgraph.h"
#include "test-qcplayout/test-qcplayout.h"
#include "test-qcplegend/test-qcplegend.h"
#include "test-qcustomplot/test-qcustomplot.h"
#include "test-reference-m4e/test-reference-m4e.h"
#include "test-repaint/test-repaint.h"
#include "test-richtext/test-richtext.h"
#include "test-rspan/test-rspan.h"
#include "test-scatter-rhi/test-scatter-rhi.h"
#include "test-selection-model/test-selection-model.h"
#include "test-signal-delivery/test-signal-delivery.h"
#include "test-span-rhi/test-span-rhi.h"
#include "test-span/test-span.h"
#include "test-spatial-index/test-spatial-index.h"
#include "test-statisticalbox/test-statisticalbox.h"
#include "test-theme/test-theme.h"
#include "test-tick-label-cache/test-tick-label-cache.h"
#include "test-waterfall/test-waterfall.h"

static bool shouldRun(const char* name)
{
    const QByteArray only = qgetenv("QCP_TEST_CLASS");
    return only.isEmpty() || only == name;
}

// QCP_TEST_LOG_DIR also writes each class's results to <dir>/<class>.txt: QtTest output never
// shows on GitHub's Windows runners, so CI prints these files instead.
static QStringList testArguments(const char* name)
{
    QStringList args { QStringLiteral("auto-tests") };
    if (!qEnvironmentVariable("QCP_TEST_FUNCTION").isEmpty())
        args << qEnvironmentVariable("QCP_TEST_FUNCTION");
    const QString dir = qEnvironmentVariable("QCP_TEST_LOG_DIR");
    if (!dir.isEmpty())
        args << QStringLiteral("-o")
             << QStringLiteral("%1/%2.txt,txt").arg(dir, QLatin1String(name))
             << QStringLiteral("-o") << QStringLiteral("-,txt");
    return args;
}

#define QCPTEST(t)                                                                                 \
    if (shouldRun(#t))                                                                             \
    {                                                                                              \
        t t##instance;                                                                             \
        failures += QTest::qExec(&t##instance, testArguments(#t));                                 \
    }

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (qEnvironmentVariableIsEmpty("QCP_TEST_CLASS")
        && !qEnvironmentVariableIsEmpty("QCP_TEST_FUNCTION"))
    {
        qCritical(
            "QCP_TEST_FUNCTION needs QCP_TEST_CLASS: name the class the function belongs to.");
        return 2;
    }
    int failures = 0;

    QCPTEST(TestQCustomPlot);
    QCPTEST(TestQCPGraph);
    QCPTEST(TestQCPCurve);
    QCPTEST(TestQCPBars);
    QCPTEST(TestQCPFinancial);
    QCPTEST(TestColorMap);
    QCPTEST(TestColorScale);
    QCPTEST(TestQCPLayout);
    QCPTEST(TestQCPLegend);
    QCPTEST(TestQCPAxisRect);
    QCPTEST(TestTickLabelCache);
    QCPTEST(TestDatacontainer);
    QCPTEST(TestLineExtruder);
    QCPTEST(TestDataSource);
    QCPTEST(TestDataSource2D);
    QCPTEST(TestTheme);
    QCPTEST(TestPaintBuffer);
    QCPTEST(TestMultiDataSource);
    QCPTEST(TestMultiGraph);
    QCPTEST(TestFillGeometry);
    QCPTEST(TestHistogram1D);
    QCPTEST(TestHistogramPlottable);
    QCPTEST(TestErrorBarGeometry);
    QCPTEST(TestDataLocator);
    QCPTEST(TestRSpan);
    QCPTEST(TestSpan);
    QCPTEST(TestItemRecording);
    QCPTEST(TestStatisticalBox);
    QCPTEST(TestWaterfall);
    QCPTEST(TestPipeline);
    QCPTEST(TestRichText);
    QCPTEST(TestOverlay);
    QCPTEST(TestGestures);
    QCPTEST(TestBusyIndicator);
    QCPTEST(TestDataSwap);
    QCPTEST(TestCreationMode);
    QCPTEST(TestGridRhi);
    QCPTEST(TestScatterRhi);
    QCPTEST(TestSpanRhi);
    QCPTEST(TestItemPosition);
    QCPTEST(TestLayerRemoval);
    QCPTEST(TestHiddenReplot);
    QCPTEST(TestColorByScalar);
    QCPTEST(TestGradientPresets);
    QCPTEST(TestAxisTickerLog);
    QCPTEST(TestIntervals);
    QCPTEST(TestPlottableGroup);
    QCPTEST(TestMisuse);
    QCPTEST(TestHandle);
    QCPTEST(TestLinkedAxes);
    QCPTEST(TestReferenceM4e);
    QCPTEST(TestAlignmentGroup);
    QCPTEST(TestOwnership);
    QCPTEST(TestSignalDelivery);
    QCPTEST(TestRepaint);
    QCPTEST(TestBuilder);
    QCPTEST(TestSpatialIndex);
    QCPTEST(TestItemIndex);
    QCPTEST(TestCoords);
    QCPTEST(TestDrawContext);
    QCPTEST(TestDrawable);
    QCPTEST(TestHitRule);
    QCPTEST(TestSelectionModel);
    QCPTEST(TestPolar);

    return failures == 0 ? 0 : 1;
}
