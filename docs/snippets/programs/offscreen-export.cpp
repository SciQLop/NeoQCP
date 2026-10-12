#include <QApplication>
#include <qcustomplot.h>

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");   // or set it in the environment
    QApplication app(argc, argv);
    QCustomPlot customPlot;
    customPlot.addGraph()->setData({ 0, 1, 2, 3 }, { 1, 0, 2, 1 });
    customPlot.rescaleAxes();
    return customPlot.savePng("plot.png", 600, 360, 2.0) ? 0 : 1;
}
