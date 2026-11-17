#ifndef QCP_AXISRECT_GRID_H
#define QCP_AXISRECT_GRID_H

#include <QList>
#include <QPointer>

#include <functional>

#include "global.h"

class QCPAlignmentGroup;
class QCPAxis;
class QCPAxisRect;
class QCPLayoutGrid;

/*! \brief The options of QCustomPlot::setAxisRectGrid(): the shares (stored, applied by the
  shared-axis step) and the per-row/per-column stretch factors.

  A stretch list shorter than the grid leaves the missing entries at 1. A longer one, or a factor
  that is not finite and greater than zero, is misuse and the call changes nothing.

  \see QCustomPlot::setAxisRectGrid
*/
struct QCPAxisRectGridOptions
{
    bool shareX = false;
    bool shareY = false;
    QList<double> rowStretch;
    QList<double> columnStretch;
};

/*! \internal

  What QCustomPlot remembers about its axis rect grid between calls: the alignment groups it made
  (one per column for the left/right sides, one per row for the top/bottom ones), the shares a later
  step will unmake, and the options last applied.
*/
struct QCPAxisRectGridState
{
    //! One axis the share moved: \a follower was linked to \a originalSource without functions (or
    //! not linked, when it is null) before the grid linked it; \a hidLabels when the grid hid its
    //! tick labels, which unsharing shows again.
    struct Share
    {
        QPointer<QCPAxis> follower;
        QPointer<QCPAxis> originalSource;
        bool hidLabels = false;
    };

    QList<QPointer<QCPAlignmentGroup>> columnGroups;
    QList<QPointer<QCPAlignmentGroup>> rowGroups;
    QList<Share> shares;
    QCPAxisRectGridOptions options;
    int rows = 0;
    int columns = 0;
    //! True while a call walks the layout: a slot that re-enters is refused, not run on half a grid.
    bool running = false;
};

namespace qcp
{
//! Builds the grid of axis rects in \a layout: validates first (one misuse and false on bad input),
//! drops the rects outside the new grid, fills the missing cells with \a makeRect and aligns every
//! column and row. Keeps the rects already in the kept cells.
bool applyAxisRectGrid(QCPLayoutGrid& layout, int rows, int columns,
                       const QCPAxisRectGridOptions& options, QCPAxisRectGridState& state,
                       const std::function<QCPAxisRect*()>& makeRect);
} // namespace qcp

#endif // QCP_AXISRECT_GRID_H
