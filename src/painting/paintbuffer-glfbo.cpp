/***************************************************************************
**                                                                        **
**  NeoQCP, a fork of QCustomPlot, an easy to use, modern plotting widget **
**  for Qt.                                                               **
**  Copyright (C) 2011-2022 Emanuel Eichhammer (QCustomPlot)              **
**  Copyright (C) 2024 The NeoQCP Authors                                 **
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

#include "paintbuffer-glfbo.h"
#include "painter.h"
#include "Profiling.hpp"


#ifdef QCP_OPENGL_FBO
////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPaintBufferGlFbo
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPPaintBufferGlFbo
  \brief A paint buffer based on OpenGL frame buffers objects, using hardware accelerated rendering

  This paint buffer is one of the OpenGL paint buffers which facilitate hardware accelerated plot
  rendering. It is based on OpenGL frame buffer objects (fbo) and is used in Qt versions 5.0 and
  higher. (See \ref QCPPaintBufferGlPbuffer used in older Qt versions.)

  The OpenGL paint buffers are used if \ref QCustomPlot::setOpenGl is set to true, and if they are
  supported by the system.
*/

/*!
  Creates a \ref QCPPaintBufferGlFbo instance with the specified \a size and \a devicePixelRatio,
  if applicable.

  All frame buffer objects shall share one OpenGL context and paint device, which need to be set up
  externally and passed via \a glContext and \a glPaintDevice. The set-up is done in \ref
  QCustomPlot::setupOpenGl and the context and paint device are managed by the parent QCustomPlot
  instance.
*/
QCPPaintBufferGlFbo::QCPPaintBufferGlFbo(const QSize& size, double devicePixelRatio,
                                         const QString& layerName,
                                         QWeakPointer<QOpenGLContext> glContext,
                                         QWeakPointer<QOpenGLPaintDevice> glPaintDevice)
        : QCPAbstractPaintBuffer(size, devicePixelRatio, layerName)
        , mGlContext(glContext)
        , mGlPaintDevice(glPaintDevice)
        , mGlFrameBuffer(0)
{
    QCPPaintBufferGlFbo::reallocateBuffer();
}

QCPPaintBufferGlFbo::~QCPPaintBufferGlFbo()
{
    if (mGlFrameBuffer)
        delete mGlFrameBuffer;
}

/* inherits documentation from base class */
QCPPainter* QCPPaintBufferGlFbo::startPainting()
{
    QSharedPointer<QOpenGLPaintDevice> paintDevice = mGlPaintDevice.toStrongRef();
    QSharedPointer<QOpenGLContext> context = mGlContext.toStrongRef();
    if (!paintDevice)
    {
        qDebug() << Q_FUNC_INFO << "OpenGL paint device doesn't exist";
        return 0;
    }
    if (!context)
    {
        qDebug() << Q_FUNC_INFO << "OpenGL context doesn't exist";
        return 0;
    }
    if (!mGlFrameBuffer)
    {
        qDebug() << Q_FUNC_INFO
                 << "OpenGL frame buffer object doesn't exist, reallocateBuffer was not called?";
        return 0;
    }

    if (QOpenGLContext::currentContext() != context.data())
        context->makeCurrent(context->surface());
    mGlFrameBuffer->bind();
    QCPPainter* result = new QCPPainter(paintDevice.data());
    return result;
}

/* inherits documentation from base class */
void QCPPaintBufferGlFbo::donePainting()
{
    if (mGlFrameBuffer && mGlFrameBuffer->isBound())
        mGlFrameBuffer->release();
    else
        qDebug() << Q_FUNC_INFO << "Either OpenGL frame buffer not valid or was not bound";
}

/* inherits documentation from base class */
void QCPPaintBufferGlFbo::draw(QCPPainter* painter) const
{
    PROFILE_HERE_N("QCPPaintBufferGlFbo::draw");
    PROFILE_PASS_TXT(mLayerName.toStdString().c_str(), mLayerName.size());
    if (!painter || !painter->isActive())
    {
        qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
        return;
    }
    if (!mGlFrameBuffer)
    {
        qDebug() << Q_FUNC_INFO
                 << "OpenGL frame buffer object doesn't exist, reallocateBuffer was not called?";
        return;
    }
    auto ctx_ref = mGlContext.toStrongRef();
    auto ctx = ctx_ref.data();
    if (QOpenGLContext::currentContext() != ctx)
        ctx->makeCurrent(ctx->surface());

    const int targetWidth = mGlFrameBuffer->width() / mDevicePixelRatio;
    const int targetHeight = mGlFrameBuffer->height() / mDevicePixelRatio;
    QRect targetRect(0, 0, targetWidth, targetHeight);

    auto image = [this]()
    {
        PROFILE_HERE_N("QOpenGLFramebufferObject::toImage");
        return this->mGlFrameBuffer->toImage();
    }();
    image.setDevicePixelRatio(mDevicePixelRatio);
    {
        PROFILE_HERE_N("QPainter::drawImage");
        painter->drawImage(targetRect, image, image.rect());
    }
}

