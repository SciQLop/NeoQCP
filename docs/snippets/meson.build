# The user guide's examples, one file per guide page. See snippets.h.
doc_snippet_srcs = files(
    'main.cpp',
    'getting-started.cpp',
    'concepts.cpp',
    'graphs.cpp',
    'plottables.cpp',
    'colormaps.cpp',
    'axes.cpp',
    'layout.cpp',
    'items.cpp',
    'interactions.cpp',
    'styling.cpp',
    'export.cpp',
    'performance.cpp',
    'polar.cpp',
)

doc_shots = executable('doc-shots', doc_snippet_srcs,
                       dependencies: [NeoQCP_dep, qtdeps],
                       override_options: ['unity=off'])
test('doc-snippets', doc_shots, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 300)
