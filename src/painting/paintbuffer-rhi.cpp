/***************************************************************************
**                                                                        **
**  NeoQCP, a fork of QCustomPlot, an easy to use, modern plotting widget **
**  for Qt.                                                               **
**  Copyright (C) 2011-2022 Emanuel Eichhammer (QCustomPlot)              **
**  Copyright (C) 2025 The NeoQCP Authors                                 **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Authors: Emanuel Eichhammer, The NeoQCP Authors              **
**  Website/Contact: https://www.qcustomplot.com/ (original)              **
**                   https://github.com/SciQLop/NeoQCP (fork)             **
** Original version: 2.1.1 (Date: 06.11.22)                               **
****************************************************************************/

#include "paintbuffer-rhi.h"
#include "painter.h"
#include "Profiling.hpp"

QCPPaintBufferRhi::QCPPaintBufferRhi(const QSize& size, double devicePixelRatio,
                                       const QString& layerName, QRhi* rhi)
    : QCPAbstractPaintBuffer(size, devicePixelRatio, layerName)
    , mRhi(rhi)
{
    QCPPaintBufferRhi::reallocateBuffer();
}

QCPPaintBufferRhi::~QCPPaintBufferRhi()
{
    delete mSrb;
    delete mTexture;
}

void QCPPaintBufferRhi::setSrb(QRhiShaderResourceBindings* srb, QRhiTexture* boundTexture)
{
    delete mSrb;
    mSrb = srb;
    mSrbBoundTexture = boundTexture;
}

QCPPainter* QCPPaintBufferRhi::startPainting()
{
    PROFILE_HERE_N("QCPPaintBufferRhi::startPainting");
    return new QCPPainter(&mStagingImage);
}

void QCPPaintBufferRhi::donePainting()
{
    PROFILE_HERE_N("QCPPaintBufferRhi::donePainting");
    mNeedsUpload = true;
}

void QCPPaintBufferRhi::draw(QCPPainter* painter) const
{
    // Fallback for export paths (savePdf, toPixmap, etc.)
    PROFILE_HERE_N("QCPPaintBufferRhi::draw");
    if (painter && painter->isActive())
    {
        const int targetWidth = mStagingImage.width() / mDevicePixelRatio;
        const int targetHeight = mStagingImage.height() / mDevicePixelRatio;
        painter->drawImage(QRect(0, 0, targetWidth, targetHeight),
                           mStagingImage, mStagingImage.rect());
    }
    else
        qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
}

void QCPPaintBufferRhi::clear(const QColor& color)
{
    PROFILE_HERE_N("QCPPaintBufferRhi::clear");
    mStagingImage.fill(color);
}

void QCPPaintBufferRhi::reallocateBuffer()
{
    PROFILE_HERE_N("QCPPaintBufferRhi::reallocateBuffer");
    setInvalidated();

    QSize pixelSize = mSize * mDevicePixelRatio;

    // ARGB32_Premultiplied is QPainter's native format (BGRA byte order on little-endian).
    // Using a BGRA8 texture avoids the per-upload CPU swizzle that RGBA8 would require.
    // Especially important on Metal (macOS) where there's no driver-side BGRA→RGBA fast path.
    mStagingImage = QImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
    mStagingImage.setDevicePixelRatio(mDevicePixelRatio);
    mStagingImage.fill(Qt::transparent);

    delete mSrb;
    mSrb = nullptr;
    mSrbBoundTexture = nullptr;
    delete mTexture;
    mTexture = nullptr;
    if (mRhi)
    {
        const auto fmt = mRhi->isTextureFormatSupported(QRhiTexture::BGRA8)
            ? QRhiTexture::BGRA8
            : QRhiTexture::RGBA8;
        mTexture = mRhi->newTexture(fmt, pixelSize);
        if (!mTexture->create())
        {
            qDebug() << Q_FUNC_INFO << "Failed to create RHI texture";
            delete mTexture;
            mTexture = nullptr;
        }
    }
}
