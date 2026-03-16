#pragma once
#include "plottable.h"
#include <axis/axis.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource-2d.h>
#include <painting/colormap-renderer.h>
#include <atomic>
#include <memory>
#include <span>

class QCPColorScale;
class QCPColorMapData;

class QCP_LIB_DECL QCPColorMap2 : public QCPAbstractPlottable
{
    Q_OBJECT

public:
    explicit QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPColorMap2() override;

    // Data source management
    void setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source);
    QCPAbstractDataSource2D* dataSource() const { return mDataSource.get(); }
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
    void setGapThreshold(double threshold) { mGapThreshold.store(threshold, std::memory_order_relaxed); }
    double gapThreshold() const { return mGapThreshold.load(std::memory_order_relaxed); }

    QCPColorGradient gradient() const { return mRenderer.gradient(); }
    QCPColorScale* colorScale() const { return mRenderer.colorScale(); }
    QCPRange dataRange() const { return mRenderer.dataRange(); }
    QCPAxis::ScaleType dataScaleType() const { return mRenderer.dataScaleType(); }
    void setDataScaleType(QCPAxis::ScaleType type);

    void setColorScale(QCPColorScale* colorScale);
    void rescaleDataRange(bool recalc = false);

    QCPColormapPipeline& pipeline() { return mPipeline; }
    const QCPColormapPipeline& pipeline() const { return mPipeline; }

public Q_SLOTS:
    void setGradient(const QCPColorGradient& gradient);
    void setDataRange(const QCPRange& range);

    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

Q_SIGNALS:
    void dataRangeChanged(const QCPRange& newRange);
    void gradientChanged(const QCPColorGradient& newGradient);
    void dataScaleTypeChanged(QCPAxis::ScaleType newType);

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

private:
    std::shared_ptr<QCPAbstractDataSource2D> mDataSource;
    std::atomic<double> mGapThreshold{1.5}; // before mPipeline: must outlive background jobs
    QCPColormapPipeline mPipeline;
    QCPColormapRenderer mRenderer;

    void onViewportChanged();
};
