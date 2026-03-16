#pragma once
#include "plottable.h"
#include <axis/axis.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource.h>
#include <painting/colormap-renderer.h>
#include <memory>
#include <span>

class QCPColorScale;
class QCPColorMapData;

class QCP_LIB_DECL QCPHistogram2D : public QCPAbstractPlottable
{
    Q_OBJECT
public:
    enum Normalization { nNone, nColumn };

    explicit QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPHistogram2D() override;

    // Data — same pattern as QCPGraph2
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

    // Binning
    void setBins(int keyBins, int valueBins);
    int keyBins() const { return mKeyBins; }
    int valueBins() const { return mValueBins; }

    // Normalization
    void setNormalization(Normalization norm);
    Normalization normalization() const { return mNormalization; }

    // Forwarded to QCPColormapRenderer
    QCPColorGradient gradient() const { return mRenderer.gradient(); }
    QCPRange dataRange() const { return mRenderer.dataRange(); }
    QCPAxis::ScaleType dataScaleType() const { return mRenderer.dataScaleType(); }
    QCPColorScale* colorScale() const { return mRenderer.colorScale(); }
    void rescaleDataRange(bool recalc = false);

    // Pipeline access
    QCPHistogramPipeline& pipeline() { return mPipeline; }
    const QCPHistogramPipeline& pipeline() const { return mPipeline; }

public Q_SLOTS:
    void setGradient(const QCPColorGradient& gradient);
    void setDataRange(const QCPRange& range);
    void setDataScaleType(QCPAxis::ScaleType type);
    void setColorScale(QCPColorScale* scale);

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
    std::shared_ptr<QCPAbstractDataSource> mDataSource;
    int mKeyBins = 100;
    int mValueBins = 100;
    Normalization mNormalization = nNone;
    QCPHistogramPipeline mPipeline;
    QCPColormapRenderer mRenderer;

    void installTransform();
};
