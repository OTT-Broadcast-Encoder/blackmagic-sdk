/* -LICENSE-START-
** Copyright (c) 2018 Blackmagic Design
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
//
//  SyncController.mm
//  Signal Generator
//

#import <CoreFoundation/CFString.h>

#import "SyncController.h"
#import "SignalGenerator3DVideoFrame.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

// SD 75% Colour Bars
static uint32_t gSD75pcColourBars[8] =
{
	0xeb80eb80, 0xa28ea22c, 0x832c839c, 0x703a7048,
	0x54c654b8, 0x41d44164, 0x237223d4, 0x10801080
};

// HD 75% Colour Bars
static uint32_t gHD75pcColourBars[8] =
{
	0xeb80eb80, 0xa888a82c, 0x912c9193, 0x8534853f,
	0x3fcc3fc1, 0x33d4336d, 0x1c781cd4, 0x10801080
};

static const NSDictionary* kPixelFormats = @{
	[NSNumber numberWithInteger:bmdFormat8BitYUV]	: @"8-bit YUV",
	[NSNumber numberWithInteger:bmdFormat10BitYUV]	: @"10-bit YUV",
	[NSNumber numberWithInteger:bmdFormat8BitARGB]	: @"8-bit RGB",
	[NSNumber numberWithInteger:bmdFormat10BitRGB]	: @"10-bit RGB"
};

constexpr bool PixelFormatIsRGB(BMDPixelFormat pf) { return (pf == bmdFormat8BitARGB) || (pf == bmdFormat10BitRGB); }

@implementation SyncController

@synthesize window;

- (void)addDevice:(IDeckLink*)deckLink
{
	// Create new PlaybackDelegate object to wrap around new IDeckLink instance
	PlaybackDelegate* device = new PlaybackDelegate(self, deckLink);

	// Initialise new PlaybackDelegate object
	if (! device->init())
	{
		NSAlert* alert = [[NSAlert alloc] init];
		alert.messageText = @"Error initialising the new device";
		alert.informativeText = @"This application is unable to initialise the new device\nAre you using a capture-only device (eg UltraStudio Mini Recorder)?";
		[alert runModal];
		[alert release];
		device->Release();
		return;
	}

	// Register profile callback with newly added device's profile manager
	if (device->getDeckLinkProfileManager() != NULL)
		device->getDeckLinkProfileManager()->SetCallback(profileCallback);

	[[deviceListPopup menu] addItemWithTitle:(NSString*)device->getDeviceName() action:nil keyEquivalent:@""];
	[[deviceListPopup lastItem] setTag:(NSInteger)device];
	[[deviceListPopup lastItem] setEnabled:IsDeviceActive(deckLink)];

	if ([deviceListPopup numberOfItems] == 1)
	{
		// We have added our first item, enable the interface
		[deviceListPopup selectItemAtIndex:0];
		[self newDeviceSelected:nil];
		[self enableInterface:YES];
	}
}

- (void)removeDevice:(IDeckLink*)deckLink
{
	PlaybackDelegate* deviceToRemove = NULL;
	PlaybackDelegate* removalCandidate = NULL;
	NSInteger index = 0;

	// Find the DeckLinkDevice that wraps the IDeckLink being removed
	for (NSMenuItem* item in [deviceListPopup itemArray])
	{
		removalCandidate = (PlaybackDelegate*)[item tag];
		
		if (removalCandidate->getDeckLinkDevice() == deckLink)
		{
			deviceToRemove = removalCandidate;
			break;
		}
		++index;
	}

	if (deviceToRemove == NULL)
		return;

	// If playback is ongoing, stop it
	if ( (selectedDevice == deviceToRemove) && (running == YES) )
		[self stopRunning];

	// Release profile callback from device to remove
	if (deviceToRemove->getDeckLinkProfileManager() != NULL)
		deviceToRemove->getDeckLinkProfileManager()->SetCallback(NULL);

	[deviceListPopup removeItemAtIndex:index];

	if ([deviceListPopup numberOfItems] == 0)
	{
		// We have removed the last item, disable the interface
		[startButton setEnabled:NO];
		[self enableInterface:NO];
		selectedDevice = NULL;
	}
	else if (selectedDevice == deviceToRemove)
	{
		// Select the first device in the list and enable the interface
		[deviceListPopup selectItemAtIndex:0];
		[self newDeviceSelected:nil];
	}

	// Release DeckLinkDevice instance
	deviceToRemove->Release();
}

- (void)haltStreams:(IDeckLinkProfile*)newProfile
{
	IDeckLink* deckLink = NULL;

	// Stop playback if running
	if (newProfile->GetDevice(&deckLink) == S_OK)
	{
		if ((selectedDevice->getDeckLinkDevice() == deckLink) && (running == YES))
			[self stopRunning];
		
		deckLink->Release();
	}
}

- (void)updateProfile:(IDeckLinkProfile*)newProfile
{
	IDeckLink*			updatedDeckLink = NULL;
	PlaybackDelegate*	updatedCandidate = NULL;
	
	// Update popups with new profile
	[self refreshDisplayModeMenu];
	[self refreshAudioChannelMenu];
	
	if (newProfile->GetDevice(&updatedDeckLink) == S_OK)
	{
		// Find menu item that corresponds with the device with updated profile
		for (NSMenuItem* item in [deviceListPopup itemArray])
		{
			updatedCandidate = (PlaybackDelegate*)[item tag];
		
			if (updatedCandidate->getDeckLinkDevice() == updatedDeckLink)
			{
				[item setEnabled:IsDeviceActive(updatedDeckLink)];
				break;
			}
		}
		updatedDeckLink->Release();
	}
	
	// A reference was added in IDeckLinkProfileCallback::ProfileActivated callback
	newProfile->Release();
}

- (void) refreshDisplayModeMenu
{
	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	IDeckLinkDisplayModeIterator*		displayModeIterator;
	IDeckLinkDisplayMode*				deckLinkDisplayMode;
	IDeckLinkOutput*					deckLinkOutput;
	int									i;

	for (i = 0; i < [videoFormatPopup numberOfItems]; i++)
	{
		deckLinkDisplayMode = (IDeckLinkDisplayMode*)[[videoFormatPopup itemAtIndex:i] tag];
		if (! deckLinkDisplayMode)
			continue;
		deckLinkDisplayMode->Release();
	}

	[videoFormatPopup removeAllItems];

	deckLinkOutput = selectedDevice->getDeviceOutput();

	if (deckLinkOutput->GetDisplayModeIterator(&displayModeIterator) != S_OK)
		return;

	while (displayModeIterator->Next(&deckLinkDisplayMode) == S_OK)
	{
		CFStringRef				modeName;
		CFStringRef				modeName3D;
		HRESULT					hr;
		bool					supported;

		// Check that display mode is supported with the active profile
		hr = deckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, deckLinkDisplayMode->GetDisplayMode(), bmdFormatUnspecified, bmdNoVideoOutputConversion, bmdSupportedVideoModeDefault, NULL, &supported);
		if (hr != S_OK || ! supported)
			continue;

		if (deckLinkDisplayMode->GetName(&modeName) != S_OK)
		{
			deckLinkDisplayMode->Release();
			deckLinkDisplayMode = NULL;
			continue;
		}

		// Add this item to the video format poup menu
		[videoFormatPopup addItemWithTitle:(NSString*)modeName];

		// Save the IDeckLinkDisplayMode in the menu item's tag
		[[videoFormatPopup lastItem] setTag:(NSInteger)deckLinkDisplayMode];

		if ([videoFormatPopup numberOfItems] == 1)
		{
			// We have added our first item, refresh pixel formats
			[videoFormatPopup selectItemAtIndex:0];
			[self newDisplayModeSelected:nil];
		}
		
		// Check Dual Stream 3D support with any pixel format
		hr = deckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, deckLinkDisplayMode->GetDisplayMode(), bmdFormatUnspecified, bmdNoVideoOutputConversion, bmdSupportedVideoModeDualStream3D, NULL, &supported);
		if (hr != S_OK || ! supported)
		{
			CFRelease(modeName);
			continue;
		}

		modeName3D = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ 3D"), modeName);
		[videoFormatPopup addItemWithTitle:(NSString*)modeName3D];

		// Save the IDeckLinkDisplayMode in the menu item's tag
		deckLinkDisplayMode->AddRef();
		[[videoFormatPopup lastItem] setTag:(NSInteger)deckLinkDisplayMode];

		CFRelease(modeName3D);
		CFRelease(modeName);
	}

	displayModeIterator->Release();
	displayModeIterator = NULL;

	[startButton setEnabled:([videoFormatPopup numberOfItems] != 0)];
}

- (void)refreshPixelFormatMenu
{
	// Populate the pixel format menu with a list of pixel formats supported with selected display mode
	IDeckLinkOutput*					deckLinkOutput;
	
	[pixelFormatPopup removeAllItems];
	
	deckLinkOutput = selectedDevice->getDeviceOutput();
	
	for (NSNumber* key in kPixelFormats)
	{
		BMDPixelFormat				pixelFormat;
		BMDSupportedVideoModeFlags	videoModeFlags = bmdSupportedVideoModeDefault;
		bool						supported;
		HRESULT						hr;
		
		pixelFormat = (BMDPixelFormat)[key intValue];
		
		if ((selectedVideoOutputFlags & bmdVideoOutputDualStream3D) != 0)
			videoModeFlags = bmdSupportedVideoModeDualStream3D;

		hr = deckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, selectedDisplayMode->GetDisplayMode(), pixelFormat, bmdNoVideoOutputConversion, videoModeFlags, NULL, &supported);
		if (hr != S_OK || ! supported)
			continue;

		[pixelFormatPopup addItemWithTitle:kPixelFormats[key]];
		[[pixelFormatPopup lastItem] setTag:(NSInteger)pixelFormat];
	}
}

- (void)refreshAudioChannelMenu
{
	IDeckLink*				deckLink;
	IDeckLinkProfileAttributes*	deckLinkAttributes;
	int64_t					maxAudioChannels;
	NSMenuItem*				audioChannelPopupItem;
	int						audioChannelSelected;
	int						currentAudioChannel;

	audioChannelSelected = [[audioChannelPopup selectedItem] tag];

	deckLink = selectedDevice->getDeckLinkDevice();

	// Get DeckLink attributes to determine number of audio channels
	if (deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**) &deckLinkAttributes) != S_OK)
		goto bail;
		
	// Get max number of audio channels supported by DeckLink device
	if (deckLinkAttributes->GetInt(BMDDeckLinkMaximumAudioChannels, &maxAudioChannels) != S_OK)
		goto bail;
	
	// Scan through Audio channel popup menu and disable invalid entries
	for (int i = 0; i < [audioChannelPopup numberOfItems]; i++)
	{
		audioChannelPopupItem = [audioChannelPopup itemAtIndex:i];
		currentAudioChannel = [audioChannelPopupItem tag];
		
		if ( maxAudioChannels >= (int64_t) currentAudioChannel )
		{
			[audioChannelPopupItem setHidden:NO];
			if ( audioChannelSelected >= currentAudioChannel )
				[audioChannelPopup selectItemAtIndex:i];
		}
		else
			[audioChannelPopupItem setHidden:YES];
	}

bail:
	if (deckLinkAttributes)
	{
		deckLinkAttributes->Release();
		deckLinkAttributes = NULL;
	}
}

- (IBAction)newDeviceSelected:(id)sender
{
	IDeckLinkOutput*				deckLinkOutput;
	IDeckLinkProfileAttributes*		deckLinkAttributes;

	// Get the DeckLinkDevice object for the selected menu item.
	selectedDevice = (PlaybackDelegate*)[[deviceListPopup selectedItem] tag];
	
	// Update the display mode popup menu
	[self refreshDisplayModeMenu];

	// Set Screen Preview callback for selected device
	deckLinkOutput = selectedDevice->getDeviceOutput();
	deckLinkOutput->SetScreenPreviewCallback(CreateCocoaScreenPreview(previewView));
	
	// Check whether HFRTC is supported by the selected device
	if ((deckLinkOutput->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&deckLinkAttributes) != S_OK) ||
		(deckLinkAttributes->GetFlag(BMDDeckLinkSupportsHighFrameRateTimecode, &hfrtcSupported) != S_OK))
	{
		hfrtcSupported = false;
	}
	
	// Update available audio channels
	[self refreshAudioChannelMenu];

	// Enable the interface
	[self enableInterface:YES];
}

- (IBAction)newDisplayModeSelected:(id)sender
{
	NSMenuItem*				videoFormatItem;
	BMDDisplayMode			bmdDisplayMode;
	
	selectedVideoOutputFlags = bmdVideoOutputFlagDefault;
	
	// If the mode title contains "3D" then enable the video in 3D mode
	videoFormatItem = [videoFormatPopup selectedItem];
	
	if ([[videoFormatItem title] hasSuffix:(NSString*)CFSTR("3D")])
		selectedVideoOutputFlags |= bmdVideoOutputDualStream3D;

	selectedDisplayMode = (IDeckLinkDisplayMode*)[videoFormatItem tag];

	bmdDisplayMode = selectedDisplayMode->GetDisplayMode();
	
	if (bmdDisplayMode == bmdModeNTSC ||
		bmdDisplayMode == bmdModeNTSC2398 ||
		bmdDisplayMode == bmdModePAL)
	{
		timeCodeFormat = bmdTimecodeVITC;
		selectedVideoOutputFlags |= bmdVideoOutputVITC;
	}
	else
	{
		timeCodeFormat = bmdTimecodeRP188Any;
		selectedVideoOutputFlags |= bmdVideoOutputRP188;
	}
	
	[self refreshPixelFormatMenu];
}


- (SignalGenerator3DVideoFrame*) CreateBlackFrame
{
	IDeckLinkOutput*				deckLinkOutput;
	IDeckLinkMutableVideoFrame*		referenceBlack = NULL;
	IDeckLinkMutableVideoFrame*		scheduleBlack = NULL;
	HRESULT							hr;
	BMDPixelFormat					pixelFormat;
	int								bytesPerRow;
	IDeckLinkVideoConversion*		frameConverter = NULL;
	SignalGenerator3DVideoFrame*	ret = NULL;

	pixelFormat = [[pixelFormatPopup selectedItem] tag];
	bytesPerRow = GetRowBytes(pixelFormat, frameWidth);

	deckLinkOutput = selectedDevice->getDeviceOutput();
	
	hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, bytesPerRow, pixelFormat, bmdFrameFlagDefault, &scheduleBlack);
	if (hr != S_OK)
		goto bail;

	// 8-bit YUV pixels can be filled directly without conversion
	if (pixelFormat == bmdFormat8BitYUV)
	{
		FillBlack(scheduleBlack);
	}
	else
	{
		// If the pixel formats are different create and fill an 8 bit YUV reference frame
		hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &referenceBlack);
		if (hr != S_OK)
			goto bail;
		FillBlack(referenceBlack);

		frameConverter = CreateVideoConversionInstance();

		hr = frameConverter->ConvertFrame(referenceBlack, scheduleBlack);
		if (hr != S_OK)
			goto bail;
	}

	ret = new SignalGenerator3DVideoFrame(scheduleBlack);

bail:
	if (referenceBlack)
	{
		referenceBlack->Release();
		referenceBlack = NULL;
	}
	if (scheduleBlack)
	{
		scheduleBlack->Release();
		scheduleBlack = NULL;
	}
	if (frameConverter)
	{
		frameConverter->Release();
		frameConverter = NULL;
	}

	return ret;
}

- (SignalGenerator3DVideoFrame*) CreateBarsFrame
{
	IDeckLinkOutput*				deckLinkOutput;
	IDeckLinkMutableVideoFrame*		referenceBarsLeft = NULL;
	IDeckLinkMutableVideoFrame*		referenceBarsRight = NULL;
	IDeckLinkMutableVideoFrame*		scheduleBarsLeft = NULL;
	IDeckLinkMutableVideoFrame*		scheduleBarsRight = NULL;
	HRESULT							hr;
	BMDPixelFormat					pixelFormat;
	int								bytesPerRow;
	IDeckLinkVideoConversion*		frameConverter = NULL;
	SignalGenerator3DVideoFrame*	ret = NULL;

	pixelFormat = [[pixelFormatPopup selectedItem] tag];
	bytesPerRow = GetRowBytes(pixelFormat, frameWidth);

	deckLinkOutput = selectedDevice->getDeviceOutput();

	frameConverter = CreateVideoConversionInstance();
	if (frameConverter == NULL)
		goto bail;
	
	// Request a left and right frame from the device
	hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, bytesPerRow, pixelFormat, bmdFrameFlagDefault, &scheduleBarsLeft);
	if (hr != S_OK)
		goto bail;

	// 8-bit YUV pixels can be filled directly without conversion
	if (pixelFormat == bmdFormat8BitYUV)
	{
		FillColourBars(scheduleBarsLeft, false);
	}
	else
	{
		// If the pixel formats are different create and fill an 8 bit YUV reference frame
		hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &referenceBarsLeft);
		if (hr != S_OK)
			goto bail;
		FillColourBars(referenceBarsLeft, false);
		
		hr = frameConverter->ConvertFrame(referenceBarsLeft, scheduleBarsLeft);
		if (hr != S_OK)
			goto bail;
	}
	
	if (selectedVideoOutputFlags == bmdVideoOutputDualStream3D)
	{
		// If 3D mode requested, fill right eye with reversed colour bars
		hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, bytesPerRow, pixelFormat, bmdFrameFlagDefault, &scheduleBarsRight);
		if (hr != S_OK)
			goto bail;

		// 8-bit YUV pixels can be filled directly without conversion
		if (pixelFormat == bmdFormat8BitYUV)
		{
			FillColourBars(scheduleBarsRight, true);
		}
		else
		{
			// If the pixel formats are different create and fill an 8 bit YUV reference frame
			hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, frameWidth*2, bmdFormat8BitYUV, bmdFrameFlagDefault, &referenceBarsRight);
			if (hr != S_OK)
				goto bail;

			FillColourBars(referenceBarsRight, true);

			hr = frameConverter->ConvertFrame(referenceBarsRight, scheduleBarsRight);
			if (hr != S_OK)
				goto bail;
		}
	}
	ret = new SignalGenerator3DVideoFrame(scheduleBarsLeft, scheduleBarsRight);

bail:
	if (referenceBarsLeft)
	{
		referenceBarsLeft->Release();
		referenceBarsLeft = NULL;
	}
	if (referenceBarsRight)
	{
		referenceBarsRight->Release();
		referenceBarsRight = NULL;
	}
	if (scheduleBarsLeft)
	{
		scheduleBarsLeft->Release();
		scheduleBarsLeft = NULL;
	}
	if (scheduleBarsRight)
	{
		scheduleBarsRight->Release();
		scheduleBarsRight = NULL;
	}
	if (frameConverter)
	{
		frameConverter->Release();
		frameConverter = NULL;
	}

	return ret;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
	//
	// Setup UI

	// Empty popup menus
	[deviceListPopup removeAllItems];
	[videoFormatPopup removeAllItems];

	// Set device list popup for manual enabling, so we can disable inactive devices
	[deviceListPopup setAutoenablesItems:NO];
	
	// Disable the interface
	[startButton setEnabled:NO];
	[self enableInterface:NO];

	//
	// Create and initialise DeckLink device discovery and profile callback objects
	deckLinkDiscovery = new DeckLinkDeviceDiscovery(self);
	profileCallback = new ProfileCallback(self);
	if ((deckLinkDiscovery != NULL) && (profileCallback != NULL))
	{
		deckLinkDiscovery->enable();
	}
	else
	{
		NSAlert* alert = [[NSAlert alloc] init];
		alert.messageText = @"This application requires the Desktop Video drivers installed.";
		alert.informativeText = @"Please install the Blackmagic Desktop Video drivers to use the features of this application.";
		[alert runModal];
		[alert release];
	}

}

- (void)enableInterface:(BOOL)enable
{
	// Set the enable state of user interface elements
	[deviceListPopup setEnabled:enable];
	[outputSignalPopup setEnabled:enable];
	[audioChannelPopup setEnabled:enable];
	[audioSampleDepthPopup setEnabled:enable];
	[videoFormatPopup setEnabled:enable];
	[pixelFormatPopup setEnabled:enable];
}

- (IBAction)toggleStart:(id)sender
{
	if (running == NO)
		[self startRunning];
	else
		[self stopRunning];
}

- (void)startRunning
{
	IDeckLinkOutput*		deckLinkOutput;
	IDeckLinkConfiguration*	deckLinkConfiguration;
	bool					pixelFormatRGB;
	HRESULT					result;

	// Determine the audio and video properties for the output stream
	outputSignal = (OutputSignal)[outputSignalPopup indexOfSelectedItem];
	audioChannelCount = [[audioChannelPopup selectedItem] tag];
	audioSampleDepth = [[audioSampleDepthPopup selectedItem] tag];
	audioSampleRate = bmdAudioSampleRate48kHz;

	frameWidth = selectedDisplayMode->GetWidth();
	frameHeight = selectedDisplayMode->GetHeight();

	selectedDisplayMode->GetFrameRate(&frameDuration, &frameTimescale);
	// Calculate the number of frames per second, rounded up to the nearest integer.  For example, for NTSC (29.97 FPS), framesPerSecond == 30.
	framesPerSecond = (frameTimescale + (frameDuration-1))  /  frameDuration;

	// m-rate frame rates with multiple 30-frame counting should implement Drop Frames compensation, refer to SMPTE 12-1
	if (frameDuration == 1001 && frameTimescale % 30000 == 0)
		dropFrames = 2 * (frameTimescale / 30000);
	else
		dropFrames = 0;
	
	timeCode = std::make_unique<Timecode>(framesPerSecond, dropFrames);
	
	// Set the SDI output to 444 if RGB mode is selected.
	pixelFormatRGB = PixelFormatIsRGB([[pixelFormatPopup selectedItem] tag]);
	deckLinkConfiguration = selectedDevice->getDeviceConfiguration();
	result = deckLinkConfiguration->SetFlag(bmdDeckLinkConfig444SDIVideoOutput, pixelFormatRGB);
	// If a device without SDI output is used (eg Intensity Pro 4K), then SetFlags will return E_NOTIMPL
	if ((result != S_OK) && (result != E_NOTIMPL))
		goto bail;
	
	// Set the video output mode
	deckLinkOutput = selectedDevice->getDeviceOutput();
	if (deckLinkOutput->EnableVideoOutput(selectedDisplayMode->GetDisplayMode(), selectedVideoOutputFlags) != S_OK)
		goto bail;
	
	// Set the audio output mode
	if (deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, audioSampleDepth, audioChannelCount, bmdAudioOutputStreamTimestamped) != S_OK)
		goto bail;

	// Generate one second of audio tone
	audioSamplesPerFrame = ((audioSampleRate * frameDuration) / frameTimescale);
	audioBufferSampleLength = (framesPerSecond * audioSampleRate * frameDuration) / frameTimescale;
	audioBuffer = malloc(audioBufferSampleLength * audioChannelCount * (audioSampleDepth / 8));
	if (audioBuffer == NULL)
		goto bail;
	FillSine(audioBuffer, audioBufferSampleLength, audioChannelCount, audioSampleDepth);

	videoFrameBlack = [self CreateBlackFrame];
	if (! videoFrameBlack)
		goto bail;

	videoFrameBars = [self CreateBarsFrame];
	if (! videoFrameBars)
		goto bail;

	// Begin video preroll by scheduling a second of frames in hardware
	totalFramesScheduled = 0;
	for (int i = 0; i < framesPerSecond; i++)
		[self scheduleNextFrame:YES];
	
	// Begin audio preroll.  This will begin calling our audio callback, which will start the DeckLink output stream.
	totalAudioSecondsScheduled = 0;
	if (deckLinkOutput->BeginAudioPreroll() != S_OK)
		goto bail;

	// Success; update the UI
	running = YES;
	playbackStopped = NO;
	[startButton setTitle:@"Stop"];
	// Disable the user interface while running (prevent the user from making changes to the output signal)
	[self enableInterface:NO];
	
	return;
	
bail:
	// *** Error-handling code.  Cleanup any resources that were allocated. *** //
	[self stopRunning];
}

- (void)stopRunning
{
	IDeckLinkOutput* deckLinkOutput = selectedDevice->getDeviceOutput();

	// Stop the audio and video output streams immediately
	deckLinkOutput->StopScheduledPlayback(0, NULL, 0);
	
	// Wait for scheduled playback to stop
	[stoppedCondition lock];
	while (!playbackStopped)
		[stoppedCondition wait];
	[stoppedCondition unlock];
	//
	deckLinkOutput->DisableAudioOutput();
	deckLinkOutput->DisableVideoOutput();

	if (videoFrameBlack != NULL)
		videoFrameBlack->Release();
	videoFrameBlack = NULL;

	if (videoFrameBars != NULL)
		videoFrameBars->Release();
	videoFrameBars = NULL;

	if (audioBuffer != NULL)
		free(audioBuffer);
	audioBuffer = NULL;

	// Success; update the UI
	running = NO;
	[startButton setTitle:@"Start"];
	// Re-enable the user interface when stopped
	[self enableInterface:YES];
}

- (void)scheduledPlaybackStopped
{
	[stoppedCondition lock];
	playbackStopped = YES;
	[stoppedCondition signal];
	[stoppedCondition unlock];
}

- (void)scheduleNextFrame:(BOOL)prerolling
{
	HRESULT							result = S_OK;
	IDeckLinkOutput*				deckLinkOutput = selectedDevice->getDeviceOutput();
	SignalGenerator3DVideoFrame*	currentFrame;
	bool							setVITC1Timecode = false;
	bool							setVITC2Timecode = false;

	if (prerolling == NO)
	{
		// If not prerolling, make sure that playback is still active
		if (running == NO)
			return;
	}
	
	if (outputSignal == kOutputSignalPip)
	{
		if ((totalFramesScheduled % framesPerSecond) == 0)
		{
			// On each second, schedule a frame of bars
			currentFrame = videoFrameBars;
		}
		else
		{
			// Schedue frames of black
			currentFrame = videoFrameBlack;
		}
	}
	else
	{
		if ((totalFramesScheduled % framesPerSecond) == 0)
		{
			// On each second, schedule a frame of black
			currentFrame = videoFrameBlack;
		}
		else
		{
			// Schedue frames of color bars
			currentFrame = videoFrameBars;
		}
	}
		
	if (timeCodeFormat == bmdTimecodeVITC)
	{
		result = currentFrame->SetTimecodeFromComponents(bmdTimecodeVITC,
														 timeCode->hours(),
														 timeCode->minutes(),
														 timeCode->seconds(),
														 timeCode->frames(),
														 bmdTimecodeFlagDefault);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not set VITC timecode on frame - result = %08x\n", result);
			goto bail;
		}
	}
	else
	{
		int frames = timeCode->frames();
		
		if (hfrtcSupported)
		{
			result = currentFrame->SetTimecodeFromComponents(bmdTimecodeRP188HighFrameRate,
															 timeCode->hours(),
															 timeCode->minutes(),
															 timeCode->seconds(),
															 frames,
															 bmdTimecodeFlagDefault);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not set HFRTC timecode on frame - result = %08x\n", result);
				goto bail;
			}
		}
		
		if (selectedDisplayMode->GetFieldDominance() != bmdProgressiveFrame)
		{
			// An interlaced or PsF frame has both VITC1 and VITC2 set with the same timecode value (SMPTE ST 12-2:2014 7.2)
			setVITC1Timecode = true;
			setVITC2Timecode = true;
		}
		else if (framesPerSecond <= 30)
		{
			// If this isn't a High-P mode, then just use VITC1 (SMPTE ST 12-2:2014 7.2)
			setVITC1Timecode = true;
		}
		else if (framesPerSecond <= 60)
		{
			// If this is a High-P mode then use VITC1 on even frames and VITC2 on odd frames. This is done because the
			// frames field of the RP188 VITC timecode cannot hold values greater than 30 (SMPTE ST 12-2:2014 7.2, 9.2)
			if ((frames & 1) == 0)
				setVITC1Timecode = true;
			else
				setVITC2Timecode = true;
			
			frames >>= 1;
		}
		
		if (setVITC1Timecode)
		{
			result = currentFrame->SetTimecodeFromComponents(bmdTimecodeRP188VITC1,
															 timeCode->hours(),
															 timeCode->minutes(),
															 timeCode->seconds(),
															 frames,
															 bmdTimecodeFlagDefault);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not set VITC1 timecode on interlaced frame - result = %08x\n", result);
				goto bail;
			}
		}
		
		if (setVITC2Timecode)
		{
			// The VITC2 timecode also has the field mark flag set
			result = currentFrame->SetTimecodeFromComponents(bmdTimecodeRP188VITC2,
															 timeCode->hours(),
															 timeCode->minutes(),
															 timeCode->seconds(),
															 frames,
															 bmdTimecodeFieldMark);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not set VITC1 timecode on interlaced frame - result = %08x\n", result);
				goto bail;
			}
		}
	}

	deckLinkOutput->ScheduleVideoFrame(currentFrame, (totalFramesScheduled * frameDuration), frameDuration, frameTimescale);

bail:
	totalFramesScheduled += 1;
	timeCode->update();
}

- (void)writeNextAudioSamples
{
	IDeckLinkOutput* deckLinkOutput = selectedDevice->getDeviceOutput();

	// Write one second of audio to the DeckLink API.
	if (outputSignal == kOutputSignalPip)
	{
		// Schedule one-frame of audio tone
		if (deckLinkOutput->ScheduleAudioSamples(audioBuffer, audioSamplesPerFrame, (totalAudioSecondsScheduled * audioBufferSampleLength), audioSampleRate, NULL) != S_OK)
			return;
	}
	else
	{
		// Schedule one-second (minus one frame) of audio tone
		if (deckLinkOutput->ScheduleAudioSamples(audioBuffer, (audioBufferSampleLength - audioSamplesPerFrame), (totalAudioSecondsScheduled * audioBufferSampleLength) + audioSamplesPerFrame, audioSampleRate, NULL) != S_OK)
			return;
	}
	
	totalAudioSecondsScheduled += 1;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	// Stop the output signal
	if (running == YES)
		[self stopRunning];

	// Disable DeckLink device discovery
	deckLinkDiscovery->disable();

	// Disable profile callback
	if (selectedDevice != NULL)
	{
		IDeckLinkProfileManager* profileManager = selectedDevice->getDeckLinkProfileManager();
		if (profileManager != NULL)
			profileManager->SetCallback(NULL);
	}
	
	// Release all DeckLinkDevice instances
	while([deviceListPopup numberOfItems] > 0)
	{
		PlaybackDelegate* device = (PlaybackDelegate*)[[deviceListPopup itemAtIndex:0] tag];
		if (device != NULL)
		{
			// Release profile callback from device
			if (device->getDeckLinkProfileManager() != NULL)
				device->getDeckLinkProfileManager()->SetCallback(NULL);

			device->Release();
		}
		[deviceListPopup removeItemAtIndex:0];
	}

	// Release all DisplayMode instances
	while([videoFormatPopup numberOfItems] > 0)
	{
		IDeckLinkDisplayMode* displayMode = (IDeckLinkDisplayMode*)[[videoFormatPopup itemAtIndex:0] tag];
		displayMode->Release();
		[videoFormatPopup removeItemAtIndex:0];
	}
	
	// Release DeckLink discovery instance
	if (deckLinkDiscovery != NULL)
	{
		deckLinkDiscovery->Release();
		deckLinkDiscovery = NULL;
	}
	
	// Release Profile callback instance
	if (profileCallback != NULL)
	{
		profileCallback->Release();
		profileCallback = NULL;
	}
}

@end


/*****************************************/

