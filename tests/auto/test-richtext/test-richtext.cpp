#include "test-richtext.h"
#include "../../../src/qcp.h"

void TestRichText::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestRichText::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestRichText::createAndSetHtml()
{
    auto item = mPlot->add<QCPItemRichText>();
    QString html = "<b>Bold</b> and <i>italic</i>";
    item->setHtml(html);
    QCOMPARE(item->html(), html);
}

void TestRichText::clearHtmlSwitchesToPlainText()
{
    auto item = mPlot->add<QCPItemRichText>();
    item->setHtml("<b>Bold</b>");
    QVERIFY(!item->html().isEmpty());

    item->clearHtml();
    QVERIFY(item->html().isEmpty());
}

void TestRichText::drawDoesNotCrash()
{
    auto item = mPlot->add<QCPItemRichText>();
    item->setHtml("<h2>Title</h2><p>Some <b>bold</b> text</p>");
    item->position->setCoords(5, 5);
    mPlot->replot();
    QVERIFY(true);
}

void TestRichText::drawPlainFallback()
{
    auto item = mPlot->add<QCPItemRichText>();
    item->setText("Plain text only");
    item->position->setCoords(5, 5);
    mPlot->replot();
    QVERIFY(true);
}
