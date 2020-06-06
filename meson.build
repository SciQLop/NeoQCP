project('QCustomPlot', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

qt5 = import('qt5')
qt5core = dependency('qt5', modules : 'Core')
qt5widgets = dependency('qt5', modules : 'Widgets')
qt5gui = dependency('qt5', modules : 'Gui')
qt5svg = dependency('qt5', modules : 'Svg')
qt5printsupport = dependency('qt5', modules : 'PrintSupport')
qt5test = dependency('qt5', modules : 'Test')


GL = dependency('GL')


qcp_moc_headers = ['src/axis/axis.h',
 'src/axis/axisticker.h',
 'src/axis/axistickerdatetime.h',
 'src/axis/axistickerfixed.h',
 'src/axis/axistickerlog.h',
 'src/axis/axistickerpi.h',
 'src/axis/axistickertext.h',
 'src/axis/axistickertime.h',
 'src/axis/range.h',
 'src/colorgradient.h',
 'src/core.h',
 'src/datacontainer.h',
 'src/global.h',
 'src/item.h',
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
 'src/paintbuffer.h',
 'src/painter.h',
 'src/plottable.h',
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
 'src/vector2d.h']

qcp_moc_files = qt5.preprocess(moc_headers : qcp_moc_headers)

QCustomPlot = library('QCustomPlot',
           'src/colorgradient.cpp',
           'src/core.cpp',
           'src/item.cpp',
           'src/layer.cpp',
           'src/layout.cpp',
           'src/lineending.cpp',
           'src/paintbuffer.cpp',
           'src/painter.cpp',
           'src/plottable.cpp',
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
           'src/colorgradient.cpp',
           'src/core.cpp',
           'src/item.cpp',
           'src/items/item-bracket.cpp',
           'src/items/item-curve.cpp',
           'src/items/item-ellipse.cpp',
           'src/items/item-line.cpp',
           'src/items/item-pixmap.cpp',
           'src/items/item-rect.cpp',
           'src/items/item-straightline.cpp',
           'src/items/item-text.cpp',
           'src/items/item-tracer.cpp',
           'src/layer.cpp',
           'src/layout.cpp',
           'src/layoutelements/layoutelement-axisrect.cpp',
           'src/layoutelements/layoutelement-colorscale.cpp',
           'src/layoutelements/layoutelement-legend.cpp',
           'src/layoutelements/layoutelement-textelement.cpp',
           'src/lineending.cpp',
           'src/paintbuffer.cpp',
           'src/painter.cpp',
           'src/plottable.cpp',
           'src/plottables/plottable-bars.cpp',
           'src/plottables/plottable-colormap.cpp',
           'src/plottables/plottable-curve.cpp',
           'src/plottables/plottable-errorbar.cpp',
           'src/plottables/plottable-financial.cpp',
           'src/plottables/plottable-graph.cpp',
           'src/plottables/plottable-statisticalbox.cpp',
           'src/scatterstyle.cpp',
           'src/selection.cpp',
           'src/selectiondecorator-bracket.cpp',
           'src/selectionrect.cpp',
           'src/vector2d.cpp',
           qcp_moc_files,
           #cpp_args:['-DQCUSTOMPLOT_USE_OPENGL'],
           dependencies : [qt5core, qt5widgets, qt5gui, qt5svg, qt5printsupport, GL],
           install : true)

QCustomPlot_dep = declare_dependency(link_with : QCustomPlot, include_directories:'src', dependencies:[qt5core, qt5widgets, qt5gui, GL])

test_srcs = [
    'tests/auto/autotest.cpp',
    'tests/auto/test-qcustomplot/test-qcustomplot.cpp',
    'tests/auto/test-qcpgraph/test-qcpgraph.cpp',
    'tests/auto/test-qcpcurve/test-qcpcurve.cpp',
    'tests/auto/test-qcpbars/test-qcpbars.cpp',
    'tests/auto/test-qcpfinancial/test-qcpfinancial.cpp',
    'tests/auto/test-qcplayout/test-qcplayout.cpp',
    'tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp',
    'tests/auto/test-colormap/test-colormap.cpp',
    'tests/auto/test-datacontainer/test-datacontainer.cpp',
    'tests/auto/test-qcplegend/test-qcplegend.cpp'
]

test_headers = [
'tests/auto/test-qcustomplot/test-qcustomplot.h',
    'tests/auto/test-qcpgraph/test-qcpgraph.h',
    'tests/auto/test-qcpcurve/test-qcpcurve.h',
    'tests/auto/test-qcpbars/test-qcpbars.h',
    'tests/auto/test-qcpfinancial/test-qcpfinancial.h',
    'tests/auto/test-qcplayout/test-qcplayout.h',
    'tests/auto/test-qcpaxisrect/test-qcpaxisrect.h',
    'tests/auto/test-colormap/test-colormap.h',
    'tests/auto/test-datacontainer/test-datacontainer.h',
    'tests/auto/test-qcplegend/test-qcplegend.h'
]
test_moc_files = qt5.preprocess(moc_headers : test_headers)

auto_tests=executable('auto-tests',test_srcs, test_moc_files, dependencies:[QCustomPlot_dep, qt5test])
test('test-auto-tests',auto_tests)
