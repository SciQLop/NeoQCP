#include "drawable.h"

#include "core.h"
#include "misuse.h"
#include "painting/command-replay-rhi.h"
#include "painting/draw-commands.h"
#include "painting/draw-context.h"
#include "painting/painter.h"
#include "painting/raster-fallback.h"
#include "plottables/plottable-draw-utils.h"

#include <QDebug>
#include <QPainter>

namespace
{
//! True when a replay of \a list depends on the view, not only on the transform: the commands that
//! measure or rasterise in pixels.
bool usesTheView(const QCPDrawList& list)
{
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        if (std::holds_alternative<qcp::TextCmd>(command)
            || std::holds_alternative<qcp::PathCmd>(command)
            || std::holds_alternative<qcp::ImageCmd>(command)
            || std::holds_alternative<qcp::PainterCmd>(command))
            return true;
    }
    return false;
}
} // namespace

struct QCPDrawnCore::State
{
    QCPDrawList recording;
    QCPResolveCache resolve;
    qcp::QCPRasterFallback raster;
    QCPExtrusionCache extrusion;
    quint64 generation = 0;
    quint64 recordedGeneration = 0;
    quint64 recordedEpoch = 0;
    int recordings = 0;
    bool recorded = false;
    bool viewDependent = false;
    bool insideDraw = false;
    bool reportedMissingAxis = false;
};

QCPDrawnCore::QCPDrawnCore() : mState(std::make_unique<State>()) { }

QCPDrawnCore::~QCPDrawnCore() = default;

bool QCPDrawnCore::contentChanged()
{
    if (mState->insideDraw)
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "contentChanged() from inside draw(): ignored, it would "
                          "re-record on every frame");
        return false;
    }
    ++mState->generation;
    return true;
}

int QCPDrawnCore::recordingCount() const
{
    return mState->recordings;
}

quint64 QCPDrawnCore::resolveRemapCount() const
{
    return mState->resolve.remapCount();
}

bool QCPDrawnCore::ensureRecording(const QCPDrawable& object, const QCPDrawnView& view)
{
    if (!view.keyAxis || !view.valueAxis)
    {
        if (!mState->reportedMissingAxis)
        {
            mState->reportedMissingAxis = true;
            qDebug() << "NeoQCP: a drawn object has no key or value axis; it is not drawn";
        }
        return false;
    }
    mState->reportedMissingAxis = false;
    const quint64 epoch = view.plot ? view.plot->viewEpoch() : 0;
    const bool stale = !mState->recorded || mState->recordedGeneration != mState->generation
        || (mState->viewDependent && mState->recordedEpoch != epoch);
    if (!stale)
        return true;
    QCPDrawContext context(view.keyAxis, view.valueAxis, view.axisRect, view.theme);
    mState->insideDraw = true;
    object.draw(context);
    mState->insideDraw = false;
    mState->recording = context.drawList();
    mState->viewDependent = usesTheView(mState->recording);
    mState->recordedGeneration = mState->generation;
    mState->recordedEpoch = epoch;
    mState->recorded = true;
    ++mState->recordings;
    return true;
}

void QCPDrawnCore::paint(const QCPDrawable& object, QCPPainter& painter, const QCPDrawnView& view)
{
    if (!ensureRecording(object, view))
        return;
    const QCPDrawFrame frame
        = qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(view.clip));
    QCPPlottableRhiLayer* plottables
        = painter.isExporting() ? nullptr : qcp::gpuLayer(view.plot, view.layer, false);
    QCPScatterRhiLayer* scatter = plottables ? view.plot->scatterRhiLayer(view.layer) : nullptr;
    if (!plottables || !scatter)
    {
        // An export has no GPU pass, and so has a screen without a backend: the painter draws it
        // all, and never touches the caches a screen frame resolves into.
        qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::All);
        return;
    }
    qcpReplayWithRhi(mState->recording, *plottables, *scatter, frame, mState->resolve,
                     mState->raster, view.plot->bufferDevicePixelRatio(),
                     view.plot->rhiOutputSize(), mState->recordedGeneration, &mState->extrusion);
    qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::PainterOnly,
                         &mState->raster);
}

std::optional<double> QCPDrawnCore::hitTest(const QCPDrawable& object, QCP::Pixels, QVariant*,
                                            const QCPDrawnView& view)
{
    if (!ensureRecording(object, view))
        return std::nullopt;
    return std::nullopt;
}
