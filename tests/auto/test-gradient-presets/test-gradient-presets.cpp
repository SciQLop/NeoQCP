#include "test-gradient-presets.h"
#include "qcustomplot.h"

// First, middle and last of the 65 stops each preset is built from (matplotlib's tables).
void TestGradientPresets::tablePresetsHitTheirStops_data()
{
    QTest::addColumn<QCPColorGradient::GradientPreset>("preset");
    QTest::addColumn<QColor>("first");
    QTest::addColumn<QColor>("middle");
    QTest::addColumn<QColor>("last");
    QTest::newRow("viridis") << QCPColorGradient::gpViridis << QColor(68, 1, 84)
                             << QColor(33, 145, 140) << QColor(253, 231, 37);
    QTest::newRow("cividis") << QCPColorGradient::gpCividis << QColor(0, 34, 78)
                             << QColor(125, 124, 120) << QColor(254, 232, 56);
    QTest::newRow("magma") << QCPColorGradient::gpMagma << QColor(0, 0, 4)
                           << QColor(183, 55, 121) << QColor(252, 253, 191);
    QTest::newRow("inferno") << QCPColorGradient::gpInferno << QColor(0, 0, 4)
                             << QColor(188, 55, 84) << QColor(252, 255, 164);
    QTest::newRow("plasma") << QCPColorGradient::gpPlasma << QColor(13, 8, 135)
                            << QColor(204, 71, 120) << QColor(240, 249, 33);
    QTest::newRow("turbo") << QCPColorGradient::gpTurbo << QColor(48, 18, 59)
                           << QColor(164, 252, 60) << QColor(122, 4, 3);
    QTest::newRow("coolwarm") << QCPColorGradient::gpCoolwarm << QColor(59, 76, 192)
                              << QColor(221, 220, 220) << QColor(180, 4, 38);
}

void TestGradientPresets::tablePresetsHitTheirStops()
{
    QFETCH(QCPColorGradient::GradientPreset, preset);
    QFETCH(QColor, first);
    QFETCH(QColor, middle);
    QFETCH(QColor, last);
    QCPColorGradient gradient(preset);
    gradient.setLevelCount(65); // one level per stop: each level lands exactly on its stop
    const QCPRange range(0.0, 1.0);
    QCOMPARE(QColor(gradient.color(0.0, range)), first);
    QCOMPARE(QColor(gradient.color(0.5, range)), middle);
    QCOMPARE(QColor(gradient.color(1.0, range)), last);
}

void TestGradientPresets::existingPresetsKeepTheirValues()
{
    QCOMPARE(int(QCPColorGradient::gpJet), 10);
    QCOMPARE(int(QCPColorGradient::gpHues), 11);
    QCOMPARE(int(QCPColorGradient::gpViridis), 12);
    QCOMPARE(int(QCPColorGradient::gpCoolwarm), 18);
}
