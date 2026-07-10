#include <QtTest/QtTest>
#include <QtWidgets/qtestsupport_widgets.h>
#include "qcustomplot.h"
#include "painting/paintbuffer-pixmap.h"

class TestPaintBuffer : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void contentDirty_newBufferIsDirty();
    void contentDirty_resetAfterReplot();
    void contentDirty_setOnResize();
    void contentDirty_setOnInvalidation();
    void contentDirty_markDirtyLayer();
    void contentDirty_fallbackMarksAllDirty();
    void contentDirty_incrementalReplotSkipsCleanBuffers();
    void contentDirty_incrementalReplotPreservesContent();
    void replotAndExport_smokeTest();
    void replotOnFirstShow_tabWidget();

    void skipRepaint_graph2PanOnly();
    void skipRepaint_disabledWithItems();
    void skipRepaint_disabledWithLegacyGraph();
    void skipRepaint_disabledOnInvalidation();
    void skipRepaint_bufferNotReuploadedOnPan();

    void setVisible_dirtiesLayerBuffer();
    void setVisible_noOpWhenUnchanged();

    void colormap2_panDirtiesLayerBuffer();
    void colormap2_panDirtiesLayerBufferLogY();
    void colormap2_stallOffsetOnPan();
    void colormap2_stallOffsetOnLogYPan();
    void colormap2_stallOffsetNullOnZoom();
    void colormap2_drawSkipsNonOverlappingStaleResample();

private:
    QCustomPlot* mPlot;
};