PlaybackDelegate::PlaybackDelegate (SyncController* owner, IDeckLink* deckLink)
	: m_controller(owner), m_deckLink(deckLink)
{
}

PlaybackDelegate::~PlaybackDelegate()
{
	if (m_deckLinkProfileManager)
	{
		m_deckLinkProfileManager->Release();
		m_deckLinkProfileManager = NULL;
	}
	
	if (m_deckLinkOutput)
	{
		m_deckLinkOutput->Release();
		m_deckLinkOutput = NULL;
	}
	
	if (m_deckLinkConfiguration)
	{
		m_deckLinkConfiguration->Release();
		m_deckLinkConfiguration = NULL;
	}
	
	if (m_deckLink)
	{
		m_deckLink->Release();
		m_deckLink = NULL;
	}
	
	if (m_deviceName)
		CFRelease(m_deviceName);
}

bool PlaybackDelegate::init()
{
	// Get output interface
	if (m_deckLink->QueryInterface(IID_IDeckLinkOutput, (void**) &m_deckLinkOutput) != S_OK)
		return false;
	
	// Get configuration interface
	if (m_deckLink->QueryInterface(IID_IDeckLinkConfiguration, (void**) &m_deckLinkConfiguration) != S_OK)
		return false;

	// Get device name
	if (m_deckLink->GetDisplayName(&m_deviceName) != S_OK)
		m_deviceName = CFStringCreateCopy(NULL, CFSTR("DeckLink"));

	// Provide the delegate to the audio and video output interfaces
	m_deckLinkOutput->SetScheduledFrameCompletionCallback(this);
	m_deckLinkOutput->SetAudioCallback(this);

	// Get the profile manager interface
	// Will return S_OK when the device has > 1 profiles
	if (m_deckLink->QueryInterface(IID_IDeckLinkProfileManager, (void**) &m_deckLinkProfileManager) != S_OK)
	{
		m_deckLinkProfileManager = NULL;
	}
	return true;
}

