
axis_tags_moc_files = qtmod.preprocess(moc_headers : ['axistag.h', 'mainwindow.h'], ui_files: ['mainwindow.ui'], dependencies: [QCustomPlot_dep])

axis_tags = executable('axis-tags',
  'axistag.cpp',
  'mainwindow.cpp',
  'main.cpp',
  axis_tags_moc_files,
  dependencies: [QCustomPlot_dep]
)
