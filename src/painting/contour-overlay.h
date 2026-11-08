#pragma once
#include <QFont>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <axis/axis.h>
#include <cstdint>
#include <painting/contour-extractor.h>

class QCPColorMapData;
class QCPDrawContext;
class QPainter;

//! Contour lines and their labels over a colourised grid.
//!
//! The lines are recorded as data-space polylines, so a pan, an axis scale change or a reversal
//! need no rebuild. The labels go in one transparent image over the visible part of the grid,
//! recorded last so it draws above the image and its lines.
class QCPContourOverlay
{
public:
    struct Label
    {
        double level;
        QPointF position; //!< pixels
    };

    //! Pixels between two labels of one level, and between any two labels.
    static constexpr double kLabelSpacing = 120.0;
    static constexpr double kLabelClearance = 30.0;
    //! Grid cells marched per dimension on screen; exports march the full grid.
    static constexpr int kScreenMaxDim = 300;

    QCPContourOverlay() = default;

    void setLevels(const QVector<double>& levels);

    [[nodiscard]] QVector<double> levels() const { return mLevels; }

    void setAutoLevels(int count);

    [[nodiscard]] int autoLevelCount() const { return mAutoCount; }

    void setPen(const QPen& pen);

    [[nodiscard]] QPen pen() const { return mPen; }

    void setLabelsEnabled(bool enabled) { mLabelsEnabled = enabled; }

    [[nodiscard]] bool labelsEnabled() const { return mLabelsEnabled; }

    [[nodiscard]] bool active() const { return !mLevels.isEmpty() || mAutoCount > 0; }

    //! A new grid replaces the old one.
    void dataChanged() { ++mGeneration; }

    //! Records the lines of \a grid and, labels on, their labels into \a context.
    void update(const QCPColorMapData* grid, QCPDrawContext& context,
                QCPAxis::ScaleType dataScaleType, const QCPAxis* keyAxis, const QCPAxis* valueAxis);

    // Exposed for tests: the levels of the last recording and the labels it placed.
    [[nodiscard]] QVector<double> lastLevels() const { return mLastLevels; }

    [[nodiscard]] const QVector<Label>& lastLabels() const { return mLastLabels; }

private:
    struct Key
    {
        uint64_t generation = 0;
        QVector<double> levels;
        int maxDim = 0;
        bool operator==(const Key&) const = default;
    };

    struct Lines
    {
        Key key;
        QVector<QCPContourExtractor::ContourLine> lines;
    };

    [[nodiscard]] QVector<double> resolveLevels(const QCPColorMapData* grid,
                                                QCPAxis::ScaleType dataScaleType) const;
    const Lines& linesFor(const QCPColorMapData* grid, const Key& key, bool exporting);
    void recordLines(QCPDrawContext& context, const QCPColorMapData* grid, const Lines& lines,
                     bool keyLog, bool valueLog) const;
    void recordLabels(QCPDrawContext& context, const QCPColorMapData* grid, const QCPAxis* keyAxis,
                      const QCPAxis* valueAxis);
    void clear();

    void invalidate() { ++mGeneration; }

    [[nodiscard]] bool labelFits(const QPointF& at, double level) const;
    void paintLabels(QPainter* painter, const QRectF& imageRect, const QRectF& visible,
                     bool keyIsVertical);

    QVector<double> mLevels;
    int mAutoCount = 0;
    QPen mPen { Qt::white, 1.0 };
    bool mLabelsEnabled = false;

    uint64_t mGeneration = 1;
    //! One slot for the screen resolution, one for exports: switching between them re-marches
    //! nothing.
    Lines mCache[2];
    const Lines* mDrawn = nullptr;
    QVector<double> mLastLevels;
    QVector<Label> mLastLabels;
};
