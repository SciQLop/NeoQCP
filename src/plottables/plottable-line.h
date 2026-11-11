#pragma once
#include "plottable-colormapped.h"
#include "plottable-color-mapper.h"
#include <QStringList>
#include <memory>
#include <optional>
#include <vector>

class QCPAbstractMultiDataSource;

//! Shared base of the line plottables.
//!
//! Owns the components (one line each, with its own pens, marker style and line style) and the
//! colour-by-scalar mapper shared by them. Subclasses supply the data and draw the components;
//! every setter here changes what they draw, so it dirties the layer through lineChanged().
class QCP_LIB_DECL QCPAbstractLine : public QCPAbstractColorMapped
{
    Q_OBJECT
public:
    enum LineStyle { lsNone, lsLine, lsStepLeft, lsStepRight, lsStepCenter, lsImpulse };
    Q_ENUM(LineStyle)

    struct Component {
        QString name;
        QPen pen;
        QPen selectedPen;
        QCPScatterStyle scatterStyle;
        QCPDataSelection selection;
        bool visible = true;
        bool inLegend = true;
        bool hiddenByErrorBars = false; //!< out of the legend as an error bound, not by the user
        //! Set through setComponentLineStyle(): a direct write leaves the cached lines stale.
        LineStyle lineStyle = lsLine;
    };

    explicit QCPAbstractLine(QCPAxis* keyAxis, QCPAxis* valueAxis,
                             QCP::Capabilities required = QCP::capRectangular | QCP::capStraightKey | QCP::capColourScale);

    // Components
    [[nodiscard]] int componentCount() const { return mComponents.size(); }
    Component& component(int index) { return mComponents[index]; }
    const Component& component(int index) const { return mComponents[index]; }
    //! Shows or hides one component and repaints the layer. Setting `component(i).visible`
    //! directly is not observed: a layer that translates after a pan would keep the old
    //! GPU entries.
    void setComponentVisible(int index, bool visible);
    //! Keeps a component out of the legend (a band edge, an error bound) while it still draws
    //! and counts for auto-scale.
    void setComponentInLegend(int index, bool inLegend);
    [[nodiscard]] QVector<int> legendComponents() const;
    void setComponentNames(const QStringList& names);
    void setComponentColors(const QList<QColor>& colors);
    void setComponentPens(const QList<QPen>& pens);

    //! The style given to new components; setLineStyle() also applies it to every component.
    [[nodiscard]] LineStyle lineStyle() const { return mLineStyle; }
    void setLineStyle(LineStyle style);
    void setComponentLineStyle(int index, LineStyle style);
    void setScatterStyle(const QCPScatterStyle& style);
    void setComponentScatterStyle(int index, const QCPScatterStyle& style);
    [[nodiscard]] int scatterSkip() const { return mScatterSkip; }
    void setScatterSkip(int skip);

    //! A pen for every component (as QCPGraph::setPen does for its single line).
    void setPen(const QPen& pen) override;
    //! Qt::NoBrush clears every component's fill; any other brush fills it to the value-axis
    //! zero baseline.
    void setBrush(const QBrush& brush) override;

    // Colour by scalar: one value per key, shared by all components.
    // QCustomPlot convention: these setters do not replot -- the caller must
    // call replot() (the first colouring may repaint on its own once
    // startColouring() has rebuilt what it needs).
    // The gradient, range and scale type are the colour mapping inherited from
    // QCPAbstractColorMapped: a coloured line follows its key axis rect's colour
    // scale, and setColorScale() on an uncoloured line makes it a user at once.
    // A plain line never shows a colour bar; clearColorValues() drops the scale.
    void setColorValues(std::shared_ptr<const std::vector<double>> values);
    void setColorValues(std::vector<double> values);
    void clearColorValues();
    [[nodiscard]] bool hasColorValues() const;
    //! A plain line shows no colour bar, and must not pick one up by being re-added.
    [[nodiscard]] bool joinsOnArrival() const override { return hasColorValues(); }

Q_SIGNALS:
    //! A component was shown or hidden: the legend group row draws one segment per visible one.
    void componentVisibilityChanged();

protected:
    //! Gives component \a index the brush a component fill takes: NoBrush clears it, anything
    //! else fills it. Called for every component by setBrush() and for the new ones by
    //! applyRememberedStyle().
    virtual void applyBrushToComponent(int index, const QBrush& brush)
    {
        Q_UNUSED(index);
        Q_UNUSED(brush);
    }
    //! Gives a component created after a setter the pen, fill and marker style that setter
    //! installed: subclasses call it when their data brings a new component.
    void applyRememberedStyle(int index);
    //! How many colour values the data takes: one per point, none before any data.
    [[nodiscard]] virtual std::optional<int> colorValueCount() const { return std::nullopt; }
    //! Whether this line can draw \a style; a subclass refuses the others as misuse.
    [[nodiscard]] virtual bool acceptsLineStyle(LineStyle) const { return true; }
    //! Whether a source is staged, so colour values are stashed instead of applied.
    [[nodiscard]] virtual bool hasPendingDataSource() const { return false; }
    //! The first colouring: the subclass rebuilds whatever the per-point colours need.
    virtual void colorValuesStarted() {}
    //! The first colouring: joins the rect's scale if this line has none, then rebuilds.
    void startColouring();
    //! The finite colour values: positive only on a log data scale. found is false with none.
    [[nodiscard]] QCPRange colorDataBounds(bool& found) const override;
    //! Feeds the base's gradient, range and scale type to the mapper, then redraws.
    void colorMappingChanged() override;
    //! The cached drawing is stale: mark the layer dirty.
    virtual void lineChanged() { markDirty(); }
    //! A component's visibility changed: mark the layer dirty.
    virtual void componentVisibilityToggled() { markDirty(); }

    static QPen defaultSelectedPen(const QPen& pen);
    //! \a style as it draws for \a pen: a style with no pen of its own takes it.
    static QCPScatterStyle markerStyle(QCPScatterStyle style, const QPen& pen);

    QVector<Component> mComponents;
    LineStyle mLineStyle = lsLine;
    int mScatterSkip = 0;
    // Set by setPen/setBrush/setScatterStyle: what a component created later must take. Unset
    // means the subclass's default palette, as before.
    std::optional<QPen> mExplicitPen;
    std::optional<QBrush> mExplicitBrush;
    std::optional<QCPScatterStyle> mExplicitScatterStyle;
    qcp::ColorScalarMapper mColor;
    // Set while a source is staged: validated against that source's size, applied by
    // QCPMultiGraph::commitPendingData().
    std::shared_ptr<const std::vector<double>> mPendingColorValues;

    friend class TestMultiGraph;
    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;
};
