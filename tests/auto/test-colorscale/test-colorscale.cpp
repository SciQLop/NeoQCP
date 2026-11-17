#include "test-colorscale.h"

namespace
{
// A 2x2 map whose values span [zMin, zMax].
QCPColorMap* mapWithRange(QCPAxis* keyAxis, QCPAxis* valueAxis, double zMin, double zMax)
{
    auto* map = keyAxis->axisRect()->add<QCPColorMap>(keyAxis, valueAxis).get();
    map->data()->setSize(2, 2);
    map->data()->setCell(0, 0, zMin);
    map->data()->setCell(1, 0, zMin);
    map->data()->setCell(0, 1, zMax);
    map->data()->setCell(1, 1, zMax);
    map->data()->recalculateDataBounds();
    return map;
}

void settle(QCustomPlot* plot, QCPColorMap2* map)
{
    plot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(!map->pipeline().isBusy(), 2000);
    QTest::qWait(50);
    plot->replot();
}
} // namespace

void TestColorScale::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(400, 300);
}

void TestColorScale::cleanup()
{
    delete mPlot;
}

void TestColorScale::aRectMakesItsScaleOnFirstUse()
{
    QCOMPARE(mPlot->findChildren<QCPColorScale*>().size(), 0);

    const auto scale = mPlot->axisRect()->colorScale();
    QVERIFY(scale);
    QCOMPARE(mPlot->findChildren<QCPColorScale*>().size(), 1);
    // The same handle on every call: the rect keeps its one scale.
    QCOMPARE(mPlot->axisRect()->colorScale().get(), scale.get());
}

void TestColorScale::scaleIsHiddenUntilUsed()
{
    const auto scale = mPlot->axisRect()->colorScale();
    mPlot->replot();
    QVERIFY2(!scale->visible(), "a scale nothing uses is hidden");

    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();
    QVERIFY2(scale->visible(), "its first user shows it");
    QCOMPARE(map->colorScale(), scale.get());
}

void TestColorScale::mapsOnOneRectShareItsScale()
{
    auto* map1 = mapWithRange(mPlot->xAxis, mPlot->yAxis, 1, 4);
    auto* map2 = mapWithRange(mPlot->xAxis, mPlot->yAxis, 6, 9);
    const auto scale = mPlot->axisRect()->colorScale();
    QCOMPARE(map1->colorScale(), scale.get());
    QCOMPARE(map2->colorScale(), scale.get());

    scale->rescaleDataRange(true);
    QCOMPARE(scale->dataRange().lower, 1.0);
    QCOMPARE(scale->dataRange().upper, 9.0);
    QCOMPARE(map1->dataRange().upper, 9.0);
    QCOMPARE(map2->dataRange().upper, 9.0);
}

void TestColorScale::scaleSitsOutsideTheRightAxis()
{
    mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();

    QCPAxisRect* rect = mPlot->axisRect();
    const auto scale = rect->colorScale();
    QCOMPARE(scale->type(), QCPAxis::atRight);
    QVERIFY2(scale->outerRect().left() > rect->rect().right(),
             qPrintable(QString("scale starts at %1, the inner rect ends at %2")
                            .arg(scale->outerRect().left())
                            .arg(rect->rect().right())));
    QCOMPARE(scale->outerRect().right(), rect->outerRect().right());
    QVERIFY(scale->outerRect().right() <= mPlot->viewport().right());
    QVERIFY(scale->outerRect().height() > 0);
    // It is placed by the rect, not by the plot's layout grid.
    QCOMPARE(mPlot->plotLayout()->elementCount(), 1);
}

void TestColorScale::rectShrinksForItsScale()
{
    QCPAxisRect* rect = mPlot->axisRect();
    rect->setMinimumMargins(QMargins(0, 0, 0, 0));
    mPlot->replot();
    const int wide = rect->rect().width();

    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();
    const auto scale = rect->colorScale();
    const int narrow = rect->rect().width();
    const int scaleWidth = scale->outerRect().width();
    QVERIFY2(qAbs(wide - narrow - scaleWidth) <= 2,
             qPrintable(QString("rect %1 -> %2, scale %3").arg(wide).arg(narrow).arg(scaleWidth)));

    map->setColorScale(nullptr);
    mPlot->replot();
    QVERIFY2(qAbs(rect->rect().width() - wide) <= 2,
             "the width comes back when the scale is unused");
}

