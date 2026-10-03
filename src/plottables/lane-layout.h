#pragma once
#include "../global.h"
#include <QObject>
#include <QRect>
#include <QStringList>
#include <QVector>
#include <optional>
#include <vector>

struct QCPLaneBand
{
    double top;
    double bottom;
};

//! Lane geometry shared by every QCPIntervals of one axis rect. Lanes keep a stable
//! index (what interval data refers to) and a separate display order.
class QCP_LIB_DECL QCPLaneLayout : public QObject
{
    Q_OBJECT
public:
    enum Placement { plStrip, plLanes };
    Q_ENUM(Placement)

    explicit QCPLaneLayout(QObject* parent = nullptr) : QObject(parent) {}

    int laneIndex(const QString& name);
    //! laneIndex for each name, with a single \c changed for all the new ones.
    QVector<int> laneIndices(const QStringList& names);
    [[nodiscard]] QStringList laneNames() const { return mNames; }
    void setDisplayOrder(const QStringList& names);
    [[nodiscard]] QStringList displayOrder() const;
    [[nodiscard]] const std::vector<int>& displayLanes() const { return mOrder; }
    bool renameLane(const QString& from, const QString& to);

    [[nodiscard]] bool laneVisible(int lane) const { return displayPosition(lane) >= 0; }
    [[nodiscard]] int visibleLaneCount() const { return static_cast<int>(mOrder.size()); }
    [[nodiscard]] int laneHeight() const { return mLaneHeight; }
    void setLaneHeight(int px);
    [[nodiscard]] Placement placement() const { return mPlacement; }
    void setPlacement(Placement placement);

    //! Pixel height of one lane: \ref laneHeight on a strip, an equal share of the axis rect
    //! in a lanes plot, whose y axis (and so its lane names) spans the whole rect.
    [[nodiscard]] double lanePixelHeight(const QRect& axisRect) const;
    [[nodiscard]] std::optional<QCPLaneBand> laneBand(int lane, const QRect& axisRect) const;
    [[nodiscard]] int laneAt(double y, const QRect& axisRect) const;
    [[nodiscard]] int totalHeight() const { return totalRows() * mLaneHeight; }

    //! How many rows each lane needs for \a owner (stacked overlaps); a lane is as tall as the
    //! most any owner asks, and at least one row.
    void setLaneRows(const QObject* owner, std::vector<int> rowsPerLane);
    void removeLaneRows(const QObject* owner);
    [[nodiscard]] int laneRows(int lane) const;
    [[nodiscard]] int totalRows() const;
    //! Row coordinate (0 = top) of the middle of \a lane, for labelling it on an axis.
    [[nodiscard]] double laneCentreRow(int lane) const;
    [[nodiscard]] int positionOf(int lane) const { return displayPosition(lane); }
    [[nodiscard]] quint64 generation() const { return mGeneration; }

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] int displayPosition(int lane) const;
    void notify();

    QStringList mNames;
    std::vector<int> mOrder; // lane indices, top to bottom
    int mLaneHeight = 14;
    Placement mPlacement = plStrip;
    quint64 mGeneration = 0;
    std::vector<std::pair<const QObject*, std::vector<int>>> mLaneRows;

    [[nodiscard]] int rowsBefore(int position) const;
};
