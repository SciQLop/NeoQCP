
big_data_for_tracy_moc_files = qtmod.preprocess(moc_headers : ['mainwindow.h'], dependencies: [NeoQCP_dep])

big_data_for_tracy = executable('big_data_for_tracy',
  'mainwindow.cpp',
  'main.cpp',
  big_data_for_tracy_moc_files,
  dependencies: [NeoQCP_dep]
)
