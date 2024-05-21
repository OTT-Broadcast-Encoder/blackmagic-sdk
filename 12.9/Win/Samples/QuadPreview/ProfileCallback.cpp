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

#include <QCoreApplication>
#include "ProfileCallback.h"

ProfileCallback::ProfileCallback(QObject* owner) :
	m_refCount(1),
	m_owner(owner),
	m_profileChangingCallback(nullptr)
{
}

// IUnknown methods

HRESULT ProfileCallback::QueryInterface(REFIID iid, LPVOID *ppv)
{
	HRESULT result = S_OK;

	if (ppv == nullptr)
		return E_INVALIDARG;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if (iid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
	}
	else if (iid == IID_IDeckLinkProfileCallback)
	{
		*ppv = static_cast<IDeckLinkProfileCallback*>(this);
		AddRef();
	}
	else
	{
		*ppv = nullptr;
		result = E_NOINTERFACE;
	}

	return result;
}

ULONG ProfileCallback::AddRef()
{
	return ++m_refCount;
}

ULONG ProfileCallback::Release()
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

// IDeckLinkProfileCallback

HRESULT ProfileCallback::ProfileChanging(IDeckLinkProfile* profileToBeActivated, dlbool_t streamsWillBeForcedToStop)
{
	// When streamsWillBeForcedToStop is true, the profile to be activated is incompatible with the current
	// profile and capture will be stopped by the DeckLink driver. It is better to notify the
	// subscriber to gracefully stop capture, so that the UI is set to a known state.
	if ((streamsWillBeForcedToStop) && (m_profileChangingCallback != nullptr))
		m_profileChangingCallback(com_ptr<IDeckLinkProfile>(profileToBeActivated));

	return S_OK;
}

HRESULT ProfileCallback::ProfileActivated(IDeckLinkProfile* activatedProfile)
{
	// New profile activated, inform subscriber to update UI
	QCoreApplication::postEvent(m_owner, new ProfileActivatedEvent(activatedProfile));

	return S_OK;
}
