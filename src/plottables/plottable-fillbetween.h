#pragma once

#include "plottable.h"

#include <optional>

class QCPGraph;

/*!
  \brief The area between two lines of a graph, one brush per side.

  A fill between owns no data. Every recording reads both lines' sources, builds the strip rows of
  their union of keys (qcp::fill::unionRows), cuts them where the lines cross
  (qcp::fill::splitAtCrossings) and records one filled polygon per run, with the brush of the side
  that run lies on: \ref brush where the first line is above the second, \ref belowBrush where it is
  below. An unset \ref belowBrush fills both sides with \ref brush, and a Qt::NoBrush leaves that
  side empty.

  It reports no key or value range of its own: the lines carry the fit, so a fill never widens what
  rescaleAxes() fits. It is not selectable by default, so a click inside it reaches the lines under
  it and pans the view.

  \see QCPAxisRect::fillBetween, QCPGraph::setComponentFill
*/
class QCP_LIB_DECL QCPFillBetween : public QCPPlottable
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPFillBetween)

public:
    QCPFillBetween();
    ~QCPFillBetween() override;

    // getters:
    [[nodiscard]] QCPGraph* firstLine() const;

    [[nodiscard]] int firstComponent() const { return mFirstComponent; }

    [[nodiscard]] QCPGraph* secondLine() const;

    [[nodiscard]] int secondComponent() const { return mSecondComponent; }

    [[nodiscard]] std::optional<QBrush> belowBrush() const { return mBelowBrush; }

    // setters:
    //! The two lines the fill spans. Passing the same graph twice fills between two of its
    //! components. Registers the fill as a dependent of both, so a data change on either makes it
    //! record again; a line that goes away leaves the fill drawing nothing until this is called
    //! again with live lines.
    void setLines(QCPGraph* first, int firstComponent, QCPGraph* second, int secondComponent);
    //! The brush of the side where the first line is below the second; none fills both sides with
    //! \ref brush, and Qt::NoBrush leaves that side empty.
    void setBelowBrush(std::optional<QBrush> brush);

signals:
    void linesChanged();
    void belowBrushChanged();

protected:
    using QCPPlottable::drawLegendIcon; // the painter form replays the icon this class records
    void draw(QCPDrawContext& context) const override;
    //! The legend row's swatch: \ref brush above, \ref belowBrush under it when there is one.
    void drawLegendIcon(QCPDrawContext& icon) const override;

    //! The recording reads both lines' data and axes: a change to either makes it stale.
    [[nodiscard]] quint64 dependencyRevision() const override;

private:
    friend class TestFillBetween; //!< reads the recording count: a stale band is that bug
    friend class TestMultiGraph;  //!< reads the ribbon command, as it did on the graph's fill
    friend class TestUserSystem;  //!< the skew band's recording follows its mapping

    void detachLines();
    void attachLines();
    //! Whether \a component names a column of \a line and draws; one misuse report when it does
    //! not name one at all.
    [[nodiscard]] static bool componentDraws(const QCPGraph& line, int component);

    QPointer<QCPGraph> mFirstLine;
    QPointer<QCPGraph> mSecondLine;
    int mFirstComponent = 0;
    int mSecondComponent = 0;
    std::optional<QBrush> mBelowBrush;
    QMetaObject::Connection mFirstDestroyed;
    QMetaObject::Connection mSecondDestroyed;
};
