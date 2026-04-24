#include "test-scatter-rhi.h"
#include "../../../src/qcp.h"

void TestScatterRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestScatterRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestScatterRhi::renderToImageProducesNonEmptyImage()
{
    QCPScatterStyle style(QCPScatterStyle::ssDisc, 10);
    QImage img = style.renderToImage(64);
    QCOMPARE(img.width(), 64);
    QCOMPARE(img.height(), 64);

    bool hasNonTransparent = false;
    for (int y = 0; y < img.height() && !hasNonTransparent; ++y)
        for (int x = 0; x < img.width() && !hasNonTransparent; ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                hasNonTransparent = true;
    QVERIFY(hasNonTransparent);
}

void TestScatterRhi::renderToImageDiscHasAlpha()
{
    QCPScatterStyle style(QCPScatterStyle::ssDisc, QColor(Qt::red), 10);
    QImage img = style.renderToImage(64);

    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    QVERIFY(qAlpha(img.pixel(32, 32)) > 0);
}

void TestScatterRhi::renderToImageRespectsPenColor()
{
    QPen pen(Qt::blue);
    pen.setWidth(2);
    QCPScatterStyle style(QCPScatterStyle::ssCircle, pen, Qt::NoBrush, 10);
    QImage img = style.renderToImage(64);

    bool hasBlue = false;
    for (int y = 0; y < img.height() && !hasBlue; ++y)
        for (int x = 0; x < img.width() && !hasBlue; ++x) {
            QColor c = QColor::fromRgba(img.pixel(x, y));
            if (c.alpha() > 0 && c.blue() > c.red() && c.blue() > c.green())
                hasBlue = true;
        }
    QVERIFY(hasBlue);
}

void TestScatterRhi::renderToImageCustomPathWorks()
{
    QPainterPath path;
    path.addRect(-5, -5, 10, 10);
    QCPScatterStyle style(path, QPen(Qt::black), QBrush(Qt::white), 10);
    QImage img = style.renderToImage(64);

    bool hasNonTransparent = false;
    for (int y = 0; y < img.height() && !hasNonTransparent; ++y)
        for (int x = 0; x < img.width() && !hasNonTransparent; ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                hasNonTransparent = true;
    QVERIFY(hasNonTransparent);
}