HRESULT	PlaybackDelegate::QueryInterface(REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes		iunknown;
	HRESULT			result = E_NOINTERFACE;

	// Initialise the return result
	*ppv = NULL;

	// Obtain the IUnknown interface and compare it the provided REFIID
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (memcmp(&iid, &IID_IDeckLinkNotificationCallback, sizeof(REFIID)) == 0)
	{
		*ppv = (IDeckLinkNotificationCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG	PlaybackDelegate::AddRef(void)
{
	return ++m_refCount;
}

ULONG	PlaybackDelegate::Release(void)
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;
	return newRefValue;
}

HRESULT		PlaybackDelegate::ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	// When a video frame has been completed, schedule next frame to maintain preroll buffer size
	[m_controller scheduleNextFrame:NO];
	return S_OK;
}

HRESULT		PlaybackDelegate::ScheduledPlaybackHasStopped ()
{
	// Notify owner that playback has stopped, so it can disable output
	[m_controller scheduledPlaybackStopped];
	return S_OK;
}

HRESULT		PlaybackDelegate::RenderAudioSamples (bool preroll)
{
	// Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
	[m_controller writeNextAudioSamples];
	
	if (preroll)
	{
		// Start audio and video output
		m_deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
	}
	
	return S_OK;
}

/*****************************************/

ProfileCallback::ProfileCallback(SyncController* owner)
: m_controller(owner), m_refCount(1)
{
}

HRESULT		ProfileCallback::ProfileChanging (IDeckLinkProfile *profileToBeActivated, bool streamsWillBeForcedToStop)
{
	// When streamsWillBeForcedToStop is true, the profile to be activated is incompatible with the current
	// profile and playback will be stopped by the DeckLink driver. It is better to notify the
	// controller to gracefully stop playback, so that the UI is set to a known state.
	if (streamsWillBeForcedToStop)
		[m_controller haltStreams:profileToBeActivated];
	return S_OK;
}

HRESULT		ProfileCallback::ProfileActivated (IDeckLinkProfile *activatedProfile)
{
	// New profile activated, inform owner to update popup menus
	// Ensure that reference is added to new profile before handing to main thread
	activatedProfile->AddRef();
	dispatch_async(dispatch_get_main_queue(), ^{
		[m_controller updateProfile:activatedProfile];
	});
	
	return S_OK;
}

HRESULT		ProfileCallback::QueryInterface (REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes		iunknown;
	HRESULT			result = E_NOINTERFACE;
	
	// Initialise the return result
	*ppv = NULL;
	
	// Obtain the IUnknown interface and compare it the provided REFIID
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	
	return result;
}

ULONG		ProfileCallback::AddRef (void)
{
	return ++m_refCount;
}

ULONG		ProfileCallback::Release (void)
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;
	
	return newRefValue;
}

