// Plots a line with a LaTeX axis label and saves it, so a CMake-built NeoQCP is checked from
// the compiler to the pixels. Exits non-zero on the first thing that does not work.
#include <qcustomplot.h>

#include <QApplication>
#include <QFile>
#include <QImage>
#include <cstdio>

static int fail(const char* what)
{
    std::fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    QVector<double> x(101), y(101);
    for (int i = 0; i < 101; ++i)
    {
        x[i] = i / 50.0 - 1;
        y[i] = x[i] * x[i];
    }
    plot.addGraph();
    plot.graph(0)->setData(x, y);
    plot.yAxis->setLabel("$B_x^2$ [nT$^2$]");
    plot.rescaleAxes();

    const QString file = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("consumer.png");
    if (!plot.savePng(file, 400, 300))
        return fail("savePng returned false");
    if (QImage(file).size() != QSize(400, 300))
        return fail("the saved image does not have the requested size");
#if NEOQCP_WITH_LATEX
    // The fonts are a Qt resource inside libNeoQCP: a linker that dropped it falls back to
    // system fonts without an error.
    if (!QFile::exists(":/JKQTMathText/fonts/xits-italic.otf"))
        return fail("the XITS math fonts are not linked in");
#endif
    std::printf("OK: NeoQCP %s, %s\n", NEOQCP_VERSION, qPrintable(file));
    return 0;
}
