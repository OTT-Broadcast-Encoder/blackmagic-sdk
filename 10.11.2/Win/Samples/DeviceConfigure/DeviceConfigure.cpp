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

#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <tuple>
#include <functional>
#include "platform.h"
#include "DeckLinkAPI.h"


// Video input connector map 
const std::vector<std::tuple<BMDVideoConnection, std::string>> kVideoInputConnections =
{
	std::make_tuple(bmdVideoConnectionSDI, "SDI video connection"),
	std::make_tuple(bmdVideoConnectionHDMI, "HDMI video connection"),
	std::make_tuple(bmdVideoConnectionOpticalSDI, "Optical SDI video connection"),
	std::make_tuple(bmdVideoConnectionComponent, "Component video connection"),
	std::make_tuple(bmdVideoConnectionComposite, "Composite video connection"),
	std::make_tuple(bmdVideoConnectionSVideo, "S-Video connection"),
};
enum { 
	kVideoInputConnectionsFlag = 0, 
	kVideoInputConnectionsString 
};

// Audio input conector map 
const std::vector<std::tuple<BMDAudioConnection, std::string>> kAudioInputConnections =
{
	std::make_tuple(bmdAudioConnectionEmbedded, "Embedded SDI/HDMI audio connection"),
	std::make_tuple(bmdAudioConnectionAESEBU, "AES/EBU audio connection"),
	std::make_tuple(bmdAudioConnectionAnalog, "Analog audio connection"),
	std::make_tuple(bmdAudioConnectionAnalogXLR, "Analog XLR audio connection"),
	std::make_tuple(bmdAudioConnectionAnalogRCA, "Analog RCA audio connection"),
	std::make_tuple(bmdAudioConnectionMicrophone, "Analog microphone audio connection"),
	std::make_tuple(bmdAudioConnectionHeadphones, "Analog headphone audio connection"),
};
enum { 
	kAudioInputConnectionsFlag = 0, 
	kAudioInputConnectionsString 
};

// Duplex Mode
const std::vector<std::tuple<BMDDuplexMode, std::string>> kDuplexMode =
{
	std::make_tuple(bmdDuplexModeFull, "Device Full Duplex"),
	std::make_tuple(bmdDuplexModeHalf, "Device Half Duplex"),
};
enum { 
	kDuplexModeValue = 0, 
	kDuplexModeString 
};

// Link Configuration - Link configuration, Link configuration valid function, display string
const std::vector<std::tuple<BMDLinkConfiguration, std::function<bool(IDeckLinkAttributes*)>, std::string>> kLinkConfiguration =
{
	std::make_tuple(bmdLinkConfigurationSingleLink, [](IDeckLinkAttributes* dla) -> bool { return true; }, "Single-link SDI video connection"),
	std::make_tuple(bmdLinkConfigurationDualLink, [](IDeckLinkAttributes* dla) -> bool { dlbool_t flag; return (dla->GetFlag(BMDDeckLinkSupportsDualLinkSDI, &flag) == S_OK) && flag; }, "Dual-link SDI video connection"),
	std::make_tuple(bmdLinkConfigurationQuadLink, [](IDeckLinkAttributes* dla) -> bool { dlbool_t flag; return (dla->GetFlag(BMDDeckLinkSupportsQuadLinkSDI, &flag) == S_OK) && flag; }, "Quad-link SDI video connection"),
};
enum { 
	kLinkConfigurationValue = 0, 
	kLinkConfigurationValidFunction, 
	kLinkConfigurationString 
};

// Video Output Mode - Capture pass through mode, idle video output mode, 444 Video output
const std::vector<std::tuple<BMDDeckLinkCapturePassthroughMode, BMDIdleVideoOutputOperation, bool>> kVideoOutputMode
{
	std::make_tuple(bmdDeckLinkCapturePassthroughModeDirect,		bmdIdleVideoOutputBlack,		false),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeCleanSwitch,	bmdIdleVideoOutputBlack,		false),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeDisabled,		bmdIdleVideoOutputBlack,		false),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeDisabled,		bmdIdleVideoOutputLastFrame,	false),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeDirect,		bmdIdleVideoOutputBlack,		true),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeCleanSwitch,	bmdIdleVideoOutputBlack,		true),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeDisabled,		bmdIdleVideoOutputBlack,		true),
	std::make_tuple(bmdDeckLinkCapturePassthroughModeDisabled,		bmdIdleVideoOutputLastFrame,	true),
};
enum { 
	kVideoOutputCapturePassthroughValue = 0, 
	kVideoOutputIdleModeValue, 
	kVideoOutputSDI444Value 
};

