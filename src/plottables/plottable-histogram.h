#pragma once
#include "plottable.h"
#include <datasource/async-pipeline.h>
#include <datasource/histogram-binner-1d.h>
#include <QColor>
#include <QList>
#include <memory>
#include <optional>
#include <vector>

//! A 1-D histogram of every value column of a multi data source, with shared bin edges.
//! Binning runs in the background; pre-binned data (setBinned) bypasses the pipeline.
class QCP_LIB_DECL QCPHistogram : public QCPAbstractPlottable
{
    Q_OBJECT
public:
    enum Style { sBars, sStep };
    Q_ENUM(Style)

    explicit QCPHistogram(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPHistogram() override;

    void setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    //! Bars with the given edges; heights[c] has edges.size() - 1 entries for every column.
    void setBinned(std::vector<double> edges, std::vector<std::vector<double>> heights);
    void setBins(int bins);
    //! nullopt = automatic edges, fixed on the first data and only widened afterwards.
    void setFixedRange(std::optional<QCPRange> range);
    void setNormalization(qcp::algo::HistogramNormalization norm);
    void setStyle(Style style);
    void setColumnColors(const QList<QColor>& colors);
    //! Forget the automatic edges: the next data sets them again.
    void resetEdges();

    [[nodiscard]] int bins() const { return mBins; }
    [[nodiscard]] std::optional<QCPRange> fixedRange() const { return mFixedRange; }
    [[nodiscard]] qcp::algo::HistogramNormalization normalization() const { return mNormalization; }
    [[nodiscard]] Style style() const { return mStyle; }
    [[nodiscard]] QList<QColor> columnColors() const { return mColors; }
    //! The drawn result, or nullptr when there is nothing to draw.
    [[nodiscard]] const qcp::algo::Histogram1D* histogram() const { return mResult.get(); }
    QCPHistogram1DPipeline& pipeline() { return mPipeline; }

    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

Q_SIGNALS:
    void edgesChanged();
    void histogramReady();

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    bool canProduceContent() const override;

private:
    std::shared_ptr<QCPAbstractMultiDataSource> mDataSource;
    std::shared_ptr<qcp::algo::Histogram1D> mResult;
    std::optional<QCPRange> mFixedRange, mAutoRange;
    int mBins = 64;
    qcp::algo::HistogramNormalization mNormalization = qcp::algo::HistogramNormalization::Count;
    Style mStyle = sBars;
    QList<QColor> mColors;
    QCPHistogram1DPipeline mPipeline;

    [[nodiscard]] bool keyLog() const;
    [[nodiscard]] bool autoEdges() const;
    [[nodiscard]] std::shared_ptr<qcp::algo::Histogram1D> pipelineResult() const;
    void installTransform();
    void refreshBinning();
    void binForExport();
    void takeResult(std::shared_ptr<qcp::algo::Histogram1D> result);
    void rememberEdges();
    void requestReplot();
};
