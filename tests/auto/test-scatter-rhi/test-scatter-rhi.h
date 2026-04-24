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

    // Task 3-5: RHI layer + integration
    void scatterRhiLayerCreatedLazily();
    void replotWithScatterDoesNotCrash();
    void replotScatterDiscDoesNotCrash();
    void replotScatterSquareDoesNotCrash();
    void replotScatterCustomDoesNotCrash();
    void exportWithScatterStillWorks();
    void clearResetsState();

private:
    QCustomPlot* mPlot = nullptr;
};
