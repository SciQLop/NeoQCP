#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "datasource/abstract-datasource.h"
#include "datasource/soa-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include <memory>
#include <span>

class QCP_LIB_DECL QCPGraph2 : public QCPAbstractPlottable, public QCPPlottableInterface1D {
    Q_OBJECT
public:
    enum LineStyle {
        lsNone,
        lsLine,
        lsStepLeft,
        lsStepRight,
        lsStepCenter,
        lsImpulse
    };
    Q_ENUMS(LineStyle)

    explicit QCPGraph2(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPGraph2() override;

    // Data source
    void setDataSource(std::unique_ptr<QCPAbstractDataSource> source);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource> source);
    QCPAbstractDataSource* dataSource() const { return mDataSource.get(); }

    // Convenience: owning
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    void setData(KC&& keys, VC&& values)
    {
        using KD = std::decay_t<KC>;
        using VD = std::decay_t<VC>;
        setDataSource(std::make_shared<QCPSoADataSource<KD, VD>>(
            std::forward<KC>(keys), std::forward<VC>(values)));
    }

    // Convenience: non-owning view from raw pointers
    template <typename K, typename V>
    void viewData(const K* keys, const V* values, int count)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            std::span<const K>(keys, count), std::span<const V>(values, count)));
    }

    // Convenience: non-owning view from spans
    template <typename K, typename V>
    void viewData(std::span<const K> keys, std::span<const V> values)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            keys, values));
    }

    void dataChanged();

    // Pipeline
    QCPGraphPipeline& pipeline() { return mPipeline; }
    const QCPGraphPipeline& pipeline() const { return mPipeline; }

    // Line style
    LineStyle lineStyle() const { return mLineStyle; }
    void setLineStyle(LineStyle style) { mLineStyle = style; }

    // Scatter style
    QCPScatterStyle scatterStyle() const { return mScatterStyle; }
    void setScatterStyle(const QCPScatterStyle& style) { mScatterStyle = style; }
    int scatterSkip() const { return mScatterSkip; }
    void setScatterSkip(int skip) { mScatterSkip = qMax(0, skip); }

    // Adaptive sampling control
    bool adaptiveSampling() const { return mAdaptiveSampling; }
    void setAdaptiveSampling(bool enabled) { mAdaptiveSampling = enabled; }

    // QCPPlottableInterface1D
    int dataCount() const override;
    double dataMainKey(int index) const override;
    double dataSortKey(int index) const override;
    double dataMainValue(int index) const override;
    QCPRange dataValueRange(int index) const override;
    QPointF dataPixelPosition(int index) const override;
    bool sortKeyIsMainKey() const override { return true; }
    QCPDataSelection selectTestRect(const QRectF& rect, bool onlySelectable) const override;
    int findBegin(double sortKey, bool expandedRange = true) const override;
    int findEnd(double sortKey, bool expandedRange = true) const override;

    // QCPAbstractPlottable
    QCPPlottableInterface1D* interface1D() override { return this; }
    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange,
                         QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange,
                           QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    void onViewportChanged();

private:
    std::shared_ptr<QCPAbstractDataSource> mDataSource;
    QCPGraphPipeline mPipeline;

    // Two-phase resampling: L1 built async, L2 computed synchronously on viewport change
    std::shared_ptr<qcp::algo::GraphResamplerCache> mL1Cache;
    std::shared_ptr<QCPAbstractDataSource> mL2Result;
    bool mNeedsResampling = false;

    void onL1Ready();
    void rebuildL2(const ViewportParams& vp);

    LineStyle mLineStyle = lsLine;
    QCPScatterStyle mScatterStyle;
    int mScatterSkip = 0;
    bool mAdaptiveSampling = true;

    // Line style transforms (pixel-space in, pixel-space out)
    static QVector<QPointF> toStepLeftLines(const QVector<QPointF>& lines, bool keyIsVertical);
    static QVector<QPointF> toStepRightLines(const QVector<QPointF>& lines, bool keyIsVertical);
    static QVector<QPointF> toStepCenterLines(const QVector<QPointF>& lines, bool keyIsVertical);
    QVector<QPointF> toImpulseLines(const QVector<QPointF>& lines, bool keyIsVertical) const;
};
