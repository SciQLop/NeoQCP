#include "test-fill-between.h"

#include "misuse-recorder.h"
#include "qcustomplot.h"

#include <painting/draw-commands.h>

#include <algorithm>
#include <cmath>
#include <string>
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
                         (void) mPlot->removePlottable(mSecond);
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

void TestFillBetween::aFillDrawsAgainWhenSetLinesNamesLiveLines()
{
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    (void) mPlot->removePlottable(mSecond);
    mSecond = nullptr;
    mPlot->replot();
    QVERIFY2(!isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the fill drew with a deleted line");

    auto* fresh = mPlot->add<QCPGraph>().get();
    fresh->setData(kKeys, std::vector<double> { 0.2, 0.2, 0.2, 0.2 });
    fill->setLines(mFirst, 0, fresh, 0);
    mPlot->replot();
    QVERIFY2(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.5)),
             "the fill did not draw again for the live lines it was given");
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

void TestFillBetween::aFillOfLinesOnAnotherKeyAxisIsReported()
{
    auto* onTheTopAxis = mPlot->add<QCPGraph>().get();
    onTheTopAxis->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setRange(0, 3);
    onTheTopAxis->setData(kKeys, kLower);

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(onTheTopAxis), greenAbove());
    qcp::setMisuseHandler(previous);

    QVERIFY2(!fill, "a fill between two key axes was made");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    // Two key axes are two systems: the two lines do not fill to each other.
    QVERIFY2(test_misuse::calls.front().what.find("fill to each other") != std::string::npos,
             test_misuse::calls.front().what.c_str());
}

void TestFillBetween::aFillOfALineWithItselfIsReported()
{
    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mFirst), greenAbove());
    qcp::setMisuseHandler(previous);

    QVERIFY2(!fill, "a fill of a line with itself, both components default, was made");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
}

void TestFillBetween::aFillOfAMissingComponentIsReported()
{
    QCPFillOptions options = greenAbove();
    options.secondComponent = 4;

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), options);
    qcp::setMisuseHandler(previous);

    QVERIFY2(!fill, "a fill of a component the line does not have was made");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
}

void TestFillBetween::aFillAcrossTwoValueAxesFollowsTheSecondAxis()
{
    mPlot->yAxis->setRange(0, 1);
    mPlot->yAxis2->setRange(0, 1);
    mSecond->setValueAxis(mPlot->yAxis2);
    mSecond->setData(kKeys, std::vector<double> { 1.0, 1.0, 1.0, 1.0 });
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    mPlot->replot();
    QVERIFY2(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.85)),
             "the band does not reach the second line on y2");

    mPlot->replot();
    const int recorded = fill->mCore.recordingCount();
    mPlot->replot();
    QCOMPARE(fill->mCore.recordingCount(), recorded);

    // The band is the second line's pixels on the first line's value axis: it moves with both.
    mPlot->yAxis2->setRange(0, 2);
    mPlot->replot();
    QVERIFY2(fill->mCore.recordingCount() > recorded, "moving y2 kept the band's recording");

    const QImage image = rendered(*mPlot);
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.85)),
             "the band kept the old range of y2");
    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.6)),
             "the band did not follow the new range of y2");
}

void TestFillBetween::aDenseFillIsRebinnedAfterAZoom()
{
    const int count = 10000;
    std::vector<double> keys;
    std::vector<double> upper(std::size_t(count), 0.0);
    std::vector<double> lower(std::size_t(count), 0.0);
    keys.reserve(std::size_t(count));
    for (int i = 0; i < count; ++i)
        keys.push_back(3.0 * i / (count - 1));
    upper[std::size_t(std::lower_bound(keys.begin(), keys.end(), 2.4) - keys.begin())] = 1.0;
    mFirst->setData(keys, upper);
    mSecond->setData(keys, lower);
    mPlot->yAxis->setRange(-0.1, 1.1);
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    mPlot->replot();

    mPlot->replot();
    const int recorded = fill->mCore.recordingCount();
    mPlot->replot();
    QCOMPARE(fill->mCore.recordingCount(), recorded);

    // The bins are pixels of the frame that made them: another frame must bin again.
    mPlot->xAxis->setRange(2.39, 2.41);
    mPlot->replot();
    QVERIFY2(fill->mCore.recordingCount() > recorded,
             "a binned fill kept its recording for another zoom");

    const QImage image = rendered(*mPlot);
    const int row = qRound(mPlot->yAxis->coordToPixel(0.5));
    int filled = 0;
    for (int x = 0; x < image.width(); ++x)
        if (isGreen(image.pixelColor(x, row)))
            ++filled;
    QVERIFY2(filled < 15,
             qPrintable(QStringLiteral("%1 pixels at value 0.5: the fill was not "
                                       "rebinned for the new zoom")
                            .arg(filled)));
}

