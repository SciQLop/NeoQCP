#ifndef QCP_TEST_REFERENCE_IMAGE_H
#define QCP_TEST_REFERENCE_IMAGE_H

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QString>
#include <QtGlobal>

namespace qcp::test
{
/*! \brief Compares \a image with the reference stored for \a testDir/\a name, or writes it.

  With \c QCP_WRITE_REFERENCE set the image is saved to
  \c tests/auto/<testDir>/reference/<name> and the comparison succeeds, which is how the images
  are made. Otherwise the sizes must match and at most \a maxBadPixels pixels of the compared area
  (\a area, the whole image when null; its outer row and column are excluded: the export painter
  sits half a pixel off the device grid and moves only those) may differ by more than 32 in a
  channel. \a why receives the first difference and the total, when the caller wants to print it.

  The path is resolved from the test binary, so the helper works from any build directory.
*/
inline bool matchesReference(const QImage& image, const QString& testDir, const QString& name,
                             QString* why = nullptr, int maxBadPixels = 20,
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
            if (qAbs(qRed(a) - qRed(b)) > 32 || qAbs(qGreen(a) - qGreen(b)) > 32
                || qAbs(qBlue(a) - qBlue(b)) > 32 || qAbs(qAlpha(a) - qAlpha(b)) > 32)
            {
                if (bad == 0)
                    first = QPoint(x, y);
                ++bad;
            }
        }
    }
    if (bad > maxBadPixels)
    {
        if (why)
            *why = QStringLiteral("%1 pixels differ (at most %2), first at %3,%4")
                       .arg(bad)
                       .arg(maxBadPixels)
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
