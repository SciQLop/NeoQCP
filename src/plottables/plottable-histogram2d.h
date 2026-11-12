#pragma once
#include "plottable-colormapped.h"
#include <axis/axis.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource.h>
#include <drawable.h>
#include <painting/colormap-renderer.h>
#include <painting/contour-overlay.h>
#include <memory>
#include <span>

class QCPColorScale;
class QCPColorMapData;
class TestColorMap;
class TestPipeline;

//! A 2-D density histogram of scattered (key, value) samples, binned in the background.
//! Bins follow the axis scales, also those of an axis given later with setKeyAxis or
//! setValueAxis.
template <typename T, typename Self = void>
class QCPHistogram2DOps;

class QCP_LIB_DECL QCPHistogram2D : public QCPAbstractColorMapped
{
    Q_OBJECT
    friend class TestColorMap;
    friend class TestPipeline;
public:
    template <typename T>
    using OpsFor = QCPHistogram2DOps<T>;

    explicit QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis);

    // Data
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    void setData(KC&& keys, VC&& values)
    {
        setDataSource(std::make_shared<QCPSoADataSource<
            std::decay_t<KC>, std::decay_t<VC>>>(
            std::forward<KC>(keys), std::forward<VC>(values)));
    }
    template <typename K, typename V>
    void viewData(const K* keys, const V* values, int count)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            std::span<const K>(keys, count), std::span<const V>(values, count)));
    }
    template <typename K, typename V>
    void viewData(std::span<const K> keys, std::span<const V> values)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            keys, values));
    }

    void setDataSource(std::shared_ptr<QCPAbstractDataSource> source);
    void setDataSource(std::unique_ptr<QCPAbstractDataSource> source);
    const QCPAbstractDataSource* dataSource() const { return mDataSource.get(); }
    void dataChanged();

    // Binning. Bins follow the axis scales: a logarithmic axis bins in log10 (dropping
    // non-positive samples), so the grid re-distributes when an axis scale is toggled.
    void setBins(int keyBins, int valueBins);
    int keyBins() const { return mKeyBins; }
    int valueBins() const { return mValueBins; }

    // Normalization
    void setNormalization(QCP::HistogramNormalization norm);
    QCP::HistogramNormalization normalization() const { return mNormalization; }
    //! Scale every key column to sum to one; overrides the normalization while on.
    void setColumnNormalized(bool normalized);
    bool columnNormalized() const { return mColumnNormalized; }

    // Re-read the key/value axis scale type and re-bin. Wired to the axes'
    // scaleTypeChanged, but also callable directly for callers that change the
    // scale with that signal blocked (e.g. SciQLop's axis wrapper).
    Q_SLOT void refreshBinning();

Q_SIGNALS:
    //! Emitted when a re-bin lands.
    void histogramReady();

public:

    //! The range of the binned (normalized) counts; \a found is false before the first binning.
    [[nodiscard]] QCPRange colorDataBounds(bool& found) const override;
    //! Before the first binning, \a recalc starts one; the range follows when it lands.
    void rescaleDataRange(bool recalc = false) override;

    // Contours, as on QCPColorMap2, over the values shown: the normalized cells.
    void setContourLevels(const QVector<double>& levels);
    [[nodiscard]] QVector<double> contourLevels() const { return mContours.levels(); }
    void setAutoContourLevels(int count);
    [[nodiscard]] int autoContourLevelCount() const { return mContours.autoLevelCount(); }
    void setContourPen(const QPen& pen);
    [[nodiscard]] QPen contourPen() const { return mContours.pen(); }
    void setContourLabelEnabled(bool enabled);
    [[nodiscard]] bool contourLabelEnabled() const { return mContours.labelsEnabled(); }
    // Exposed for tests: see QCPColorMap2.
    [[nodiscard]] QVector<double> lastContourLevels() const { return mContours.lastLevels(); }
    [[nodiscard]] const QVector<QCPContourOverlay::Label>& lastContourLabels() const
    {
        return mContours.lastLabels();
    }

    // Pipeline access
    QCPHistogramPipeline& pipeline() { return mPipeline; }
    const QCPHistogramPipeline& pipeline() const { return mPipeline; }

    //! The recording is the data: new data makes it stale, a pan does not.
    [[nodiscard]] const void* dataSourceIdentity() const override { return mDataSource.get(); }
    //! An export bins synchronously, so the screen and export recordings differ.
    [[nodiscard]] bool resolvesForTheExport() const override { return true; }
    using QCPAbstractColorMapped::drawLegendIcon;
public Q_SLOTS:
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

protected:
    void draw(QCPDrawContext& context) const override;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    void colorMappingChanged() override;
    void systemChanged() override { followAxisScales(); }
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    void setPipelineScheduler(QCPPipelineScheduler* scheduler) override { mPipeline.setScheduler(scheduler); }
    bool canProduceContent() const override;

