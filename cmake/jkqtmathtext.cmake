# JKQTMathText (plus the jkqtcommon it leans on) out of JKQtPlotter, from the same files and
# revision as the Meson overlay in subprojects/packagefiles/jkqtmathtext. JKQtPlotter's own CMake
# would build its whole plotting stack, so only the sources are fetched.
include(FetchContent)
FetchContent_Declare(jkqtplotter
    GIT_REPOSITORY https://github.com/jkriege2/JKQtPlotter.git
    GIT_TAG v5.0.0-beta2
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-configure)
FetchContent_MakeAvailable(jkqtplotter)

set(JKQT_LIB ${jkqtplotter_SOURCE_DIR}/lib)

# XITS only, embedded, as in the Meson overlay.
set(JKQTMATHTEXT_SOURCES
    ${JKQT_LIB}/jkqtmathtext/jkqtmathtext.h
    ${JKQT_LIB}/jkqtmathtext/jkqtmathtextlabel.h
    ${JKQT_LIB}/jkqtmathtext/parsers/jkqtmathtextparser.h
    ${JKQT_LIB}/jkqtmathtext/parsers/jkqtmathtextlatexparser.h
    ${JKQT_LIB}/jkqtmathtext/jkqtmathtext.cpp
    ${JKQT_LIB}/jkqtmathtext/jkqtmathtextlabel.cpp
    ${JKQT_LIB}/jkqtmathtext/jkqtmathtexttools.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextboxinstructionnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextbracenode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextdecoratednode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextfracnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtexthorizontallistnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextinstructionnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextmatrixnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextmodifyenvironmentnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextnodetools.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextnoopnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextsqrtnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextsubsupernode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextsymbolnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtexttextnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextverbatimnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextverticallistnode.cpp
    ${JKQT_LIB}/jkqtmathtext/nodes/jkqtmathtextwhitespacenode.cpp
    ${JKQT_LIB}/jkqtmathtext/parsers/jkqtmathtextlatexparser.cpp
    ${JKQT_LIB}/jkqtmathtext/parsers/jkqtmathtextparser.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpbasicimagetools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpcachingtools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpcodestructuring.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpconcurrencytools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpcsstools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpdebuggingtools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpdrawingtools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpenhancedpainter.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpexpected.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpgeometrytools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtphighrestimer.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpicons.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpmathtools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqtpstringtools.cpp
    ${JKQT_LIB}/jkqtcommon/jkqttools.cpp
    ${JKQT_LIB}/jkqtmathtext/resources/xits.qrc)

# An object library: its objects go inside libNeoQCP, like Meson's link_whole, so consumers link
# nothing more. No public NeoQCP header includes a JKQTMathText one.
add_library(jkqtmathtext OBJECT ${JKQTMATHTEXT_SOURCES})
set_target_properties(jkqtmathtext PROPERTIES AUTORCC ON CXX_STANDARD 17 POSITION_INDEPENDENT_CODE ON)
target_include_directories(jkqtmathtext PUBLIC ${JKQT_LIB})
target_compile_definitions(jkqtmathtext PRIVATE JKQTMATHTEXT_COMPILED_WITH_XITS)
target_link_libraries(jkqtmathtext PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
# Upstream is not warning-clean and it is not ours to fix.
target_compile_options(jkqtmathtext PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>)

function(neoqcp_link_jkqtmathtext target)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:jkqtmathtext>)
    target_include_directories(${target} PRIVATE ${JKQT_LIB})
    add_dependencies(${target} jkqtmathtext)
endfunction()
