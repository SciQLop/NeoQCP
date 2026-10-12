// Every code example in the user guide is a DOC_SNIPPET: compiled and run by the
// doc-snippets test, and drawn to docs/images/<name>.png by the doc-shots program.
// A DOC_CODE example is compiled and run the same way, but has no picture.
// The guide pages include the lines between the "// [name]" and "// [/name]" markers.
#pragma once

#include <qcustomplot.h>

#include <functional>
#include <vector>

struct DocSnippet
{
    const char* name;
    QSize size;
    std::function<void(QCustomPlot*)> draw;
    bool hasImage;
};

std::vector<DocSnippet>& doc_snippets();

struct DocSnippetRegistrar
{
    DocSnippetRegistrar(const char* name, QSize size, std::function<void(QCustomPlot*)> draw,
                        bool hasImage = true)
    {
        doc_snippets().push_back({ name, size, std::move(draw), hasImage });
    }
};

#define DOC_SNIPPET(name, width, height)                                                       \
    static void name(QCustomPlot* customPlot);                                                 \
    static const DocSnippetRegistrar name##_registrar { #name, QSize(width, height), name };  \
    static void name(QCustomPlot* customPlot)

#define DOC_CODE(name)                                                                         \
    static void name(QCustomPlot* customPlot);                                                 \
    static const DocSnippetRegistrar name##_registrar { #name, QSize(600, 360), name, false }; \
    static void name(QCustomPlot* customPlot)
