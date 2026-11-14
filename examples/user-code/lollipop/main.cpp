// Target program (milestone 6): a plot type the user writes. Does not compile yet.

#include <qcustomplot.h>

#include <QApplication>

using namespace QCP::Literals;

// A stem from zero to each value and a disc at its tip. Click picks one lollipop.
class QCPLollipop : public QCPPlottable1D<QCPGraphData>
{
public:
    using QCPPlottable1D::QCPPlottable1D;

    void draw(QCPDrawContext& context) const override
    {
        for (int i = 0; i < dataCount(); ++i)
        {
            const QCPGraphData& point = at(i);
            const QCP::Data tip { point.key, point.value };
            const bool picked = selection().contains(i);
            const QPen stem(pen().color(), picked ? 3.5 : 1.5);
            context.setDataIndex(i);
            context.line(QCP::Data { point.key, 0 }, tip, stem);
            context.circle(tip, picked ? 8_px : 5_px, stem, brush());
        }
    }

    void drawLegendIcon(QCPDrawContext& icon) const override
    {
        icon.line(QCP::AxisRatio { 0.5, 1.0 }, QCP::AxisRatio { 0.5, 0.4 }, pen());
        icon.circle(QCP::AxisRatio { 0.5, 0.35 }, 3_px, pen(), brush());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(640, 400);

    const std::vector<double> month { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    const std::vector<double> rainfall { 78, 61, 55, 52, 63, 49, 62, 53, 47, 64, 79, 76 };

    const QColor blue(31, 119, 180);
    auto ax = plot.axisRect();
    auto lollipops = ax->add<QCPLollipop>(month, rainfall)
                         .setName("rainfall (mm)")
                         .setPen(QPen(blue))
                         .setBrush(blue);
    lollipops->setSelectable(QCP::stSingleData);

    plot.setInteractions(QCP::iSelectPlottables);
    plot.legend->setVisible(true);
    ax->xAxis()->setLabel("month");
    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
