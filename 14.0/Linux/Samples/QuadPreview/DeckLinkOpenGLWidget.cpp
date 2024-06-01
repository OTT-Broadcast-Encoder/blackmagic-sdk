/* -LICENSE-START-
** Copyright (c) 2022 Blackmagic Design
**  
** Permission is hereby granted, free of charge, to any person or organization 
** obtaining a copy of the software and accompanying documentation (the 
** "Software") to use, reproduce, display, distribute, sub-license, execute, 
** and transmit the Software, and to prepare derivative works of the Software, 
** and to permit third-parties to whom the Software is furnished to do so, in 
** accordance with:
** 
** (1) if the Software is obtained from Blackmagic Design, the End User License 
** Agreement for the Software Development Kit (“EULA”) available at 
** https://www.blackmagicdesign.com/EULA/DeckLinkSDK; or
** 
** (2) if the Software is obtained from any third party, such licensing terms 
** as notified by that third party,
** 
** and all subject to the following:
** 
** (3) the copyright notices in the Software and this entire statement, 
** including the above license grant, this restriction and the following 
** disclaimer, must be included in all copies of the Software, in whole or in 
** part, and all derivative works of the Software, unless such copies or 
** derivative works are solely in the form of machine-executable object code 
** generated by a source language processor.
** 
** (4) THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
** DEALINGS IN THE SOFTWARE.
** 
** A copy of the Software is available free of charge at 
** https://www.blackmagicdesign.com/desktopvideo_sdk under the EULA.
** 
** -LICENSE-END-
*/

#include "platform.h"
#include "DeckLinkOpenGLWidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QOpenGLFunctions>


///
/// DeckLinkOpenGLOverlay
///

DeckLinkOpenGLOverlayWidget::DeckLinkOpenGLOverlayWidget(QWidget* parent) :
	QWidget(parent)
{
	m_delegate = new DeckLinkPreviewOverlay(this);
}

DeckLinkPreviewOverlay* DeckLinkOpenGLOverlayWidget::delegate()
{
	return m_delegate;
}

void DeckLinkOpenGLOverlayWidget::paintEvent(QPaintEvent *)
{
	m_delegate->paint(this);
}

///
/// DeckLinkOpenGLWidget
///

DeckLinkOpenGLWidget::DeckLinkOpenGLWidget(QWidget* parent) :
	QOpenGLWidget(parent)
{
	GetDeckLinkOpenGLScreenPreviewHelper(m_deckLinkScreenPreviewHelper);
	m_delegate = make_com_ptr<ScreenPreviewCallback>();

	connect(m_delegate.get(), &ScreenPreviewCallback::frameArrived, this, &DeckLinkOpenGLWidget::setFrame, Qt::QueuedConnection);

	m_overlayWidget = new DeckLinkOpenGLOverlayWidget(this);

}

DeckLinkOpenGLWidget::~DeckLinkOpenGLWidget()
{
}

/// QOpenGLWidget methods

void DeckLinkOpenGLWidget::initializeGL()
{
	if (m_deckLinkScreenPreviewHelper)
	{
		m_deckLinkScreenPreviewHelper->InitializeGL();
	}
}

void DeckLinkOpenGLWidget::paintGL()
{
	if (m_deckLinkScreenPreviewHelper)
	{
		m_deckLinkScreenPreviewHelper->PaintGL();
	}
}

void DeckLinkOpenGLWidget::resizeGL(int width, int height)
{
	m_overlayWidget->resize(width, height);

	QOpenGLFunctions* f = context()->functions();
	f->glViewport(0, 0, width, height);
}

/// Other methods

IDeckLinkScreenPreviewCallback* DeckLinkOpenGLWidget::delegate()
{
	return m_delegate.get();
}

DeckLinkPreviewOverlay* DeckLinkOpenGLWidget::overlay()
{
	return m_overlayWidget->delegate();
}

void DeckLinkOpenGLWidget::clear()
{
	overlay()->clear();
	if (m_delegate)
		m_delegate->DrawFrame(nullptr);
}

void DeckLinkOpenGLWidget::setFrame(com_ptr<IDeckLinkVideoFrame> frame)
{
	if (m_deckLinkScreenPreviewHelper)
	{
		m_deckLinkScreenPreviewHelper->SetFrame(frame.get());
		overlay()->setFrame(frame);

		update();
		m_overlayWidget->update();
	}
}
