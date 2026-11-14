#include "plottable-line.h"
#include "../core.h"
#include "../datasource/abstract-multi-datasource.h"
#include "../misuse.h"
#include <algorithm>

/*! \fn void QCPLine::lineStyleChanged(QCPLine::LineStyle lineStyle)

  This signal is emitted when \ref setLineStyle changes \ref lineStyle, after the plot delivers its
  deferred signals. A call that only restyles components that drifted emits nothing.
*/

/*! \fn void QCPLine::scatterStyleChanged(const QCPScatterStyle& scatterStyle)

  This signal is emitted when \ref scatterStyle changes, by \ref setScatterStyle or by \ref
  setComponentScatterStyle on component 0, after the plot delivers its deferred signals.
*/

/*! \fn void QCPLine::scatterSkipChanged(int scatterSkip)

  This signal is emitted when \ref scatterSkip changes, after the plot delivers its deferred
  signals.
*/

QCPLine::QCPLine(QCPAxis* keyAxis, QCPAxis* valueAxis,
                                 QCP::Capabilities required)
    : QCPColorMapped(keyAxis, valueAxis, false, required)
{
    // A coloured line on a fresh rect keeps what the mapper had: gpJet over 0..1. Written through
    // the members, not the setters: the subclasses are not constructed yet.
    mGradient = QCPColorGradient(QCPColorGradient::gpJet);
    mDataRange = QCPRange(0, 1);
}

QCPLine::~QCPLine() = default;

QCPScatterStyle QCPLine::markerStyle(QCPScatterStyle style, const QPen& pen)
{
    if (!style.isPenDefined())
        style.setPen(pen);
    return style;
}

QPen QCPLine::defaultSelectedPen(const QPen& pen)
{
    QPen sel = pen;
    sel.setWidthF(pen.widthF() + 1.5);
    QColor c = pen.color();
    c.setAlphaF(qMin(1.0, c.alphaF() * 1.3));
    sel.setColor(c);
    return sel;
}

// --- Component API ---

void QCPLine::setComponentNames(const QStringList& names)
{
    int n = qMin(names.size(), mComponents.size());
    if (std::equal(names.cbegin(), names.cbegin() + n, mComponents.cbegin(),
                   [](const QString& name, const auto& c) { return c.name == name; }))
        return;
    update();
    for (int i = 0; i < n; ++i)
        mComponents[i].name = names[i];
}

void QCPLine::setComponentColors(const QList<QColor>& colors)
{
    int n = qMin(colors.size(), mComponents.size());
    if (std::equal(colors.cbegin(), colors.cbegin() + n, mComponents.cbegin(),
                   [](const QColor& color, const auto& c) { return c.pen.color() == color; }))
        return;
    update();
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen.setColor(colors[i]);
        mComponents[i].selectedPen = defaultSelectedPen(mComponents[i].pen);
    }
}

QCPScatterStyle QCPLine::scatterStyle() const
{
    if (!mComponents.isEmpty())
        return mComponents.first().scatterStyle;
    return mExplicitScatterStyle.value_or(QCPScatterStyle());
}

void QCPLine::setComponentPens(const QList<QPen>& pens)
{
    int n = qMin(pens.size(), mComponents.size());
    if (std::equal(pens.cbegin(), pens.cbegin() + n, mComponents.cbegin(),
                   [](const QPen& pen, const auto& c) { return c.pen == pen; }))
        return;
    update();
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen = pens[i];
        mComponents[i].selectedPen = defaultSelectedPen(pens[i]);
    }
}

// The early return compares every component, not mPen alone: the components can have been given
// another pen since the last call (setComponentPens, setComponentColors, component(i).pen).
void QCPLine::setPen(const QPen& pen)
{
    // An equal pen everywhere draws nothing new (2.x's setPen did not repaint either).
    const QPen selected = defaultSelectedPen(pen);
    if (mExplicitPen == pen
        && std::all_of(mComponents.cbegin(), mComponents.cend(), [&](const QCPGraphComponent& c)
                       { return c.pen == pen && c.selectedPen == selected; }))
        return;
    // pen() is mPen: a call that only re-pens the components repaints without a penChanged.
    const bool penMoved = mPen != pen;
    mPen = pen;
    mExplicitPen = pen;
    for (auto& c : mComponents) {
        c.pen = pen;
        c.selectedPen = defaultSelectedPen(pen);
    }
    lineChanged();
    if (penMoved)
        qcpEmit(static_cast<QCPPlottable*>(this), mParentPlot,
                &QCPPlottable::penChanged, mPen);
}

// Same for the brush: setComponentFill()/clearComponentFill() can have replaced a component's
// fill since the last call.
void QCPLine::setBrush(const QBrush& brush)
{
    // brush() is mBrush: re-filling components that drifted repaints without a brushChanged.
    const bool brushMoved = mBrush != brush;
    mBrush = brush;
    mExplicitBrush = brush;
    for (int i = 0; i < mComponents.size(); ++i)
        applyBrushToComponent(i, brush);
    lineChanged();
    if (brushMoved)
        qcpEmit(static_cast<QCPPlottable*>(this), mParentPlot,
                &QCPPlottable::brushChanged, mBrush);
}

void QCPLine::applyRememberedStyle(int index)
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