std::string GetColorModelString(bool is444)
{
	return is444 ? "RGB444" : "YUV422";
}

std::string GetIdleOutputString(BMDIdleVideoOutputOperation idleOutputMode)
{
	return (idleOutputMode == bmdIdleVideoOutputBlack) ? "Black Frame" : "Last Frame";
}

std::string GetPassThroughModeString(BMDDeckLinkCapturePassthroughMode passThroughMode)
{
	switch (passThroughMode)
	{
	case bmdDeckLinkCapturePassthroughModeDirect:
		return "Direct";
	case bmdDeckLinkCapturePassthroughModeCleanSwitch:
		return "Clean Switch";
	case bmdDeckLinkCapturePassthroughModeDisabled:
	default:
		return "Disabled";
	}
}

std::string	GetDisplayNameAttribute(IDeckLinkAttributes* deckLinkAttributes)
{
	dlstring_t deckLinkName;
	std::string retString;

	if (deckLinkAttributes->GetString(BMDDeckLinkDisplayName, &deckLinkName) == S_OK)
	{
		retString = DlToStdString(deckLinkName);
		DeleteString(deckLinkName);
	}
	else
		retString = "";

	return retString;
}

void DisplayUsage(IDeckLinkConfiguration* deckLinkConfiguration, IDeckLinkAttributes* deckLinkAttributes, std::vector<std::string>& displayNames)
{
	int64_t videoIOSupport;

	std::tuple<BMDDeckLinkCapturePassthroughMode, BMDIdleVideoOutputOperation, bool> currentVideoOutputMode;

	fprintf(stderr,
		"\n"
		"Usage: DeviceConfigure -d <device id> [OPTIONS]\n"
		"\n"
		"    -h/?: help\n"
		"    -d <device id>:\n"
		);

	{
		std::string selectedDisplayName = (deckLinkAttributes != NULL) ? GetDisplayNameAttribute(deckLinkAttributes) : "";

		// Loop through all available devices
		for (size_t i = 0; i < displayNames.size(); i++)
		{
			fprintf(stderr,
				"       %c%2d:  %s\n",
				(displayNames[i] == selectedDisplayName) ? '*' : ' ',
				(int)i,
				displayNames[i].c_str()
				);
		}
	}

	if ((deckLinkConfiguration == NULL) || (deckLinkAttributes == NULL))
	{
		fprintf(stderr, "        No DeckLink device selected\n");
		return;
	}

	// Get Video IO support for selected device
	if (deckLinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) != S_OK)
		videoIOSupport = 0;

	fprintf(stderr, "    -l <link width id>:\n");
	{
		int64_t supportedVideoConnections;

		if ((deckLinkAttributes->GetInt(BMDDeckLinkVideoOutputConnections, &supportedVideoConnections) == S_OK) 
			&& (((BMDVideoConnection)supportedVideoConnections & bmdVideoConnectionSDI) != 0))
		{
			int64_t currentLinkConfig;
			if (deckLinkConfiguration->GetInt(bmdDeckLinkConfigSDIOutputLinkConfiguration, &currentLinkConfig) != S_OK)
				currentLinkConfig = (int64_t) bmdLinkConfigurationSingleLink;

			// Iterate through all available link configurations on the delected DeckLink device
			for (size_t i = 0; i < kLinkConfiguration.size(); i++)
			{
				if (std::get<kLinkConfigurationValidFunction>(kLinkConfiguration[i])(deckLinkAttributes))
				{

					fprintf(stderr,
						"       %c%2d:  %s\n",
						((BMDLinkConfiguration)currentLinkConfig == std::get<kLinkConfigurationValue>(kLinkConfiguration[i])) ? '*' : ' ',
						(int)i,
						(std::get<kLinkConfigurationString>(kLinkConfiguration[i])).c_str()
						);
				}
			}
		}
		else
			fprintf(stderr, "       No SDI connections on the selected device\n");
	}

	fprintf(stderr, "    -p <duplex mode id>:\n");
	{
		dlbool_t supportsDuplexMode;

		// Get current duplex mode configuration
		if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsDuplexModeConfiguration, &supportsDuplexMode) != S_OK) || !supportsDuplexMode)
			fprintf(stderr, "       Duplex mode not supported by the selected device\n");
		else
		{
			int64_t currentDuplexMode;

			if (deckLinkConfiguration->GetInt(bmdDeckLinkConfigDuplexMode, &currentDuplexMode) != S_OK)
				currentDuplexMode = bmdDuplexModeFull;

			for (size_t i = 0; i < kDuplexMode.size(); i++)
				fprintf(stderr,
				"       %c%2d:  %s\n",
				((BMDDuplexMode)currentDuplexMode == std::get<kDuplexModeValue>(kDuplexMode[i])) ? '*' : ' ',
				(int)i,
				(std::get<kDuplexModeString>(kDuplexMode[i])).c_str()
				);
		}
	}

	fprintf(stderr, "    -v <video input connector id>:\n");

	if ((BMDVideoIOSupport)videoIOSupport & bmdDeviceSupportsCapture)
	{
		int64_t currentVideoInputConnection;
		int64_t supportedVideoInputConnections;

		// Get current video input connector configuration
		if (deckLinkConfiguration->GetInt(bmdDeckLinkConfigVideoInputConnection, &currentVideoInputConnection) != S_OK)
			currentVideoInputConnection = 0;

		if (deckLinkAttributes->GetInt(BMDDeckLinkVideoInputConnections,&supportedVideoInputConnections) == S_OK)
		{
			for (size_t i = 0; i < kVideoInputConnections.size(); i++)
			{
				if (((BMDVideoConnection)supportedVideoInputConnections & std::get<kVideoInputConnectionsFlag>(kVideoInputConnections[i])) != 0)
					fprintf(stderr,
					"       %c%2d:  %s\n",
					((BMDVideoConnection)currentVideoInputConnection & std::get<kVideoInputConnectionsFlag>(kVideoInputConnections[i])) ? '*' : ' ',
					(int)i,
					(std::get<kVideoInputConnectionsString>(kVideoInputConnections[i])).c_str()
					);

			}
		}
		else
			fprintf(stderr, "       Unable to get video input connections\n");
	}
	else
		fprintf(stderr, "       Capture operation is not supported by the selected device\n");

	fprintf(stderr, "    -a <audio input connector id>:\n");

	if ((BMDVideoIOSupport)videoIOSupport & bmdDeviceSupportsCapture)
	{
		int64_t currentAudioInputConnection;
		int64_t supportedAudioInputConnections;

		// Get current audio input connector configuration
		if (deckLinkConfiguration->GetInt(bmdDeckLinkConfigAudioInputConnection, &currentAudioInputConnection) != S_OK)
			currentAudioInputConnection = 0;

		if (deckLinkAttributes->GetInt(BMDDeckLinkAudioInputConnections, &supportedAudioInputConnections) == S_OK)
		{
			for (size_t i = 0; i < kAudioInputConnections.size(); i++)
			{
				if (((BMDAudioConnection)supportedAudioInputConnections & std::get<kAudioInputConnectionsFlag>(kAudioInputConnections[i])) != 0)
					fprintf(stderr,
					"       %c%2d:  %s\n",
					((BMDAudioConnection)currentAudioInputConnection & std::get<kAudioInputConnectionsFlag>(kAudioInputConnections[i])) ? '*' : ' ',
					(int)i,
					(std::get<kAudioInputConnectionsString>(kAudioInputConnections[i])).c_str()
					);

			}
		}
		else
			fprintf(stderr, "       Unable to get audio input connections\n");
	}
	else
		fprintf(stderr, "       Capture operation is not supported by the selected device\n");

	fprintf(stderr, "    -o <video output mode id>:\n");

	if ((BMDVideoIOSupport)videoIOSupport & bmdDeviceSupportsPlayback)
	{
		dlbool_t dummyFlag;
		int64_t dummyInt;

		// Check whether idle, passthrough and 444 output mode supported for the device
		bool supportsIdleOutput = (deckLinkAttributes->GetFlag(BMDDeckLinkSupportsIdleOutput, &dummyFlag) == S_OK) && dummyFlag;
		bool supportsSDI444Output = deckLinkConfiguration->GetFlag(bmdDeckLinkConfig444SDIVideoOutput, &dummyFlag) == S_OK;
		bool supportsCapturePassThrough = (deckLinkConfiguration->GetInt(bmdDeckLinkConfigCapturePassThroughMode, &dummyInt) == S_OK);

		{
			int64_t currentPassThroughMode;
			int64_t currentIdleMode;
			dlbool_t currentSDI444OutputMode;

			if (deckLinkConfiguration->GetInt(bmdDeckLinkConfigCapturePassThroughMode, &currentPassThroughMode) != S_OK)
				currentPassThroughMode = (int64_t)bmdDeckLinkCapturePassthroughModeDisabled;

			if (deckLinkConfiguration->GetInt(bmdDeckLinkConfigVideoOutputIdleOperation, &currentIdleMode) != S_OK)
				currentIdleMode = (int64_t)bmdIdleVideoOutputBlack;

			if (deckLinkConfiguration->GetFlag(bmdDeckLinkConfig444SDIVideoOutput, &currentSDI444OutputMode) != S_OK)
				currentSDI444OutputMode = (dlbool_t)false;

			currentVideoOutputMode = std::make_tuple((BMDDeckLinkCapturePassthroughMode)currentPassThroughMode, (BMDIdleVideoOutputOperation)currentIdleMode, (currentSDI444OutputMode != (dlbool_t)false));
		}

		fprintf(stderr,
			"        ID   %-15s\t%-20s\t%-20s\n",
			"Color space",
			"Pass-through mode",
			"Idle output"
			);

		for (size_t i = 0; i < kVideoOutputMode.size(); i++)
		{
			if ((!supportsIdleOutput && (std::get<kVideoOutputIdleModeValue>(kVideoOutputMode[i]) == bmdIdleVideoOutputLastFrame)) ||
				(!supportsSDI444Output && std::get<kVideoOutputSDI444Value>(kVideoOutputMode[i])) ||
				(!supportsCapturePassThrough && (std::get<kVideoOutputCapturePassthroughValue>(kVideoOutputMode[i]) != bmdDeckLinkCapturePassthroughModeDisabled)))
				continue;

			fprintf(stderr,
				"       %c%2d:  %-15s\t%-20s\t%-20s\n",
				(kVideoOutputMode[i] == currentVideoOutputMode) ? '*' : ' ',
				(int)i,
				GetColorModelString(std::get<kVideoOutputSDI444Value>(kVideoOutputMode[i])).c_str(),
				GetPassThroughModeString(std::get<kVideoOutputCapturePassthroughValue>(kVideoOutputMode[i])).c_str(),
				GetIdleOutputString(std::get<kVideoOutputIdleModeValue>(kVideoOutputMode[i])).c_str()
				);

		}
	}
	else
		fprintf(stderr, "       Playback operation is not supported by the selected device\n");

	fprintf(stderr, "    -n <new device label>\n");
	{
		int64_t persistentID;

		// Check whether Persistent ID exists, if so we can set device label
		if (deckLinkAttributes->GetInt(BMDDeckLinkPersistentID, &persistentID) == S_OK)
		{
			// Get current device label
			dlstring_t deviceLabelStr;
			std::string deviceLabel;

			if (deckLinkConfiguration->GetString(bmdDeckLinkConfigDeviceInformationLabel, &deviceLabelStr) == S_OK)
			{
				deviceLabel = DlToStdString(deviceLabelStr);
				DeleteString(deviceLabelStr);
			}
			else
				deviceLabel = "";

			fprintf(stderr,
				"       * %s\n",
				deviceLabel.empty() ? "<Label not set>" : deviceLabel.c_str()
				);
		}
		else
			fprintf(stderr, "       Selected device does not have persistent ID\n");
	}

	fprintf(stderr,
		"\n"
		"    * = Current configuration\n"
		"\n"
		"Reconfigure DeckLink device. eg. modify label for device 1:\n"
		"\n"
		"    DeviceConfigure.exe -d 1 -n \"New Label\"\n\n"
	);

}