void TestFillBetween::aZoomedFillRecordsOnlyItsWindow()
{
    // One band per row: a 1% view records the rows around it, not the whole data set. Four
    // times the rows over four times the span is the same density, so the same window holds
    // the same rows and the count must not follow the data.
    const auto recordedPoints = [this](int count, double from, double to, double view)
    {
        std::vector<double> keys;
        std::vector<double> upper(std::size_t(count), 1.0);
        std::vector<double> lower(std::size_t(count), 0.0);
        keys.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i)
            keys.push_back(from + (to - from) * i / (count - 1));
        mFirst->setData(keys, upper);
        mSecond->setData(keys, lower);
        mPlot->yAxis->setRange(-0.1, 1.1);
        mPlot->xAxis->setRange(view, view + 1.0);
        const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
            QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
        mPlot->replot();
        int points = 0;
        for (const qcp::QCPDrawCommand& command : fill->mCore.recordedList().commands)
            if (const auto* band = std::get_if<qcp::FillBetweenCmd>(&command))
                points += int(band->upper->size());
        return points;
    };

    const int small = recordedPoints(10000, 0.0, 100.0, 50.0);
    QVERIFY2(small > 20, qPrintable(QStringLiteral("%1 points: the band is gone").arg(small)));
    QVERIFY2(small < 1000,
             qPrintable(QStringLiteral("%1 points for a 1%% window of 10k rows: the fill read "
                                       "its whole data set")
                            .arg(small)));
    const int large = recordedPoints(40000, -150.0, 250.0, 50.0);
    QVERIFY2(large <= 2 * small,
             qPrintable(QStringLiteral("%1 points for four times the rows, %2 for one: the "
                                       "fill reads its whole data set")
                            .arg(large)
                            .arg(small)));
}

void TestFillBetween::aSteppedFillSwapsColourWhereTheLinesSwap()
{
    mFirst->setLineStyle(QCPGraph::lsStepLeft);
    QCPFillOptions options = greenAbove();
    options.belowBrush = kRed;
    mPlot->axisRect()->fillBetween(QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond),
                                   options);
    mPlot->replot();
    const QImage image = rendered(*mPlot);

    // Stepped, the first line holds 0.8 up to key 2, so at key 1.6 it is still above the second
    // line, which is 0.6 there: the colours swap where the drawn lines swap, not at the crossing
    // of the straight segments, key 1.5.
    QVERIFY2(isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 1.6, 0.7)),
             "the fill cut at the linear crossing, not where the stepped lines swap");
    QVERIFY2(!isRed(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 1.6, 0.7)),
             "the below brush fills the side the stepped first line is above on");
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

// The legend row of a fill is a swatch of its brushes: the upper half where the first line is above,
// the lower half where it is below. The row must not replay the band into a 24 px icon.
// Each side is one band whatever the lines do: a thousand crossings still record one command per
// brush, and a shared brush records one command, crossings and all.
void TestFillBetween::aFillThatCrossesOftenRecordsOneBandPerSide()
{
    const int count = 1001;
    std::vector<double> keys;
    std::vector<double> upper;
    std::vector<double> lower;
    keys.reserve(std::size_t(count));
    upper.reserve(std::size_t(count));
    lower.reserve(std::size_t(count));
    for (int i = 0; i < count; ++i)
    {
        const bool firstAbove = i % 2 == 0;
        keys.push_back(3.0 * i / (count - 1));
        upper.push_back(firstAbove ? 0.8 : 0.2);
        lower.push_back(firstAbove ? 0.2 : 0.8);
    }
    mFirst->setData(keys, upper);
    mSecond->setData(keys, lower);

    const auto brushesOf = [](const QCPFillBetween& fill)
    {
        std::vector<QColor> brushes;
        for (const qcp::QCPDrawCommand& command : fill.mCore.recordedList().commands)
            if (const auto* band = std::get_if<qcp::FillBetweenCmd>(&command))
                brushes.push_back(band->brush.color());
        return brushes;
    };

    QCPFillOptions twoBrushes = greenAbove();
    twoBrushes.belowBrush = kRed;
    const QCPHandle<QCPFillBetween> split = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), twoBrushes);
    QVERIFY(split);
    mPlot->replot();
    QCOMPARE(split->mCore.recordedCommandCount(), 2);
    const std::vector<QColor> brushes = brushesOf(*split);
    QVERIFY2(std::find(brushes.begin(), brushes.end(), kGreen) != brushes.end(),
             "the above side is not one green band");
    QVERIFY2(std::find(brushes.begin(), brushes.end(), kRed) != brushes.end(),
             "the below side is not one red band");

    mPlot->removePlottable(split.get());
    const QCPHandle<QCPFillBetween> oneSide = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(oneSide);
    mPlot->replot();
    QCOMPARE(oneSide->mCore.recordedCommandCount(), 1);
}

