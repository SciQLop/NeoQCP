#include "test-theme.h"

void TestTheme::lightFactoryDefaults()
{
    auto theme = QCPTheme::light();
    QCOMPARE(theme->background(), QColor(Qt::white));
    QCOMPARE(theme->foreground(), QColor(Qt::black));
    QCOMPARE(theme->grid(), QColor(200, 200, 200));
    QCOMPARE(theme->subGrid(), QColor(220, 220, 220));
    QCOMPARE(theme->selection(), QColor(Qt::blue));
    QCOMPARE(theme->legendBackground(), QColor(Qt::white));
    QCOMPARE(theme->legendBorder(), QColor(Qt::black));
    delete theme;
}

void TestTheme::darkFactoryColors()
{
    auto theme = QCPTheme::dark();
    QCOMPARE(theme->background(), QColor("#1e1e1e"));
    QCOMPARE(theme->foreground(), QColor("#e0e0e0"));
    QCOMPARE(theme->grid(), QColor("#3a3a3a"));
    QCOMPARE(theme->subGrid(), QColor("#2a2a2a"));
    QCOMPARE(theme->selection(), QColor("#4a9eff"));
    QCOMPARE(theme->legendBackground(), QColor("#2a2a2a"));
    QCOMPARE(theme->legendBorder(), QColor("#555555"));
    delete theme;
}

void TestTheme::changedSignalEmitted()
{
    QCPTheme theme;
    QSignalSpy spy(&theme, &QCPTheme::changed);
    theme.setBackground(Qt::red);
    QCOMPARE(spy.count(), 1);
    theme.setForeground(Qt::green);
    QCOMPARE(spy.count(), 2);
}

void TestTheme::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->show();
}

void TestTheme::cleanup()
{
    delete mPlot;
}

void TestTheme::applyThemePropagatesBackground()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    QCOMPARE(mPlot->backgroundBrush().color(), QColor("#1e1e1e"));
    delete theme;
}

void TestTheme::applyThemePropagatesAxisColors()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    auto* axis = mPlot->xAxis;
    QCOMPARE(axis->basePen().color(), QColor("#e0e0e0"));
    QCOMPARE(axis->tickPen().color(), QColor("#e0e0e0"));
    QCOMPARE(axis->subTickPen().color(), QColor("#e0e0e0"));
    QCOMPARE(axis->labelColor(), QColor("#e0e0e0"));
    QCOMPARE(axis->tickLabelColor(), QColor("#e0e0e0"));
    delete theme;
}

void TestTheme::applyThemePropagatesGridColors()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    auto* grid = mPlot->xAxis->grid();
    QCOMPARE(grid->pen().color(), QColor("#3a3a3a"));
    QCOMPARE(grid->subGridPen().color(), QColor("#2a2a2a"));
    QCOMPARE(grid->zeroLinePen().color(), QColor("#3a3a3a"));
    delete theme;
}

void TestTheme::applyThemePropagatesLegendColors()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    QCOMPARE(mPlot->legend->brush().color(), QColor("#2a2a2a"));
    QCOMPARE(mPlot->legend->borderPen().color(), QColor("#555555"));
    QCOMPARE(mPlot->legend->textColor(), QColor("#e0e0e0"));
    delete theme;
}

void TestTheme::applyThemePropagatesTextElementColor()
{
    auto* title = new QCPTextElement(mPlot, "Test Title");
    mPlot->plotLayout()->insertRow(0);
    mPlot->plotLayout()->addElement(0, 0, title);
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    QCOMPARE(title->textColor(), QColor("#e0e0e0"));
    QCOMPARE(title->selectedTextColor(), QColor("#4a9eff"));
    delete theme;
}

void TestTheme::applyThemePreservesPenStyle()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    QCOMPARE(mPlot->xAxis->grid()->pen().style(), Qt::DotLine);
    QCOMPARE(mPlot->xAxis->basePen().capStyle(), Qt::SquareCap);
    delete theme;
}

void TestTheme::perElementOverrideAfterTheme()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    mPlot->xAxis->setLabelColor(Qt::yellow);
    QCOMPARE(mPlot->xAxis->labelColor(), QColor(Qt::yellow));
    QCOMPARE(mPlot->xAxis->basePen().color(), QColor("#e0e0e0"));
    delete theme;
}

void TestTheme::themeSharing()
{
    auto* plot2 = new QCustomPlot(nullptr);
    plot2->show();
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    plot2->setTheme(theme);

    QCOMPARE(mPlot->backgroundBrush().color(), QColor("#1e1e1e"));
    QCOMPARE(plot2->backgroundBrush().color(), QColor("#1e1e1e"));

    theme->setBackground(Qt::red);
    QCoreApplication::processEvents();

    QCOMPARE(mPlot->backgroundBrush().color(), QColor(Qt::red));
    QCOMPARE(plot2->backgroundBrush().color(), QColor(Qt::red));

    mPlot->setTheme(nullptr);
    plot2->setTheme(nullptr);
    delete plot2;
    delete theme;
}

void TestTheme::setThemeNull_revertsToOwned()
{
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);
    QCOMPARE(mPlot->backgroundBrush().color(), QColor("#1e1e1e"));

    mPlot->setTheme(nullptr);
    QCOMPARE(mPlot->theme()->background(), QColor(Qt::white));

    delete theme;
}

void TestTheme::qssProxyProperties()
{
    mPlot->setProperty("themeBackground", QColor("#1e1e1e"));
    mPlot->setProperty("themeForeground", QColor("#e0e0e0"));
    QCoreApplication::processEvents();

    QCOMPARE(mPlot->backgroundBrush().color(), QColor("#1e1e1e"));
    QCOMPARE(mPlot->xAxis->basePen().color(), QColor("#e0e0e0"));
}

void TestTheme::coalescing()
{
    int replotCount = 0;
    connect(mPlot, &QCustomPlot::afterReplot, [&replotCount]() { ++replotCount; });

    auto* theme = mPlot->theme();
    theme->setBackground(Qt::red);
    theme->setForeground(Qt::green);
    theme->setGrid(Qt::blue);
    QCOMPARE(replotCount, 0);

    // Two processEvents() calls needed: the first processes the coalesced
    // theme timer (which calls applyTheme → replot(rpQueuedReplot)),
    // the second processes the queued replot timer.
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(replotCount, 1);
}

void TestTheme::applyThemePropagatesColorScaleAxis()
{
    auto* colorScale = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, colorScale);
    auto* theme = QCPTheme::dark();
    mPlot->setTheme(theme);

    QCOMPARE(colorScale->axis()->basePen().color(), QColor("#e0e0e0"));
    QCOMPARE(colorScale->axis()->labelColor(), QColor("#e0e0e0"));
    QCOMPARE(colorScale->axis()->tickLabelColor(), QColor("#e0e0e0"));

    delete theme;
}

void TestTheme::busyIndicatorDefaults()
{
    QCPTheme theme;
    QCOMPARE(theme.busyIndicatorSymbol(), QStringLiteral("⟳"));
    QCOMPARE(theme.busyFadeAlpha(), 0.3);
    QCOMPARE(theme.busyShowDelayMs(), 500);
    QCOMPARE(theme.busyHideDelayMs(), 500);
}

void TestTheme::busyIndicatorLightFactory()
{
    auto* theme = QCPTheme::light();
    QCOMPARE(theme->busyIndicatorSymbol(), QStringLiteral("⟳"));
    QCOMPARE(theme->busyFadeAlpha(), 0.3);
    QCOMPARE(theme->busyShowDelayMs(), 500);
    QCOMPARE(theme->busyHideDelayMs(), 500);
    delete theme;
}
