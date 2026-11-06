#pragma once
#include "plottable.h"
#include "plottable-color-mapper.h"
#include <QStringList>
#include <memory>
#include <vector>

class QCPAbstractMultiDataSource;

//! Shared base of the line plottables.
//!
//! Owns the components (one line each, with its own pens, marker style and line style) and the
//! colour-by-scalar mapper shared by them. Subclasses supply the data and draw the components;
//! every setter here changes what they draw, so it dirties the layer through lineChanged().
class QCP_LIB_DECL QCPAbstractLine : public QCPAbstractPlottable
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

    explicit QCPAbstractLine(QCPAxis* keyAxis, QCPAxis* valueAxis);

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
    void setColorValues(std::shared_ptr<const std::vector<double>> values);
    void setColorValues(std::vector<double> values);
    void clearColorValues();
    [[nodiscard]] bool hasColorValues() const;
    void setColorGradient(const QCPColorGradient& gradient);
    [[nodiscard]] QCPColorGradient colorGradient() const;
    void setColorRange(const QCPRange& range);
    [[nodiscard]] QCPRange colorRange() const;
    void setColorScaleType(QCPAxis::ScaleType type);
    [[nodiscard]] QCPAxis::ScaleType colorScaleType() const;

Q_SIGNALS:
    //! A component was shown or hidden: the legend group row draws one segment per visible one.
    void componentVisibilityChanged();

protected:
    //! Whether a decoration derived from a pen (fill or error bar without its own) must be
    //! rebuilt: called with the number of components that were just repenned.
    virtual void pensChanged(int componentCount) { Q_UNUSED(componentCount); }
    //! Applies a brush to every component: NoBrush clears them, anything else fills them.
    virtual void applyBrushToComponents(const QBrush& brush) { Q_UNUSED(brush); }
    //! The source the colour values must match the size of, when the subclass has one.
    [[nodiscard]] virtual const QCPAbstractMultiDataSource* colorValueSource() const
    {
        return nullptr;
    }
    //! Whether a source is staged, so colour values are stashed instead of applied.
    [[nodiscard]] virtual bool hasPendingDataSource() const { return false; }
    //! The first colouring: the subclass rebuilds whatever the per-point colours need.
    virtual void colorValuesStarted() {}
    //! The cached drawing is stale: mark the layer dirty.
    virtual void lineChanged() { markDirty(); }
    //! A component's visibility changed: mark the layer dirty.
    virtual void componentVisibilityToggled() { markDirty(); }

    static QPen defaultSelectedPen(const QPen& pen);

    QVector<Component> mComponents;
    LineStyle mLineStyle = lsLine;
    int mScatterSkip = 0;
    qcp::ColorScalarMapper mColor;
    // Set while a source is staged: validated against that source's size, applied by
    // QCPMultiGraph::commitPendingData().
    std::shared_ptr<const std::vector<double>> mPendingColorValues;

    friend class TestMultiGraph;
    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;
};