int main(int argc, char* argv[])
{
	// Configuration settings
	bool						displayHelp					= false;
	int							deckLinkIndex				= -1;
	int							linkConfigurationIndex		= -1;
	int							duplexModeIndex				= -1;
	int							videoInputConnectorIndex	= -1;
	int							audioInputConnectorIndex	= -1;
	int							videoOutputModeIndex		= -1;
	std::string					newDeviceLabel;

	int							idx;

	IDeckLinkIterator*			deckLinkIterator		= NULL;
	IDeckLink*					deckLink				= NULL;
	IDeckLink*					selectedDeckLink		= NULL;
	IDeckLinkAttributes*		deckLinkAttributes		= NULL;
	IDeckLinkConfiguration*		deckLinkConfiguration	= NULL;

	std::vector<std::string>	deckLinkDisplayNames;

	HRESULT						result;
	
	result = CoInitialize(NULL);
	if (FAILED(result))
	{
		fprintf(stderr, "Initialization of COM failed - result = %08x.\n", result);
		return 1;
	}

	// Process the command line arguments
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-d") == 0)
			deckLinkIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-l") == 0)
			linkConfigurationIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-p") == 0)
			duplexModeIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-v") == 0)
			videoInputConnectorIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-a") == 0)
			audioInputConnectorIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-o") == 0)
			videoOutputModeIndex = atoi(argv[++i]);

		else if (strcmp(argv[i], "-n") == 0)
			newDeviceLabel = std::string(argv[++i]);

		else if ((strcmp(argv[i], "?") == 0) || (strcmp(argv[i], "-h") == 0))
			displayHelp = true;

		else
		{
			// Unknown argument on command line
			fprintf(stderr, "Unknown argument %s\n", argv[i]);
			displayHelp = true;
		}
	}

	if (deckLinkIndex < 0)
	{
		fprintf(stderr, "You must select a device\n");
		displayHelp = true;
	}

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = GetDeckLinkIterator(&deckLinkIterator);
	if (result != S_OK)
	{
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		goto bail;
	}
	
	// Obtain the required DeckLink device
	idx = 0;

	while ((result = deckLinkIterator->Next(&deckLink)) == S_OK)
	{
		dlstring_t deckLinkName;

		result = deckLink->GetDisplayName(&deckLinkName);
		if (result == S_OK)
		{
			deckLinkDisplayNames.push_back(DlToStdString(deckLinkName));
			DeleteString(deckLinkName);
		}

		if (idx++ == deckLinkIndex)
			selectedDeckLink = deckLink;
		else
			deckLink->Release();
	}

	if (selectedDeckLink != NULL)
	{
		if (selectedDeckLink->QueryInterface(IID_IDeckLinkConfiguration, (void**)&deckLinkConfiguration) != S_OK)
		{
			fprintf(stderr, "Unable to query IDeckLinkConfiguration interface\n");
			goto bail;
		}

		if (selectedDeckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes) != S_OK)
		{
			fprintf(stderr, "Unable to query IDeckLinkAttributes interface\n");
			goto bail;
		}
	}
	else
		displayHelp = true;

	if (displayHelp)
	{
		DisplayUsage(deckLinkConfiguration, deckLinkAttributes, deckLinkDisplayNames);
		goto bail;
	}

	// Set link width configuration
	if (linkConfigurationIndex != -1)
	{
		if (std::get<kLinkConfigurationValidFunction>(kLinkConfiguration[linkConfigurationIndex])(deckLinkAttributes))
		{
			if (deckLinkConfiguration->SetInt(bmdDeckLinkConfigSDIOutputLinkConfiguration, (int64_t)std::get<kLinkConfigurationValue>(kLinkConfiguration[linkConfigurationIndex])) != S_OK)
			{
				fprintf(stderr, "Unable to set SDI output Link Configuration\n");
				goto bail;
			}
		}
		else
		{
			fprintf(stderr, "Link width not supported by device\n");
			goto bail;
		}
	}
	
	// Set duplex mode configuration
	if (duplexModeIndex != -1)
	{
		dlbool_t supportsDuplexMode;

		if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsDuplexModeConfiguration, &supportsDuplexMode) != S_OK) || !supportsDuplexMode)
		{
			fprintf(stderr, "Duplex mode not supported by device\n");
			goto bail;
		}
		else if (deckLinkConfiguration->SetInt(bmdDeckLinkConfigDuplexMode, (int64_t)std::get<kDuplexModeValue>(kDuplexMode[duplexModeIndex])) != S_OK)
		{
			fprintf(stderr, "Unable to set Duplex mode on device\n");
			goto bail;
		}
	}
	
	// Set video input connection configuration
	if (videoInputConnectorIndex != -1)
	{
		int64_t supportedVideoInputConnections;

		if ((deckLinkAttributes->GetInt(BMDDeckLinkVideoInputConnections, &supportedVideoInputConnections) != S_OK)
			|| (((BMDVideoConnection)supportedVideoInputConnections & std::get<kVideoInputConnectionsFlag>(kVideoInputConnections[videoInputConnectorIndex])) == 0))
		{
			fprintf(stderr, "Invalid video input connector for device\n");
			goto bail;
		}
		else if (deckLinkConfiguration->SetInt(bmdDeckLinkConfigVideoInputConnection, (int64_t)std::get<kVideoInputConnectionsFlag>(kVideoInputConnections[videoInputConnectorIndex])) != S_OK)
		{
			fprintf(stderr, "Unable to set video input connector\n");
			goto bail;
		}
	}

	// Set audio input connection configuration
	if (audioInputConnectorIndex != -1)
	{
		int64_t supportedAudioInputConnections;

		if ((deckLinkAttributes->GetInt(BMDDeckLinkAudioInputConnections, &supportedAudioInputConnections) != S_OK) 
			|| (((BMDAudioConnection)supportedAudioInputConnections & std::get<kAudioInputConnectionsFlag>(kAudioInputConnections[audioInputConnectorIndex])) == 0))
		{
			fprintf(stderr, "Invalid audio input connector for device\n");
			goto bail;
		}
		else if (deckLinkConfiguration->SetInt(bmdDeckLinkConfigAudioInputConnection, (int64_t)std::get<kAudioInputConnectionsFlag>(kAudioInputConnections[audioInputConnectorIndex])) != S_OK)
		{
			fprintf(stderr, "Unable to set audio input connector\n");
			goto bail;
		}
	}
	
	// Set video output mode configurations
	if (videoOutputModeIndex != -1)
	{
		dlbool_t dummyFlag;
		int64_t dummyInt;

		bool supportsIdleOutput = (deckLinkAttributes->GetFlag(BMDDeckLinkSupportsIdleOutput, &dummyFlag) == S_OK) && dummyFlag;
		bool supportsSDI444Output = deckLinkConfiguration->GetFlag(bmdDeckLinkConfig444SDIVideoOutput, &dummyFlag) == S_OK;
		bool supportsCapturePassThrough = (deckLinkConfiguration->GetInt(bmdDeckLinkConfigCapturePassThroughMode, &dummyInt) == S_OK);

		if (supportsCapturePassThrough && (deckLinkConfiguration->SetInt(bmdDeckLinkConfigCapturePassThroughMode, (int64_t)std::get<kVideoOutputCapturePassthroughValue>(kVideoOutputMode[videoOutputModeIndex])) == E_FAIL))
		{
			fprintf(stderr, "Unable to set video capture passthrough mode\n");
			goto bail;
		}

		if (supportsIdleOutput && (deckLinkConfiguration->SetInt(bmdDeckLinkConfigVideoOutputIdleOperation, (int64_t)std::get<kVideoOutputIdleModeValue>(kVideoOutputMode[videoOutputModeIndex])) != S_OK))
		{
			fprintf(stderr, "Unable to set video output idle mode\n");
			goto bail;
		}

		if (supportsSDI444Output && (deckLinkConfiguration->SetFlag(bmdDeckLinkConfig444SDIVideoOutput, (dlbool_t)std::get<kVideoOutputSDI444Value>(kVideoOutputMode[videoOutputModeIndex])) == E_FAIL))
		{
			fprintf(stderr, "Unable to set video output 444 mode\n");
			goto bail;
		}
	}

	// Set new device label
	if (!newDeviceLabel.empty())
	{
		int64_t persistentID;

		if (deckLinkAttributes->GetInt(BMDDeckLinkPersistentID, &persistentID) != S_OK)
		{
			fprintf(stderr, "Unable to set new display label, device has no persistent ID\n");
			goto bail;
		}
		else
		{
			dlstring_t deviceLabelStr = StdToDlString(newDeviceLabel);
			result = deckLinkConfiguration->SetString(bmdDeckLinkConfigDeviceInformationLabel, deviceLabelStr);
			DeleteString(deviceLabelStr);

			if (result != S_OK)
			{
				fprintf(stderr, "Unable to set device label\n");
				goto bail;
			}
		}
	}

	// OK to write new configuration - print configuration
	fprintf(stderr, "Updating device with configuration:\n - Device display name: %s\n", GetDisplayNameAttribute(deckLinkAttributes).c_str());
	if (linkConfigurationIndex != -1)
		fprintf(stderr, " - Link width: %s\n", (std::get<kLinkConfigurationString>(kLinkConfiguration[linkConfigurationIndex])).c_str());
	if (duplexModeIndex != -1)
		fprintf(stderr, " - Duplex Mode: %s\n", (std::get<kDuplexModeString>(kDuplexMode[duplexModeIndex])).c_str());
	if (videoInputConnectorIndex != -1)
		fprintf(stderr, " - Video Input Connector: %s\n", (std::get<kVideoInputConnectionsString>(kVideoInputConnections[videoInputConnectorIndex])).c_str());
	if (audioInputConnectorIndex != -1)
		fprintf(stderr, " - Audio Input Connector: %s\n", (std::get<kAudioInputConnectionsString>(kAudioInputConnections[audioInputConnectorIndex])).c_str());
	if (videoOutputModeIndex != -1)
	{
		fprintf(stderr, " - Video Output Color mode: %s\n", GetColorModelString(std::get<kVideoOutputSDI444Value>(kVideoOutputMode[videoOutputModeIndex])).c_str());
		fprintf(stderr, " - Video Output Pass-through mode: %s\n", GetPassThroughModeString(std::get<kVideoOutputCapturePassthroughValue>(kVideoOutputMode[videoOutputModeIndex])).c_str());
		fprintf(stderr, " - Video Output Idle mode: %s\n", GetIdleOutputString(std::get<kVideoOutputIdleModeValue>(kVideoOutputMode[videoOutputModeIndex])).c_str());
	}

	// Write configuration to preferences
	result = deckLinkConfiguration->WriteConfigurationToPreferences();
	switch (result)
	{
		case S_OK :
			fprintf(stderr, "Done.\n");
			break;

		case S_FALSE :
			fprintf(stderr, "No configuration to save, use ?/-h option for help\n");
			break;

		case E_ACCESSDENIED :
			fprintf(stderr, "Unable to save configuration, insufficient privileges to write to system preferences\n");
			break;

		case E_FAIL:
			fprintf(stderr, "Unable to save configuration, an unexpected failure occurred\n");
			break;
	}

bail:
	if (deckLinkConfiguration != NULL)
	{
		deckLinkConfiguration->Release();
		deckLinkConfiguration = NULL;
	}

	if (deckLinkAttributes != NULL)
	{
		deckLinkAttributes->Release();
		deckLinkAttributes = NULL;
	}

	if (selectedDeckLink != NULL)
	{
		selectedDeckLink->Release();
		selectedDeckLink = NULL;
	}

	if (deckLinkIterator != NULL)
	{
		deckLinkIterator->Release();
		deckLinkIterator = NULL;
	}

	CoUninitialize();

	return(result == S_OK) ? 0 : 1;
}
