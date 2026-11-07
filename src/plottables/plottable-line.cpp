#include "plottable-line.h"
#include "../core.h"
#include "../datasource/abstract-multi-datasource.h"
#include "../misuse.h"
#include <algorithm>

QCPAbstractLine::QCPAbstractLine(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
{
}

QCPScatterStyle QCPAbstractLine::markerStyle(QCPScatterStyle style, const QPen& pen)
{
    if (!style.isPenDefined())
        style.setPen(pen);
    return style;
}

QPen QCPAbstractLine::defaultSelectedPen(const QPen& pen)
{
    QPen sel = pen;
    sel.setWidthF(pen.widthF() + 1.5);
    QColor c = pen.color();
    c.setAlphaF(qMin(1.0, c.alphaF() * 1.3));
    sel.setColor(c);
    return sel;
}

// --- Component API ---

void QCPAbstractLine::setComponentNames(const QStringList& names)
{
    int n = qMin(names.size(), mComponents.size());
    if (std::equal(names.cbegin(), names.cbegin() + n, mComponents.cbegin(),
                   [](const QString& name, const auto& c) { return c.name == name; }))
        return;
    markDirty();
    for (int i = 0; i < n; ++i)
        mComponents[i].name = names[i];
}

void QCPAbstractLine::setComponentColors(const QList<QColor>& colors)
{
    int n = qMin(colors.size(), mComponents.size());
    if (std::equal(colors.cbegin(), colors.cbegin() + n, mComponents.cbegin(),
                   [](const QColor& color, const auto& c) { return c.pen.color() == color; }))
        return;
    markDirty();
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen.setColor(colors[i]);
        mComponents[i].selectedPen = defaultSelectedPen(mComponents[i].pen);
    }
    pensChanged(n);
}

void QCPAbstractLine::setComponentPens(const QList<QPen>& pens)
{
    int n = qMin(pens.size(), mComponents.size());
    if (std::equal(pens.cbegin(), pens.cbegin() + n, mComponents.cbegin(),
                   [](const QPen& pen, const auto& c) { return c.pen == pen; }))
        return;
    markDirty();
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen = pens[i];
        mComponents[i].selectedPen = defaultSelectedPen(pens[i]);
    }
    pensChanged(n);
}

// No equality early return: the components can have been given another pen since the last call
// (setComponentPens, setComponentColors, component(i).pen), so mPen alone does not say what is
// drawn. The fan-out is cheap.
void QCPAbstractLine::setPen(const QPen& pen)
{
    mPen = pen;
    mExplicitPen = pen;
    for (auto& c : mComponents) {
        c.pen = pen;
        c.selectedPen = defaultSelectedPen(pen);
    }
    pensChanged(mComponents.size());
    lineChanged();
}

// Same for the brush: setComponentFill()/clearComponentFill() can have replaced a component's
// fill since the last call.
void QCPAbstractLine::setBrush(const QBrush& brush)
{
    mBrush = brush;
    mExplicitBrush = brush;
    for (int i = 0; i < mComponents.size(); ++i)
        applyBrushToComponent(i, brush);
    lineChanged();
}

void QCPAbstractLine::applyRememberedStyle(int index)
{
    if (index < 0 || index >= mComponents.size())
        return;
    auto& c = mComponents[index];
    if (mExplicitPen) {
        c.pen = *mExplicitPen;
        c.selectedPen = defaultSelectedPen(*mExplicitPen);
    }
    if (mExplicitScatterStyle)
        c.scatterStyle = *mExplicitScatterStyle;
    if (mExplicitBrush)
        applyBrushToComponent(index, *mExplicitBrush);
}

void QCPAbstractLine::setComponentInLegend(int index, bool inLegend)
{
    if (index < 0 || index >= mComponents.size())
        return;
    const bool hidden = mComponents[index].hiddenByErrorBars;
    if (mComponents[index].inLegend == inLegend && !hidden)
        return;
    markDirty();
    mComponents[index].hiddenByErrorBars = false; // from here on, the caller's choice
    if (mComponents[index].inLegend != inLegend)
    {
        mComponents[index].inLegend = inLegend;
        qcpEmit(this, mParentPlot, &QCPAbstractLine::componentVisibilityChanged);
    }
}

QVector<int> QCPAbstractLine::legendComponents() const
{
    QVector<int> shown;
    for (int i = 0; i < mComponents.size(); ++i)
        if (mComponents[i].inLegend)
            shown.append(i);
    return shown;
}

