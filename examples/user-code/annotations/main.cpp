// Target program (milestone 6): annotation connectors, one style per panel.
// https://matplotlib.org/stable/gallery/userdemo/connectorstyle_demo.html

#include <qcustomplot.h>

#include <QApplication>

#include <array>

using namespace QCP::Literals;

const QCP::Data point { 0.3, 0.2 }, label { 0.8, 0.6 };
const QColor blue(31, 119, 180), grey(128, 128, 128);

void showPanel(QCPAxisRect* ax)
{
    ax->line(std::array { point.x, label.x }, std::array { point.y, label.y },
             { .pen = Qt::NoPen, .scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, blue, 5) });
    ax->xAxis()->setRange(0, 1);
    ax->yAxis()->setRange(0, 1.25);
    for (const auto& axis : ax->axes())
    {
        axis->setVisible(true);
        axis->setTicks(false);
        axis->grid()->setVisible(false);
    }
}

void demoConnector(QCPAxisRect* ax, const QCPConnector& connector, const QString& name)
{
    showPanel(ax);
    ax->annotation(point, label, "",
                   { .connector = connector, .pen = QPen(grey), .head = QCPLineEnding::esLineArrow,
                     .shrink = 5_px });
    ax->text(QCP::AxisRatio { 0.05, 0.05 }, name);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(700, 630);
    plot.setAxisRectGrid(3, 5);

    const struct
    {
        int row, column;
        QCPConnector connector;
        QString name;
    } demos[] = {
        { 0, 0, QCPConnector::curve(90, 0), "curve(90, 0)" },
        { 1, 0, QCPConnector::curve(0, 90), "curve(0, 90)" },
        { 0, 1, QCPConnector::arc(0), "arc(0)" },
        { 1, 1, QCPConnector::arc(0.3), "arc(0.3)" },
        { 2, 1, QCPConnector::arc(-0.3), "arc(-0.3)" },
        { 0, 2, QCPConnector::elbow(-90, 180), "elbow(-90, 180)" },
        { 1, 2, QCPConnector::elbow(-90, 180, 5_px), "elbow(-90, 180,\n5 px)" },
        { 2, 2, QCPConnector::elbow(-90, 10, 5_px), "elbow(-90, 10,\n5 px)" },
        { 0, 3, QCPConnector::arms(-90, 30_px, 0, 30_px), "arms(-90, 30 px,\n0, 30 px)" },
        { 1, 3, QCPConnector::arms(-90, 30_px, 0, 30_px, 5_px), "arms(-90, 30 px,\n0, 30 px, 5 px)" },
        { 2, 3, QCPConnector::arms(-90, 0_px, 0, 40_px), "arms(-90, 0 px,\n0, 40 px)" },
        { 0, 4, QCPConnector::bracket(0.3), "bracket(0.3)" },
        { 1, 4, QCPConnector::bracket(-0.3), "bracket(-0.3)" },
        { 2, 4, QCPConnector::bracket(-0.2, 180), "bracket(-0.2, 180)" },
    };
    for (const auto& demo : demos)
        demoConnector(plot.axisRect(demo.row, demo.column), demo.connector, demo.name);

    // matplotlib leaves this cell empty; it shows the text and its arrow as one annotation.
    auto spare = plot.axisRect(2, 0);
    showPanel(spare);
    spare->annotation(point, label, "text and\narrow",
                      { .connector = QCPConnector::elbow(-90, 180, 5_px), .pen = QPen(grey),
                        .head = QCPLineEnding::esLineArrow, .shrink = 5_px,
                        .textStyle = { .pen = QPen(grey), .brush = Qt::white,
                                       .padding = QMarginsF(4, 2, 4, 2) } });

    plot.show();
    return app.exec();
}