/*****************************************/

DeckLinkDeviceDiscovery::DeckLinkDeviceDiscovery(SyncController* delegate)
: m_uiDelegate(delegate), m_deckLinkDiscovery(NULL), m_refCount(1)
{
	m_deckLinkDiscovery = CreateDeckLinkDiscoveryInstance();
}


DeckLinkDeviceDiscovery::~DeckLinkDeviceDiscovery()
{
	if (m_deckLinkDiscovery != NULL)
	{
		// Uninstall device arrival notifications and release discovery object
		m_deckLinkDiscovery->UninstallDeviceNotifications();
		m_deckLinkDiscovery->Release();
		m_deckLinkDiscovery = NULL;
	}
}

bool		DeckLinkDeviceDiscovery::enable()
{
	HRESULT		result = E_FAIL;

	// Install device arrival notifications
	if (m_deckLinkDiscovery != NULL)
		result = m_deckLinkDiscovery->InstallDeviceNotifications(this);

	return result == S_OK;
}

void		DeckLinkDeviceDiscovery::disable()
{
	// Uninstall device arrival notifications
	if (m_deckLinkDiscovery != NULL)
		m_deckLinkDiscovery->UninstallDeviceNotifications();
}

HRESULT		DeckLinkDeviceDiscovery::DeckLinkDeviceArrived (/* in */ IDeckLink* deckLink)
{
	// Update UI (add new device to menu) from main thread
	// AddRef the IDeckLink instance before handing it off to the main thread
	deckLink->AddRef();
	dispatch_async(dispatch_get_main_queue(), ^{
		[m_uiDelegate addDevice:deckLink];
	});

	return S_OK;
}

