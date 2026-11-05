#pragma once

// Private to the library: a command whose fill the GPU cannot express is painted into an image, so
// the RHI replay can draw it at the command's place in the layer instead of the painter pass.

#include "draw-commands.h"

#include <QImage>
#include <QRectF>

class QPainter;

namespace qcp
{
//! True when \a command's fill cannot become a GPU vertex colour: a texture, gradient or hatch
//! brush, a free-form polygon (no fan centre), or series of different lengths.
bool fillNeedsRaster(const QCPDrawCommand& command);

//! The pixel bounds the rasterised fill has to cover; when \a withStroke the outline is rasterised
//! with it, so its outer half and antialiasing fit inside.
QRectF fallbackPixelBounds(const QCPDrawCommand& command, const QCPDrawFrame& frame, double dpr,
                           bool withStroke);

//! Paints \a command's fill into \a painter, \a withStroke adds the outline. Pixels, no scaling.
void paintFallback(QPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                   bool withStroke);

//! Rasterises \a command over \a bounds (logical pixels) at \a dpr: premultiplied ARGB,
//! antialiased.
QImage rasterizeFallback(const QCPDrawCommand& command, const QRectF& bounds,
                         const QCPDrawFrame& frame, double dpr, bool withStroke);
} // namespace qcp
