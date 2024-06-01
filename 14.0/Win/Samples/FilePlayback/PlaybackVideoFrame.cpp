/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
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

#include "stdafx.h"
#include "SourceReader.h"
#include "PlaybackVideoFrame.h"

PlaybackVideoFrame::PlaybackVideoFrame(CComPtr<IMFSample> readSample, int64_t streamTimestamp, long frameWidth, long frameHeight, long defaultStride, BMDPixelFormat pixelFormat) :
	m_refCount(1),
	m_readSample(readSample),
	m_frameWidth(frameWidth),
	m_frameHeight(frameHeight),
	m_pixelFormat(pixelFormat),
	m_frameFlags(bmdFrameFlagDefault),
	m_streamTimestamp(streamTimestamp)
{
	HRESULT hr = m_readSample->ConvertToContiguousBuffer(&m_readBuffer);
	if (hr == S_OK)
	{
		m_read2DBuffer = m_readBuffer;

		if (m_read2DBuffer)
			// If available, use 2D buffer
			hr = m_read2DBuffer->Lock2D(&m_lockedBuffer, &m_frameRowBytes);
		else
		{
			// Use non-2D version.
			hr = m_readBuffer->Lock(&m_lockedBuffer, nullptr, nullptr);
			if (defaultStride < 0)
				// Bottom-up orientation. 
				m_frameFlags |= bmdFrameFlagFlipVertical;
			m_frameRowBytes = abs(defaultStride);
		}
	}

	m_bufferIsLocked = (hr == S_OK);
}

PlaybackVideoFrame::~PlaybackVideoFrame()
{
	if (m_bufferIsLocked)
	{
		if (m_read2DBuffer)
			(void)m_read2DBuffer->Unlock2D();
		else
			(void)m_readBuffer->Unlock();
	}
}

HRESULT __stdcall PlaybackVideoFrame::GetBytes(void** buffer)
{
	if (m_bufferIsLocked)
	{
		*buffer = m_lockedBuffer;
		return S_OK;
	}
	else
	{
		*buffer = nullptr;
		return E_FAIL;
	}
}

BMDTimeValue PlaybackVideoFrame::GetStreamTime(BMDTimeScale timeScale)
{
	return (BMDTimeValue)(((m_streamTimestamp * timeScale) + (kMFTimescale / 2)) / kMFTimescale);
}

HRESULT __stdcall PlaybackVideoFrame::QueryInterface(REFIID riid, void** ppv)
{
	HRESULT result = E_NOINTERFACE;

	if (ppv == NULL)
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = NULL;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if (riid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}

	else if (riid == IID_IDeckLinkVideoFrame)
	{
		*ppv = (IDeckLinkVideoFrame*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG __stdcall PlaybackVideoFrame::AddRef(void)
{
	return ++m_refCount;
}

ULONG __stdcall PlaybackVideoFrame::Release(void)
{
	ULONG newRefCount = --m_refCount;
	if (newRefCount == 0)
		delete this;

	return newRefCount;
}
