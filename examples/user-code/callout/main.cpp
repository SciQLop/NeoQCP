// Target program (milestone 6): a callout annotation, dragged and resized.

#include <qcustomplot.h>
#include <QApplication>

#include <cmath>
#include <numbers>
#include <vector>

using namespace QCP::Literals;

// A rounded text box in pixels from a data point, with a pointer whose tip sits on the point.
class Callout : public QCPItem
{
public:
    Callout(QCP::Data tip, QString text) : mTip(tip), mText(std::move(text)) { }

    QCPItemAnchor* const tip = createAnchor("tip", [this] { return mTip; });
    QCPItemAnchor* const box = createAnchor("box", [this] { return mTip + offset(mBox.center()); });

protected:
    enum Part { Pointer, Box, Corner };

    void draw(QCPDrawContext& context) const override
    {
        const auto topLeft = mTip + offset(mBox.topLeft());
        const auto bottomRight = mTip + offset(mBox.bottomRight());
        const QPen outline(QColor(60, 60, 60), 1.2);
        const QColor paper(255, 250, 225);

        context.setPart(Pointer);
        context.polygon({ mTip, mTip + offset(mBox.center() + QPointF(-12, 0)),
                          mTip + offset(mBox.center() + QPointF(12, 0)) },
                        outline, paper);
        context.setPart(Box);
        context.roundedRect(topLeft, bottomRight, 6_px, outline, paper);
        context.text(topLeft, bottomRight, mText, { .padding = QMarginsF(8, 6, 8, 6) });
        context.setPart(Corner);
        context.rect(bottomRight + QCP::PixelOffset { -8, -8 }, bottomRight, Qt::NoPen,
                     QColor(60, 60, 60));
    }

    void mousePressEvent(QCPMouseEvent* event) override
    {
        if (event->part() == Pointer)
            event->ignore(); // a press on the pointer pans the plot
    }

    void mouseMoveEvent(QCPMouseEvent* event) override
    {
        const QCP::PixelOffset moved = event->pixels() - event->lastPixels();
        const QPointF delta(moved.dx, moved.dy);
        if (event->part() == Box)
            mBox.translate(delta);
        else
            mBox.setBottomRight(mBox.bottomRight() + delta);
        mBox.setSize(mBox.size().expandedTo(QSizeF(80, 36)));
        update();
    }

private:
    static QCP::PixelOffset offset(QPointF p) { return { p.x(), p.y() }; }

    QCP::Data mTip;
    QString mText;
    QRectF mBox { 30, -110, 170, 60 }; // pixels from the tip; y grows downwards
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(720, 460);

    std::vector<double> t, signal;
    for (int i = 0; i <= 400; ++i)
    {
        t.push_back(i / 40.0);
        signal.push_back(std::sin(t.back()) * std::exp(-t.back() / 8));
    }
    auto ax = plot.axisRect();
    ax->line(t, signal, { .pen = QPen(QColor(31, 119, 180), 2), .name = "damped sine" });

    const double firstMax = std::numbers::pi / 2;
    auto peak = ax->add<Callout>(QCP::Data { firstMax, std::exp(-firstMax / 8) },
                                 "First maximum.\nDrag the box; resize it from its corner.");

    ax->ellipse(peak->tip + QCP::PixelOffset { -7, -7 }, peak->tip + QCP::PixelOffset { 7, 7 },
                { .pen = QPen(QColor(214, 39, 40), 2) });
    ax->text(peak->box + QCP::PixelOffset { 0, 50 }, "hangs below the box",
             { .alignment = Qt::AlignHCenter | Qt::AlignTop });

    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
