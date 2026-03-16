project('NeoQCP', 'cpp',
  version : '2.1.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'buildtype=release'],
  meson_version : '>=1.1.0',
  license : 'GPL3',
  )

qtmod = import('qt6')
qtdeps = dependency('qt6', modules : ['Core','Widgets','Gui','Svg','PrintSupport'], private_headers: true)
qttest = dependency('qt6', modules : ['Test'])

optional_deps = []
cpp_args = []

if get_option ( 'tracy_enable')
    if get_option ( 'buildtype') != 'debugoptimized'
        warning ( 'Profiling builds should set -- buildtype = debugoptimized')
    endif
    tracy = dependency ( 'tracy', static : true )
    optional_deps += [ tracy ]
endif

config_data = configuration_data()
version = meson.project_version().split('.')
config_data.set('NEOQCP_VERSION_MAJOR', version[0])
config_data.set('NEOQCP_VERSION_MINOR', version[1])
config_data.set('NEOQCP_VERSION_PATCH', version[2])
config_data.set_quoted('NEOQCP_VERSION', meson.project_version())

config_data.set('QCP_DEVICEPIXELRATIO_SUPPORTED', true)
config_data.set('QCP_DEVICEPIXELRATIO_FLOAT', true)

config_h = configure_file(
  output : 'neoqcp_config.h',
  configuration : config_data
)

NEOQCP_INCLUDE_DIR = include_directories('src','.')


neoqcp_moc_headers = ['src/axis/axis.h',
            'src/axis/axisticker.h',
            'src/axis/axistickerdatetime.h',
            'src/axis/axistickerfixed.h',
            'src/axis/axistickerlog.h',
            'src/axis/axistickerpi.h',
            'src/axis/axistickertext.h',
            'src/axis/axistickertime.h',
            'src/axis/range.h',
            'src/axis/labelpainter.h',
            'src/colorgradient.h',
            'src/theme.h',
            'src/core.h',
            'src/datacontainer.h',
            'src/global.h',
            'src/items/item.h',
            'src/items/item-bracket.h',
            'src/items/item-curve.h',
            'src/items/item-ellipse.h',
            'src/items/item-line.h',
            'src/items/item-pixmap.h',
            'src/items/item-rect.h',
            'src/items/item-straightline.h',
            'src/items/item-text.h',
            'src/items/item-richtext.h',
            'src/items/item-tracer.h',
            'src/items/item-hspan.h',
            'src/items/item-rspan.h',
            'src/items/item-vspan.h',
            'src/layer.h',
            'src/layout.h',
            'src/layoutelements/layoutelement-axisrect.h',
            'src/layoutelements/layoutelement-colorscale.h',
            'src/layoutelements/layoutelement-legend.h',
            'src/layoutelements/layoutelement-legend-group.h',
            'src/layoutelements/layoutelement-textelement.h',
            'src/lineending.h',
            'src/painting/paintbuffer.h',
            'src/painting/painter.h',
            'src/painting/paintbuffer-pixmap.h',
            'src/plottables/plottable.h',
            'src/plottables/plottable-bars.h',
            'src/plottables/plottable-colormap.h',
            'src/plottables/plottable-curve.h',
            'src/plottables/plottable-errorbar.h',
            'src/plottables/plottable-financial.h',
            'src/plottables/plottable-graph.h',
            'src/plottables/plottable-graph2.h',
            'src/plottables/plottable-multigraph.h',
            'src/plottables/plottable-waterfall.h',
            'src/plottables/plottable-statisticalbox.h',
            'src/datasource/pipeline-scheduler.h',
            'src/datasource/async-pipeline.h',
            'src/plottables/plottable-colormap2.h',
            'src/plottables/plottable-histogram2d.h',
            'src/qcp.h',
            'src/scatterstyle.h',
            'src/selection.h',
            'src/selectiondecorator-bracket.h',
            'src/selectionrect.h',
            'src/vector2d.h',
            'src/polar/layoutelement-angularaxis.h',
            'src/polar/polargraph.h',
            'src/polar/polargrid.h',
            'src/polar/radialaxis.h',
            ]

neoqcp_moc_files = qtmod.compile_moc(headers: neoqcp_moc_headers, include_directories: NEOQCP_INCLUDE_DIR)


qmake = find_program('qmake6', 'qmake')
qt_libexec = run_command(qmake, '-query', 'QT_HOST_LIBEXECS', check: true).stdout().strip()
qt_bin = run_command(qmake, '-query', 'QT_HOST_BINS', check: true).stdout().strip()
qsb = find_program('qsb', dirs: [qt_libexec, qt_bin])

