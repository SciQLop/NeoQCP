#include "test-colorscale.h"

namespace
{
// A 2x2 map whose values span [zMin, zMax].
QCPColorMap* mapWithRange(QCPAxis* keyAxis, QCPAxis* valueAxis, double zMin, double zMax)
{
    auto* map = new QCPColorMap(keyAxis, valueAxis);
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
    auto* map = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
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

// Two stacked rects share their right margin through a group; only the top one shows a scale. Both
// must stop short of it, so their right edges line up left of the bar.
void TestColorScale::groupedRectsLeaveRoomForTheScale()
{
    auto* bottom = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(1, 0, bottom);
    auto* group = new QCPMarginGroup(mPlot);
    QCPAxisRect* top = mPlot->axisRect();
    top->setMarginGroup(QCP::msRight, group);
    bottom->setMarginGroup(QCP::msRight, group);
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

void TestColorScale::theScaleIsNotOnTheDataLayer()
{
    auto* map = mapWithRange(mPlot->xAxis, mPlot->yAxis, 0, 1);
    mPlot->replot();
    const auto scale = mPlot->axisRect()->colorScale();
    QVERIFY(scale->visible());
    QVERIFY2(scale->layer() != map->layer(), "a visible bar on the map's layer stops pans translating");
    QCOMPARE(scale->layer(), mPlot->layer("axes"));
}
