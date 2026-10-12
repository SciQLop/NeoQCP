// The user-guide snippets, one live plot at a time.
#include "snippets.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#endif

std::vector<DocSnippet>& doc_snippets()
{
    static std::vector<DocSnippet> all;
    return all;
}

// The plot named by ?plot=<name> in the page address, so the docs can link to one plot.
QString requested_plot()
{
#ifdef __EMSCRIPTEN__
    const auto search = emscripten::val::global("location")["search"].as<std::string>();
    return QUrlQuery(QString::fromStdString(search).mid(1)).queryItemValue("plot");
#else
    return {};
#endif
}

class Gallery : public QWidget
{
public:
    Gallery()
    {
        auto* layout = new QHBoxLayout(this);
        mList = new QListWidget;
        mList->setMaximumWidth(260);
        layout->addWidget(mList);
        auto names = QStringList();
        for (const auto& snippet : doc_snippets())
            if (snippet.hasImage)
                names.append(QString::fromLatin1(snippet.name));
        // The showcase plots first, then the guide's examples.
        std::stable_partition(names.begin(), names.end(),
                              [](const QString& name) { return name.startsWith("gallery_"); });
        mList->addItems(names);
        connect(mList, &QListWidget::currentTextChanged, this, &Gallery::show_snippet);
        // An empty plot before the first show, so the window composites through QRhi from the
        // start: switching a shown window to QRhi loses the WebGL context. The real plot comes
        // once the window has its real size, which some plots (the orbit's 1:1 axes) depend on.
        mPlot = new QCustomPlot;
        layout->addWidget(mPlot);
        const int row = int(std::max<qsizetype>(0, names.indexOf(requested_plot())));
        QTimer::singleShot(0, this, [this, row] { mList->setCurrentRow(row); });
    }

private:
    void show_snippet(const QString& name)
    {
        delete mPlot;
        mPlot = new QCustomPlot;
        layout()->addWidget(mPlot);
        layout()->activate();
        mPlot->setViewport(mPlot->rect());   // its resize event only comes once shown
        for (const auto& snippet : doc_snippets())
            if (name == QLatin1String(snippet.name))
                snippet.draw(mPlot);
        // The guide only needs pictures, so most snippets set no interactions; here every
        // plot can be explored, with trackpad and touch gestures. Snippets that set their own
        // keep them.
        mPlot->setInteractions(mPlot->interactions() | QCP::iRangeDrag | QCP::iRangeZoom
                               | QCP::iGestures | QCP::iSelectPlottables | QCP::iSelectLegend);
        mPlot->replot();
    }

    QListWidget* mList = nullptr;
    QCustomPlot* mPlot = nullptr;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Gallery gallery;
    gallery.setWindowState(Qt::WindowFullScreen);
    gallery.show();
    return app.exec();
}
