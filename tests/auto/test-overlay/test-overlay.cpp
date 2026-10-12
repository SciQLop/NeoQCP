#include "test-overlay.h"
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

void TestOverlay::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->show();
    QTest::qWaitForWindowExposed(mPlot);
}

void TestOverlay::cleanup()
{
    delete mPlot;
}

void TestOverlay::showMessageStoresText()
{
    auto* ov = mPlot->overlay();
    QVERIFY(ov != nullptr);
    ov->showMessage("hello", QCPOverlay::Info);
    QCOMPARE(ov->text(), QString("hello"));
    QCOMPARE(ov->level(), QCPOverlay::Info);
    QVERIFY(ov->visible());
}

void TestOverlay::clearMessageHidesOverlay()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("hello", QCPOverlay::Error);
    ov->clearMessage();
    QCOMPARE(ov->text(), QString());
    QVERIFY(!ov->visible());
}

void TestOverlay::showMessageEmitsSignal()
{
    auto* ov = mPlot->overlay();
    QSignalSpy spy(ov, &QCPOverlay::messageChanged);
    ov->showMessage("test", QCPOverlay::Warning);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QString("test"));
}

void TestOverlay::compactRectIsSingleLine()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("test", QCPOverlay::Info, QCPOverlay::Compact, QCPOverlay::Top);
    QApplication::processEvents();
    QApplication::processEvents();
    QRect r = ov->overlayRect();
    QFontMetrics fm(ov->font());
    QCOMPARE(r.height(), fm.height() + 8);
    QCOMPARE(r.width(), mPlot->width());
}

void TestOverlay::fitContentRectFitsText()
{
    auto* ov = mPlot->overlay();
    QString longText = "Line one\nLine two\nLine three";
    ov->showMessage(longText, QCPOverlay::Info, QCPOverlay::FitContent, QCPOverlay::Top);
    QApplication::processEvents();
    QApplication::processEvents();
    QRect r = ov->overlayRect();
    QFontMetrics fm(ov->font());
    QVERIFY(r.height() > fm.height() + 8);
    QCOMPARE(r.width(), mPlot->width());
}

void TestOverlay::fullWidgetRectCoversWidget()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("error", QCPOverlay::Error, QCPOverlay::FullWidget);
    QApplication::processEvents();
    QApplication::processEvents();
    QRect r = ov->overlayRect();
    QCOMPARE(r, mPlot->rect());
}

void TestOverlay::positionTop()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("top", QCPOverlay::Info, QCPOverlay::Compact, QCPOverlay::Top);
    QApplication::processEvents();
    QApplication::processEvents();
    QCOMPARE(ov->overlayRect().top(), 0);
}

void TestOverlay::positionBottom()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("bot", QCPOverlay::Info, QCPOverlay::Compact, QCPOverlay::Bottom);
    QApplication::processEvents();
    QApplication::processEvents();
    QCOMPARE(ov->overlayRect().bottom(), mPlot->height() - 1);
}

void TestOverlay::positionLeft()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("left", QCPOverlay::Info, QCPOverlay::Compact, QCPOverlay::Left);
    QApplication::processEvents();
    QApplication::processEvents();
    QRect r = ov->overlayRect();
    QCOMPARE(r.left(), 0);
    QFontMetrics fm(ov->font());
    QCOMPARE(r.width(), fm.height() + 8);
}

void TestOverlay::positionRight()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("right", QCPOverlay::Info, QCPOverlay::Compact, QCPOverlay::Right);
    QApplication::processEvents();
    QApplication::processEvents();
    QCOMPARE(ov->overlayRect().right(), mPlot->width() - 1);
}

void TestOverlay::opacityRoundtrip()
{
    auto* ov = mPlot->overlay();
    ov->setOpacity(0.75);
    QCOMPARE(ov->opacity(), 0.75);
    ov->setOpacity(-0.5);
    QCOMPARE(ov->opacity(), 0.0);
    ov->setOpacity(1.5);
    QCOMPARE(ov->opacity(), 1.0);
}

void TestOverlay::showMessageTriggersReplot()
{
    auto* ov = mPlot->overlay();
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    ov->showMessage("trigger", QCPOverlay::Info);
    QApplication::processEvents();
    QApplication::processEvents();
    QVERIFY(spy.count() >= 1);
}

