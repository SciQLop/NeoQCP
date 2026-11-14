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
    customPlot->add<QCPGraph>();
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

DOC_CODE(migrating_handles)
{
    // [migrating_handles]
    auto rect = customPlot->axisRect();
    QCPAxisRect* rawRect = customPlot->axisRect().get();
    QList<QCPAxis*> axes = qcp::pointers(rect->axes());
    QPointer<QCPAxisRect> guarded(customPlot->axisRect());
    // [/migrating_handles]
    rawRect->setupFullAxesBox(true);
    Q_UNUSED(axes);
    Q_UNUSED(guarded);
}

DOC_CODE(migrating_bindings)
{
    // [migrating_bindings]
    QProperty<QString> unit(QStringLiteral("nT"));
    QBindable<QString> label(customPlot->yAxis, "label");
    label.setBinding([&unit] { return QStringLiteral("B (%1)").arg(unit.value()); });
    unit = QStringLiteral("T"); // the setter runs: the label is now "B (T)"
    // [/migrating_bindings]
    if (customPlot->yAxis->label() != QStringLiteral("B (T)"))
        qFatal("migrating_bindings: the binding did not drive the setter");
    label.takeBinding(); // unit dies with this function
}

DOC_CODE(migrating_binding_guarded)
{
    // [migrating_binding_guarded]
    QPointer<QCPAxis> source(customPlot->xAxis);
    QBindable<QString>(customPlot->yAxis, "label").setBinding([source] {
        return source ? QBindable<QString>(source.data(), "label").value() : QString();
    });
    // [/migrating_binding_guarded]
    customPlot->xAxis->setLabel(QStringLiteral("time"));
    if (customPlot->yAxis->label() != QStringLiteral("time"))
        qFatal("migrating_binding_guarded: the binding did not follow its source");
}
