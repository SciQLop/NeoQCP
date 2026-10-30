#pragma once
#include <painting/contour-extractor.h>
#include <QFont>
#include <QPen>
#include <QRectF>
#include <QPointF>
#include <QVector>
#include <cstdint>

class QCPAxis;
class QCPColorMapData;
class QCPColormapRenderer;
class QCPPainter;
class QPainter;

//! Contour lines and their labels over a grid drawn by a QCPColormapRenderer.
//!
//! Lines and labels live in the image's UV space (see QCPContourExtractor) and reach pixels
//! through the rectangle the image is drawn into, so they follow any axis scale and reversal
//! exactly like the image. On screen the lines and labels are GPU quads over the image; exports
//! and painter-drawn images get them painted.
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

    explicit QCPContourOverlay(QCPColormapRenderer& renderer) : mRenderer(renderer) { }

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

    //! Before the renderer draws \a grid: uploads the GPU lines, or hands the painter lines to
    //! the renderer for an export or a painter-drawn image.
    void update(const QCPColorMapData* grid, const QCPPainter* painter, bool imageWasInvalidated,
                const QCPAxis* keyAxis, const QCPAxis* valueAxis);
    //! After the renderer drew \a grid.
    void drawLabels(const QCPColorMapData* grid, QCPPainter* painter, const QCPAxis* keyAxis,
                    const QCPAxis* valueAxis);

    // Exposed for tests: what the last update() uploaded, and the labels the last draw placed.
    [[nodiscard]] const QVector<float>& lastUv() const { return mLastUv; }
    [[nodiscard]] QVector<double> lastLevels() const { return mLastLevels; }
    [[nodiscard]] const QVector<Label>& lastLabels() const { return mLastLabels; }
    //! How many times the grid was marched. Repeated exports of one frame must not re-march it.
    [[nodiscard]] uint64_t extractionCount() const { return mExtractionCount; }

private:
    struct Key
    {
        uint64_t generation = 0;
        QVector<double> levels;
        int maxDim = 0;
        bool mirrorX = false, mirrorY = false;
        bool operator==(const Key&) const = default;
    };
    struct Lines
    {
        Key key;
        QVector<QCPContourExtractor::ContourLine> lines;
        QVector<QLineF> segments; //!< all levels, for the painter
    };
    //! What the GPU label overlay looks like.
    struct LabelKey
    {
        Key lines;
        QRectF imageRect, area;
        double dpr = 0;
        QFont font;
        QColor color;
        bool operator==(const LabelKey&) const = default;
    };

    [[nodiscard]] QVector<double> resolveLevels(const QCPColorMapData* grid) const;
    const Lines& linesFor(const QCPColorMapData* grid, const Key& key, bool exporting);
    void clear();
    void invalidate() { ++mGeneration; }
    [[nodiscard]] bool labelFits(const QPointF& at, double level) const;
    void paintLabels(QPainter* painter, const QRectF& imageRect, const QRectF& area);

    QCPColormapRenderer& mRenderer;
    QVector<double> mLevels;
    int mAutoCount = 0;
    QPen mPen { Qt::white, 1.0 };
    bool mLabelsEnabled = false;

    uint64_t mGeneration = 1;
    //! One slot for the screen resolution, one for exports: switching between them re-marches nothing.
    Lines mCache[2];
    const Lines* mDrawn = nullptr;
    Key mLastKey;
    LabelKey mLastLabelKey;
    QVector<float> mLastUv;
    QVector<double> mLastLevels;
    QVector<Label> mLastLabels;
    uint64_t mExtractionCount = 0;
};
