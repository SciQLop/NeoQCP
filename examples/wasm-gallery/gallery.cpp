// The user-guide snippets, one live plot at a time.
#include "snippets.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>

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
        for (const auto& snippet : doc_snippets())
            if (snippet.hasImage)
                mList->addItem(QString::fromLatin1(snippet.name));
        connect(mList, &QListWidget::currentTextChanged, this, &Gallery::show_snippet);
        mList->setCurrentRow(0);
    }

private:
    void show_snippet(const QString& name)
    {
        delete mPlot;
        mPlot = new QCustomPlot;
        layout()->addWidget(mPlot);
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
