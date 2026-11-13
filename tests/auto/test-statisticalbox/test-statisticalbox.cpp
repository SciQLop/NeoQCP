#include "test-statisticalbox.h"
#include "painting/draw-commands.h"
#include "reference-image.h"
#include <QImage>
#include <functional>

namespace
{
const QString kDir = QStringLiteral("test-reference-m4e");

//! The statistical box of the reference scenes, with what it recorded: the recording is what these
//! tests hold to the reference images.
class RecordingBox : public QCPStatisticalBox
{
public:
    using QCPStatisticalBox::QCPStatisticalBox;

    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }

    [[nodiscard]] int recordedCommands() const { return mCore.recordedCommandCount(); }

    [[nodiscard]] const QCPDrawList& iconList() const { return mCore.iconRecordedList(); }
};

struct Scene
{
    QImage image;
    int commands = 0;
};

QImage imageOf(QCustomPlot& plot)
{
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

//! The pixels around \a at that are mostly red: what a marker in a red pen leaves.
int redPixelsNear(const QImage& image, const QPointF& at, int radius)
{
    int red = 0;
    for (int y = int(at.y()) - radius; y <= int(at.y()) + radius; ++y)
        for (int x = int(at.x()) - radius; x <= int(at.x()) + radius; ++x)
        {
            if (!image.valid(x, y))
                continue;
            const QColor colour = image.pixelColor(x, y);
            if (colour.red() > 150 && colour.red() > colour.blue() + 60)
                ++red;
        }
    return red;
}

//! The scene the reference suite renders: the same three boxes, width and axes, through a plottable
//! that says what it recorded.
Scene statisticalBoxScene(bool verticalKey, bool selected)
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QFont font = qcp::test::referenceFont();
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis* axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }
    QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
    QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
    key->setRange(0, 4);
    value->setRange(0, 10);
    auto* box = new RecordingBox(key, value);
    box->addData(0.8, 1.0, 2.5, 4.0, 6.0, 8.5);
    box->addData(1.8, 0.5, 3.0, 5.2, 7.0, 9.5);
    box->addData(2.8, 2.0, 3.5, 5.5, 7.5, 9.0);
    box->setWidth(0.5);
    box->setWhiskerWidth(0.3);
    if (selected)
    {
        box->setSelectable(QCP::stDataRange);
        box->setSelection(QCPDataSelection(QCPDataRange(1, 2)));
    }
    plot.replot();
    return { imageOf(plot), box->recordedCommands() };
}

//! A plot to poke at: three boxes with outliers, so every part of the drawing is recorded.
RecordingBox* makeBoxPlot(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 4);
    plot.yAxis->setRange(0, 10);
    auto* box = new RecordingBox(plot.xAxis, plot.yAxis);
    box->addData(0.8, 1.0, 2.5, 4.0, 6.0, 8.5);
    box->addData(1.8, 0.5, 3.0, 5.2, 7.0, 9.5);
    box->addData(2.8, 2.0, 3.5, 5.5, 7.5, 9.0);
    box->data()->begin()[0].outliers = QVector<double> { 0.6, 9.2 };
    box->setWidth(0.5);
    box->setWhiskerWidth(0.3);
    plot.replot();
    return box;
}
} // namespace

void TestStatisticalBox::recordedBoxesMatchTheirReferences_data()
{
    QTest::addColumn<bool>("verticalKey");
    QTest::addColumn<bool>("selected");
    QTest::addColumn<QString>("name");
    QTest::newRow("plain") << false << false << QStringLiteral("statbox-plain.png");
    QTest::newRow("vertical-key") << true << false << QStringLiteral("statbox-vertical-key.png");
    QTest::newRow("selected") << false << true << QStringLiteral("statbox-selected.png");
}

