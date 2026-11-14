// Target program (milestone 6): a spectrogram over a time series, sharing time. Does not compile yet.
// https://matplotlib.org/stable/gallery/images_contours_and_fields/specgram_demo.html

#include <qcustomplot.h>

#include <QApplication>

#include <cmath>
#include <complex>
#include <numbers>
#include <random>
#include <vector>

constexpr double pi = std::numbers::pi;

struct Spectrogram
{
    std::vector<double> time, frequency, power;
};

// What matplotlib's specgram computes: Hann windows of nfft samples, overlap samples shared,
// a one-sided power spectral density per window. A plain DFT; the 0 Hz row is left out for the
// log axis.
Spectrogram spectrogram(const std::vector<double>& signal, int nfft, double rate, int overlap)
{
    std::vector<double> window(nfft);
    double energy = 0;
    for (int n = 0; n < nfft; ++n)
    {
        window[n] = 0.5 - 0.5 * std::cos(2 * pi * n / (nfft - 1));
        energy += window[n] * window[n];
    }
    Spectrogram s;
    for (int k = 1; k <= nfft / 2; ++k)
        s.frequency.push_back(k * rate / nfft);
    for (std::size_t start = 0; start + nfft <= signal.size(); start += nfft - overlap)
    {
        s.time.push_back((start + nfft / 2.0) / rate);
        for (int k = 1; k <= nfft / 2; ++k)
        {
            std::complex<double> sum = 0;
            for (int n = 0; n < nfft; ++n)
                sum += window[n] * signal[start + n] * std::polar(1.0, -2 * pi * k * n / nfft);
            s.power.push_back((k < nfft / 2 ? 2 : 1) * std::norm(sum) / (rate * energy));
        }
    }
    return s;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(720, 560);

    std::mt19937 rng(19680801);
    std::uniform_real_distribution<double> noise(0.0, 0.01);
    const double dt = 0.0005;
    std::vector<double> t, x;
    for (int i = 0; i < 41000; ++i)
    {
        t.push_back(i * dt);
        const bool chirp = t.back() > 10 && t.back() < 12;
        x.push_back(std::sin(2 * pi * 100 * t.back())
                    + (chirp ? 2 * std::sin(2 * pi * 400 * t.back()) : 0) + noise(rng));
    }
    const Spectrogram s = spectrogram(x, 1024, 1 / dt, 128);

    auto spectrum = plot.axisRect();
    auto signal = plot.addAxisRect({ .shareX = spectrum });

    spectrum->colorMap(s.time, s.frequency, s.power,
                       { .gradient = QCPColorGradient::gpViridis, .dataScale = QCPScale::log() });
    spectrum->yAxis()->setScale(QCPScale::log());
    spectrum->yAxis()->setLabel("Frequency (Hz)");
    spectrum->colorScale()->setLabel("power (1/Hz)");

    signal->line(t, x, { .pen = QPen(QColor(31, 119, 180)) });
    signal->yAxis()->setLabel("Signal");
    signal->xAxis()->setLabel("Time (s)");

    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.rescaleAxes();
    signal->xAxis()->setRange(0, 20);
    plot.show();
    return app.exec();
}
