# The user guide's examples, one file per guide page. See snippets.h.
# The online gallery builds them too (examples/wasm-gallery), so the list is always defined.
doc_snippet_pages = files(
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
    'gallery.cpp',
    'migrating.cpp',
    'performance.cpp',
    'polar.cpp',
)

if get_option('with_tests') and not is_wasm
    doc_shots = executable('doc-shots', files('main.cpp') + doc_snippet_pages,
                           dependencies: [NeoQCP_dep, qtdeps],
                           override_options: ['unity=off'])
    test('doc-snippets', doc_shots, env: ['QT_QPA_PLATFORM=offscreen'], timeout: 300)

    # The guide's complete programs. The window one blocks in app.exec(), so it is only built.
    executable('doc-first-window', 'programs/first-window.cpp', dependencies: [NeoQCP_dep, qtdeps])
    doc_offscreen_export = executable('doc-offscreen-export', 'programs/offscreen-export.cpp',
                                      dependencies: [NeoQCP_dep, qtdeps])
    test('doc-offscreen-export', doc_offscreen_export, workdir: meson.current_build_dir())
endif
