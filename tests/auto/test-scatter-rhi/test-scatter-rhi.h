#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestScatterRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void renderToImageProducesNonEmptyImage();
    void renderToImageDiscHasAlpha();
    void renderToImageRespectsPenColor();
    void renderToImageCustomPathWorks();

private:
    QCustomPlot* mPlot = nullptr;
};
