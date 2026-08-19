# Only JKQTMathText (plus the jkqtcommon it leans on) out of JKQtPlotter: the
# LaTeX parser/renderer, none of the plotting stack.
#
# This build file exists because meson's CMake subproject support cannot drive
# CMake's AUTOMOC (mesonbuild/meson#7959), which upstream relies on -- so the
# Q_OBJECT headers are listed explicitly and moc'd the way NeoQCP does it.
project('jkqtmathtext', 'cpp',
    version: '5.0.0',
    license: 'LGPL-2.1-or-later',
    default_options: ['cpp_std=c++17'])

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])

jkqtmathtext_inc = include_directories('lib')

# The only Q_OBJECT carriers in what we build. jkqtcommon has none.
jkqtmathtext_moc_headers = [
    'lib/jkqtmathtext/jkqtmathtext.h',
    'lib/jkqtmathtext/jkqtmathtextlabel.h',
    'lib/jkqtmathtext/parsers/jkqtmathtextparser.h',
    'lib/jkqtmathtext/parsers/jkqtmathtextlatexparser.h',
]

# XITS only: Fira Math is an alternative style we never select, and skipping it
# keeps ~0.2 MB and one more font out of every wheel. Embedding rather than
# shipping .otf as package data avoids any runtime path lookup inside a wheel.
jkqtmathtext_resources = qt6.compile_resources(
    sources: 'lib/jkqtmathtext/resources/xits.qrc')

jkqtmathtext_sources = files(
    'lib/jkqtmathtext/jkqtmathtext.cpp',
    'lib/jkqtmathtext/jkqtmathtextlabel.cpp',
    'lib/jkqtmathtext/jkqtmathtexttools.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextboxinstructionnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextbracenode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextdecoratednode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextfracnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtexthorizontallistnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextinstructionnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextmatrixnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextmodifyenvironmentnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextnodetools.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextnoopnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextsqrtnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextsubsupernode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextsymbolnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtexttextnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextverbatimnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextverticallistnode.cpp',
    'lib/jkqtmathtext/nodes/jkqtmathtextwhitespacenode.cpp',
    'lib/jkqtmathtext/parsers/jkqtmathtextlatexparser.cpp',
    'lib/jkqtmathtext/parsers/jkqtmathtextparser.cpp',
)

jkqtcommon_sources = files(
    'lib/jkqtcommon/jkqtpbasicimagetools.cpp',
    'lib/jkqtcommon/jkqtpcachingtools.cpp',
    'lib/jkqtcommon/jkqtpcodestructuring.cpp',
    'lib/jkqtcommon/jkqtpconcurrencytools.cpp',
    'lib/jkqtcommon/jkqtpcsstools.cpp',
    'lib/jkqtcommon/jkqtpdebuggingtools.cpp',
    'lib/jkqtcommon/jkqtpdrawingtools.cpp',
    'lib/jkqtcommon/jkqtpenhancedpainter.cpp',
    'lib/jkqtcommon/jkqtpexpected.cpp',
    'lib/jkqtcommon/jkqtpgeometrytools.cpp',
    'lib/jkqtcommon/jkqtphighrestimer.cpp',
    'lib/jkqtcommon/jkqtpicons.cpp',
    'lib/jkqtcommon/jkqtpmathtools.cpp',
    'lib/jkqtcommon/jkqtpstringtools.cpp',
    'lib/jkqtcommon/jkqttools.cpp',
)

jkqtmathtext_lib = static_library('jkqtmathtext',
    jkqtmathtext_sources,
    jkqtcommon_sources,
    jkqtmathtext_resources,
    qt6.compile_moc(headers: jkqtmathtext_moc_headers,
                    include_directories: jkqtmathtext_inc),
    include_directories: jkqtmathtext_inc,
    dependencies: qt6_dep,
    # Upstream is not warning-clean and it is not ours to fix.
    cpp_args: ['-w', '-DJKQTMATHTEXT_COMPILED_WITH_XITS'])

# link_whole, not link_with: the font qresource registers itself through a
# static initializer, which a linker is free to drop from an archive member
# nothing else references -- the failure mode is silent fallback to system
# fonts, not a link error.
jkqtmathtext_dep = declare_dependency(
    link_whole: jkqtmathtext_lib,
    include_directories: jkqtmathtext_inc,
    dependencies: qt6_dep)
