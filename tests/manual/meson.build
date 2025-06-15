
manual_moc_files = qtmod.preprocess(moc_headers : ['mainwindow.h'], ui_files: ['mainwindow.ui'], dependencies: [NeoQCP_dep])

manual = executable('manual',
  'mainwindow.cpp',
  'main.cpp',
  manual_moc_files,
  dependencies: [NeoQCP_dep]
)
