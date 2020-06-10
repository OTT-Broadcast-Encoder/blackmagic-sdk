/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "atlbase.h"
#include "mfidl.h"
#include "DeckLinkAPI_h.h"

class AudioSampleQueue : IUnknown
{
public:
	AudioSampleQueue(uint32_t channelCount, BMDAudioSampleType sampleBitDepth);
	virtual ~AudioSampleQueue() = default;

	// IUnknown interface
	virtual HRESULT	__stdcall	QueryInterface(REFIID iid, LPVOID *ppv) override;
	virtual ULONG	__stdcall	AddRef() override;
	virtual ULONG	__stdcall	Release() override;

	// Other public methods
	bool						WaitForInputSample(IMFSample** sample, bool& waitCancelled);
	void						AudioPacketArrived(CComPtr<IDeckLinkAudioInputPacket> packet);
	void						CancelCapture(void);
	void						Reset(void);

private:
	std::atomic<ULONG>								m_refCount;

	uint32_t										m_channelCount;
	BMDAudioSampleType								m_sampleBitDepth;
	std::queue<CComPtr<IDeckLinkAudioInputPacket>>	m_audioPacketQueue;

	bool											m_cancelCapture;
	std::condition_variable							m_audioPacketQueueCondition;
	std::mutex										m_audioPacketQueueMutex;
};
