# Draws the images the API reference (Doxygen) refers to, into documentation/images.
# Run: doc-image-generator <output-directory>
doc_image_generator = executable('doc-image-generator',
    'main.cpp', 'mainwindow.cpp',
    qtmod.compile_moc(headers: 'mainwindow.h', dependencies: NeoQCP_dep),
    cpp_args: '-DGNU_PNG_PATH="@0@"'.format(meson.current_source_dir() / 'gnu.png'),
    dependencies: [NeoQCP_dep, qtdeps],
    override_options: ['unity=off'])
test('doc-image-generator', doc_image_generator,
     args: [meson.current_build_dir() / 'images'],
     env: ['QT_QPA_PLATFORM=offscreen'], timeout: 120)