/* inherits documentation from base class */
void QCPPaintBufferGlFbo::clear(const QColor& color)
{
    QSharedPointer<QOpenGLContext> context = mGlContext.toStrongRef();
    if (!context)
    {
        qDebug() << Q_FUNC_INFO << "OpenGL context doesn't exist";
        return;
    }
    if (!mGlFrameBuffer)
    {
        qDebug() << Q_FUNC_INFO
                 << "OpenGL frame buffer object doesn't exist, reallocateBuffer was not called?";
        return;
    }

    if (QOpenGLContext::currentContext() != context.data())
        context->makeCurrent(context->surface());
    mGlFrameBuffer->bind();
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mGlFrameBuffer->release();
}

/* inherits documentation from base class */
void QCPPaintBufferGlFbo::reallocateBuffer()
{
    // release and delete possibly existing framebuffer:
    if (mGlFrameBuffer)
    {
        if (mGlFrameBuffer->isBound())
            mGlFrameBuffer->release();
        delete mGlFrameBuffer;
        mGlFrameBuffer = 0;
    }

    QSharedPointer<QOpenGLPaintDevice> paintDevice = mGlPaintDevice.toStrongRef();
    QSharedPointer<QOpenGLContext> context = mGlContext.toStrongRef();
    if (!paintDevice)
    {
        qDebug() << Q_FUNC_INFO << "OpenGL paint device doesn't exist";
        return;
    }
    if (!context)
    {
        qDebug() << Q_FUNC_INFO << "OpenGL context doesn't exist";
        return;
    }

    // create new fbo with appropriate size:
    context->makeCurrent(context->surface());
    QOpenGLFramebufferObjectFormat frameBufferFormat;
    frameBufferFormat.setSamples(context->format().samples());
    frameBufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    mGlFrameBuffer = new QOpenGLFramebufferObject(mSize * mDevicePixelRatio, frameBufferFormat);
    if (paintDevice->size() != mSize * mDevicePixelRatio)
        paintDevice->setSize(mSize * mDevicePixelRatio);
    paintDevice->setDevicePixelRatio(mDevicePixelRatio);
}

#ifdef NEOQCP_BATCH_DRAWING


NeoQCPBatchDrawingHelper::NeoQCPBatchDrawingHelper(const QSize& size, double devicePixelRatio,
                                                   QWeakPointer<QOpenGLContext> glContext,
                                                   QWeakPointer<QOpenGLPaintDevice> glPaintDevice)
        : mSize(size)
        , mDevicePixelRatio(devicePixelRatio)
        , mGlContext(glContext)
        , mGlPaintDevice(glPaintDevice)
        , mGlFrameBuffer(0)
        , mResolveFbo(nullptr)
#ifdef NEOQCP_MANUAL_GL_IMAGE
        , mGlImage(nullptr)
#endif
{
    reallocateBuffer();
}

NeoQCPBatchDrawingHelper::~NeoQCPBatchDrawingHelper()
{
#ifdef NEOQCP_MANUAL_GL_IMAGE
    if (mGlImage)
    {
        delete mGlImage;
        mGlImage = nullptr;
    }
#endif
    if (mResolveFbo)
    {
        if (mResolveFbo->isBound())
            mResolveFbo->release();
        delete mResolveFbo;
        mResolveFbo = nullptr;
    }
    if (mGlFrameBuffer)
    {
        if (mGlFrameBuffer->isBound())
            mGlFrameBuffer->release();
        delete mGlFrameBuffer;
        mGlFrameBuffer = nullptr;
    }
}