void QCPAbstractLine::setComponentVisible(int index, bool visible)
{
    if (index < 0 || index >= mComponents.size() || mComponents[index].visible == visible)
        return;
    mComponents[index].visible = visible;
    componentVisibilityToggled();
    qcpEmit(this, mParentPlot, &QCPAbstractLine::componentVisibilityChanged);
}

void QCPAbstractLine::setLineStyle(LineStyle style)
{
    if (!acceptsLineStyle(style))
        return;
    // mLineStyle alone is not enough: a component may have been given its own style since.
    const bool allComponentsFollow = std::all_of(
        mComponents.cbegin(), mComponents.cend(), [style](const auto& c) { return c.lineStyle == style; });
    if (style == mLineStyle && allComponentsFollow)
        return;
    mLineStyle = style;
    for (auto& c : mComponents)
        c.lineStyle = style;
    lineChanged();
}

void QCPAbstractLine::setComponentLineStyle(int index, LineStyle style)
{
    if (!acceptsLineStyle(style))
        return;
    if (index < 0 || index >= mComponents.size() || mComponents[index].lineStyle == style)
        return;
    mComponents[index].lineStyle = style;
    lineChanged();
}

void QCPAbstractLine::setScatterStyle(const QCPScatterStyle& style)
{
    mExplicitScatterStyle = style;
    for (auto& c : mComponents)
        c.scatterStyle = style;
    lineChanged();
}

void QCPAbstractLine::setComponentScatterStyle(int index, const QCPScatterStyle& style)
{
    if (index < 0 || index >= mComponents.size())
        return;
    mComponents[index].scatterStyle = style;
    lineChanged();
}

void QCPAbstractLine::setScatterSkip(int skip)
{
    skip = qMax(0, skip);
    if (skip == mScatterSkip)
        return;
    mScatterSkip = skip;
    lineChanged();
}

// --- Colour by scalar ---

void QCPAbstractLine::setColorValues(std::shared_ptr<const std::vector<double>> values)
{
    markDirty();
    if (!values || values->empty())
        return clearColorValues();
    // A pending source (staged but not yet committed) is what the values will apply to
    // once the swap lands -- validate against its size, not the still-displayed source's.
    const std::optional<int> expected = colorValueCount();
    if (!expected)
    {
        // Before any data there is nothing to compare against: nothing to colour, not misuse.
        clearColorValues();
        return;
    }
    if (static_cast<int>(values->size()) != *expected)
    {
        clearColorValues(); // refused: the line is drawn uncoloured, not left stale
        QCP_CHECK_OR_RETURN(false, "setColorValues() needs one value per key", );
        return;
    }
    if (hasPendingDataSource())
    {
        mPendingColorValues = std::move(values);
        return;
    }
    const bool wasColoured = mColor.hasValues();
    mColor.setValues(std::move(values));
    if (!wasColoured)
    {
        lineChanged();
        colorValuesStarted();
    }
}

void QCPAbstractLine::setColorValues(std::vector<double> values)
{
    setColorValues(std::make_shared<const std::vector<double>>(std::move(values)));
}

void QCPAbstractLine::clearColorValues()
{
    // Every "uncolour" route (wrong length, an empty vector, or an explicit call) must
    // also drop a stash left by an earlier, valid setColorValues() while a source was
    // pending -- otherwise a line "coloured" only through that stash survives this call
    // and the swap re-colours it from the stale stash.
    mPendingColorValues.reset();
    if (!mColor.hasValues())
        return;
    mColor.clearValues();
    lineChanged();
}

bool QCPAbstractLine::hasColorValues() const { return mColor.hasValues(); }

void QCPAbstractLine::setColorGradient(const QCPColorGradient& gradient)
{
    if (gradient == mColor.gradient())
        return;
    markDirty();
    mColor.setGradient(gradient);
}

QCPColorGradient QCPAbstractLine::colorGradient() const { return mColor.gradient(); }

void QCPAbstractLine::setColorRange(const QCPRange& range)
{
    if (range == mColor.range())
        return;
    markDirty();
    mColor.setRange(range);
}

QCPRange QCPAbstractLine::colorRange() const { return mColor.range(); }

void QCPAbstractLine::setColorScaleType(QCPAxis::ScaleType type)
{
    if (type == mColor.scaleType())
        return;
    markDirty();
    mColor.setScaleType(type);
}

QCPAxis::ScaleType QCPAbstractLine::colorScaleType() const { return mColor.scaleType(); }
