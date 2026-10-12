// The user-guide snippets, one live plot at a time.
#include "snippets.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>

#include <algorithm>

std::vector<DocSnippet>& doc_snippets()
{
    static std::vector<DocSnippet> all;
    return all;
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
        mList->setCurrentRow(0);
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
