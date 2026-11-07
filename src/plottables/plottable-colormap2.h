#pragma once
#include "plottable-colormapped.h"
#include <axis/axis.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource-2d.h>
#include <drawable.h>
#include <painting/colormap-renderer.h>
#include <painting/contour-overlay.h>
#include <atomic>
#include <memory>
#include <span>
#include <QPen>

class QCPColorScale;
class QCPColorMapData;
class TestColorMap;
class TestPipeline;

template <typename T, typename Self = void>
class QCPColorMap2Ops;

class QCP_LIB_DECL QCPColorMap2 : public QCPDrawn<QCPAbstractColorMapped>
{
    Q_OBJECT
    friend class TestColorMap;
    friend class TestPipeline;

public:
    template <typename T>
    using OpsFor = QCPColorMap2Ops<T>;

    explicit QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis);

    // Data source management
    void setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source);
    [[nodiscard]] QCPAbstractDataSource2D* dataSource() const { return mDataSource.get(); }
    void dataChanged();

    // Owning setData
    template <IndexableNumericRange XC, IndexableNumericRange YC, IndexableNumericRange ZC>
    void setData(XC&& x, YC&& y, ZC&& z)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::decay_t<XC>, std::decay_t<YC>, std::decay_t<ZC>>>(
            std::forward<XC>(x), std::forward<YC>(y), std::forward<ZC>(z)));
    }

    // Non-owning views
    template <typename X, typename Y, typename Z>
    void viewData(const X* x, int nx, const Y* y, int ny, const Z* z, int nz)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::span<const X>, std::span<const Y>, std::span<const Z>>>(
            std::span<const X>{x, static_cast<size_t>(nx)},
            std::span<const Y>{y, static_cast<size_t>(ny)},
            std::span<const Z>{z, static_cast<size_t>(nz)}));
    }
    template <typename X, typename Y, typename Z>
    void viewData(std::span<const X> x, std::span<const Y> y, std::span<const Z> z)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::span<const X>, std::span<const Y>, std::span<const Z>>>(x, y, z));
    }

    // Properties
    void setGapThreshold(double threshold);
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }

    using QCPAbstractColorMapped::colorDataBounds;
    QCPRange colorDataBounds(bool& found) const override;
    //! The resampled view's range, or all the data's when \a recalc.
    void rescaleDataRange(bool recalc = false) override;
    //! The range of the colour values: of the resampled view, or of all data with \a recalc
    //! (or before the first resample). \a found is false when there is none.
    [[nodiscard]] QCPRange colorDataBounds(bool& found, bool recalc) const;

    QCPColormapPipeline& pipeline() { return mPipeline; }
    const QCPColormapPipeline& pipeline() const { return mPipeline; }

    [[nodiscard]] bool hasRenderedRange() const { return mCore.recordedCommandCount() > 0; }

    //! The recording is the data: a new source makes it stale, a pan does not.
    [[nodiscard]] const void* dataSourceIdentity() const override { return mDataSource.get(); }
    //! An export resolves a pending resample synchronously, so the two recordings differ.
    [[nodiscard]] bool resolvesForTheExport() const override { return true; }

    // Contour overlay
    void setContourLevels(const QVector<double>& levels);
    [[nodiscard]] QVector<double> contourLevels() const { return mContours.levels(); }

    void setContourPen(const QPen& pen);
    [[nodiscard]] QPen contourPen() const { return mContours.pen(); }

    //! Writes each contour's level on it, along the line, in the contour pen's colour.
    void setContourLabelEnabled(bool enabled);
    [[nodiscard]] bool contourLabelEnabled() const { return mContours.labelsEnabled(); }

    void setAutoContourLevels(int count);
    [[nodiscard]] int autoContourLevelCount() const { return mContours.autoLevelCount(); }

    // Exposed for tests: the levels the last rebuild marched (explicit
    // levels filtered to finite values, or resolved auto levels).
    [[nodiscard]] QVector<double> lastContourLevels() const { return mContours.lastLevels(); }

    using ContourLabel = QCPContourOverlay::Label;
    static constexpr double kContourLabelSpacing = QCPContourOverlay::kLabelSpacing;
    static constexpr double kContourLabelClearance = QCPContourOverlay::kLabelClearance;
    // Exposed for tests: the labels the last draw placed.
    [[nodiscard]] const QVector<ContourLabel>& lastContourLabels() const { return mContours.lastLabels(); }

    using QCPDrawn<QCPAbstractColorMapped>::drawLegendIcon;
public Q_SLOTS:
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

protected:
    void draw(QCPDrawContext& context) const override;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    void colorMappingChanged() override;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    void setPipelineScheduler(QCPPipelineScheduler* scheduler) override { mPipeline.setScheduler(scheduler); }
    bool canProduceContent() const override;
    void axesChanged() override { followAxes(); }

private:
    [[nodiscard]] bool resampledForAnotherSize() const;
    void installResampleTransform();

    std::shared_ptr<QCPAbstractDataSource2D> mDataSource;
    // Captured by value in the pipeline transform (re-baked by setGapThreshold / setDataScaleType)
    // so background jobs never reference this object's memory.
    double mGapThreshold = 1.5;
    QCPColormapPipeline mPipeline;
    QCPColormapRenderer mRenderer;

    QCPContourOverlay mContours;
    void contoursChanged();

    void onViewportChanged();
    void followAxes();
    QList<QMetaObject::Connection> mAxisLinks;
    void queueViewportChange();
    bool mViewportChangeQueued = false;
};

// The fluent handle of a 2D colour map; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPColorMap2Ops : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPColorMap2Ops<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPColorMap2Ops<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    // Same arguments as QCPColorMap2::setData: x and y axes ranges and z, three numeric ranges.
    template <IndexableNumericRange XC, IndexableNumericRange YC, IndexableNumericRange ZC>
    Ret setData(XC&& x, YC&& y, ZC&& z)
    {
        return this->apply(
            [&](T& p) { p.setData(std::forward<XC>(x), std::forward<YC>(y), std::forward<ZC>(z)); });
    }
    Ret setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source)
    {
        return this->apply([&](T& p) { p.setDataSource(std::move(source)); });
    }
    Ret setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source)
    {
        return this->apply([&](T& p) { p.setDataSource(std::move(source)); });
    }
    Ret setGapThreshold(double threshold)
    {
        return this->apply([&](T& p) { p.setGapThreshold(threshold); });
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
