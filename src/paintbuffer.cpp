/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2022 Emanuel Eichhammer                            **
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
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: https://www.qcustomplot.com/                         **
**             Date: 06.11.22                                             **
**          Version: 2.1.1                                                **
****************************************************************************/

#include "paintbuffer.h"

#include "painter.h"

#include "Profiling.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPAbstractPaintBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPAbstractPaintBuffer
  \brief The abstract base class for paint buffers, which define the rendering backend

  This abstract base class defines the basic interface that a paint buffer needs to provide in
  order to be usable by QCustomPlot.

  A paint buffer manages both a surface to draw onto, and the matching paint device. The size of
  the surface can be changed via \ref setSize. External classes (\ref QCustomPlot and \ref
  QCPLayer) request a painter via \ref startPainting and then perform the draw calls. Once the
  painting is complete, \ref donePainting is called, so the paint buffer implementation can do
  clean up if necessary. Before rendering a frame, each paint buffer is usually filled with a color
  using \ref clear (usually the color is \c Qt::transparent), to remove the contents of the
  previous frame.

  The simplest paint buffer implementation is \ref QCPPaintBufferPixmap which allows regular
  software rendering via the raster engine. Hardware accelerated rendering via pixel buffers and
  frame buffer objects is provided by \ref QCPPaintBufferGlPbuffer and \ref QCPPaintBufferGlFbo.
  They are used automatically if \ref QCustomPlot::setOpenGl is enabled.
*/

/* start documentation of pure virtual functions */

/*! \fn virtual QCPPainter *QCPAbstractPaintBuffer::startPainting() = 0

  Returns a \ref QCPPainter which is ready to draw to this buffer. The ownership and thus the
  responsibility to delete the painter after the painting operations are complete is given to the
  caller of this method.

  Once you are done using the painter, delete the painter and call \ref donePainting.

  While a painter generated with this method is active, you must not call \ref setSize, \ref
  setDevicePixelRatio or \ref clear.

  This method may return 0, if a painter couldn't be activated on the buffer. This usually
  indicates a problem with the respective painting backend.
*/

/*! \fn virtual void QCPAbstractPaintBuffer::draw(QCPPainter *painter) const = 0

  Draws the contents of this buffer with the provided \a painter. This is the method that is used
  to finally join all paint buffers and draw them onto the screen.
*/

/*! \fn virtual void QCPAbstractPaintBuffer::clear(const QColor &color) = 0

  Fills the entire buffer with the provided \a color. To have an empty transparent buffer, use the
  named color \c Qt::transparent.

  This method must not be called if there is currently a painter (acquired with \ref startPainting)
  active.
*/

/*! \fn virtual void QCPAbstractPaintBuffer::reallocateBuffer() = 0

  Reallocates the internal buffer with the currently configured size (\ref setSize) and device
  pixel ratio, if applicable (\ref setDevicePixelRatio). It is called as soon as any of those
  properties are changed on this paint buffer.

  \note Subclasses of \ref QCPAbstractPaintBuffer must call their reimplementation of this method
  in their constructor, to perform the first allocation (this can not be done by the base class
  because calling pure virtual methods in base class constructors is not possible).
*/

/* end documentation of pure virtual functions */
/* start documentation of inline functions */

/*! \fn virtual void QCPAbstractPaintBuffer::donePainting()

  If you have acquired a \ref QCPPainter to paint onto this paint buffer via \ref startPainting,
  call this method as soon as you are done with the painting operations and have deleted the
  painter.

  paint buffer subclasses may use this method to perform any type of cleanup that is necessary. The
  default implementation does nothing.
*/

/* end documentation of inline functions */

/*!
  Creates a paint buffer and initializes it with the provided \a size and \a devicePixelRatio.

  Subclasses must call their \ref reallocateBuffer implementation in their respective constructors.
*/
QCPAbstractPaintBuffer::QCPAbstractPaintBuffer(const QSize &size, double devicePixelRatio, const QString& layerName) :
  mSize(size),
  mDevicePixelRatio(devicePixelRatio),
  mLayerName(layerName),
  mInvalidated(true)
{
}

QCPAbstractPaintBuffer::~QCPAbstractPaintBuffer()
{
}

/*!
  Sets the paint buffer size.

  The buffer is reallocated (by calling \ref reallocateBuffer), so any painters that were obtained
  by \ref startPainting are invalidated and must not be used after calling this method.

  If \a size is already the current buffer size, this method does nothing.
*/
void QCPAbstractPaintBuffer::setSize(const QSize &size)
{
  if (mSize != size)
  {
    mSize = size;
    reallocateBuffer();
  }
}

/*!
  Sets the invalidated flag to \a invalidated.

  This mechanism is used internally in conjunction with isolated replotting of \ref QCPLayer
  instances (in \ref QCPLayer::lmBuffered mode). If \ref QCPLayer::replot is called on a buffered
  layer, i.e. an isolated repaint of only that layer (and its dedicated paint buffer) is requested,
  QCustomPlot will decide depending on the invalidated flags of other paint buffers whether it also
  replots them, instead of only the layer on which the replot was called.

  The invalidated flag is set to true when \ref QCPLayer association has changed, i.e. if layers
  were added or removed from this buffer, or if they were reordered. It is set to false as soon as
  all associated \ref QCPLayer instances are drawn onto the buffer.

  Under normal circumstances, it is not necessary to manually call this method.
*/
void QCPAbstractPaintBuffer::setInvalidated(bool invalidated)
{
  mInvalidated = invalidated;
}