void QCPLine::setComponentInLegend(int index, bool inLegend)
{
    if (index < 0 || index >= mComponents.size())
        return;
    const bool hidden = mComponents[index].hiddenByErrorBars;
    if (mComponents[index].inLegend == inLegend && !hidden)
        return;
    update();
    mComponents[index].hiddenByErrorBars = false; // from here on, the caller's choice
    if (mComponents[index].inLegend != inLegend)
    {
        mComponents[index].inLegend = inLegend;
        qcpEmit(this, mParentPlot, &QCPLine::componentVisibilityChanged);
    }
}

QVector<int> QCPLine::legendComponents() const
{
    QVector<int> shown;
    for (int i = 0; i < mComponents.size(); ++i)
        if (mComponents[i].inLegend)
            shown.append(i);
    return shown;
}

void QCPLine::setComponentVisible(int index, bool visible)
{
    if (index < 0 || index >= mComponents.size() || mComponents[index].visible == visible)
        return;
    mComponents[index].visible = visible;
    componentVisibilityToggled();
    qcpEmit(this, mParentPlot, &QCPLine::componentVisibilityChanged);
}

void QCPLine::setLineStyle(LineStyle style)
{
    if (!acceptsLineStyle(style))
        return;
    // mLineStyle alone is not enough: a component may have been given its own style since.
    const bool allComponentsFollow = std::all_of(
        mComponents.cbegin(), mComponents.cend(), [style](const auto& c) { return c.lineStyle == style; });
    if (style == mLineStyle && allComponentsFollow)
        return;
    const bool moved = style != mLineStyle;
    mLineStyle = style;
    for (auto& c : mComponents)
        c.lineStyle = style;
    lineChanged();
    if (moved)
        qcpEmit(this, mParentPlot, &QCPLine::lineStyleChanged, mLineStyle);
}

void QCPLine::setComponentLineStyle(int index, LineStyle style)
{
    if (!acceptsLineStyle(style))
        return;
    if (index < 0 || index >= mComponents.size() || mComponents[index].lineStyle == style)
        return;
    mComponents[index].lineStyle = style;
    lineChanged();
}

void QCPLine::setScatterStyle(const QCPScatterStyle& style)
{
    const QCPScatterStyle before = scatterStyle();
    mExplicitScatterStyle = style;
    for (auto& c : mComponents)
        c.scatterStyle = style;
    lineChanged();
    scatterStyleMoved(before);
}

void QCPLine::setComponentScatterStyle(int index, const QCPScatterStyle& style)
{
    if (index < 0 || index >= mComponents.size())
        return;
    const QCPScatterStyle before = scatterStyle();
    mComponents[index].scatterStyle = style;
    lineChanged();
    scatterStyleMoved(before); // scatterStyle() is component 0's
}

void QCPLine::scatterStyleMoved(const QCPScatterStyle& before)
{
    if (scatterStyle() != before)
        qcpEmit(this, mParentPlot, &QCPLine::scatterStyleChanged, scatterStyle());
}

void QCPLine::setScatterSkip(int skip)
{
    skip = qMax(0, skip);
    if (skip == mScatterSkip)
        return;
    mScatterSkip = skip;
    lineChanged();
    qcpEmit(this, mParentPlot, &QCPLine::scatterSkipChanged, mScatterSkip);
}

// --- Colour by scalar ---

void QCPLine::setColorValues(std::shared_ptr<const std::vector<double>> values)
{
    // Joining or leaving a scale signals colorScaleChanged: deliver it once this line is done,
    // so a slot may delete it.
    const QCPEmissionScope scope(mParentPlot);
    update();
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
        startColouring();
}

void QCPLine::startColouring()
{
    // A line is a user of a scale only while it is coloured: joining on the first colouring
    // keeps a plain line from showing the rect's colour bar.
    if (!colorScale())
        followKeyAxisRectScale();
    lineChanged();
    colorValuesStarted();
}

void QCPLine::setColorValues(std::vector<double> values)
{
    setColorValues(std::make_shared<const std::vector<double>>(std::move(values)));
}

void QCPLine::clearColorValues()
{
    const QCPEmissionScope scope(mParentPlot);
    // Every "uncolour" route (wrong length, an empty vector, or an explicit call) must
    // also drop a stash left by an earlier, valid setColorValues() while a source was
    // pending -- otherwise a line "coloured" only through that stash survives this call
    // and the swap re-colours it from the stale stash.
    mPendingColorValues.reset();
    const bool hadValues = mColor.hasValues();
    if (hadValues)
        mColor.clearValues();
    leaveRectScale();
    if (hadValues)
        lineChanged();
}

bool QCPLine::hasColorValues() const { return mColor.hasValues(); }

QCPRange QCPLine::colorDataBounds(bool& found) const
{
    found = false;
    if (!mColor.values())
        return { };
    const bool log = dataScaleType() == QCPAxis::stLogarithmic;
    QCPRange bounds;
    for (const double value : *mColor.values())
    {
        if (!std::isfinite(value) || (log && value <= 0))
            continue;
        if (!found)
            bounds = QCPRange(value, value);
        else
            bounds.expand(value);
        found = true;
    }
    return bounds;
}

void QCPLine::colorMappingChanged()
{
    mColor.setGradient(gradient());
    mColor.setRange(dataRange());
    mColor.setScaleType(dataScaleType());
    lineChanged();
}
