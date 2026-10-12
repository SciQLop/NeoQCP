# The user-guide snippets as a live gallery, built for the browser (Qt for WebAssembly).
#
# From the repository root:
#   source ~/emsdk/emsdk_env.sh                      # Emscripten 4.0.7 for Qt 6.11
#   printf "[constants]\nqt_wasm = '%s'\n" ~/Qt/6.11.2/wasm_multithread > qt.ini
#   meson setup build-wasm --cross-file qt.ini --cross-file examples/wasm-gallery/emscripten.ini
#   meson compile -C build-wasm && examples/wasm-gallery/build_site.sh build-wasm/examples/wasm-gallery _site
#   (cd _site && python3 ../examples/wasm-gallery/serve.py 8765)   # http://127.0.0.1:8765/?plot=gallery_orbit
# CI (.github/workflows/pages.yml) does the same and publishes _site to GitHub Pages.

qt_wasm = qt_prefix

# Qt is static here, and meson links only its module archives: the bundled libraries, the
# resource objects and the plugins come from Qt's .prl files, as Qt's CMake does.
qt_static_link = run_command(python3, files('qt_static_link.py'), qt_wasm,
    'Svg', 'PrintSupport', 'Widgets', 'OpenGL', 'Gui', 'Core',
    '--plugins', 'platforms/qwasm', 'imageformats/qgif', 'imageformats/qico',
    'imageformats/qjpeg', 'imageformats/qsvg', 'iconengines/qsvgicon',
    check: true).stdout().strip().split('\n')

# What qt_add_executable sets for a wasm target: the page's qtloader.js calls
# neoqcp_gallery_entry, and threads run on a pool of web workers.
emscripten_link_args = [
    '-sMODULARIZE=1', '-sEXPORT_NAME=neoqcp_gallery_entry',
    '-sEXPORTED_RUNTIME_METHODS=UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,FS,callMain',
    '-sEXPORTED_FUNCTIONS=_main,__embind_initialize_bindings',
    '-sPTHREAD_POOL_SIZE=4', '-sINITIAL_MEMORY=256MB', '-sMAXIMUM_MEMORY=4GB',
    '-sALLOW_MEMORY_GROWTH', '-sSTACK_SIZE=5MB', '-sMAX_WEBGL_VERSION=2', '-sWASM_BIGINT=1',
    '-sERROR_ON_UNDEFINED_SYMBOLS=1', '-sFETCH', '-lembind',
]
# Emscripten optimises the wasm (wasm-opt) and minifies the JavaScript at link time.
if get_option('optimization') not in ['plain', '0']
    emscripten_link_args += '-O' + get_option('optimization')
endif

executable('neoqcp_gallery', 'gallery.cpp', doc_snippet_pages,
    include_directories: include_directories('../../docs/snippets'),
    dependencies: [NeoQCP_dep, qtdeps],
    link_args: qt_static_link + emscripten_link_args,
    override_options: ['unity=off'])

fs = import('fs')
fs.copyfile(qt_wasm / 'plugins/platforms/qtloader.js')
