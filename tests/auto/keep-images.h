#pragma once
#include <QImage>
#include <QRegularExpression>
#include <QString>

// With QCP_TEST_LOG_DIR set (CI does), keeps both pictures of a failed comparison next to the
// test logs, so a failure seen only on one platform can be looked at.
inline void keepImagesForCi(const QString& what, const QImage& a, const QImage& b)
{
    const QString dir = qEnvironmentVariable("QCP_TEST_LOG_DIR");
    if (dir.isEmpty())
        return;
    const QString stem = QString(what).replace(QRegularExpression("[^A-Za-z0-9_-]+"), "_");
    a.save(dir + "/" + stem + "-a.png");
    b.save(dir + "/" + stem + "-b.png");
}
