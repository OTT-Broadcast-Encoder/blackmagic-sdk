/* -LICENSE-START-
** Copyright (c) 2017 Blackmagic Design
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

#ifndef BMD_DECKLINKAPICONFIGURATION_v10_11_H
#define BMD_DECKLINKAPICONFIGURATION_v10_11_H

#include "DeckLinkAPIConfiguration.h"

// Interface ID Declarations

BMD_CONST REFIID IID_IDeckLinkConfiguration_v10_11                       = /* EF90380B-4AE5-4346-9077-E288E149F129 */ {0xEF,0x90,0x38,0x0B,0x4A,0xE5,0x43,0x46,0x90,0x77,0xE2,0x88,0xE1,0x49,0xF1,0x29};

/* Enum BMDDeckLinkConfigurationID_v10_11 - DeckLink Configuration ID */

typedef uint32_t BMDDeckLinkConfigurationID_v10_11;
enum _BMDDeckLinkConfigurationID_v10_11 {

    /* Video Input/Output Integers */

    bmdDeckLinkConfigDuplexMode_v10_11                              = /* 'dupx' */ 0x64757078,
};

// Forward Declarations

class IDeckLinkConfiguration_v10_11;

/* Interface IDeckLinkConfiguration_v10_11 - DeckLink Configuration interface */

class BMD_PUBLIC IDeckLinkConfiguration_v10_11 : public IUnknown
{
public:
    virtual HRESULT SetFlag (/* in */ BMDDeckLinkConfigurationID cfgID, /* in */ bool value) = 0;
    virtual HRESULT GetFlag (/* in */ BMDDeckLinkConfigurationID cfgID, /* out */ bool *value) = 0;
    virtual HRESULT SetInt (/* in */ BMDDeckLinkConfigurationID cfgID, /* in */ int64_t value) = 0;
    virtual HRESULT GetInt (/* in */ BMDDeckLinkConfigurationID cfgID, /* out */ int64_t *value) = 0;
    virtual HRESULT SetFloat (/* in */ BMDDeckLinkConfigurationID cfgID, /* in */ double value) = 0;
    virtual HRESULT GetFloat (/* in */ BMDDeckLinkConfigurationID cfgID, /* out */ double *value) = 0;
    virtual HRESULT SetString (/* in */ BMDDeckLinkConfigurationID cfgID, /* in */ const char *value) = 0;
    virtual HRESULT GetString (/* in */ BMDDeckLinkConfigurationID cfgID, /* out */ const char **value) = 0;
    virtual HRESULT WriteConfigurationToPreferences (void) = 0;

protected:
    virtual ~IDeckLinkConfiguration_v10_11 () {} // call Release method to drop reference count
};


#endif /* defined(BMD_DECKLINKAPICONFIGURATION_v10_11_H) */