void TestStatisticalBox::recordedBoxesMatchTheirReferences()
{
    QFETCH(bool, verticalKey);
    QFETCH(bool, selected);
    QFETCH(QString, name);
    const Scene scene = statisticalBoxScene(verticalKey, selected);
    QVERIFY2(scene.commands > 0, "the statistical box recorded nothing");
    QString why;
    QVERIFY2(qcp::test::matchesReference(scene.image, kDir, name, &why),
             qPrintable(QStringLiteral("%1: %2").arg(name, why)));
}

void TestStatisticalBox::everyStyleSetterReRecords()
{
    QCustomPlot plot;
    RecordingBox* box = makeBoxPlot(plot);
    QVERIFY2(box->recordedCommands() > 0, "the statistical box recorded nothing");
    const auto reRecords = [&](const std::function<void()>& change)
    {
        const int before = box->recordings();
        change();
        plot.replot();
        return box->recordings() == before + 1;
    };
    QVERIFY2(reRecords([&] { box->setWidth(0.3); }), "setWidth did not remake the recording");
    QVERIFY2(reRecords([&] { box->setWhiskerWidth(0.1); }),
             "setWhiskerWidth did not remake the recording");
    QVERIFY2(reRecords([&] { box->setWhiskerPen(QPen(Qt::red)); }),
             "setWhiskerPen did not remake the recording");
    QVERIFY2(reRecords([&] { box->setWhiskerBarPen(QPen(Qt::blue)); }),
             "setWhiskerBarPen did not remake the recording");
    QVERIFY2(reRecords([&] { box->setWhiskerAntialiased(true); }),
             "setWhiskerAntialiased did not remake the recording");
    QVERIFY2(reRecords([&] { box->setMedianPen(QPen(Qt::green)); }),
             "setMedianPen did not remake the recording");
    QVERIFY2(reRecords([&] { box->setOutlierStyle(QCPScatterStyle(QCPScatterStyle::ssCross, 8)); }),
             "setOutlierStyle did not remake the recording");
    QVERIFY2(reRecords([&] { box->setPen(QPen(Qt::darkCyan)); }),
             "setPen did not remake the recording");
    QVERIFY2(reRecords([&] { box->setBrush(QBrush(Qt::yellow)); }),
             "setBrush did not remake the recording");
    box->setSelectable(QCP::stDataRange);
    QVERIFY2(reRecords([&] { box->setSelection(QCPDataSelection(QCPDataRange(1, 2))); }),
             "a selection change did not remake the recording");
}

// 2.x applied the box's pen to an outlier style that carries none of its own.
void TestStatisticalBox::anUnstyledOutlierMarkerTakesTheBoxPen()
{
    QCustomPlot plot;
    RecordingBox* box = makeBoxPlot(plot);
    box->setPen(QPen(Qt::red, 2));
    box->setOutlierStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
    plot.replot();
    const QPointF outlier(plot.xAxis->coordToPixel(0.8), plot.yAxis->coordToPixel(9.2));
    QVERIFY2(redPixelsNear(imageOf(plot), outlier, 6) > 0,
             "the outlier marker is not drawn in the box's pen");
}

// The legend row paints the icon from a recording of its own, boxed by the unit square, and not
// from the chart's data: the icon is drawn even with no data at all.
void TestStatisticalBox::theLegendIconIsRecorded()
{
    QCustomPlot plot;
    auto* box = new RecordingBox(plot.xAxis, plot.yAxis);
    plot.legend->setVisible(true);
    [[maybe_unused]] const QPixmap painted = plot.toPixmap(300, 200);
    const QCPDrawList& icon = box->iconList();
    QVERIFY2(icon.data.keyBounded && icon.data.valueBounded, "the legend icon was not recorded");
    QCOMPARE(icon.data.bounds.keyLower, 0.0);
    QCOMPARE(icon.data.bounds.keyUpper, 1.0);
    QCOMPARE(icon.data.bounds.valueLower, 0.0);
    QCOMPARE(icon.data.bounds.valueUpper, 1.0);
}
