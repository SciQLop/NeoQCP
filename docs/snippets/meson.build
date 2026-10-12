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
    'migrating.cpp',
    'performance.cpp',
    'polar.cpp',
)

doc_shots = executable('doc-shots', doc_snippet_srcs,
                       dependencies: [NeoQCP_dep, qtdeps],
                       override_options: ['unity=off'])
test('doc-snippets', doc_shots, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 300)

# The guide's complete programs. The window one blocks in app.exec(), so it is only built.
executable('doc-first-window', 'programs/first-window.cpp', dependencies: [NeoQCP_dep, qtdeps])
doc_offscreen_export = executable('doc-offscreen-export', 'programs/offscreen-export.cpp',
                                  dependencies: [NeoQCP_dep, qtdeps])
test('doc-offscreen-export', doc_offscreen_export, workdir: meson.current_build_dir())
