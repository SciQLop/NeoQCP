
text_document_integration_moc_files = qtmod.preprocess(moc_headers : ['mainwindow.h', 'qcpdocumentobject.h'], ui_files: ['mainwindow.ui'], dependencies: [QCustomPlot_dep])

text_document_integration = executable('text_document_integration',
  'mainwindow.cpp',
  'qcpdocumentobject.cpp',
  'main.cpp',
  text_document_integration_moc_files,
  dependencies: [QCustomPlot_dep]
)