void TestColorScale::typeTopPutsTheScaleAbove()
{
    mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();

    QCPAxisRect* rect = mPlot->axisRect();
    const auto scale = rect->colorScale();
    scale->setType(QCPAxis::atTop);
    mPlot->replot();

    QVERIFY2(scale->outerRect().bottom() <= rect->rect().top(),
             qPrintable(QString("scale bottom %1, rect top %2")
                            .arg(scale->outerRect().bottom())
                            .arg(rect->rect().top())));
    QVERIFY(scale->outerRect().top() >= mPlot->viewport().top());
    QVERIFY(scale->outerRect().width() > 0);
}

void TestColorScale::optingOutHidesTheScale()
{
    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();
    const auto scale = mPlot->axisRect()->colorScale();
    QVERIFY(scale->visible());

    map->setColorScale(nullptr);
    mPlot->replot();
    QVERIFY2(!scale->visible(), "no user left, so the rect hides the scale");
}

void TestColorScale::aUserMadeScaleStillWorks()
{
    const auto own = mPlot->axisRect()->colorScale();
    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);

    // The 2.x program: a scale in the layout, handed to the map.
    auto* userScale = mPlot->add<QCPColorScale>(0, 1).get();
    QVERIFY(userScale);
    map->setColorScale(userScale);
    map->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    mPlot->replot();

    QCOMPARE(map->colorScale(), userScale);
    QCOMPARE(userScale->gradient(), QCPColorGradient(QCPColorGradient::gpHot));
    QVERIFY2(!own->visible(), "the rect's own scale has no user left");
    QVERIFY(userScale->visible());
    QVERIFY(userScale->outerRect().width() > 0);
}

void TestColorScale::aScalesInternalRectMakesNoScale()
{
    auto* scale = mPlot->add<QCPColorScale>(0, 1).get();
    QVERIFY(scale);
    QCPAxisRect* inner = scale->axis()->axisRect();
    QVERIFY(inner);
    QVERIFY2(!inner->colorScale(), "a scale's internal rect gets no scale of its own");
    QCOMPARE(mPlot->findChildren<QCPColorScale*>().size(), 1);
}

void TestColorScale::aSecondMapTakesTheRectsGradient()
{
    mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    const auto scale = mPlot->axisRect()->colorScale();
    scale->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
    scale->setDataRange(QCPRange(0, 1));

    mPlot->xAxis->setRange(0, 1);
    mPlot->yAxis->setRange(0, 1);
    auto* map = mPlot->add<QCPColorMap2>().get();
    QCOMPARE(map->gradient(), scale->gradient());
    QCOMPARE(map->dataRange(), scale->dataRange());
    map->setData(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 },
                 std::vector<double> { 0, 0, 1, 1 });
    settle(mPlot, map);

    const QImage img = mPlot->toPixmap().toImage();
    const QColor c = img.pixelColor(qRound(mPlot->xAxis->coordToPixel(0.9)),
                                    qRound(mPlot->yAxis->coordToPixel(0.5)));
    QVERIFY2(qAbs(c.red() - c.green()) <= 2 && qAbs(c.green() - c.blue()) <= 2,
             qPrintable(QString("the renderer kept its own gradient: pixel is (%1,%2,%3)")
                            .arg(c.red())
                            .arg(c.green())
                            .arg(c.blue())));
}

void TestColorScale::aBarScaleChangeInstallsItsTickerOnce()
{
    const auto scale = mPlot->add<QCPColorScale>(0, 1);
    QVERIFY(scale);
    QCPAxis* bar = scale->axis();
    QSignalSpy installs(bar, &QCPAxis::tickerChanged);

    // The bar takes a scale of its own and the colour scale follows it: two paths to one setScale,
    // which must install the scale's ticker once, not twice.
    bar->setScale(QCPScale::log());
    QVERIFY2(scale->dataScale().isLog(), "the colour scale did not follow the bar's scale");
    QVERIFY(dynamic_cast<QCPAxisTickerLog*>(bar->ticker().data()) != nullptr);
    // Two installs: the axis' own setScale, and the rect's opposite axis, which follows through
    // signal synchronization. The colour scale's follow-up adds none; without that guard it would
    // install a third, equal ticker.
    QCOMPARE(installs.count(), 2);
}

namespace
{
// The channels of two colours are within a tolerance of each other: the renderer rounds.
bool sameColour(const QColor& a, const QColor& b, int tolerance)
{
    return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance
        && qAbs(a.blue() - b.blue()) <= tolerance;
}
} // namespace

