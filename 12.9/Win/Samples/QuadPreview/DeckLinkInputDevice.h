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
#include <QString>
#include "QuadPreviewEvents.h"
#include <DeckLinkAPI.h>
#include "com_ptr.h"

class DeckLinkInputDevice : public IDeckLinkInputCallback
{
public:
	using DeckLinkDisplayModeQueryFunc = std::function<void(IDeckLinkDisplayMode*)>;

	DeckLinkInputDevice(QObject* parent, com_ptr<IDeckLink>& deckLink);
	virtual ~DeckLinkInputDevice();

	// IUnknown interface
	virtual HRESULT		QueryInterface(REFIID iid, LPVOID *ppv) override;
	virtual ULONG		AddRef() override;
	virtual ULONG		Release() override;

	// IDeckLinkInputCallback interface
	virtual HRESULT		VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override;
	virtual HRESULT		VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override;

	// Other methods
	bool								initialize(void);
	HRESULT								getDeviceName(QString& deviceName);
	bool								isCapturing(void) const { return m_currentlyCapturing; }
	bool								supportsFormatDetection(void) const { return m_supportsFormatDetection; }
	BMDVideoConnection					getVideoConnections(void) const { return (BMDVideoConnection) m_supportedInputConnections; }
	bool								isActive(void);

	bool								startCapture(BMDDisplayMode displayMode, IDeckLinkScreenPreviewCallback* screenPreviewCallback, bool applyDetectedInputMode);
	void								stopCapture(void);

	void								querySupportedVideoModes(DeckLinkDisplayModeQueryFunc func);
	HRESULT								setInputVideoConnection(BMDVideoConnection connection);

	com_ptr<IDeckLink>					getDeckLinkInstance(void) const { return m_deckLink; }
	com_ptr<IDeckLinkInput>				getDeckLinkInput(void) const { return m_deckLinkInput; }
	com_ptr<IDeckLinkConfiguration>		getDeckLinkConfiguration(void) const { return m_deckLinkConfig; }

private:
	std::atomic<ULONG>					m_refCount;
	QObject*							m_owner;
	//
	com_ptr<IDeckLink>					m_deckLink;
	com_ptr<IDeckLinkInput>				m_deckLinkInput;
	com_ptr<IDeckLinkConfiguration>		m_deckLinkConfig;
	//
	bool								m_supportsFormatDetection;
	bool								m_currentlyCapturing;
	bool								m_applyDetectedInputMode;
	bool								m_lastValidFrameStatus;
	int64_t								m_supportedInputConnections;
	BMDVideoConnection					m_selectedInputConnection;
	//
};

class DeckLinkInputFormatChangedEvent : public QEvent
{
public:
	DeckLinkInputFormatChangedEvent(BMDDisplayMode displayMode) :
		QEvent(kVideoFormatChangedEvent), m_displayMode(displayMode) {}
	virtual ~DeckLinkInputFormatChangedEvent() {}

	BMDDisplayMode displayMode() const { return m_displayMode; }

private:
	BMDDisplayMode m_displayMode;
};

namespace
{
	inline bool isDeviceActive(com_ptr<IDeckLink>& deckLink)
	{
		com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, deckLink);
		int64_t								intAttribute;

		if (!deckLinkAttributes)
			return false;

		if (deckLinkAttributes->GetInt(BMDDeckLinkDuplex, &intAttribute) != S_OK)
			return false;

		return ((BMDDuplexMode) intAttribute) != bmdDuplexInactive;
	}
}

