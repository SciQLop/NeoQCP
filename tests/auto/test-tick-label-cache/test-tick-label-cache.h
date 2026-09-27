#include <QtTest/QtTest>
#include "qcustomplot.h"

// QCP::phCacheLabels draws tick labels from cached pixmaps. Every test renders the plot's
// live buffers with the hint off and on, after the same change, and expects the same image.
class TestTickLabelCache : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void plainLabelsMatch();
    void fontChangeRefreshesLabels();
    void colourChangeRefreshesLabels();
    void themeSwitchRefreshesLabels();
    void bufferDprChangeKeepsLabelSize();
    void selectedTickLabelsUseSelectedFontAndColour();
    void rotatedLabelsMatch();
    void multiLineLabelsMatch();
    void dateTimePanMatches();
    void logAxisBeautifulPowersMatch();
    void linearToLogSwitchRefreshesPowers();
    void manyLabelsMatch();
    void pdfExportKeepsVectorText();

};
