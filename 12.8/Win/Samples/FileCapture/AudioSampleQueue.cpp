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
#include "mfapi.h"
#include "SampleQueue.h"
#include "AudioSampleQueue.h"

AudioSampleQueue::AudioSampleQueue(uint32_t channelCount, BMDAudioSampleType sampleBitDepth)
	: m_refCount(1),
	m_channelCount(channelCount),
	m_sampleBitDepth(sampleBitDepth),
	m_cancelCapture(false)
{
}

// IUnknown methods

HRESULT AudioSampleQueue::QueryInterface(REFIID iid, LPVOID* ppv)
{
	*ppv = nullptr;
	return E_NOTIMPL;
}

ULONG AudioSampleQueue::AddRef(void)
{
	return ++m_refCount;
}

ULONG AudioSampleQueue::Release(void)
{
	ULONG newRefValue = --m_refCount;

	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

// Other methods

bool AudioSampleQueue::WaitForInputSample(IMFSample** sample, bool& waitCancelled)
{
	CComPtr<IMFSample>					audioSample;
	CComPtr<IDeckLinkAudioInputPacket>	audioPacket;
	CComPtr<IMFMediaBuffer>				sampleMediaBuffer;
	void*								sampleBuffer;
	void*								audioPacketBuffer;
	BMDTimeValue						packetTime;
	uint32_t							bufferLength;
	HRESULT								hr = S_OK;

	{
		std::unique_lock<std::mutex> lock(m_audioPacketQueueMutex);
		m_audioPacketQueueCondition.wait(lock, [&]{ return !m_audioPacketQueue.empty() || m_cancelCapture; });

		if (!m_audioPacketQueue.empty())
		{
			audioPacket = m_audioPacketQueue.front();
			m_audioPacketQueue.pop();
		}
	}

	if (audioPacket != nullptr)
	{
		// Get pointer to audio buffer to copy to IMFMediaBuffer
		hr = audioPacket->GetBytes(&audioPacketBuffer);
		if (hr != S_OK)
			goto bail;

		// Get time and duration of captured packet
		hr = audioPacket->GetPacketTime(&packetTime, kMFTimescale);
		if (hr != S_OK)
			goto bail;

		bufferLength = audioPacket->GetSampleFrameCount() * (m_channelCount * (m_sampleBitDepth / 8));

		// Create a new memory buffer.
		hr = MFCreateMemoryBuffer(bufferLength, &sampleMediaBuffer);
		if (hr != S_OK)
			goto bail;

		// Lock the buffer and copy the audio samples to the buffer.
		hr = sampleMediaBuffer->Lock((BYTE**)&sampleBuffer, NULL, NULL);
		if (hr != S_OK)
			goto bail;

		memcpy(sampleBuffer, audioPacketBuffer, bufferLength);

		sampleMediaBuffer->Unlock();

		// Set the data length of the buffer.
		hr = sampleMediaBuffer->SetCurrentLength(bufferLength);
		if (hr != S_OK)
			goto bail;

		// Create a media sample and add the buffer to the sample.
		hr = MFCreateSample(&audioSample);
		if (hr != S_OK)
			goto bail;

		hr = audioSample->AddBuffer(sampleMediaBuffer);
		if (hr != S_OK)
			goto bail;

		// Set the time stamp and the duration.
		hr = audioSample->SetSampleTime((LONGLONG)packetTime);
		if (hr != S_OK)
			goto bail;

		hr = audioSample->SetSampleDuration((LONGLONG)audioPacket->GetSampleFrameCount() * kMFTimescale / bmdAudioSampleRate48kHz);

		*sample = audioSample.Detach();
	}

	waitCancelled = m_cancelCapture;

bail:
	return (hr == S_OK);
}

void AudioSampleQueue::AudioPacketArrived(CComPtr<IDeckLinkAudioInputPacket> packet)
{
	{
		std::lock_guard<std::mutex> lock(m_audioPacketQueueMutex);
		m_audioPacketQueue.push(packet);
	}
	m_audioPacketQueueCondition.notify_one();
}

void AudioSampleQueue::CancelCapture()
{
	{
		// signal cancel flag to terminate wait condition
		std::lock_guard<std::mutex> lock(m_audioPacketQueueMutex);
		m_cancelCapture = true;
	}
	m_audioPacketQueueCondition.notify_one();
}

void AudioSampleQueue::Reset()
{
	std::lock_guard<std::mutex> lock(m_audioPacketQueueMutex);
	m_cancelCapture = false;
}
