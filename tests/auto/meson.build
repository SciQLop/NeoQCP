
test_srcs = [
    'autotest.cpp',
    'test-qcustomplot/test-qcustomplot.cpp',
    'test-qcpgraph/test-qcpgraph.cpp',
    'test-qcpcurve/test-qcpcurve.cpp',
    'test-qcpbars/test-qcpbars.cpp',
    'test-qcpfinancial/test-qcpfinancial.cpp',
    'test-qcplayout/test-qcplayout.cpp',
    'test-qcpaxisrect/test-qcpaxisrect.cpp',
    'test-colormap/test-colormap.cpp',
    'test-datacontainer/test-datacontainer.cpp',
    'test-qcplegend/test-qcplegend.cpp'
]

test_headers = [
    'test-qcustomplot/test-qcustomplot.h',
    'test-qcpgraph/test-qcpgraph.h',
    'test-qcpcurve/test-qcpcurve.h',
    'test-qcpbars/test-qcpbars.h',
    'test-qcpfinancial/test-qcpfinancial.h',
    'test-qcplayout/test-qcplayout.h',
    'test-qcpaxisrect/test-qcpaxisrect.h',
    'test-colormap/test-colormap.h',
    'test-datacontainer/test-datacontainer.h',
    'test-qcplegend/test-qcplegend.h'
]
test_moc_files = qtmod.compile_moc(headers : test_headers)

auto_tests=executable('auto-tests',test_srcs, test_moc_files, dependencies:[NeoQCP_dep, qtdeps, qttest])
test('test-auto-tests', auto_tests)
