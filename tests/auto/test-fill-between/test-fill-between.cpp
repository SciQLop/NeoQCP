#include "test-fill-between.h"

#include "misuse-recorder.h"
#include "qcustomplot.h"

#include <cmath>
#include <vector>

namespace
{
const std::vector<double> kKeys = { 0.0, 1.0, 2.0, 3.0 };
const std::vector<double> kUpper = { 0.8, 0.8, 0.2, 0.2 };
const std::vector<double> kLower = { 0.0, 0.0, 1.0, 1.0 };
const QColor kGreen(0, 170, 0);
const QColor kRed(200, 0, 0);

QImage rendered(QCustomPlot& plot)
{
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

QColor pixelAt(const QImage& image, const QCPAxis& keyAxis, const QCPAxis& valueAxis, double key,
               double value)
{
    return image.pixelColor(qRound(keyAxis.coordToPixel(key)),
                            qRound(valueAxis.coordToPixel(value)));
}

bool isGreen(const QColor& colour)
{
    return colour.green() > 150 && colour.red() < 100;
}

bool isRed(const QColor& colour)
{
    return colour.red() > 150 && colour.blue() < 100;
}

QCPFillOptions greenAbove(const QCPFillOptions& options = { })
{
    QCPFillOptions filled = options;
    filled.brush = kGreen;
    return filled;
}

int childIndex(const QCPLayerable* child)
{
    if (!child || !child->layer())
        return -1;
    const auto children = child->layer()->children();
    for (int i = 0; i < children.size(); ++i)
        if (children.at(i).get() == child)
            return i;
    return -1;
}
} // namespace

void TestFillBetween::init()
{
    mPlot = new QCustomPlot;
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-0.2, 1.2);
    mFirst = mPlot->add<QCPGraph>().get();
    mFirst->setData(kKeys, kUpper);
    mSecond = mPlot->add<QCPGraph>().get();
    mSecond->setData(kKeys, kLower);
}

void TestFillBetween::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    mFirst = nullptr;
    mSecond = nullptr;
}

void TestFillBetween::aFillTakesItsBrushBySide()
{
    QCPFillOptions options = greenAbove();
    options.belowBrush = kRed;
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   options);
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the first line's side does not take the fill's brush");
    QVERIFY2(isRed(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 2.5, 0.6)),
             "the second line's side does not take the below brush");
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.9)),
             "the fill drew above both lines");
}

void TestFillBetween::oneBrushFillsBothSides()
{
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   greenAbove());
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the first line's side is empty");
    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 2.5, 0.6)),
             "the second line's side is empty without a below brush");
}

void TestFillBetween::aNoBrushSideStaysEmpty()
{
    QCPFillOptions options = greenAbove();
    options.belowBrush = QBrush(Qt::NoBrush);
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   options);
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the brush side is empty");
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 2.5, 0.6)),
             "a NoBrush side was filled anyway");
}

void TestFillBetween::aFillFollowsItsLinesData()
{
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   greenAbove());
    mPlot->replot();
    QVERIFY(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)));

    mSecond->setData(kKeys, std::vector<double> { 0.5, 0.5, 1.5, 1.5 });
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.25)),
             "the fill kept the old position of the second line");
    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.65)),
             "the fill did not follow the second line up");
}

void TestFillBetween::aFillFollowsALineThatShrinks()
{
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   greenAbove());
    mPlot->replot();
    QVERIFY(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)));

    mSecond->setData(std::vector<double> { 0.0, 3.0 }, std::vector<double> { 0.0, 0.0 });
    mPlot->replot();
    QVERIFY2(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "two points of the second line left the fill empty");

    mSecond->setData(std::vector<double> { }, std::vector<double> { });
    mPlot->replot();
    const QImage image = rendered(*mPlot);
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the fill of an empty line still drew");
}

void TestFillBetween::aGapBreaksTheFill()
{
    mFirst->setData(kKeys, std::vector<double> { 0.8, 0.8, std::nan(""), 0.2 });
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   greenAbove());
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 1.5, 0.65)),
             "the fill crossed the gap of the first line");
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 2.5, 0.65)),
             "the fill drew after the gap");
}

void TestFillBetween::aFillBetweenTwoComponents()
{
    auto* graph = mPlot->add<QCPGraph>().get();
    graph->setData(kKeys, std::vector<std::vector<double>> { kUpper, kLower });
    const QCPHandle<QCPGraph> columns(graph);

    QCPFillOptions options = greenAbove();
    options.belowBrush = kRed;
    options.firstComponent = 0;
    options.secondComponent = 1;
    mPlot->axisRect()->fillBetween(columns, columns, options);
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the first component's side is empty");
    QVERIFY2(isRed(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 2.5, 0.6)),
             "the second component's side is empty");
}

void TestFillBetween::aFillOnALogAxisClosesAtTheBound()
{
    mPlot->yAxis->setScale(QCPScale::log());
    mPlot->yAxis->setRange(0.1, 10.0);
    mSecond->setData(kKeys, std::vector<double> { 0.0, 0.0, 0.0, 0.0 });
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   greenAbove());
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.115)),
             "the fill of a zero line does not reach the log axis' lower bound");
}

void TestFillBetween::aFillDrawsUnderItsLines()
{
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    const int fillIndex = childIndex(fill.get());
    QVERIFY(fillIndex >= 0);
    QVERIFY2(fillIndex < childIndex(mFirst), "the fill draws above the first line");
    QVERIFY2(fillIndex < childIndex(mSecond), "the fill draws above the second line");
}

void TestFillBetween::aFillWhoseLineIsDeletedDrawsNothing()
{
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    // QCPGraph::dataChanged() is a virtual hook, not a signal: a selection change is the signal a
    // user's slot hangs from, and deleting the other line there is the same hazard.
    QObject::connect(mFirst, &QCPPlottable::selectionChanged, mPlot,
                     [this]
                     {
                         mPlot->removePlottable(mSecond);
                         mSecond = nullptr;
                     });
    mFirst->setSelection(QCPDataSelection(QCPDataRange(0, 4)));
    QVERIFY(!mSecond);
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    QVERIFY2(fill, "the fill went away with its line");
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the fill still drew after its line was deleted");
}

void TestFillBetween::linesOfTwoRectsAreRefused()
{
    QCustomPlot other;
    other.resize(400, 300);
    other.xAxis->setRange(0, 3);
    auto* foreign = other.add<QCPGraph>().get();
    foreign->setData(kKeys, kLower);

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(foreign), greenAbove());
    qcp::setMisuseHandler(previous);

    QVERIFY2(!fill, "a fill between two rects was made");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
}

void TestFillBetween::aFillIsNotSelectableByDefault()
{
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    mPlot->replot();

    QCOMPARE(fill->selectable(), QCP::stNone);
    const QPointF inside(mPlot->xAxis->coordToPixel(0.5), mPlot->yAxis->coordToPixel(0.4));
    QVERIFY2(fill->selectTest(inside, /*onlySelectable=*/true) < 0,
             "a click inside the fill selects it");
    QVERIFY2(!mPlot->plottableAt(inside, /*onlySelectable=*/true),
             "the fill is hit as a selectable plottable");
}
