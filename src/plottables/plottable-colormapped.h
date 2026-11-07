#pragma once
#include "plottable.h"
#include <axis/axis.h>
#include <colorgradient.h>
#include <layoutelements/layoutelement-colorscale.h>

//! The colour mapping a plottable shares with a QCPColorScale: gradient, data range, scale type.
class QCP_LIB_DECL QCPAbstractColorMapped : public QCPAbstractPlottable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QCPRange dataRange READ dataRange WRITE setDataRange NOTIFY dataRangeChanged)
    Q_PROPERTY(QCPAxis::ScaleType dataScaleType READ dataScaleType WRITE setDataScaleType NOTIFY
                   dataScaleTypeChanged)
    Q_PROPERTY(QCPColorGradient gradient READ gradient WRITE setGradient NOTIFY gradientChanged)
    Q_PROPERTY(QCPColorScale* colorScale READ colorScale WRITE setColorScale)
    /// \endcond

public:
    explicit QCPAbstractColorMapped(QCPAxis* keyAxis, QCPAxis* valueAxis,
                                    bool transparentNan = false);
    ~QCPAbstractColorMapped() override;

    // getters:
    [[nodiscard]] QCPRange dataRange() const { return mDataRange; }

    [[nodiscard]] QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }

    [[nodiscard]] QCPColorGradient gradient() const { return mGradient; }

    //! Null when no scale drives this plottable, including after the scale is deleted.
    [[nodiscard]] QCPColorScale* colorScale() const { return mColorScale; }

    // setters:
    Q_SLOT void setDataRange(const QCPRange& dataRange);
    Q_SLOT void setDataScaleType(QCPAxis::ScaleType scaleType);
    Q_SLOT void setGradient(const QCPColorGradient& gradient);
    void setColorScale(QCPColorScale* colorScale);

    // non-property methods:
    //! Fits the data range to colorDataBounds(). A class whose bounds can be cached or partial
    //! overrides it to say what \a recalculate means for it.
    virtual void rescaleDataRange(bool recalculate = false);

    //! The range of the coloured values over all the data; \a found is false when there is none.
    [[nodiscard]] virtual QCPRange colorDataBounds(bool& found) const = 0;

signals:
    void dataRangeChanged(const QCPRange& newRange);
    void dataScaleTypeChanged(QCPAxis::ScaleType scaleType);
    void gradientChanged(const QCPColorGradient& newGradient);
    void colorScaleChanged();

protected:
    //! Called after the gradient, range or scale type changed: repaint, or hand it to a renderer.
    virtual void colorMappingChanged() { markDirty(); }

    //! Takes the key axis rect's scale, if it has one, and hands its values to the renderer.
    //! Called at the end of a subclass constructor, where colorMappingChanged() is the subclass's.
    void followKeyAxisRectScale();

    // property members:
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType;
    QCPColorGradient mGradient;

private:
    friend class QCPColorScale;
    void applyDataRange(const QCPRange& dataRange);
    void applyDataScaleType(QCPAxis::ScaleType scaleType);
    void applyGradient(const QCPColorGradient& gradient);
    void detachColorScale();

    QCPColorScale* mColorScale = nullptr;
};