composite_vert_qsb = custom_target('composite_vert_qsb',
    input: 'src/painting/shaders/composite.vert',
    output: 'composite.vert.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])

composite_frag_qsb = custom_target('composite_frag_qsb',
    input: 'src/painting/shaders/composite.frag',
    output: 'composite.frag.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])

plottable_vert_qsb = custom_target('plottable_vert_qsb',
    input: 'src/painting/shaders/plottable.vert',
    output: 'plottable.vert.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])

plottable_frag_qsb = custom_target('plottable_frag_qsb',
    input: 'src/painting/shaders/plottable.frag',
    output: 'plottable.frag.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])

python3 = find_program('python3')
embedded_shaders = custom_target('embedded_shaders',
    input: [composite_vert_qsb, composite_frag_qsb, plottable_vert_qsb, plottable_frag_qsb],
    output: 'embedded_shaders.h',
    command: [python3, files('src/painting/shaders/embed_shaders.py'),
              '@OUTPUT@',
              'composite_vert_qsb_data:@INPUT0@',
              'composite_frag_qsb_data:@INPUT1@',
              'plottable_vert_qsb_data:@INPUT2@',
              'plottable_frag_qsb_data:@INPUT3@'])

NeoQCP = static_library('NeoQCP',
           'src/colorgradient.cpp',
           'src/theme.cpp',
           'src/core.cpp',
           'src/layer.cpp',
           'src/layout.cpp',
           'src/lineending.cpp',
           'src/painting/paintbuffer.cpp',
           'src/painting/painter.cpp',
           'src/painting/paintbuffer-pixmap.cpp',
           'src/painting/paintbuffer-rhi.cpp',
           'src/painting/plottable-rhi-layer.cpp',
           'src/painting/colormap-rhi-layer.cpp',
           'src/painting/colormap-renderer.cpp',
           'src/painting/line-extruder.cpp',
           'src/scatterstyle.cpp',
           'src/selection.cpp',
           'src/selectiondecorator-bracket.cpp',
           'src/selectionrect.cpp',
           'src/vector2d.cpp',
           'src/axis/axis.cpp',
           'src/axis/axisticker.cpp',
           'src/axis/axistickerdatetime.cpp',
           'src/axis/axistickerfixed.cpp',
           'src/axis/axistickerlog.cpp',
           'src/axis/axistickerpi.cpp',
           'src/axis/axistickertext.cpp',
           'src/axis/axistickertime.cpp',
           'src/axis/range.cpp',
           'src/axis/labelpainter.cpp',
           'src/items/item.cpp',
           'src/items/item-bracket.cpp',
           'src/items/item-curve.cpp',
           'src/items/item-ellipse.cpp',
           'src/items/item-line.cpp',
           'src/items/item-pixmap.cpp',
           'src/items/item-rect.cpp',
           'src/items/item-straightline.cpp',
           'src/items/item-text.cpp',
           'src/items/item-richtext.cpp',
           'src/items/item-tracer.cpp',
           'src/items/item-hspan.cpp',
           'src/items/item-rspan.cpp',
           'src/items/item-vspan.cpp',
           'src/layoutelements/layoutelement-axisrect.cpp',
           'src/layoutelements/layoutelement-colorscale.cpp',
           'src/layoutelements/layoutelement-legend.cpp',
           'src/layoutelements/layoutelement-legend-group.cpp',
           'src/layoutelements/layoutelement-textelement.cpp',
           'src/plottables/plottable.cpp',
           'src/plottables/plottable-bars.cpp',
           'src/plottables/plottable-colormap.cpp',
           'src/plottables/plottable-curve.cpp',
           'src/plottables/plottable-errorbar.cpp',
           'src/plottables/plottable-financial.cpp',
           'src/plottables/plottable-graph.cpp',
           'src/plottables/plottable-graph2.cpp',
           'src/plottables/plottable-multigraph.cpp',
           'src/plottables/plottable-waterfall.cpp',
           'src/datasource/resample.cpp',
           'src/datasource/pipeline-scheduler.cpp',
           'src/datasource/async-pipeline.cpp',
           'src/plottables/plottable-colormap2.cpp',
           'src/plottables/plottable-histogram2d.cpp',
           'src/plottables/plottable-statisticalbox.cpp',
           'src/data-locator.cpp',
           'src/polar/layoutelement-angularaxis.cpp',
           'src/polar/polargraph.cpp',
           'src/polar/polargrid.cpp',
           'src/polar/radialaxis.cpp',
           embedded_shaders,
           neoqcp_moc_files,
           include_directories: NEOQCP_INCLUDE_DIR,
           cpp_args:cpp_args,
           dependencies: [qtdeps] + optional_deps,
           install: true,
           extra_files: [neoqcp_moc_headers, 'src/Profiling.hpp', 'src/painting/paintbuffer-rhi.h', 'src/painting/plottable-rhi-layer.h', 'src/painting/colormap-rhi-layer.h']
           )



NeoQCP_dep = declare_dependency(link_with: NeoQCP,
                                include_directories: NEOQCP_INCLUDE_DIR,
                                dependencies: [qtdeps] + optional_deps,
                                compile_args: cpp_args)

if get_option ( 'with_examples')
    subdir('examples')
endif

if get_option ( 'with_tests')
    subdir('tests')
endif

