#pragma once

#include <painting/draw-context.h>
#include <plottables/plottable.h>

#include <QBrush>
#include <QVector>
#include <memory>
#include <vector>

/*!
  A filled band between two series, with a label: a plottable that only records commands.

  Everything it draws is in draw(), in data coordinates, so a pan and a zoom both reuse the
  recording. setData and setBrush open a new generation, which is what makes the next frame record
  again.
*/
class RangeBand : public QCPAbstractPlottable
{
public:
    RangeBand(QCPAxis* keyAxis, QCPAxis* valueAxis)
        : QCPAbstractPlottable(keyAxis, valueAxis)
    { }

    void setData(const QVector<double>& keys, const QVector<double>& lower,
                 const QVector<double>& upper)
    {
        mKeys = keys;
        mLower = lower;
        mUpper = upper;
        contentChanged();
    }

    void setBrush(const QBrush& brush)
    {
        mBrush = brush;
        contentChanged();
    }

    void setLabel(const QString& label)
    {
        mLabel = label;
        contentChanged();
    }

    void draw(QCPDrawContext& context) const override
    {
        if (mKeys.size() < 2)
            return;
        std::vector<QPointF> lower, upper;
        lower.reserve(mKeys.size());
        upper.reserve(mKeys.size());
        for (int i = 0; i < mKeys.size(); ++i)
        {
            lower.emplace_back(mKeys[i], mLower[i]);
            upper.emplace_back(mKeys[i], mUpper[i]);
        }
        context.fillBetween(std::make_shared<const std::vector<QPointF>>(std::move(upper)),
                            std::make_shared<const std::vector<QPointF>>(std::move(lower)),
                            mBrush);

        QCPTextStyle style;
        style.color = QColor(20, 20, 20);
        style.alignment = Qt::AlignBottom | Qt::AlignHCenter;
        const int middle = mKeys.size() / 2;
        context.text(QCP::Data { mKeys[middle], mUpper[middle] }, mLabel, style);
    }

private:
    QVector<double> mKeys, mLower, mUpper;
    QString mLabel = QStringLiteral("band");
    QBrush mBrush = QColor(90, 160, 240, 90);
};
