
test_srcs = [
    'auto/autotest.cpp',
    'auto/test-qcustomplot/test-qcustomplot.cpp',
    'auto/test-qcpgraph/test-qcpgraph.cpp',
    'auto/test-qcpcurve/test-qcpcurve.cpp',
    'auto/test-qcpbars/test-qcpbars.cpp',
    'auto/test-qcpfinancial/test-qcpfinancial.cpp',
    'auto/test-qcplayout/test-qcplayout.cpp',
    'auto/test-qcpaxisrect/test-qcpaxisrect.cpp',
    'auto/test-colormap/test-colormap.cpp',
    'auto/test-datacontainer/test-datacontainer.cpp',
    'auto/test-qcplegend/test-qcplegend.cpp'
]

test_headers = [
    'auto/test-qcustomplot/test-qcustomplot.h',
    'auto/test-qcpgraph/test-qcpgraph.h',
    'auto/test-qcpcurve/test-qcpcurve.h',
    'auto/test-qcpbars/test-qcpbars.h',
    'auto/test-qcpfinancial/test-qcpfinancial.h',
    'auto/test-qcplayout/test-qcplayout.h',
    'auto/test-qcpaxisrect/test-qcpaxisrect.h',
    'auto/test-colormap/test-colormap.h',
    'auto/test-datacontainer/test-datacontainer.h',
    'auto/test-qcplegend/test-qcplegend.h'
]
test_moc_files = qtmod.compile_moc(headers : test_headers)

auto_tests=executable('auto-tests',test_srcs, test_moc_files, dependencies:[QCustomPlot_dep, qtdeps, qttest])
test('test-auto-tests', auto_tests)
