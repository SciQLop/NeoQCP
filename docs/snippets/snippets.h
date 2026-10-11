// Every code example in the user guide is a DOC_SNIPPET: compiled and run by the
// doc-snippets test, and drawn to docs/images/<name>.png by the doc-shots program.
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
};

std::vector<DocSnippet>& doc_snippets();

struct DocSnippetRegistrar
{
    DocSnippetRegistrar(const char* name, QSize size, std::function<void(QCustomPlot*)> draw)
    {
        doc_snippets().push_back({ name, size, std::move(draw) });
    }
};

#define DOC_SNIPPET(name, width, height)                                                       \
    static void name(QCustomPlot* customPlot);                                                 \
    static const DocSnippetRegistrar name##_registrar { #name, QSize(width, height), name };  \
    static void name(QCustomPlot* customPlot)
