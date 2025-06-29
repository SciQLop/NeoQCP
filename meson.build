project('NeoQCP', 'cpp',
  version : '2.1.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'buildtype=release'],
  meson_version : '>=1.1.0',
  license : 'GPL3',
  )

qtmod = import('qt6')
qtdeps = dependency('qt6', modules : ['Core','Widgets','Gui','Svg','PrintSupport', 'OpenGL'], method:'qmake')
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

if get_option ( 'with_opengl')
    GL = dependency('gl', required : true)
    qtopengl_dep = dependency('qt6', modules : ['OpenGL'], method:'qmake')
    optional_deps += [ qtopengl_dep, GL ]
endif

config_data = configuration_data()
version = meson.project_version().split('.')
config_data.set('NEOQCP_VERSION_MAJOR', version[0])
config_data.set('NEOQCP_VERSION_MINOR', version[1])
config_data.set('NEOQCP_VERSION_PATCH', version[2])
config_data.set_quoted('NEOQCP_VERSION', meson.project_version())

config_data.set('QCP_DEVICEPIXELRATIO_SUPPORTED', true)
config_data.set('QCP_DEVICEPIXELRATIO_FLOAT', true)

if get_option ( 'enable_batch_drawing')
    config_data.set('NEOQCP_BATCH_DRAWING', true)
endif
if get_option ( 'with_opengl')
    config_data.set('NEOQCP_USE_OPENGL', true)
    config_data.set('QCUSTOMPLOT_USE_OPENGL', true)
    config_data.set('QCP_OPENGL_FBO', true)
    config_data.set('QCP_OPENGL_OFFSCREENSURFACE', true)
endif

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
            'src/items/item-tracer.h',
            'src/layer.h',
            'src/layout.h',
            'src/layoutelements/layoutelement-axisrect.h',
            'src/layoutelements/layoutelement-colorscale.h',
            'src/layoutelements/layoutelement-legend.h',
            'src/layoutelements/layoutelement-textelement.h',
            'src/lineending.h',
            'src/painting/paintbuffer.h',
            'src/painting/painter.h',
            'src/painting/paintbuffer-pixmap.h',
            'src/painting/paintbuffer-glfbo.h',
            'src/plottables/plottable.h',
            'src/plottables/plottable-bars.h',
            'src/plottables/plottable-colormap.h',
            'src/plottables/plottable-curve.h',
            'src/plottables/plottable-errorbar.h',
            'src/plottables/plottable-financial.h',
            'src/plottables/plottable-graph.h',
            'src/plottables/plottable-statisticalbox.h',
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


NeoQCP = static_library('NeoQCP',
           'src/colorgradient.cpp',
           'src/core.cpp',
           'src/layer.cpp',
           'src/layout.cpp',
           'src/lineending.cpp',
           'src/painting/paintbuffer.cpp',
           'src/painting/painter.cpp',
           'src/painting/paintbuffer-pixmap.cpp',
           'src/painting/paintbuffer-glfbo.cpp',
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
           'src/items/item-tracer.cpp',
           'src/layoutelements/layoutelement-axisrect.cpp',
           'src/layoutelements/layoutelement-colorscale.cpp',
           'src/layoutelements/layoutelement-legend.cpp',
           'src/layoutelements/layoutelement-textelement.cpp',
           'src/plottables/plottable.cpp',
           'src/plottables/plottable-bars.cpp',
           'src/plottables/plottable-colormap.cpp',
           'src/plottables/plottable-curve.cpp',
           'src/plottables/plottable-errorbar.cpp',
           'src/plottables/plottable-financial.cpp',
           'src/plottables/plottable-graph.cpp',
           'src/plottables/plottable-statisticalbox.cpp',
           'src/polar/layoutelement-angularaxis.cpp',
           'src/polar/polargraph.cpp',
           'src/polar/polargrid.cpp',
           'src/polar/radialaxis.cpp',
           neoqcp_moc_files,
           include_directories: NEOQCP_INCLUDE_DIR,
           cpp_args:cpp_args,
           dependencies: [qtdeps] + optional_deps,
           install: true,
           extra_files: [neoqcp_moc_headers, 'src/Profiling.hpp']
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

