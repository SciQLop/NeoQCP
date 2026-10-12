#include "snippets.h"

#include <QDir>
#include <QTemporaryDir>

// The example shows the [[nodiscard]] warning; the build should not.
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wunused-result")
QT_WARNING_DISABLE_CLANG("-Wunused-result")
QT_WARNING_DISABLE_MSVC(4834)
DOC_CODE(migrating_nodiscard)
{
    customPlot->addGraph();
    QTemporaryDir dir;
    const QString previous = QDir::currentPath();
    QDir::setCurrent(dir.path());
    // [migrating_nodiscard]
    customPlot->removeGraph(0);                  // warning: result ignored
    if (!customPlot->savePng("plot.png"))        // fine
        qWarning() << "export failed";
    // [/migrating_nodiscard]
    QDir::setCurrent(previous);
}
QT_WARNING_POP
