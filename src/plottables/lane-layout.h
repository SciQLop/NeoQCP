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

    [[nodiscard]] std::optional<QCPLaneBand> laneBand(int lane, const QRect& axisRect) const;
    [[nodiscard]] int laneAt(double y, const QRect& axisRect) const;
    [[nodiscard]] int totalHeight() const { return visibleLaneCount() * mLaneHeight; }
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
};