// A colour bar's axis takes the data scale, so the bar's pixel for z = 1 is the entry the lookup
// gives z = 1, the scale-space fraction 0.60870 rather than the linear 0.50005. A map drawn through
// the same scale gives a diverging gradient's mirror colours for z and -z and the middle at zero.
void TestColorScale::theColourBarFollowsTheDataScale()
{
    std::vector<double> keys;
    std::vector<double> zs; // one value per key and row, the key moving fastest
    for (const double z : { -1e4, -1e4, -1.0, -1.0, 0.0, 0.0, 1.0, 1.0, 1e4, 1e4 })
    {
        keys.push_back(double(keys.size()));
        zs.insert(zs.end(), { z, z });
    }
    auto* map = mPlot->add<QCPColorMap2>().get();
    map->setData(keys, std::vector<double> { 0, 1 }, zs);
    map->setGradient(QCPColorGradient(QCPColorGradient::gpCoolwarm));
    map->setDataScale(QCPScale::symlog(1));
    map->setDataRange(QCPRange(-1e4, 1e4));
    const QCPRange dataRange(-1e4, 1e4);
    mPlot->xAxis->setRange(0, keys.back());
    mPlot->yAxis->setRange(0, 1);
    settle(mPlot, map);

    const auto scale = mPlot->axisRect()->colorScale();
    QVERIFY(scale);
    QVERIFY2(scale->dataScale().isSymlog(), "the bar did not take the data scale");
    QCOMPARE(scale->axis()->scale(), QCPScale::symlog(1));

    const QImage img = mPlot->toPixmap(400, 300).toImage();
    auto lookup = [&](double z) {
        return QColor::fromRgba(scale->gradient().color(z, dataRange, scale->dataScale()));
    };
    const int barX = scale->axis()->axisRect()->rect().center().x();
    const QColor barPixel = img.pixelColor(barX, qRound(scale->axis()->coordToPixel(1.0)));
    QVERIFY2(sameColour(barPixel, lookup(1.0), 3),
             qPrintable(QStringLiteral("the bar pixel for z = 1 is (%1,%2,%3), the lookup gives "
                                       "(%4,%5,%6)")
                            .arg(barPixel.red())
                            .arg(barPixel.green())
                            .arg(barPixel.blue())
                            .arg(lookup(1.0).red())
                            .arg(lookup(1.0).green())
                            .arg(lookup(1.0).blue())));

    // Each flat run of the map is probed at its centre, so no interpolation is in the sample.
    const int row2 = qRound(mPlot->yAxis->coordToPixel(0.5));
    auto cell = [&](double key) {
        return img.pixelColor(qRound(mPlot->xAxis->coordToPixel(key)), row2);
    };
    const QColor low = lookup(-1e4);
    const QColor middle = lookup(0.0);
    const QColor high = lookup(1e4);
    QVERIFY2(!sameColour(middle, low, 3) && !sameColour(middle, high, 3),
             "zero is the middle of the diverging gradient, not one of its ends");
    struct Column
    {
        double key;
        QColor want;
        const char* name;
    };
    const QColor nearMiddle = cell(4.5);
    const QColor justAbove = cell(6.5);
    for (const Column& column : { Column { 0.5, low, "z = -1e4" },
                                  Column { 2.5, lookup(-1.0), "z = -1" },
                                  Column { 4.5, middle, "z = 0" },
                                  Column { 6.5, lookup(1.0), "z = 1" },
                                  Column { 8.5, high, "z = 1e4" } })
    {
        const QColor got = cell(column.key);
        QVERIFY2(sameColour(got, column.want, 3),
                 qPrintable(QStringLiteral("the map's %1 column is %2, the lookup gives %3")
                                .arg(column.name, got.name(), column.want.name())));
    }
    // z = 1 sits at the scale-space fraction 0.60870, so it is clearly off the middle colour: a
    // linear renderer would paint the middle here.
    QVERIFY2(!sameColour(justAbove, nearMiddle, 10),
             "z = 1 is drawn through the scale, not at the linear middle");
}

