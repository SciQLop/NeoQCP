#pragma once
#include "plottable.h"
#include <datasource/async-pipeline.h>
#include <datasource/histogram-binner-1d.h>
#include <drawable.h>
#include <QColor>
#include <QPen>
#include <QList>
#include <memory>
#include <optional>
#include <vector>

//! A 1-D histogram of every value column of a multi data source, with shared bin edges.
//! Binning runs in the background; pre-binned data (setBinned) bypasses the pipeline.
//! Bins follow the key axis scale, also on an axis given later with setKeyAxis: a scale
//! change forgets the edges and bins again.
template <typename T, typename Self = void>
class QCPHistogramOps;

class QCP_LIB_DECL QCPHistogram : public QCPPlottable
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPHistogram)
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(int bins READ bins WRITE setBins NOTIFY binsChanged)
    Q_PROPERTY(QCP::HistogramNormalization normalization READ normalization WRITE setNormalization
                   NOTIFY normalizationChanged)
    Q_PROPERTY(Style style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(QList<QColor> columnColors READ columnColors WRITE setColumnColors NOTIFY
                   columnColorsChanged)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPHistogramOps<T>;

    enum Style { sBars, sStep };
    Q_ENUM(Style)

    QCPHistogram();
    ~QCPHistogram() override;

    void setDataSource(std::shared_ptr<QCPMultiDataSource> source);
    //! Bars with the given edges; heights[c] has edges.size() - 1 entries for every column.
    void setBinned(std::vector<double> edges, std::vector<std::vector<double>> heights);
    void setBins(int bins);
    //! nullopt = automatic edges, fixed on the first data and only widened afterwards.
    void setFixedRange(std::optional<QCPRange> range);
    void setNormalization(QCP::HistogramNormalization norm);
    void setStyle(Style style);
    void setColumnColors(const QList<QColor>& colors);
    //! Forget the automatic edges: the next data sets them again.
    void resetEdges();

    [[nodiscard]] int bins() const { return mBins; }
    [[nodiscard]] std::optional<QCPRange> fixedRange() const { return mFixedRange; }
    [[nodiscard]] QCP::HistogramNormalization normalization() const { return mNormalization; }
    [[nodiscard]] Style style() const { return mStyle; }
    [[nodiscard]] QList<QColor> columnColors() const { return mColors; }
    //! The drawn result, or nullptr when there is nothing to draw.
    [[nodiscard]] const qcp::algo::Histogram1D* histogram() const { return mResult.get(); }
    QCPHistogram1DPipeline& pipeline() { return mPipeline; }

    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

public Q_SLOTS:
    //! Bin again, e.g. after the data changed in place.
    void refreshBinning();

Q_SIGNALS:
    //! Emitted when a re-bin lands, after the data or an axis scale changed.
    void histogramReady();

signals:
    void binsChanged(int bins);
    void normalizationChanged(QCP::HistogramNormalization normalization);
    void styleChanged(QCPHistogram::Style style);
    void columnColorsChanged(const QList<QColor>& columnColors);

protected:
    void draw(QCPDrawContext& context) const override;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    void setPipelineScheduler(QCPPipelineScheduler* scheduler) override
    {
        if (scheduler)
            mPipeline.setScheduler(scheduler);
        else
            mPipeline.deferJobs(); // outside a plot the transforms wait, they never block
    }
    bool canProduceContent() const override;
    //! An export bins synchronously, so the screen and export recordings differ.
    [[nodiscard]] bool resolvesForTheExport() const override { return true; }

    void systemDidChange() override { followKeyAxis(); }

private:
    void followKeyAxis();
    QMetaObject::Connection mKeyScaleLink;
    std::shared_ptr<QCPMultiDataSource> mDataSource;
    std::shared_ptr<qcp::algo::Histogram1D> mResult;
    std::optional<QCPRange> mFixedRange, mAutoRange;
    int mBins = 64;
    QCP::HistogramNormalization mNormalization = QCP::HistogramNormalization::Count;
    Style mStyle = sBars;
    QList<QColor> mColors;
    QCPHistogram1DPipeline mPipeline;
    // Results of jobs started before the last binning change or setBinned are dropped.
    uint64_t mAcceptedGeneration = 0;
    //! The key scale the binning was made for: a new key axis, or a scaleTypeChanged that was
    //! blocked, leaves it behind, and draw() catches up.
    bool mBinnedKeyLog = false;
    bool mResetQueued = false;
    //! The key scale a stale-scale reset is in flight for.
    std::optional<bool> mResetForLog;

    [[nodiscard]] bool keyLog() const;
    [[nodiscard]] bool autoEdges() const;
    [[nodiscard]] bool scaleIsStale() const;
    [[nodiscard]] std::shared_ptr<qcp::algo::Histogram1D> pipelineResult() const;
    void installTransform();
    void binForExport();
    void queueResetEdges();
    void resetForTheStaleScale();
    [[nodiscard]] bool coversAutoEdges(const qcp::algo::Histogram1D& h) const;
    void onPipelineFinished(uint64_t generation);
    void storeResult(std::shared_ptr<qcp::algo::Histogram1D> result);
    void takeResult(std::shared_ptr<qcp::algo::Histogram1D> result);
    void clearResult();
    void rememberEdges();
    void requestReplot();

    struct ColumnLook { QColor fill; QPen outline; };
    [[nodiscard]] ColumnLook columnLook(int column) const;
    void recordRects(QCPDrawContext& context, int column, const QPen& pen, const QBrush& brush) const;
    void recordBars(QCPDrawContext& context, int column, const ColumnLook& look) const;
    void recordStep(QCPDrawContext& context, int column, const ColumnLook& look) const;
    [[nodiscard]] double baseValue(QCPDrawContext& context) const;
    [[nodiscard]] static bool drawableHeight(double height, bool log);
    [[nodiscard]] static bool absentHeight(double height, bool log);
};

// The fluent handle of a QCPHistogram; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPHistogramOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPHistogramOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPHistogramOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setDataSource(std::shared_ptr<QCPMultiDataSource> source)
    {
        return this->apply([&](T& o) { o.setDataSource(std::move(source)); });
    }

    Ret setBins(int bins)
    {
        return this->apply([&](T& o) { o.setBins(bins); });
    }

    Ret setStyle(QCPHistogram::Style style)
    {
        return this->apply([&](T& o) { o.setStyle(style); });
    }
    Ret setBinned(std::vector<double> edges, std::vector<std::vector<double>> heights)
    {
        return this->apply([&](T& p) { p.setBinned(std::move(edges), std::move(heights)); });
    }
    Ret setFixedRange(std::optional<QCPRange> range)
    {
        return this->apply([&](T& p) { p.setFixedRange(std::move(range)); });
    }
    Ret setNormalization(QCP::HistogramNormalization norm)
    {
        return this->apply([&](T& p) { p.setNormalization(norm); });
    }
    Ret setColumnColors(const QList<QColor>& colors)
    {
        return this->apply([&](T& p) { p.setColumnColors(colors); });
    }

};