void TestOverlay::collapseToggle()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("Line one\nLine two\nLine three",
                    QCPOverlay::Info, QCPOverlay::FitContent, QCPOverlay::Top);
    ov->setCollapsible(true);
    QVERIFY(!ov->isCollapsed());

    QSignalSpy spy(ov, &QCPOverlay::collapsedChanged);
    ov->setCollapsed(true);
    QVERIFY(ov->isCollapsed());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toBool(), true);

    ov->setCollapsed(false);
    QVERIFY(!ov->isCollapsed());
    QCOMPARE(spy.count(), 2);
}

void TestOverlay::clickPassThroughCompact()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("status", QCPOverlay::Info, QCPOverlay::Compact, QCPOverlay::Top);
    QApplication::processEvents();
    QApplication::processEvents();

    QRect r = ov->overlayRect();
    QPointF center = r.center();
    QCPLayerable* layerable = ov;
    double dist = layerable->selectTest(center, false);
    QCOMPARE(dist, -1.0);
}

void TestOverlay::clickBlockedFullWidget()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("error", QCPOverlay::Error, QCPOverlay::FullWidget);
    QApplication::processEvents();
    QApplication::processEvents();

    QPointF center = mPlot->rect().center();
    QCPLayerable* layerable = ov;
    double dist = layerable->selectTest(center, false);
    QCOMPARE(dist, 0.0);
}

void TestOverlay::overlayStaysTopmost()
{
    mPlot->overlay();
    mPlot->addLayer("userLayer");
    mPlot->replot();
    QApplication::processEvents();

    auto* notifLayer = mPlot->layer("notification");
    QVERIFY(notifLayer != nullptr);
    QCOMPARE(notifLayer->index(), mPlot->layerCount() - 1);
}

void TestOverlay::overlaySurvivesClear()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("persist", QCPOverlay::Info);
    mPlot->clearPlottables();
    mPlot->clearItems();
    mPlot->clearGraphs();
    QCOMPARE(ov->text(), QString("persist"));
    QVERIFY(ov->visible());
    QCOMPARE(mPlot->overlay(), ov);
}

void TestOverlay::overlayAccessorCreatesLazily()
{
    auto* ov1 = mPlot->overlay();
    auto* ov2 = mPlot->overlay();
    QVERIFY(ov1 != nullptr);
    QCOMPARE(ov1, ov2);
    QCOMPARE(ov1->layer()->name(), QString("notification"));
}

void TestOverlay::overlayExcludedFromExport()
{
    auto* ov = mPlot->overlay();
    ov->showMessage("should not appear", QCPOverlay::Error, QCPOverlay::FullWidget);
    QApplication::processEvents();
    QApplication::processEvents();

    QPixmap px = mPlot->toPixmap(400, 300);
    QImage img = px.toImage();
    QColor centerColor = img.pixelColor(200, 150);
    QColor errorColor = QColor(178, 34, 34);
    QVERIFY(centerColor != errorColor);
}

void TestOverlay::updateWhileVisibleRedrawsLayer_data()
{
    QTest::addColumn<int>("change");
    QTest::newRow("showMessage") << 0;
    QTest::newRow("setOpacity") << 1;
    QTest::newRow("setFont") << 2;
    QTest::newRow("setCollapsed") << 3;
    QTest::newRow("setCollapsible") << 4;
}

// The notification layer is buffered: a change to a message already on screen must
// dirty that buffer, or the replot keeps painting the old message (SciQLop tutorial:
// an Info message followed by a Warning stayed green until cleared).
void TestOverlay::updateWhileVisibleRedrawsLayer()
{
    QFETCH(int, change);
    auto* ov = mPlot->overlay();
    ov->setCollapsible(change == 3);
    ov->showMessage("Info first", QCPOverlay::Info, QCPOverlay::FitContent);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    auto buffer = ov->layer()->mPaintBuffer.toStrongRef();
    QVERIFY(buffer);
    QVERIFY(!buffer->contentDirty());

    switch (change)
    {
        case 0: ov->showMessage("Data gap detected", QCPOverlay::Warning, QCPOverlay::FitContent); break;
        case 1: ov->setOpacity(0.5); break;
        case 2: { QFont f = ov->font(); f.setPointSize(f.pointSize() + 4); ov->setFont(f); break; }
        case 3: ov->setCollapsed(true); break;
        case 4: ov->setCollapsible(true); break;
    }
    QVERIFY(buffer->contentDirty());
}
