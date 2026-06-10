#pragma once
#include "plottable-multigraph.h"
#include <memory>

// Immutable snapshot: built whole, never mutated. Async resample jobs hold a
// shared_ptr to one generation while the graph swaps in the next — like the
// SoA sources, the adapter co-owns its inner source so an in-flight job can
// never observe a parameter change, a source swap, or a freed source.
class QCPWaterfallDataAdapter : public QCPAbstractMultiDataSource {
public:
    QCPWaterfallDataAdapter(std::shared_ptr<QCPAbstractMultiDataSource> source,
                            QVector<double> offsets, QVector<double> normFactors,
                            double gain);

    QCPAbstractMultiDataSource* source() const { return mSource.get(); }

    int columnCount() const override;
    int size() const override;
    double keyAt(int i) const override;
    QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override;
    int findBegin(double sortKey, bool expandedRange = true) const override;
    int findEnd(double sortKey, bool expandedRange = true) const override;

    double valueAt(int column, int i) const override;
    QCPRange valueRange(int column, bool& found, QCP::SignDomain sd = QCP::sdBoth,
                        const QCPRange& inKeyRange = QCPRange()) const override;
    QVector<QPointF> getLines(int column, int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis) const override;
    QVector<QPointF> getOptimizedLineData(int column, int begin, int end, int pixelWidth,
                                           QCPAxis* keyAxis, QCPAxis* valueAxis) const override;

private:
    const std::shared_ptr<QCPAbstractMultiDataSource> mSource;
    const QVector<double> mOffsets;
    const QVector<double> mNormFactors;
    const double mGain;

    double transform(int column, double rawValue) const;
};

class QCP_LIB_DECL QCPWaterfallGraph : public QCPMultiGraph {
    Q_OBJECT
public:
    enum OffsetMode { omUniform, omCustom };
    Q_ENUM(OffsetMode)

    explicit QCPWaterfallGraph(QCPAxis* keyAxis, QCPAxis* valueAxis);

    void setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source) override;

    OffsetMode offsetMode() const { return mOffsetMode; }
    void setOffsetMode(OffsetMode mode);
    double uniformSpacing() const { return mUniformSpacing; }
    void setUniformSpacing(double spacing);
    QVector<double> offsets() const { return mUserOffsets; }
    void setOffsets(const QVector<double>& offsets);

    bool normalize() const { return mNormalize; }
    void setNormalize(bool enabled);
    double gain() const { return mGain; }
    void setGain(double gain);

    void invalidateNormalization();

protected:
    void dataChanged() override;
    QCPRange getValueRange(bool& foundRange,
                           QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

private:
    OffsetMode mOffsetMode = omUniform;
    double mUniformSpacing = 1.0;
    QVector<double> mUserOffsets;
    bool mNormalize = true;
    double mGain = 1.0;
    bool mNormDirty = true;
    QVector<double> mCachedNormFactors;

    std::shared_ptr<QCPAbstractMultiDataSource> mOriginalSource;
    std::shared_ptr<QCPWaterfallDataAdapter> mAdapter;

    double effectiveOffset(int component) const;
    // Builds a fresh immutable adapter and pushes it through
    // QCPMultiGraph::setDataSource — every data/parameter change goes through
    // here, so line/L1/L2 caches are invalidated and what is drawn always
    // matches the current parameters.
    void rebuildAdapter();
    void recomputeNormFactors();

    friend class TestWaterfall;
};
