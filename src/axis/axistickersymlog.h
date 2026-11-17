#ifndef QCP_AXISTICKERSYMLOG_H
#define QCP_AXISTICKERSYMLOG_H

#include "axisticker.h"
#include "scale.h"

#include <vector>

class QCP_LIB_DECL QCPAxisTickerSymlog : public QCPAxisTicker
{
public:
    explicit QCPAxisTickerSymlog(const QCPScale& scale = QCPScale::symlog());

    // getters:
    [[nodiscard]] QCPScale scale() const { return mScale; }

protected:
    // property members:
    QCPScale mScale;

    // non-property members:
    //! The last range held no decade, so the ticks are evenly spaced and the sub ticks with them.
    bool mLinearFallback = false;

    [[nodiscard]] double decadeStep() const;
    [[nodiscard]] QVector<double> symlogDecades(const QCPRange& range) const;
    void addBandSubTicks(QVector<double>& candidates, int subTickCount) const;
    void addOutsideSubTicks(std::vector<double>& candidates, int subTickCount,
                            const QVector<double>& ticks) const;

    // reimplemented virtual methods:
    virtual QVector<double> createTickVector(double tickStep, const QCPRange& range) override;
    virtual QVector<double> createSubTickVector(int subTickCount,
                                                const QVector<double>& ticks) override;
};

#endif // QCP_AXISTICKERSYMLOG_H