void NeoQCPBatchDrawingHelper::batch_draw(
    const QList<QSharedPointer<QCPAbstractPaintBuffer>>& buffers, QCPPainter* painter) const
{
    PROFILE_HERE_N("QCPPaintBufferGlFbo::batch_draw");
    if (!painter || !painter->isActive())
    {
        qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
        return;
    }
    if (std::size(buffers) == 1)
    {
        // If we are the only buffer, just draw ourselves
        buffers[0]->draw(painter);
        return;
    }
    // If we are not the only buffer, we need to draw all buffers
    auto ctx_ref = mGlContext.toStrongRef();
    auto ctx = ctx_ref.data();
    if (QOpenGLContext::currentContext() != ctx)
        ctx->makeCurrent(ctx->surface());

    const int targetWidth = mGlFrameBuffer->width() / mDevicePixelRatio;
    const int targetHeight = mGlFrameBuffer->height() / mDevicePixelRatio;
    QRect targetRect(0, 0, targetWidth, targetHeight);

    mGlFrameBuffer->bind();
    if (!mGlFrameBuffer->isValid() || !mGlFrameBuffer->isBound())
    {
        qDebug() << Q_FUNC_INFO << "Destination framebuffer object is not valid";
        return;
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glViewport(0, 0, mGlFrameBuffer->width(), mGlFrameBuffer->height());
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, mGlFrameBuffer->width(), 0, mGlFrameBuffer->height(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0, 1.0, 1.0, 1.0);

    // Clear the destination buffer before drawing
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);


    {


        PROFILE_HERE_N("QOpenGLFramebufferObject::blitFramebuffer");
        for (auto& buffer : buffers)
        {
            auto glBuffer = qSharedPointerCast<QCPPaintBufferGlFbo>(buffer);
            if (!glBuffer || !glBuffer->mGlFrameBuffer || !glBuffer->mGlFrameBuffer->isValid())
            {
                qDebug() << Q_FUNC_INFO << "Invalid buffer passed";
                continue;
            }
            QOpenGLFramebufferObject::blitFramebuffer(mResolveFbo, glBuffer->mGlFrameBuffer,
                                                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
            mGlFrameBuffer->bind();
            glBindTexture(GL_TEXTURE_2D, mResolveFbo->texture());
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex2f(0, 0);
            glTexCoord2f(1, 0);
            glVertex2f(mGlFrameBuffer->width(), 0);
            glTexCoord2f(1, 1);
            glVertex2f(mGlFrameBuffer->width(), mGlFrameBuffer->height());
            glTexCoord2f(0, 1);
            glVertex2f(0, mGlFrameBuffer->height());
            glEnd();
        }
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glPopAttrib();
    }
#ifdef NEOQCP_MANUAL_GL_IMAGE
    glBindTexture(GL_TEXTURE_2D, mGlFrameBuffer->texture());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    {
        PROFILE_HERE_N("glGetTexImage");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, mGlImage->bits());
    }
    {
        PROFILE_HERE_N("QPainter::drawImage");
        painter->drawImage(targetRect, mGlImage->mirrored(), mGlImage->rect());
    }

#else
    auto image = [this]()
    {
        PROFILE_HERE_N("QOpenGLFramebufferObject::toImage");
        return mGlFrameBuffer->toImage();
    }();
    image.setDevicePixelRatio(mDevicePixelRatio);
    {
        PROFILE_HERE_N("QPainter::drawImage");
        painter->drawImage(targetRect, image, image.rect());
    }
#endif
}

void NeoQCPBatchDrawingHelper::setSize(const QSize &size)
{
    if (mSize != size)
    {
        mSize = size;
        reallocateBuffer();
    }
}

void NeoQCPBatchDrawingHelper::setDevicePixelRatio(double ratio)
{
    if (mDevicePixelRatio != ratio)
    {
        mDevicePixelRatio = ratio;
        reallocateBuffer();
    }
}

void NeoQCPBatchDrawingHelper::setSizeAndDevicePixelRatio(const QSize &size, double ratio)
{
    if (mSize != size || mDevicePixelRatio != ratio)
    {
        mSize = size;
        mDevicePixelRatio = ratio;
        reallocateBuffer();
    }
}


void NeoQCPBatchDrawingHelper::reallocateBuffer()
{

    if (mGlFrameBuffer)
    {
        if (mGlFrameBuffer->isBound())
            mGlFrameBuffer->release();
        delete mGlFrameBuffer;
        mGlFrameBuffer = 0;
    }
    mGlFrameBuffer = new QOpenGLFramebufferObject(mSize * mDevicePixelRatio,
                                                  QOpenGLFramebufferObject::CombinedDepthStencil);

#ifdef NEOQCP_MANUAL_GL_IMAGE
    if (mGlImage)
    {
        delete mGlImage;
    }
#endif
    if (mResolveFbo)
    {
        if (mResolveFbo->isBound())
            mResolveFbo->release();
        delete mResolveFbo;
    }
    mResolveFbo = new QOpenGLFramebufferObject(mSize * mDevicePixelRatio,
                                               QOpenGLFramebufferObject::CombinedDepthStencil);

#ifdef NEOQCP_MANUAL_GL_IMAGE
    mGlImage = new QImage(mResolveFbo->size(), QImage::Format_ARGB32_Premultiplied);
    mGlImage->setDevicePixelRatio(mDevicePixelRatio);
#endif
}
#endif // NEOQCP_BATCH_DRAWING
#endif // QCP_OPENGL_FBO
