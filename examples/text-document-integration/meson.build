
# moc takes -I paths only: from the system-include Qt it would not find QTextObjectInterface's
# Q_DECLARE_INTERFACE ("Undefined interface").
text_document_integration_moc_files = qtmod.preprocess(moc_headers : ['mainwindow.h', 'qcpdocumentobject.h'], ui_files: ['mainwindow.ui'], dependencies: [NeoQCP_dep, qtdeps.as_system('non-system')])

text_document_integration = executable('text_document_integration',
  'mainwindow.cpp',
  'qcpdocumentobject.cpp',
  'main.cpp',
  text_document_integration_moc_files,
  dependencies: [NeoQCP_dep]
)
