#pragma once
#include <QtTest/QtTest>

class TestGradientPresets : public QObject
{
    Q_OBJECT
private slots:
    void tablePresetsHitTheirStops_data();
    void tablePresetsHitTheirStops();
    void existingPresetsKeepTheirValues();
};
