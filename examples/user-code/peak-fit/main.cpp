// Target program (milestone 6): a peak-fit editor over two panels sharing x. Does not compile yet.

#include <qcustomplot.h>

#include <QApplication>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

// The fit: plain data and arithmetic, no NeoQCP type. Centres and widths are the user's; a linear
// least-squares solve finds the baseline and the three amplitudes.
struct Gaussian
{
    double centre;
    double width;
};

struct Fit
{
    std::vector<double> model;
    std::vector<double> residuals;
};

double basis(const std::vector<Gaussian>& peaks, std::size_t k, double x)
{
    if (k == 0)
        return 1.0;
    const double u = (x - peaks[k - 1].centre) / peaks[k - 1].width;
    return std::exp(-0.5 * u * u);
}

// Gauss-Jordan elimination on the augmented normal equations, n x (n + 1).
std::vector<double> solve(std::vector<std::vector<double>> a)
{
    const std::size_t n = a.size();
    for (std::size_t col = 0; col < n; ++col)
        for (std::size_t row = 0; row < n; ++row)
            if (row != col)
            {
                const double f = a[row][col] / a[col][col];
                for (std::size_t j = col; j <= n; ++j)
                    a[row][j] -= f * a[col][j];
            }
    std::vector<double> x(n);
    for (std::size_t i = 0; i < n; ++i)
        x[i] = a[i][n] / a[i][i];
    return x;
}

Fit fitAmplitudes(const std::vector<double>& x, const std::vector<double>& y,
                  const std::vector<Gaussian>& peaks)
{
    const std::size_t n = peaks.size() + 1;
    std::vector<std::vector<double>> normal(n, std::vector<double>(n + 1, 0.0));
    for (std::size_t i = 0; i < x.size(); ++i)
        for (std::size_t k = 0; k < n; ++k)
        {
            for (std::size_t j = 0; j < n; ++j)
                normal[k][j] += basis(peaks, k, x[i]) * basis(peaks, j, x[i]);
            normal[k][n] += basis(peaks, k, x[i]) * y[i];
        }
    const std::vector<double> amplitude = solve(normal);
    Fit fit;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        double model = 0;
        for (std::size_t k = 0; k < n; ++k)
            model += amplitude[k] * basis(peaks, k, x[i]);
        fit.model.push_back(model);
        fit.residuals.push_back(y[i] - model);
    }
    return fit;
}

// One peak guess: a band across the panel, dragged by its body, resized from either edge.
class Peak : public QCPItem
{
public:
    Peak(double centre, double width) : mGaussian { centre, width } { }

    Gaussian gaussian() const { return mGaussian; }

protected:
    enum Part { Body, LeftEdge, RightEdge };

    void draw(QCPDrawContext& context) const override
    {
        const double left = mGaussian.centre - mGaussian.width;
        const double right = mGaussian.centre + mGaussian.width;
        const QPen edge(QColor(214, 39, 40), 2);

        context.setPart(Body);
        context.rect(QCP::DataXRatioY { left, 0 }, QCP::DataXRatioY { right, 1 }, Qt::NoPen,
                     QColor(214, 39, 40, 40));
        context.line(QCP::DataXRatioY { mGaussian.centre, 0 },
                     QCP::DataXRatioY { mGaussian.centre, 1 }, QPen(edge.color(), 1, Qt::DashLine));
        context.setPart(LeftEdge);
        context.line(QCP::DataXRatioY { left, 0 }, QCP::DataXRatioY { left, 1 }, edge);
        context.setPart(RightEdge);
        context.line(QCP::DataXRatioY { right, 0 }, QCP::DataXRatioY { right, 1 }, edge);
    }

    void mousePressEvent(QCPMouseEvent* event) override { event->accept(); }

    void mouseMoveEvent(QCPMouseEvent* event) override
    {
        const double dx = event->data().x - event->lastData().x;
        switch (event->part())
        {
            case Body: mGaussian.centre += dx; break;
            case LeftEdge: mGaussian.width -= dx; break;
            case RightEdge: mGaussian.width += dx; break;
        }
        mGaussian.width = std::max(mGaussian.width, 0.3);
        update();
    }

private:
    Gaussian mGaussian;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(800, 560);

    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0.0, 6.0);
    const std::vector<Gaussian> truth { { 25, 2.5 }, { 48, 4 }, { 70, 3 } };
    const double height[] = { 120, 80, 150 };
    std::vector<double> energy, counts;
    for (int i = 0; i <= 400; ++i)
    {
        energy.push_back(i * 0.25);
        double c = 12 + noise(rng);
        for (std::size_t k = 0; k < truth.size(); ++k)
            c += height[k] * basis(truth, k + 1, energy.back());
        counts.push_back(c);
    }

    plot.setAxisRectGrid(2, 1, { .shareX = true, .rowStretch = { 1, 0.35 } });
    auto spectrum = plot.axisRect(0, 0);
    auto residuals = plot.axisRect(1, 0);
    spectrum->yAxis()->setLabel("counts");
    residuals->yAxis()->setLabel("residual");
    residuals->xAxis()->setLabel("energy (keV)");

    QList<QCPHandle<Peak>> peaks;
    for (double centre : { 22.0, 52.0, 67.0 })
        peaks << spectrum->add<Peak>(centre, 4.0);
    const auto guesses = [&] {
        std::vector<Gaussian> g;
        for (const auto& peak : peaks)
            g.push_back(peak->gaussian());
        return g;
    };

    const Fit first = fitAmplitudes(energy, counts, guesses());
    spectrum->line(energy, counts, { .pen = QPen(Qt::gray), .name = "spectrum" });
    auto model = spectrum->line(energy, first.model,
                                { .pen = QPen(Qt::black, 2), .name = "fit" });
    auto misfit = residuals->line(energy, first.residuals, { .pen = QPen(Qt::gray), .name = "residual" });

    const auto refit = [&] {
        const Fit fit = fitAmplitudes(energy, counts, guesses());
        model->setData(energy, fit.model);
        misfit->setData(energy, fit.residuals);
    };
    for (const auto& peak : peaks)
        QObject::connect(peak, &QCPItem::changed, &plot, refit);

    plot.legend->setVisible(true);
    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