void TestFillBetween::aFillsLegendIconShowsItsBrushes()
{
    QCPFillOptions options = greenAbove();
    options.belowBrush = kRed;
    const auto fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), options);
    QVERIFY(fill);
    const auto oneSided = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(oneSided);

    const auto iconOf = [](const QCPHandle<QCPFillBetween>& plottable)
    {
        QImage icon(40, 20, QImage::Format_ARGB32);
        icon.fill(Qt::white);
        QCPPainter painter(&icon);
        plottable->drawLegendIcon(&painter, QRectF(0, 0, 40, 20));
        painter.end();
        return icon;
    };

    const QImage split = iconOf(fill);
    QVERIFY2(isGreen(split.pixelColor(20, 6)), "the icon's upper half is not the fill's brush");
    QVERIFY2(isRed(split.pixelColor(20, 14)), "the icon's lower half is not the below brush");

    const QImage plain = iconOf(oneSided);
    QVERIFY2(isGreen(plain.pixelColor(20, 6)), "the icon's upper half is not the fill's brush");
    QVERIFY2(isGreen(plain.pixelColor(20, 14)), "one brush fills both halves of the icon");
}

// A cartesian fill is a pixel recording: panning inside the window it recorded must reuse it, as the
// graph's own fills did, so the GPU can shift the band instead of rebuilding it.
void TestFillBetween::aCartesianFillIsNotReRecordedByAPan()
{
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(mFirst), QCPHandle<QCPGraph>(mSecond), greenAbove());
    QVERIFY(fill);
    mPlot->replot();
    mPlot->replot();
    const int recorded = fill->mCore.recordingCount();
    mPlot->replot();
    QCOMPARE(fill->mCore.recordingCount(), recorded);

    mPlot->xAxis->setRange(0.2, 3.2); // a pan, the same span, inside the recorded window
    mPlot->replot();
    QCOMPARE(fill->mCore.recordingCount(), recorded);

    // The band is cropped to the view and binned to its pixels: another scale is another recording.
    mPlot->xAxis->setRange(0.2, 2.6);
    mPlot->replot();
    QVERIFY2(fill->mCore.recordingCount() > recorded, "a zoom kept the band's recording");
}

// Before the first data neither line has a column to fill to: the call is kept, as the graph's fills
// were, and the band appears with the data.
void TestFillBetween::aFillMadeBeforeTheDataDrawsWhenItComes()
{
    auto* first = mPlot->add<QCPGraph>().get();
    auto* second = mPlot->add<QCPGraph>().get();
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(first), QCPHandle<QCPGraph>(second), greenAbove());
    QVERIFY2(fill, "a fill made before the data was refused");
    mPlot->replot();
    QVERIFY2(!isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "a band is drawn before the lines have data");

    first->setData(kKeys, kUpper);
    second->setData(kKeys, kLower);
    mPlot->replot();
    QVERIFY2(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the band did not come with the data");
}

// A column the fill names can disappear under it: the fill draws nothing, and says why once per
// draw, as a component that never named a column does.
void TestFillBetween::aFillOfAColumnThatDisappearsDrawsNothing()
{
    auto* graph = mPlot->add<QCPGraph>().get();
    graph->setData(kKeys, std::vector<std::vector<double>> { kUpper, kLower });
    const QCPHandle<QCPFillBetween> fill = mPlot->axisRect()->fillBetween(
        QCPHandle<QCPGraph>(graph), QCPHandle<QCPGraph>(graph),
        { .brush = kGreen, .firstComponent = 0, .secondComponent = 1 });
    QVERIFY(fill);
    mPlot->replot();
    QVERIFY2(isGreen(pixelAt(rendered(*mPlot), *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "the band between the two columns is not drawn");

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    graph->setData(kKeys, std::vector<std::vector<double>> { kUpper }); // the second column goes
    mPlot->replot();
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    const QImage image = rendered(*mPlot);
    qcp::setMisuseHandler(previous);
    QVERIFY2(!isGreen(pixelAt(image, *mPlot->xAxis, *mPlot->yAxis, 0.5, 0.4)),
             "a column that disappeared still draws a band");
}
