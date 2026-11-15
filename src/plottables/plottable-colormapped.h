#pragma once
#include "plottable.h"
#include <axis/axis.h>
#include <colorgradient.h>
#include <layoutelements/layoutelement-colorscale.h>

//! The colour mapping a plottable shares with a QCPColorScale: gradient, data range, scale type.
class QCP_LIB_DECL QCPColorMapped : public QCPPlottable
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPColorMapped)
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QCPRange dataRange READ dataRange WRITE setDataRange NOTIFY dataRangeChanged)
    Q_PROPERTY(QCPAxis::ScaleType dataScaleType READ dataScaleType WRITE setDataScaleType NOTIFY
                   dataScaleTypeChanged)
    Q_PROPERTY(QCPColorGradient gradient READ gradient WRITE setGradient NOTIFY gradientChanged)
    Q_PROPERTY(
        QCPColorScale* colorScale READ colorScale WRITE setColorScale NOTIFY colorScaleChanged)
    /// \endcond

public:
    explicit QCPColorMapped(bool transparentNan = false,
                            QCP::Capabilities required = QCP::capRectangular
                                | QCP::capStraightKey | QCP::capColourScale);
    ~QCPColorMapped() override;

    // getters:
    [[nodiscard]] QCPRange dataRange() const { return mDataRange; }

    [[nodiscard]] QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }

    [[nodiscard]] QCPColorGradient gradient() const { return mGradient; }

    //! Null when no scale drives this plottable, including after the scale is deleted.
    [[nodiscard]] QCPHandle<QCPColorScale> colorScale() const
    {
        return QCPHandle<QCPColorScale>(mColorScale);
    }

    // setters:
    Q_SLOT void setDataRange(const QCPRange& dataRange);
    Q_SLOT void setDataScaleType(QCPAxis::ScaleType scaleType);
    Q_SLOT void setGradient(const QCPColorGradient& gradient);
    //! Joins \a colorScale, or none with null. The choice sticks: a plottable given null never
    //! joins its rect's scale on its own, and one given a scale keeps it through moves and new
    //! data. Until called, the plottable follows its key axis rect's scale. Virtual: a subclass
    //! with its own rule (a frame that has no rect scale to fall back to) refuses through a base
    //! pointer and the property.
    virtual void setColorScale(QCPColorScale* colorScale);
    //! A move to another rect's axes takes that rect's scale when this one followed its old rect's;
    //! a scale the user picked, or none, stays.
    void setSystem(QCPCoordinateSystem* system) override;

    // non-property methods:
    //! Fits the data range to colorDataBounds(). A class whose bounds can be cached or partial
    //! overrides it to say what \a recalculate means for it.
    virtual void rescaleDataRange(bool recalculate = false);

    //! The range of the coloured values over all the data; \a found is false when there is none.
    [[nodiscard]] virtual QCPRange colorDataBounds(bool& found) const = 0;

    //! Whether being added to a plot makes this plottable join its new key axis rect's scale.
    //! A colour map always does; a line only once it has colour values.
    [[nodiscard]] virtual bool joinsOnArrival() const { return true; }

signals:
    void dataRangeChanged(const QCPRange& newRange);
    void dataScaleTypeChanged(QCPAxis::ScaleType scaleType);
    void gradientChanged(const QCPColorGradient& newGradient);
    void colorScaleChanged();

protected:
    //! Called after the gradient, range or scale type changed: repaint, or hand it to a renderer.
    virtual void colorMappingChanged() { update(); }

    //! Takes the key axis rect's scale, if it has one, and hands its values to the renderer.
    //! Does nothing once setColorScale() was called. Called at the end of a subclass constructor,
    //! where colorMappingChanged() is the subclass's.
    void followKeyAxisRectScale();
    //! Leaves the rect's scale when this plottable follows it; a scale the user picked stays.
    //! For a plottable that stops colouring: a plain line shows no colour bar.
    void leaveRectScale();
    //! Whether the scale attached is one setColorScale() picked, not the rect's.
    [[nodiscard]] bool hasUserColorScale() const;

    // property members:
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType;
    QCPColorGradient mGradient;
    //! Whether the caller set each one, and so whether a scale may still fill it when it is joined.
    bool mGradientChosen = false;
    bool mDataRangeChosen = false;

private:
    //! Which scale this plottable uses: its rect's (the default), the one setColorScale() picked,
    //! or none. Only setColorScale() changes it; dropping the scale on the way (no colour values
    //! left, the scale deleted, a take) keeps it.
    enum class ScaleChoice
    {
        Rect,
        User,
        None
    };

    friend class QCPColorScale;
    // adopt() joins the new rect's scale through followKeyAxisRectScale(); take() leaves the scale.
    friend class QCustomPlot;
    // Its bar shows while one of its users shows the scale.
    friend class QCPAxisRect;
    //! Whether this plottable maps anything through its scale now: a rect shows its own scale only
    //! while one of its users does. A curve whose values no longer match its data does not.
    [[nodiscard]] virtual bool showsColorScale() const { return true; }
    //! \a keepsChosenValues is for arriving in a rect whose scale already has one: the values a
    //! caller chose with setGradient() or setDataRange() are then not the scale's to overwrite.
    //! setColorScale(), which names a scale the caller picked, takes that scale's values whole.
    void attachColorScale(QCPColorScale* colorScale, bool keepsChosenValues = false);
    void applyDataRange(const QCPRange& dataRange);
    void applyDataScaleType(QCPAxis::ScaleType scaleType);
    void applyGradient(const QCPColorGradient& gradient);
    void detachColorScale();

    ScaleChoice mScaleChoice = ScaleChoice::Rect;
    QCPColorScale* mColorScale = nullptr;
};
