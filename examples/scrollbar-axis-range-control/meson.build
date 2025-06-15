
scrollbar_axis_range_control_moc_files = qtmod.preprocess(moc_headers : ['mainwindow.h'], ui_files: ['mainwindow.ui'], dependencies: [QCustomPlot_dep])

scrollbar_axis_range_control = executable('scrollbar_axis_range_control',
  'mainwindow.cpp',
  'main.cpp',
  scrollbar_axis_range_control_moc_files,
  dependencies: [NeoQCP_dep]
)