HRESULT		DeckLinkDeviceDiscovery::DeckLinkDeviceRemoved (/* in */ IDeckLink* deckLink)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		[m_uiDelegate removeDevice:deckLink];
	});
	return S_OK;
}

HRESULT		DeckLinkDeviceDiscovery::QueryInterface (REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes		iunknown;
	HRESULT			result = E_NOINTERFACE;

	// Initialise the return result
	*ppv = NULL;

	// Obtain the IUnknown interface and compare it the provided REFIID
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (memcmp(&iid, &IID_IDeckLinkDeviceNotificationCallback, sizeof(REFIID)) == 0)
	{
		*ppv = (IDeckLinkDeviceNotificationCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG		DeckLinkDeviceDiscovery::AddRef (void)
{
	return ++m_refCount;
}

ULONG		DeckLinkDeviceDiscovery::Release (void)
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

/*****************************************/


void	FillSine (void* audioBuffer, uint32_t samplesToWrite, uint32_t channels, uint32_t sampleDepth)
{
	if (sampleDepth == 16)
	{
		int16_t*		nextBuffer;
		
		nextBuffer = (int16_t*)audioBuffer;
		for (int32_t i = 0; i < samplesToWrite; i++)
		{
			int16_t		sample;
			
			sample = (int16_t)(24576.0 * sin((i * 2.0 * M_PI) / 48.0));
			for (int32_t ch = 0; ch < channels; ch++)
				*(nextBuffer++) = sample;
		}
	}
	else if (sampleDepth == 32)
	{
		int32_t*		nextBuffer;
		
		nextBuffer = (int32_t*)audioBuffer;
		for (int32_t i = 0; i < samplesToWrite; i++)
		{
			int32_t		sample;
			
			sample = (int32_t)(1610612736.0 * sin((i * 2.0 * M_PI) / 48.0));
			for (int32_t ch = 0; ch < channels; ch++)
				*(nextBuffer++) = sample;
		}
	}
}

void	FillColourBars (IDeckLinkVideoFrame* theFrame, bool reversed)
{
	uint32_t*		nextWord;
	uint32_t		width;
	uint32_t		height;
	uint32_t*		bars;
	uint8_t			numBars;

	theFrame->GetBytes((void**)&nextWord);
	width = theFrame->GetWidth();
	height = theFrame->GetHeight();
	
	if (width > 720)
	{
		bars = gHD75pcColourBars;
		numBars = ARRAY_SIZE(gHD75pcColourBars);
	}
	else
	{
		bars = gSD75pcColourBars;
		numBars = ARRAY_SIZE(gSD75pcColourBars);
	}

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x+=2)
		{
			int pos = x * numBars / width;
			
			if (reversed)
				pos = numBars - pos - 1;

			*(nextWord++) = bars[pos];
		}
	}
}

void	FillBlack (IDeckLinkVideoFrame* theFrame)
{
	uint32_t*		nextWord;
	uint32_t		width;
	uint32_t		height;
	uint32_t		wordsRemaining;
	
	theFrame->GetBytes((void**)&nextWord);
	width = theFrame->GetWidth();
	height = theFrame->GetHeight();
	
	wordsRemaining = (width*2 * height) / 4;
	
	while (wordsRemaining-- > 0)
		*(nextWord++) = 0x10801080;
}

int		GetRowBytes (BMDPixelFormat pixelFormat, int frameWidth)
{
	int bytesPerRow;
	
	// Refer to DeckLink SDK Manual - 2.7.4 Pixel Formats
	switch (pixelFormat)
	{
		case bmdFormat8BitYUV:
			bytesPerRow = frameWidth * 2;
			break;
			
		case bmdFormat10BitYUV:
			bytesPerRow = ((frameWidth + 47) / 48) * 128;
			break;
			
		case bmdFormat10BitRGB:
			bytesPerRow = ((frameWidth + 63) / 64) * 256;
			break;
			
		case bmdFormat8BitARGB:
		case bmdFormat8BitBGRA:
		default:
			bytesPerRow = frameWidth * 4;
			break;
	}
	
	return bytesPerRow;
}

bool	IsDeviceActive(IDeckLink* deckLink)
{
	IDeckLinkProfileAttributes*		deckLinkAttributes = NULL;
	int64_t							intAttribute = bmdDuplexInactive;
	
	if (deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**) &deckLinkAttributes) != S_OK)
		return false;
	
	deckLinkAttributes->GetInt(BMDDeckLinkDuplex, &intAttribute);
	deckLinkAttributes->Release();
	
	return ((BMDDuplexMode) intAttribute) != bmdDuplexInactive;
}
