/* -LICENSE-START-
** Copyright (c) 2020 Blackmagic Design
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
#include "ProfileCallback.h"

ProfileCallback::ProfileCallback() : 
	m_refCount(1),
	m_haltStreamsCallback(nullptr),
	m_profileActivatedCallback(nullptr)
{
}

HRESULT ProfileCallback::ProfileChanging(IDeckLinkProfile *profileToBeActivated, BOOL streamsWillBeForcedToStop)
{
	// When streamsWillBeForcedToStop is true, the profile to be activated is incompatible with the current
	// profile and capture will be stopped by the DeckLink driver. It is better to notify the
	// controller to gracefully stop capture, so that the UI is set to a known state.
	if (m_haltStreamsCallback && streamsWillBeForcedToStop)
	{
		CComPtr<IDeckLinkProfile> profile = profileToBeActivated;
		m_haltStreamsCallback(profile);
	}
	return S_OK;
}

HRESULT ProfileCallback::ProfileActivated(IDeckLinkProfile *activatedProfile)
{
	// New profile activated
	if (m_profileActivatedCallback)
	{
		CComPtr<IDeckLinkProfile> profile = activatedProfile;
		m_profileActivatedCallback(profile);
	}

	return S_OK;
}

HRESULT ProfileCallback::QueryInterface(REFIID iid, LPVOID *ppv)
{
	HRESULT result = E_NOINTERFACE;

	if (ppv == nullptr)
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = nullptr;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if (iid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (iid == IID_IDeckLinkProfileCallback)
	{
		*ppv = static_cast<IDeckLinkProfileCallback*>(this);
		AddRef();
		result = S_OK;

	}

	return result;
}

ULONG ProfileCallback::AddRef(void)
{
	return ++m_refCount;
}

ULONG ProfileCallback::Release(void)
{
	ULONG newRefValue = --m_refCount;

	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

