
qcp_benchmark_moc_files = qtmod.compile_moc(sources : 'benchmark.cpp', dependencies:[NeoQCP_dep, qttest])

qcp_benchmark=executable('qcp_benchmark', 'benchmark.cpp', qcp_benchmark_moc_files,
                        dependencies:[NeoQCP_dep, qttest],
                        cpp_args: ['-DQT_WIDGETS_LIB', '-DQT_GUI_LIB']
                        )
benchmark('qcp_benchmark', auto_tests)