// setType moves the range, the label and the ticker to the new axis, and the scale with them: the
// bar used to stay linear under a symlog ticker, so its pixels no longer matched its own lookup.
void TestColorScale::setTypeCarriesTheDataScale()
{
    mapWithRange(mPlot->xAxis, mPlot->yAxis, -1e4, 1e4);
    const auto scale = mPlot->axisRect()->colorScale();
    scale->setDataScale(QCPScale::symlog(1));
    scale->setDataRange(QCPRange(-1e4, 1e4));
    scale->setType(QCPAxis::atBottom);
    QCOMPARE(scale->axis()->scale(), QCPScale::symlog(1));
    mPlot->replot();

    const QImage img = mPlot->toPixmap(400, 300).toImage();
    auto lookup = [&](double z) {
        return QColor::fromRgba(scale->gradient().color(z, scale->dataRange(), scale->dataScale()));
    };
    const int barY = scale->axis()->axisRect()->rect().center().y();
    const QColor barPixel = img.pixelColor(qRound(scale->axis()->coordToPixel(1.0)), barY);
    QVERIFY2(sameColour(barPixel, lookup(1.0), 3),
             qPrintable(QStringLiteral("the bar pixel for z = 1 is %1, the lookup gives %2")
                            .arg(barPixel.name(), lookup(1.0).name())));
}

// A constant map leaves rescaleDataRange() with no data width; on a symlog scale that width is kept
// in the scale's own space and centred on the constant through toScale, at c = 0 and either side.
void TestColorScale::aConstantMapRescalesInScaleSpace()
{
    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 0);
    const auto scale = mPlot->axisRect()->colorScale();
    scale->setDataScale(QCPScale::symlog(1));
    scale->setDataRange(QCPRange(1, 1000));
    mPlot->replot();

    for (const double constant : { 0.0, -5.0, 5.0 })
    {
        map->data()->setCell(0, 0, constant);
        map->data()->setCell(1, 0, constant);
        map->data()->setCell(0, 1, constant);
        map->data()->setCell(1, 1, constant);
        map->data()->recalculateDataBounds();

        const QCPScale& zs = scale->dataScale();
        const QCPRange before = scale->dataRange();
        const double oldWidth = zs.toScale(before.upper) - zs.toScale(before.lower);

        scale->rescaleDataRange(true);
        const QCPRange got = scale->dataRange();
        const double newWidth = zs.toScale(got.upper) - zs.toScale(got.lower);
        QVERIFY2(qAbs(newWidth - oldWidth) < 1e-9,
                 qPrintable(QStringLiteral("constant %1: scale-space width %2, expected %3")
                                .arg(constant)
                                .arg(newWidth)
                                .arg(oldWidth)));
        QVERIFY2(qAbs(zs.toScale(got.lower) + zs.toScale(got.upper) - 2.0 * zs.toScale(constant))
                     < 1e-9,
                 qPrintable(QStringLiteral("constant %1: not centred on it in scale space")
                                .arg(constant)));
        QVERIFY2(got.lower < constant && constant < got.upper,
                 qPrintable(QStringLiteral("constant %1 is outside (%2, %3)")
                                .arg(constant)
                                .arg(got.lower)
                                .arg(got.upper)));
    }
}

// Two stacked rects share their right margin through a group; only the top one shows a scale. Both
// must stop short of it, so their right edges line up left of the bar.
void TestColorScale::groupedRectsLeaveRoomForTheScale()
{
    auto* bottom = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(1, 0, bottom);
    QCPAxisRect* top = mPlot->axisRect();
    QVERIFY(QCPAlignmentGroup::create({ top, bottom }, QCP::msRight));
    mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();
    mPlot->replot();

    const auto scale = top->colorScale();
    QVERIFY(scale->visible());
    QVERIFY2(top->rect().right() <= scale->outerRect().left(),
             qPrintable(QString("rect right %1, scale left %2")
                            .arg(top->rect().right())
                            .arg(scale->outerRect().left())));
    QCOMPARE(bottom->rect().right(), top->rect().right());
}

void TestColorScale::theWheelZoomsTheRectsScale()
{
    mPlot->setInteractions(QCP::iRangeZoom);
    mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 10);
    const auto scale = mPlot->axisRect()->colorScale();
    scale->setDataRange(QCPRange(0, 10));
    mPlot->replot();
    const QCPRange before = scale->dataRange();

    const QPointF pos = QRectF(scale->outerRect()).center();
    QWheelEvent event(pos, mPlot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(mPlot, &event);
    QVERIFY2(scale->dataRange() != before, "the wheel over the bar zooms its range");
}

