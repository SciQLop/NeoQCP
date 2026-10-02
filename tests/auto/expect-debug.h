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
