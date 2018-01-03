/* -LICENSE-START-
** Copyright (c) 2009 Blackmagic Design
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
#import "MyController.h"
#include <libkern/OSAtomic.h>

@implementation MyController

- (id)init
{
	if (self = [super init])
	{
		delegate = new VideoDelegate(self);
	}
	
	return self;
}

- (void)dealloc
{
	if (delegate != NULL)
		delegate->Release();
	
	[super dealloc];
}

- (void)awakeFromNib
{
	IDeckLinkIterator*		iterator;
	int						i;

	[startButton setEnabled:NO];
	
	iterator = CreateDeckLinkIteratorInstance();
	if (iterator != NULL)
	{
		for (i = 0; i < MAX_DECKLINK; i++)
		{
			if (iterator->Next(&deckLink[i]) != S_OK)
				break;

			if (i == 0)
			{
				[inputCardMenu removeAllItems];
				[outputCardMenu removeAllItems];
				[startButton setEnabled:YES];
			}
			
			// Add this deckLink instance to the popup menus
			CFStringRef			cardName;
			
			if (deckLink[i]->GetModelName(&cardName) != S_OK)
				cardName = CFSTR("Unknown DeckLink");
			
			[[inputCardMenu menu] addItemWithTitle:(NSString*)cardName action:nil keyEquivalent:@""];
			[[outputCardMenu menu] addItemWithTitle:(NSString*)cardName action:nil keyEquivalent:@""];
			
			CFRelease(cardName);
		}
		
		iterator->Release();
	}
	
	// Hide the timecode
	[captureTime setHidden:YES];
	[captureTimeHeader setHidden:YES];
	
	running = NO;
}

- (void)windowWillClose:(NSNotification*)notification
{
	[[NSApplication sharedApplication] terminate:self];
}


- (IBAction)toggleStart:(id)sender
{
	HRESULT				theResult;
	
	if (running == NO)
	{
		// Obtain the input and output interfaces
		if (deckLink[[inputCardMenu indexOfSelectedItem]]->QueryInterface(IID_IDeckLinkInput, (void**)&inputCard) != S_OK)
			goto bail;
		if (deckLink[[outputCardMenu indexOfSelectedItem]]->QueryInterface(IID_IDeckLinkOutput, (void**)&outputCard) != S_OK)
		{
			inputCard->Release();
			goto bail;
		}
		
		// Turn on video input
		theResult = inputCard->SetCallback(delegate);
		if (theResult != S_OK)
			printf("SetCallback failed with result %08x\n", (unsigned int)theResult);
		//
		theResult = inputCard->EnableVideoInput((BMDDisplayMode)[[videoFormatMenu selectedItem] tag], bmdFormat8BitYUV, 0);
		if (theResult != S_OK)
			printf("EnableVideoInput failed with result %08x\n", (unsigned int)theResult);
		
		// Turn on video output
		theResult = outputCard->EnableVideoOutput([[videoFormatMenu selectedItem] tag], bmdVideoOutputFlagDefault);
		if (theResult != S_OK)
			printf("EnableVideoOutput failed with result %08x\n", (unsigned int)theResult);
		//
		theResult = outputCard->StartScheduledPlayback(0, 600, 1.0);
		if (theResult != S_OK)
			printf("StartScheduledPlayback failed with result %08x\n", (unsigned int)theResult);
		
		// Sart the input stream running
		theResult = inputCard->StartStreams();
		if (theResult != S_OK)
			printf("Input StartStreams failed with result %08x\n", (unsigned int)theResult);
		
		running = YES;
		[captureTimeHeader setHidden:NO];
		[captureTime setHidden:NO];
		[captureTime setStringValue:@""];
		[startButton setTitle:@"Stop"];
	}
	else
	{
		running = NO;
		
		inputCard->StopStreams();
		outputCard->StopScheduledPlayback(0, NULL, 600);
		
		outputCard->DisableVideoOutput();
		inputCard->DisableVideoInput();
		
		[captureTimeHeader setHidden:YES];
		[captureTime setHidden:YES];
		[startButton setTitle:@"Start"];
	}
	
bail:
	return;
}


- (IBAction)videoFormatChanged:(id)sender
{
	if (running == YES)
	{
		outputCard->StopScheduledPlayback(0, NULL, 600);
		outputCard->DisableVideoOutput();
		
		inputCard->StopStreams();
		inputCard->EnableVideoInput((BMDDisplayMode)[[videoFormatMenu selectedItem] tag], bmdFormat8BitYUV, 0);
		outputCard->EnableVideoOutput([[videoFormatMenu selectedItem] tag], bmdVideoOutputFlagDefault);
		outputCard->StartScheduledPlayback(0, 600, 1.0);
		inputCard->StartStreams();
	}
}

@end

static inline bool	operator== (const REFIID& iid1, const REFIID& iid2)
{ 
	return CFEqual(&iid1, &iid2);
}

VideoDelegate::VideoDelegate (MyController* controller)
{
	mController = controller;
	mRefCount = 1;
}

HRESULT		VideoDelegate::QueryInterface (REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes		iunknown;
	HRESULT			result = E_NOINTERFACE;
	
	// Initialise the return result
	*ppv = NULL;
	
	// Obtain the IUnknown interface and compare it the provided REFIID
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (iid == iunknown)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (iid == IID_IDeckLinkInputCallback)
	{
		*ppv = (IDeckLinkInputCallback*)this;
		AddRef();
		result = S_OK;
	}
	
	return result;
}

ULONG	VideoDelegate::AddRef (void)
{
	return OSAtomicIncrement32(&mRefCount);
}

ULONG	VideoDelegate::Release (void)
{
	int32_t		newRefValue;
	
	newRefValue = OSAtomicDecrement32(&mRefCount);
	if (newRefValue == 0)
	{
		delete this;
		return 0;
	}
	
	return newRefValue;
}

HRESULT		VideoDelegate::VideoInputFormatChanged (BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode* newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	return S_OK;
}

HRESULT		VideoDelegate::VideoInputFrameArrived (IDeckLinkVideoInputFrame* arrivedFrame, IDeckLinkAudioInputPacket*)
{
	NSAutoreleasePool*		pool = [[NSAutoreleasePool alloc] init];
	
	if (mController->running == YES)
	{
		BMDTimeValue		frameTime, frameDuration;
		int					hours, minutes, seconds, frames;
		HRESULT				theResult;
		
		arrivedFrame->GetStreamTime(&frameTime, &frameDuration, 600);
		theResult = mController->outputCard->ScheduleVideoFrame(arrivedFrame, frameTime, frameDuration, 600);
		if (theResult != S_OK)
			printf("Scheduling failed with error = %08x\n", (unsigned int)theResult);
		
		hours = (frameTime / (600 * 60*60));
		minutes = (frameTime / (600 * 60)) % 60;
		seconds = (frameTime / 600) % 60;
		frames = (frameTime / 6) % 100;
		[mController->captureTime setStringValue:[NSString stringWithFormat:@"%02d:%02d:%02d:%02d", hours, minutes, seconds, frames]];
	}
	
	[pool release];
	
	return S_OK;
}