void TestColorScale::theWheelZoomsTheScaleOnce()
{
    mPlot->setInteractions(QCP::iRangeZoom);
    mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 10);
    const auto scale = mPlot->axisRect()->colorScale();
    scale->setDataRange(QCPRange(0, 10));
    mPlot->replot();

    // The inner rect keeps its opposite axes equal, so one wheel step must reach the colour axis
    // once, not once per synchronized axis.
    QCPAxis* axis = scale->axis();
    const QPointF pos = QRectF(scale->outerRect()).center();
    const double centre = axis->pixelToCoord(pos.y());
    const double factor = mPlot->axisRect()->rangeZoomFactor(Qt::Vertical);
    const QCPRange expected((0.0 - centre) * factor + centre, (10.0 - centre) * factor + centre);

    QWheelEvent event(pos, mPlot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(mPlot, &event);

    const QCPRange after = scale->dataRange();
    QVERIFY2(qAbs(after.lower - expected.lower) < 1e-9
                 && qAbs(after.upper - expected.upper) < 1e-9,
             qPrintable(QStringLiteral("one wheel step zooms the scale once: got %1..%2, expected %3..%4")
                            .arg(after.lower)
                            .arg(after.upper)
                            .arg(expected.lower)
                            .arg(expected.upper)));
}

void TestColorScale::theScaleIsNotOnTheDataLayer()
{
    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();
    const auto scale = mPlot->axisRect()->colorScale();
    QVERIFY(scale->visible());
    QVERIFY2(scale->layer() != map->layer(), "a visible bar on the map's layer stops pans translating");
    QCOMPARE(scale->layer(), mPlot->layer("axes"));
}

void TestColorScale::aMovedMapFollowsItsNewRectsScale()
{
    auto* bottom = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(1, 0, bottom);
    QCPAxisRect* top = mPlot->axisRect();
    auto* moved = mapWithRange(top->axis(QCPAxis::atBottom), top->axis(QCPAxis::atLeft), 0, 1);
    QCOMPARE(moved->colorScale(), top->colorScale().get());

    moved->setKeyAxis(bottom->axis(QCPAxis::atBottom));
    moved->setValueAxis(bottom->axis(QCPAxis::atLeft));
    QCOMPARE(moved->colorScale(), bottom->colorScale().get());

    // A scale the user picked, or none at all, is the user's: the move keeps it.
    auto* chosen = mapWithRange(top->axis(QCPAxis::atBottom), top->axis(QCPAxis::atLeft), 0, 1);
    auto* own = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, own);
    chosen->setColorScale(own);
    auto* none = mapWithRange(top->axis(QCPAxis::atBottom), top->axis(QCPAxis::atLeft), 0, 1);
    none->setColorScale(nullptr);
    chosen->setKeyAxis(bottom->axis(QCPAxis::atBottom));
    chosen->setValueAxis(bottom->axis(QCPAxis::atLeft));
    none->setKeyAxis(bottom->axis(QCPAxis::atBottom));
    none->setValueAxis(bottom->axis(QCPAxis::atLeft));
    QCOMPARE(chosen->colorScale(), own);
    QVERIFY(!none->colorScale());
}

void TestColorScale::aScaleSlotDeletingTheMovedMapIsSafe()
{
    auto* bottom = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(1, 0, bottom);
    QCPAxisRect* top = mPlot->axisRect();
    QPointer<QCPColorMap> moved
        = mapWithRange(top->axis(QCPAxis::atBottom), top->axis(QCPAxis::atLeft), 0, 1);
    bool ran = false;
    connect(moved, &QCPColorMapped::colorScaleChanged, mPlot, [&] {
        ran = true;
        delete moved.data();
    }, Qt::SingleShotConnection);
    // Either setter may complete the move: the slot may have deleted the map after the first.
    moved->setKeyAxis(bottom->axis(QCPAxis::atBottom));
    if (moved)
        moved->setValueAxis(bottom->axis(QCPAxis::atLeft));
    QVERIFY(ran);
    QVERIFY(!moved);
    mPlot->replot();
}

void TestColorScale::anOptOutSurvivesATakeAndAdd()
{
    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    map->setColorScale(nullptr);

    QCustomPlot other;
    other.resize(400, 300);
    auto added = other.add(mPlot->take(qcp::makeHandle<QCPHandle<QCPColorMap>>(map)));
    QVERIFY(added);
    QCOMPARE(added->colorScale(), nullptr);
    QVERIFY2(other.findChildren<QCPColorScale*>().isEmpty(), "the opted-out map made the new rect's scale");
}
