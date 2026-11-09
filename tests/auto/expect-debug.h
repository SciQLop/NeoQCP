#pragma once
#include <QRegularExpression>
#include <QtTest/QtTest>

// NeoQCP prefixes its debug output with Q_FUNC_INFO, which every compiler spells its own way
// ("QCPAxis*" with GCC, "QCPAxis *" with clang, "__cdecl ... class QCPAxis *" with MSVC).
// Match the function name and the message text, not the whole signature.
inline void expectDebugFrom(const char* function, const QString& text)
{
    QTest::ignoreMessage(QtDebugMsg,
                         QRegularExpression(QRegularExpression::escape(QString::fromLatin1(function))
                                            + QStringLiteral(R"(\(.*\).*\s)")
                                            + QRegularExpression::escape(text)
                                            + QStringLiteral(R"(\s*$)")));
}

// A drawn object whose axes are gone is not recorded, and says so once: the item's positions are
// no longer resolved one by one, so one message stands for the object.
inline void expectMissingAxis()
{
    QTest::ignoreMessage(QtDebugMsg,
                         QRegularExpression(QStringLiteral(
                             "a drawn object has no key or value axis; it is not drawn")));
}
