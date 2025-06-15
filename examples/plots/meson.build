
plots_moc_files = qtmod.preprocess(moc_headers : ['mainwindow.h'], ui_files: ['mainwindow.ui'], dependencies: [QCustomPlot_dep])

plots = executable('plots',
  'mainwindow.cpp',
  'main.cpp',
  plots_moc_files,
  dependencies: [NeoQCP_dep]
)