/*!
  Sets the device pixel ratio to \a ratio. This is useful to render on high-DPI output devices.
  The ratio is automatically set to the device pixel ratio used by the parent QCustomPlot instance.

  The buffer is reallocated (by calling \ref reallocateBuffer), so any painters that were obtained
  by \ref startPainting are invalidated and must not be used after calling this method.

  \note This method is only available for Qt versions 5.4 and higher.
*/
void QCPAbstractPaintBuffer::setDevicePixelRatio(double ratio)
{
  if (!qFuzzyCompare(ratio, mDevicePixelRatio))
  {
    mDevicePixelRatio = ratio;
    reallocateBuffer();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPaintBufferPixmap
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPPaintBufferPixmap
  \brief A paint buffer based on QPixmap, using software raster rendering

  This paint buffer is the default and fall-back paint buffer which uses software rendering and
  QPixmap as internal buffer. It is used if \ref QCustomPlot::setOpenGl is false.
*/

/*!
  Creates a pixmap paint buffer instancen with the specified \a size and \a devicePixelRatio, if
  applicable.
*/
QCPPaintBufferPixmap::QCPPaintBufferPixmap(const QSize &size, double devicePixelRatio, const QString &layerName) :
  QCPAbstractPaintBuffer(size, devicePixelRatio, layerName)
{
  QCPPaintBufferPixmap::reallocateBuffer();
}

QCPPaintBufferPixmap::~QCPPaintBufferPixmap()
{
}

/* inherits documentation from base class */
QCPPainter *QCPPaintBufferPixmap::startPainting()
{
  QCPPainter *result = new QCPPainter(&mBuffer);
  return result;
}

/* inherits documentation from base class */
void QCPPaintBufferPixmap::draw(QCPPainter *painter) const
{
  if (painter && painter->isActive())
    painter->drawPixmap(0, 0, mBuffer);
  else
    qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
}

void QCPPaintBufferPixmap::batch_draw(const QList<QSharedPointer<QCPAbstractPaintBuffer> > &buffers, QCPPainter *painter) const
{
  PROFILE_HERE_N("QCPPaintBufferPixmap::batch_draw");
  for (const auto &buffer : buffers)
  {
    buffer->draw(painter);
  }
}

/* inherits documentation from base class */
void QCPPaintBufferPixmap::clear(const QColor &color)
{
  mBuffer.fill(color);
}

/* inherits documentation from base class */
void QCPPaintBufferPixmap::reallocateBuffer()
{
  setInvalidated();
  if (!qFuzzyCompare(1.0, mDevicePixelRatio))
  {
    mBuffer = QPixmap(mSize*mDevicePixelRatio);
    mBuffer.setDevicePixelRatio(mDevicePixelRatio);
  } else
  {
    mBuffer = QPixmap(mSize);
  }
}


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
QCPPaintBufferGlFbo::QCPPaintBufferGlFbo(const QSize &size, double devicePixelRatio, const QString& layerName, QWeakPointer<QOpenGLContext> glContext, QWeakPointer<QOpenGLPaintDevice> glPaintDevice) :
  QCPAbstractPaintBuffer(size, devicePixelRatio, layerName),
  mGlContext(glContext),
  mGlPaintDevice(glPaintDevice),
  mGlFrameBuffer(0),
  mGlImage(nullptr)
{
  QCPPaintBufferGlFbo::reallocateBuffer();
}

QCPPaintBufferGlFbo::~QCPPaintBufferGlFbo()
{
  if (mGlFrameBuffer)
    delete mGlFrameBuffer;
}

/* inherits documentation from base class */
QCPPainter *QCPPaintBufferGlFbo::startPainting()
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
    qDebug() << Q_FUNC_INFO << "OpenGL frame buffer object doesn't exist, reallocateBuffer was not called?";
    return 0;
  }

  if (QOpenGLContext::currentContext() != context.data())
    context->makeCurrent(context->surface());
  mGlFrameBuffer->bind();
  QCPPainter *result = new QCPPainter(paintDevice.data());
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
 void QCPPaintBufferGlFbo::draw(QCPPainter *painter) const
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
     qDebug() << Q_FUNC_INFO << "OpenGL frame buffer object doesn't exist, reallocateBuffer was not called?";
     return;
   }
   auto ctx_ref = mGlContext.toStrongRef();
   auto ctx = ctx_ref.data();
   if (QOpenGLContext::currentContext() != ctx)
       ctx->makeCurrent(ctx->surface());

   const int targetWidth = mGlFrameBuffer->width() / mDevicePixelRatio;
   const int targetHeight = mGlFrameBuffer->height() / mDevicePixelRatio;
   QRect targetRect(0, 0, targetWidth, targetHeight);

   auto image = [this](){
       PROFILE_HERE_N("QOpenGLFramebufferObject::toImage");
       return this->mGlFrameBuffer->toImage();
   }();
   image.setDevicePixelRatio(mDevicePixelRatio);
   {
       PROFILE_HERE_N("QPainter::drawImage");
       painter->drawImage(targetRect, image, image.rect());
   }

}

 void QCPPaintBufferGlFbo::batch_draw(const QList<QSharedPointer<QCPAbstractPaintBuffer> > &buffers, QCPPainter *painter) const
 {
     PROFILE_HERE_N("QCPPaintBufferGlFbo::batch_draw");
     if (!painter || !painter->isActive())
     {
         qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
         return;
     }
     if(std::size(buffers) ==1 && buffers.first().data() == this)
     {
         // If we are the only buffer, just draw ourselves
         draw(painter);
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
     if (mGlImage==nullptr)
     {
         mGlImage = new QImage(mGlFrameBuffer->size(), QImage::Format_ARGB32_Premultiplied);
         mGlImage->setDevicePixelRatio(mDevicePixelRatio);
     }
     else if (mGlImage->size() != mGlFrameBuffer->size())
     {
         delete mGlImage;
         mGlImage = new QImage(mGlFrameBuffer->size(), QImage::Format_ARGB32_Premultiplied);
         mGlImage->setDevicePixelRatio(mDevicePixelRatio);
     }
     QOpenGLFramebufferObject destFbo(mGlFrameBuffer->size(), QOpenGLFramebufferObject::CombinedDepthStencil);
     destFbo.bind();
     if (!destFbo.isValid() || !destFbo.isBound())
     {
         qDebug() << Q_FUNC_INFO << "Destination framebuffer object is not valid";
         return;
     }

     glPushAttrib(GL_ALL_ATTRIB_BITS);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
     glViewport(0, 0, destFbo.width(), destFbo.height());
     glMatrixMode(GL_PROJECTION);
     glPushMatrix();
     glLoadIdentity();
     glOrtho(0, destFbo.width(), 0, destFbo.height(), -1, 1);
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

         // FBO to retrieve non multi-sampled buffers texture
         QOpenGLFramebufferObject resolveFbo(mGlFrameBuffer->size(), QOpenGLFramebufferObject::CombinedDepthStencil);
         //resolveFbo.bind();

         PROFILE_HERE_N("QOpenGLFramebufferObject::blitFramebuffer");
         for(auto &buffer: buffers)
         {
             auto glBuffer = qSharedPointerCast<QCPPaintBufferGlFbo>(buffer);
             if (!glBuffer || !glBuffer->mGlFrameBuffer || !glBuffer->mGlFrameBuffer->isValid())
             {
                 qDebug() << Q_FUNC_INFO << "Invalid buffer passed";
                 continue;
             }
             QOpenGLFramebufferObject::blitFramebuffer(
                 &resolveFbo,
                 glBuffer->mGlFrameBuffer,
                 GL_COLOR_BUFFER_BIT, GL_NEAREST
                 );
             destFbo.bind();
             glBindTexture(GL_TEXTURE_2D, resolveFbo.texture());
             glBegin(GL_QUADS);
             glTexCoord2f(0, 0); glVertex2f(0, 0);
             glTexCoord2f(1, 0); glVertex2f(destFbo.width(), 0);
             glTexCoord2f(1, 1); glVertex2f(destFbo.width(), destFbo.height());
             glTexCoord2f(0, 1); glVertex2f(0, destFbo.height());
             glEnd();
         }
         glMatrixMode(GL_PROJECTION);
         glPopMatrix();
         glMatrixMode(GL_MODELVIEW);
         glPopMatrix();
         glPopAttrib();
     }
#ifdef NEOQCP_MANUAL_GL_IMAGE
     glBindTexture(GL_TEXTURE_2D, destFbo.texture());
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
     auto image = [&destFbo](){
         PROFILE_HERE_N("QOpenGLFramebufferObject::toImage");
         return destFbo.toImage();
     }();
     image.setDevicePixelRatio(mDevicePixelRatio);
     {
         PROFILE_HERE_N("QPainter::drawImage");
         painter->drawImage(targetRect, image, image.rect());
     }
#endif
}

/* inherits documentation from base class */
void QCPPaintBufferGlFbo::clear(const QColor &color)
{
  QSharedPointer<QOpenGLContext> context = mGlContext.toStrongRef();
  if (!context)
  {
    qDebug() << Q_FUNC_INFO << "OpenGL context doesn't exist";
    return;
  }
  if (!mGlFrameBuffer)
  {
    qDebug() << Q_FUNC_INFO << "OpenGL frame buffer object doesn't exist, reallocateBuffer was not called?";
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
  mGlFrameBuffer = new QOpenGLFramebufferObject(mSize*mDevicePixelRatio, frameBufferFormat);
  if (paintDevice->size() != mSize*mDevicePixelRatio)
    paintDevice->setSize(mSize*mDevicePixelRatio);
  paintDevice->setDevicePixelRatio(mDevicePixelRatio);
}


#endif // QCP_OPENGL_FBO