private:
    void redrawLater();
    void followAxisScales();

    std::shared_ptr<QCPAbstractDataSource> mDataSource;
    int mKeyBins = 100;
    int mValueBins = 100;
    QCP::HistogramNormalization mNormalization = QCP::HistogramNormalization::Count;
    bool mColumnNormalized = false;
    QCPHistogramPipeline mPipeline;
    QCPColormapRenderer mRenderer;
    QCPContourOverlay mContours;
    //! The binned counts as shown, after the normalization, with the colour bounds of the
    //! non-empty cells. Rebuilt lazily after the bins or the normalization change.
    struct Shown
    {
        std::shared_ptr<QCPColorMapData> normalized;
        QCPRange bounds;
        bool found = false;
        bool stale = true;
    };
    mutable Shown mShown;
    //! The axis scales the binning was made for: a new axis or a blocked scaleTypeChanged
    //! leaves them behind, and draw() catches up.
    bool mBinnedKeyLog = false;
    bool mBinnedValueLog = false;
    bool mRebinQueued = false;
    QMetaObject::Connection mKeyScaleLink;
    QMetaObject::Connection mValueScaleLink;

    // finiteKeyValueBounds is an O(N) scan over the raw scatter; selectTest
    // calls it per mouse event and getKeyRange per rescale. The bounds only
    // change with the data — memoize per (keyPositiveOnly, valuePositiveOnly),
    // invalidated by setDataSource/dataChanged.
    struct BoundsCache
    {
        bool valid = false;
        bool found = false;
        QCPRange key;
        QCPRange value;
    };
    mutable BoundsCache mBoundsCache[2][2];
    void invalidateBoundsCache() const;
    bool cachedFiniteBounds(QCPRange& keyOut, QCPRange& valueOut,
                            bool keyPositiveOnly, bool valuePositiveOnly) const;

    void installTransform();
    [[nodiscard]] bool binnedForTheAxisScales() const;
    //! The binned cells, or nullptr when there are none (yet).
    [[nodiscard]] const QCPColorMapData* binned() const;
    //! The cells as drawn: binned(), or theirs after the normalization.
    [[nodiscard]] const QCPColorMapData* shown() const;
    void invalidateShown();
    void contoursChanged();
    void queueRefreshBinning();
    void binForExport();
};

// The fluent handle of a 2D histogram; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPHistogram2DOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPHistogram2DOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPHistogram2DOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    // Same arguments as QCPHistogram2D::setData: two numeric ranges (keys, values), one point per index.
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    Ret setData(KC&& keys, VC&& values)
    {
        return this->apply([&](T& p) { p.setData(std::forward<KC>(keys), std::forward<VC>(values)); });
    }
    Ret setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
    {
        return this->apply([&](T& p) { p.setDataSource(std::move(source)); });
    }
    Ret setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
    {
        return this->apply([&](T& p) { p.setDataSource(std::move(source)); });
    }
    Ret setBins(int keyBins, int valueBins)
    {
        return this->apply([&](T& p) { p.setBins(keyBins, valueBins); });
    }
    Ret setNormalization(QCP::HistogramNormalization norm)
    {
        return this->apply([&](T& p) { p.setNormalization(norm); });
    }
    Ret setColumnNormalized(bool normalized)
    {
        return this->apply([&](T& p) { p.setColumnNormalized(normalized); });
    }
    Ret setContourLevels(const QVector<double>& levels)
    {
        return this->apply([&](T& p) { p.setContourLevels(levels); });
    }
    Ret setAutoContourLevels(int count)
    {
        return this->apply([&](T& p) { p.setAutoContourLevels(count); });
    }
    Ret setContourPen(const QPen& pen)
    {
        return this->apply([&](T& p) { p.setContourPen(pen); });
    }
    Ret setContourLabelEnabled(bool enabled)
    {
        return this->apply([&](T& p) { p.setContourLabelEnabled(enabled); });
    }
    Ret setGradient(const QCPColorGradient& gradient)
    {
        return this->apply([&](T& p) { p.setGradient(gradient); });
    }
    Ret setDataRange(const QCPRange& range)
    {
        return this->apply([&](T& p) { p.setDataRange(range); });
    }
    Ret setDataScaleType(QCPAxis::ScaleType type)
    {
        return this->apply([&](T& p) { p.setDataScaleType(type); });
    }

    // The scale the map follows; an empty or dead handle detaches it.
    Ret setColorScale(const QCPHandle<QCPColorScale>& scale)
    {
        return this->apply([&](T& p) { p.setColorScale(scale.get()); });
    }
};
