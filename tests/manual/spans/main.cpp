#include <QApplication>
#include "../../../src/qcp.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QCustomPlot plot;
    plot.resize(800, 600);
    plot.xAxis->setRange(0, 100);
    plot.yAxis->setRange(0, 100);
    plot.setInteractions(QCP::iSelectItems | QCP::iRangeDrag | QCP::iRangeZoom);

    // vertical span
    auto* vspan = new QCPItemSpan(&plot, Qt::Vertical);
    vspan->setRange(QCPRange(20, 40));
    vspan->setBrush(QBrush(QColor(0, 100, 255, 60)));
    vspan->setBorderPen(QPen(Qt::blue, 2));

    // horizontal span
    auto* hspan = new QCPItemSpan(&plot, Qt::Horizontal);
    hspan->setRange(QCPRange(30, 60));
    hspan->setBrush(QBrush(QColor(255, 100, 0, 60)));
    hspan->setBorderPen(QPen(Qt::red, 2));

    // rectangle span
    auto* rspan = new QCPItemRSpan(&plot);
    rspan->setKeyRange(QCPRange(60, 80));
    rspan->setValueRange(QCPRange(20, 80));
    rspan->setBrush(QBrush(QColor(0, 200, 0, 60)));
    rspan->setBorderPen(QPen(Qt::green, 2));

    // connect delete
    QObject::connect(vspan, &QCPItemSpan::deleteRequested, [&]() {
        plot.removeItem(vspan);
        plot.replot();
    });

    plot.setWindowTitle("Span Items Manual Test");
    plot.show();

    return app.exec();
}
