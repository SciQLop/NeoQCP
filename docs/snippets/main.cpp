// doc-shots [output-dir] [name ...]
//
// Runs every user-guide snippet on its own offscreen plot. With an output directory, saves
// each one as <output-dir>/<name>.png (names with '_' become '-'); with names, only those.
// Without arguments it is the doc-snippets test: every snippet must run and draw.
#include "snippets.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>

#include <cstdio>

std::vector<DocSnippet>& doc_snippets()
{
    static std::vector<DocSnippet> all;
    return all;
}

namespace
{
// Graph2, MultiGraph and ColorMap2 resample on worker threads: wait for them, or the
// picture shows a plot that has not received its data yet.
bool wait_until_idle(QCustomPlot& plot)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000)
    {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        bool busy = false;
        for (int i = 0; i < plot.plottableCount(); ++i)
            busy = busy || plot.plottable(i)->busy();
        if (!busy)
            return true;
    }
    return false;
}

QString image_name(const char* name)
{
    return QString::fromLatin1(name).replace('_', '-') + QStringLiteral(".png");
}

bool selected(const DocSnippet& snippet, const QStringList& names)
{
    return names.isEmpty() || names.contains(QString::fromLatin1(snippet.name));
}

bool render(const DocSnippet& snippet, const QString& output_dir)
{
    QCustomPlot plot;
    plot.resize(snippet.size);
    snippet.draw(&plot);
    plot.replot();
    if (!wait_until_idle(plot))
    {
        std::fprintf(stderr, "%s: plottables still busy after 10 s\n", snippet.name);
        return false;
    }
    plot.replot();
    if (output_dir.isEmpty())
        return true;
    const QString path = QDir(output_dir).filePath(image_name(snippet.name));
    if (!plot.savePng(path, snippet.size.width(), snippet.size.height(), 2.0))
    {
        std::fprintf(stderr, "%s: could not write %s\n", snippet.name, qPrintable(path));
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    const QString output_dir = args.isEmpty() ? QString() : args.takeFirst();

    int failures = 0;
    for (const auto& snippet : doc_snippets())
    {
        if (!selected(snippet, args))
            continue;
        if (render(snippet, output_dir))
            std::printf("ok   %s\n", snippet.name);
        else
            ++failures;
    }
    return failures == 0 ? 0 : 1;
}
