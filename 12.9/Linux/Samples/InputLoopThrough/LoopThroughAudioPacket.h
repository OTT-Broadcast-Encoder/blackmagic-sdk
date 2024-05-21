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

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include "DeckLinkAPI.h"

class LoopThroughAudioPacket
{
public:
	// Allow construction of LoopThroughAudioPacket with optional deleter so the buffer can be released externally
	using Deleter = std::function<void(void)>;
	
	LoopThroughAudioPacket(void* audioBuffer, long sampleFrameCount, const Deleter& deleter = nullptr) :
		m_audioBuffer(audioBuffer),
		m_sampleFrameCount(sampleFrameCount),
		m_deleter(deleter),
		m_audioStreamTime(0),
		m_inputPacketArrivedReferenceTime(0),
		m_outputPacketScheduledReferenceTime(0)
	{
	}
	
	virtual ~LoopThroughAudioPacket(void)
	{
		// If constructed with deleter, call to release/free buffer
		if (m_deleter)
			m_deleter();
	};

	void*			getBuffer(void) const { return m_audioBuffer; }
	long			getSampleFrameCount(void) const { return m_sampleFrameCount; }

	void			setAudioPacket(void* audioBuffer, long sampleFrameCount, const Deleter& deleter = nullptr)
	{
		if (m_deleter)
			// Call deleter on previously assigned buffer
			m_deleter();
		
		m_audioBuffer		= audioBuffer;
		m_sampleFrameCount	= sampleFrameCount;
		m_deleter			= deleter;
	}
	
	void			setAudioStreamTime(const BMDTimeValue time) { m_audioStreamTime = time; }
	void			setInputPacketArrivedReferenceTime(const BMDTimeValue time) { m_inputPacketArrivedReferenceTime = time; }
	void			setOutputPacketScheduledReferenceTime(const BMDTimeValue time) { m_outputPacketScheduledReferenceTime = time; }

	BMDTimeValue	getAudioStreamTime(void) const { return m_audioStreamTime; }
	BMDTimeValue	getProcessingLatency(void) const { return m_outputPacketScheduledReferenceTime - m_inputPacketArrivedReferenceTime; }

private:
	void*			m_audioBuffer;
	long			m_sampleFrameCount;
	Deleter			m_deleter;

	BMDTimeValue	m_audioStreamTime;
	BMDTimeValue	m_inputPacketArrivedReferenceTime;
	BMDTimeValue	m_outputPacketScheduledReferenceTime;
};