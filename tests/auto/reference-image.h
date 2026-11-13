#ifndef QCP_TEST_REFERENCE_IMAGE_H
#define QCP_TEST_REFERENCE_IMAGE_H

#include "keep-images.h"

#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QString>
#include <QtGlobal>
#include <QtTest/QTest>

namespace qcp::test
{
//! The family of the font the test runner loads from tests/auto/fonts before any test.
inline QString referenceFontFamily()
{
    return QStringLiteral("DejaVu Sans");
}

/*! \brief The font a reference picture draws its text with, so it does not depend on the fonts the
  machine has. No hinting: FreeType's default hinting differs by platform.
*/
inline QFont referenceFont(int pointSize = -1)
{
    QFont font(referenceFontFamily(), pointSize);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

/*! \brief Whether this machine draws the reference pictures' text like the machines that made
  them. Offscreen on macOS draws it with Core Text, with no FreeType option; macos-latest, on
  cocoa with FreeType, checks the pictures on macOS.
*/
inline bool drawsReferenceText()
{
#ifdef Q_OS_MACOS
    return QGuiApplication::platformName() != QLatin1String("offscreen");
#else
    return true;
#endif
}

//! Skips the running test function from inside a helper: QSKIP alone only leaves the helper.
inline void skipTheReferenceComparison()
{
    QTest::ThrowOnSkipEnabler throwing;
    [] { QSKIP("offscreen on macOS draws text with Core Text, unlike the reference pictures; "
               "macos-latest compares them on macOS"); }();
}

/*! \brief How far an export may drift from its reference.

  \a pixels pixels may differ, by more than \a channel in any channel. {0, 0} is a pixel-exact
  comparison.
*/
struct Tolerance
{
    int pixels = 20;
    int channel = 32;
};

/*! \brief Compares \a image with the reference stored for \a testDir/\a name, or writes it.

  With \c QCP_WRITE_REFERENCE set the image is saved to
  \c tests/auto/<testDir>/reference/<name> and the comparison succeeds, which is how the images
  are made. Otherwise the sizes must match and at most \a tolerance pixels differ by more than
  \a tolerance's channel in a channel, inside the compared area (\a area, the whole image when
  null; its outer row and column are excluded: the export painter sits half a pixel off the device
  grid and moves only those). \a why receives the first difference and the total, when the caller
  wants to print it. With \c QCP_TEST_LOG_DIR set (CI does), a failed comparison also keeps the
  picture (-a) and its reference (-b) there, so a failure seen on one platform only can be looked at.

  The path is resolved from the test binary, so the helper works from any build directory.
*/
inline bool matchesReference(const QImage& image, const QString& testDir, const QString& name,
                             QString* why = nullptr, Tolerance tolerance = {},
                             const QRect& area = QRect())
{
    const QString path = QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral("../../../tests/auto/") + testDir
                                               + QStringLiteral("/reference/") + name);
    if (qEnvironmentVariableIsSet("QCP_WRITE_REFERENCE"))
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (!image.save(path))
        {
            if (why)
                *why = QStringLiteral("could not write ") + path;
            return false;
        }
        return true;
    }
    // simplify: every reference picture holds text (at least its axes' tick labels), so all are
    // skipped here; a text-free picture would need a flag from its caller to be compared.
    if (!drawsReferenceText())
        skipTheReferenceComparison();
    const QImage reference = QImage(path).convertToFormat(QImage::Format_ARGB32);
    if (reference.isNull())
    {
        if (why)
            *why = QStringLiteral("could not read ") + path;
        return false;
    }
    if (reference.size() != image.size())
    {
        if (why)
            *why = QStringLiteral("size %1x%2, reference %3x%4")
                       .arg(image.width())
                       .arg(image.height())
                       .arg(reference.width())
                       .arg(reference.height());
        return false;
    }
    const QImage actual = image.convertToFormat(QImage::Format_ARGB32);
    const QRect used = area.isNull() ? actual.rect() : area.intersected(actual.rect());
    int bad = 0;
    QPoint first;
    for (int y = used.top() + 1; y < used.bottom(); ++y)
    {
        for (int x = used.left() + 1; x < used.right(); ++x)
        {
            const QRgb a = actual.pixel(x, y);
            const QRgb b = reference.pixel(x, y);
            if (qAbs(qRed(a) - qRed(b)) > tolerance.channel
                || qAbs(qGreen(a) - qGreen(b)) > tolerance.channel
                || qAbs(qBlue(a) - qBlue(b)) > tolerance.channel
                || qAbs(qAlpha(a) - qAlpha(b)) > tolerance.channel)
            {
                if (bad == 0)
                    first = QPoint(x, y);
                ++bad;
            }
        }
    }
    if (bad > tolerance.pixels)
    {
        keepImagesForCi(testDir + QLatin1Char('-') + name, actual, reference);
        if (why)
            *why = QStringLiteral("%1 pixels differ (at most %2), first at %3,%4")
                       .arg(bad)
                       .arg(tolerance.pixels)
                       .arg(first.x())
                       .arg(first.y());
        return false;
    }
    if (why)
        why->clear();
    return true;
}
} // namespace qcp::test

#endif // QCP_TEST_REFERENCE_IMAGE_H
