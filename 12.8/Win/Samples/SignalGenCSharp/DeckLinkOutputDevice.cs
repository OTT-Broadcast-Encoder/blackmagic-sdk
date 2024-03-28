﻿/* -LICENSE-START-
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

using System;
using System.Collections.Generic;
using System.Threading;
using DeckLinkAPI;

namespace SignalGenCSharp
{
	class DeckLinkOutputInvalidException : Exception { }
	class DeckLinkStartPlaybackException : Exception { }

	public class DeckLinkScheduledFrameCompletedEventArgs : EventArgs
	{
		public readonly IDeckLinkVideoFrame completedFrame;
		public readonly _BMDOutputFrameCompletionResult completionResult;

		public DeckLinkScheduledFrameCompletedEventArgs(IDeckLinkVideoFrame completedFrame, _BMDOutputFrameCompletionResult completionResult)
		{
			this.completedFrame = completedFrame;
			this.completionResult = completionResult;
		}
	}

	public class DeckLinkRenderAudioSamplesEventArgs : EventArgs
	{
		public readonly uint bufferedAudioSamples;

		public DeckLinkRenderAudioSamplesEventArgs(uint bufferedAudioSamples) => this.bufferedAudioSamples = bufferedAudioSamples;
	}

	public class DeckLinkOutputDevice : IDeckLinkVideoOutputCallback, IDeckLinkAudioOutputCallback
	{
		const uint kMinimumVideoPrerollSize = 2;  // Sample use 2 frames preroll as minimum

		enum PlaybackState { Idle, Prerolling, Running };

		private readonly IDeckLink m_deckLink;
		private readonly IDeckLinkOutput m_deckLinkOutput;
		private readonly IDeckLinkConfiguration m_deckLinkConfiguration;
		private readonly IDeckLinkProfileManager m_deckLinkProfileManager;

		private readonly string m_displayName;
		private readonly long m_availableOutputConnections;
		private readonly int m_supportsColorspaceMetadata;

		private uint m_audioPrerollSize;

		private long m_frameDuration;
		private long m_frameTimescale;

		PlaybackState m_state;
		private readonly object m_lockState;
		private EventWaitHandle m_stopScheduledPlaybackEvent;

		public event EventHandler<DeckLinkScheduledFrameCompletedEventArgs> ScheduledFrameCompleted;
		public event EventHandler<DeckLinkRenderAudioSamplesEventArgs> RenderAudioSamples;

		public DeckLinkOutputDevice(IDeckLink deckLink, IDeckLinkProfileCallback profileCallback)
		{
			m_deckLink = deckLink;

			// Ensure card has a playback interface (not a DeckLink Mini Recorder, for instance)
			var deckLinkAttributes = m_deckLink as IDeckLinkProfileAttributes;
			deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkVideoIOSupport, out long ioSupportAttribute);
			if (!((_BMDVideoIOSupport)ioSupportAttribute).HasFlag(_BMDVideoIOSupport.bmdDeviceSupportsPlayback))
				throw new DeckLinkOutputInvalidException();

			deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkVideoOutputConnections, out m_availableOutputConnections);

			deckLinkAttributes.GetFlag(_BMDDeckLinkAttributeID.BMDDeckLinkSupportsColorspaceMetadata, out m_supportsColorspaceMetadata);

			m_deckLinkOutput = m_deckLink as IDeckLinkOutput;

			// Get device configuration interface.
			// We hold onto interface for object lifecycle in order to temporarily configure output link width
			m_deckLinkConfiguration = m_deckLink as IDeckLinkConfiguration;

			// Get device display name
			m_displayName = DeckLinkDeviceTools.GetDisplayLabel(m_deckLink);

			// Get profile manager for the device
			m_deckLinkProfileManager = m_deckLink as IDeckLinkProfileManager;
			if (m_deckLinkProfileManager != null)
			{
				// Target device has more that 1 profile, set callback to monitor profile changes
				m_deckLinkProfileManager.SetCallback(profileCallback);
			}

			m_lockState = new object();
			m_state = PlaybackState.Idle;

			m_stopScheduledPlaybackEvent = new EventWaitHandle(false, EventResetMode.AutoReset);
		}

		~DeckLinkOutputDevice()
		{
			if (m_deckLinkProfileManager != null)
			{
				m_deckLinkProfileManager.SetCallback(null);
			}
		}

		public IDeckLink DeckLink => m_deckLink;
		public IDeckLinkOutput DeckLinkOutput => m_deckLinkOutput;
		public string DisplayName => m_displayName;
		public bool IsActive => DeckLinkDeviceTools.IsDeviceActive(m_deckLink);
		public uint AudioWaterLevel => m_audioPrerollSize;
		public bool SupportsColorspaceMetadata => Convert.ToBoolean(m_supportsColorspaceMetadata);

		public bool IsRunning
		{
			get
			{
				lock (m_lockState)
				{
					return m_state != PlaybackState.Idle;
				}
			}
		}

		public _BMDLinkConfiguration CurrentLinkConfiguration
		{
			get
			{
				m_deckLinkConfiguration.GetInt(_BMDDeckLinkConfigurationID.bmdDeckLinkConfigSDIOutputLinkConfiguration, out long currentLinkConfiguration);
				return (_BMDLinkConfiguration)currentLinkConfiguration;
			}
			set
			{
				m_deckLinkConfiguration.SetInt(_BMDDeckLinkConfigurationID.bmdDeckLinkConfigSDIOutputLinkConfiguration, (long)value);
			}
		}

		public bool HasSDIOutputConnection
		{
			get
			{
				var outputConnections = (_BMDVideoConnection)m_availableOutputConnections;
				return outputConnections.HasFlag(_BMDVideoConnection.bmdVideoConnectionSDI) || outputConnections.HasFlag(_BMDVideoConnection.bmdVideoConnectionOpticalSDI);
			}
		}

		public bool IsLinkConfigurationSupported(_BMDLinkConfiguration configuration)
		{
			var deckLinkAttributes = m_deckLink as IDeckLinkProfileAttributes;

			switch (configuration)
			{
				case _BMDLinkConfiguration.bmdLinkConfigurationSingleLink:
					return true;

				case _BMDLinkConfiguration.bmdLinkConfigurationDualLink:
					// Check whether device with current profile supports dual-link configuration
					deckLinkAttributes.GetFlag(_BMDDeckLinkAttributeID.BMDDeckLinkSupportsDualLinkSDI, out int supportsDualLink);
					return Convert.ToBoolean(supportsDualLink);

				case _BMDLinkConfiguration.bmdLinkConfigurationQuadLink:
					// Check whether device with current profile supports quad-link configuration
					deckLinkAttributes.GetFlag(_BMDDeckLinkAttributeID.BMDDeckLinkSupportsQuadLinkSDI, out int supportsQuadLink);
					return Convert.ToBoolean(supportsQuadLink);

				default:
					return false;
			}
		}

		public bool IsDisplayModeSupported(_BMDDisplayMode displayMode, _BMDPixelFormat pixelFormat)
		{
			_BMDVideoConnection videoConnection = HasSDIOutputConnection ? _BMDVideoConnection.bmdVideoConnectionSDI : _BMDVideoConnection.bmdVideoConnectionUnspecified;

			_BMDSupportedVideoModeFlags supportedFlags = DeckLinkDeviceTools.GetSDIConfigurationVideoModeFlags(HasSDIOutputConnection, CurrentLinkConfiguration);

			m_deckLinkOutput.DoesSupportVideoMode(videoConnection, displayMode, pixelFormat, _BMDVideoOutputConversionMode.bmdNoVideoOutputConversion, 
				supportedFlags, out _, out int supported);
			return Convert.ToBoolean(supported);
		}

		public bool DoesDisplayModeSupport3D(_BMDDisplayMode displayMode)
		{
			m_deckLinkOutput.GetDisplayMode(displayMode, out IDeckLinkDisplayMode deckLinkDisplayMode);
			return deckLinkDisplayMode.GetFlags().HasFlag(_BMDDisplayModeFlags.bmdDisplayModeSupports3D);
		}

		public bool IsColorspaceSupported(_BMDDisplayMode displayMode, _BMDColorspace colorspace)
		{
			m_deckLinkOutput.GetDisplayMode(displayMode, out IDeckLinkDisplayMode deckLinkDisplayMode);
			var displayHeight = deckLinkDisplayMode.GetHeight();

			switch (colorspace)
			{
				case _BMDColorspace.bmdColorspaceRec601:
					// Rec.601 is supported for SD display modes
					return displayHeight < 720;

				case _BMDColorspace.bmdColorspaceRec709:
					// Rec.709 is supported for HD/UHD display modes
					return displayHeight >= 720;
			}

			return false;
		}

		public uint MaxAudioChannels
		{
			get
			{
				var deckLinkAttributes  = m_deckLink as IDeckLinkProfileAttributes;
				deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkMaximumAudioChannels, out long maxAudioChannels);
				return Convert.ToUInt32(maxAudioChannels);
			}
		}

		public bool SupportsHFRTimecode
		{
			get
			{
				var deckLinkAttributes = m_deckLink as IDeckLinkProfileAttributes;
				deckLinkAttributes.GetFlag(_BMDDeckLinkAttributeID.BMDDeckLinkSupportsHighFrameRateTimecode, out int supportsHFRTC);
				return Convert.ToBoolean(supportsHFRTC);
			}
		}

		public IEnumerable<IDeckLinkDisplayMode> DisplayModes
		{
			get
			{
				_BMDSupportedVideoModeFlags supportedFlags = DeckLinkDeviceTools.GetSDIConfigurationVideoModeFlags(HasSDIOutputConnection, CurrentLinkConfiguration);
				_BMDVideoConnection videoConnection = HasSDIOutputConnection ? _BMDVideoConnection.bmdVideoConnectionSDI : _BMDVideoConnection.bmdVideoConnectionUnspecified;

				// Create a display mode iterator
				m_deckLinkOutput.GetDisplayModeIterator(out IDeckLinkDisplayModeIterator displayModeIterator);
				if (displayModeIterator == null)
					yield break;

				// Scan through all display modes
				while (true)
				{
					displayModeIterator.Next(out IDeckLinkDisplayMode displayMode);

					if (displayMode != null)
					{
						// Check display mode is supported for selected link configuration
						m_deckLinkOutput.DoesSupportVideoMode(videoConnection, displayMode.GetDisplayMode(), _BMDPixelFormat.bmdFormatUnspecified, 
							_BMDVideoOutputConversionMode.bmdNoVideoOutputConversion, supportedFlags, out _, out int supported);
						if (!Convert.ToBoolean(supported))
							continue;

						yield return displayMode;
					}
					else
						yield break;
				}
			}
		}

		public uint VideoWaterLevel
		{
			get
			{
				var deckLinkAttributes = m_deckLink as IDeckLinkProfileAttributes;
				deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkMinimumPrerollFrames, out long minimumPrerollFrames);
				return Math.Max((uint)minimumPrerollFrames, kMinimumVideoPrerollSize);
			}
		}

		public void StartPlayback(_BMDDisplayMode displayMode, _BMDPixelFormat pixelFormat, _BMDVideoOutputFlags videoOutputFlags, _BMDAudioSampleType audioSampleType, uint audioChannelCount, IDeckLinkScreenPreviewCallback screenPreviewCallback)
		{
			try
			{
				m_deckLinkOutput.GetDisplayMode(displayMode, out IDeckLinkDisplayMode deckLinkDisplayMode);
				deckLinkDisplayMode.GetFrameRate(out m_frameDuration, out m_frameTimescale);

				m_deckLinkOutput.SetScreenPreviewCallback(screenPreviewCallback);

				m_deckLinkOutput.SetScheduledFrameCompletionCallback(this);
				m_deckLinkOutput.SetAudioCallback(this);

				// Get audio preroll size
				m_audioPrerollSize = (uint)(((long)(VideoWaterLevel * m_frameDuration) * (uint)_BMDAudioSampleRate.bmdAudioSampleRate48kHz) / m_frameTimescale);

				m_deckLinkOutput.EnableVideoOutput(displayMode, videoOutputFlags);
				m_deckLinkOutput.EnableAudioOutput(_BMDAudioSampleRate.bmdAudioSampleRate48kHz, audioSampleType, audioChannelCount, _BMDAudioOutputStreamType.bmdAudioOutputStreamContinuous);
				m_deckLinkOutput.BeginAudioPreroll();

				lock (m_lockState)
				{
					m_state = PlaybackState.Prerolling;
				}
			}
			catch (Exception)
			{
				throw new DeckLinkStartPlaybackException();
			}
		}

		public void StopPlayback()
		{
			PlaybackState state;
			lock (m_lockState)
			{
				state = m_state;
			}

			switch (state)
			{
				case PlaybackState.Running:
					m_deckLinkOutput.StopScheduledPlayback(0, out _, 100);
					m_stopScheduledPlaybackEvent.WaitOne();
					goto case PlaybackState.Prerolling;

				case PlaybackState.Prerolling:
					// Dereference DeckLinkOutputDevice delegate from callbacks
					m_deckLinkOutput.SetScheduledFrameCompletionCallback(null);
					m_deckLinkOutput.SetAudioCallback(null);
					m_deckLinkOutput.SetScreenPreviewCallback(null);

					// Disable video and audio outputs
					m_deckLinkOutput.DisableAudioOutput();
					m_deckLinkOutput.DisableVideoOutput();
					break;
			}

			lock (m_lockState)
			{
				m_state = PlaybackState.Idle;
			}
		}

		#region callbacks
		// Explicit implementation of IDeckLinkVideoOutputCallback and IDeckLinkAudioOutputCallback
		void IDeckLinkVideoOutputCallback.ScheduledFrameCompleted(IDeckLinkVideoFrame completedFrame, _BMDOutputFrameCompletionResult result)
		{
			// When a video frame has been completed, generate event to schedule next frame
			ScheduledFrameCompleted?.Invoke(this, new DeckLinkScheduledFrameCompletedEventArgs(completedFrame, result));
		}

		void IDeckLinkVideoOutputCallback.ScheduledPlaybackHasStopped()
		{
			m_stopScheduledPlaybackEvent.Set();
		}

		void IDeckLinkAudioOutputCallback.RenderAudioSamples(int preroll)
		{
			// Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
			m_deckLinkOutput.GetBufferedAudioSampleFrameCount(out uint bufferedAudioSampleCount);

			if (bufferedAudioSampleCount < m_audioPrerollSize)
			{
				RenderAudioSamples?.Invoke(this, new DeckLinkRenderAudioSamplesEventArgs(bufferedAudioSampleCount));
			}
			else if (Convert.ToBoolean(preroll))
			{
				// Ensure sufficient video frames are prerolled
				m_deckLinkOutput.GetBufferedVideoFrameCount(out uint bufferedVideoFrameCount);

				if (bufferedVideoFrameCount >= VideoWaterLevel)
				{
					m_deckLinkOutput.EndAudioPreroll();
					m_deckLinkOutput.StartScheduledPlayback(0, 100, 1.0);
					lock (m_lockState)
					{
						m_state = PlaybackState.Running;
					}
				}
			}
		}
		#endregion
	}

	public static class DeckLinkDeviceTools
	{
		public static string GetDisplayLabel(IDeckLink device)
		{
			device.GetDisplayName(out string displayName);
			return displayName;
		}

		public static bool IsDeviceActive(IDeckLink device)
		{
			var deckLinkAttributes = device as IDeckLinkProfileAttributes;
			deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkDuplex, out long duplexMode);
			return ((_BMDDuplexMode)duplexMode != _BMDDuplexMode.bmdDuplexInactive);
		}

		public static _BMDSupportedVideoModeFlags GetSDIConfigurationVideoModeFlags(bool supportsSDIOutput, _BMDLinkConfiguration linkConfiguration = _BMDLinkConfiguration.bmdLinkConfigurationSingleLink)
		{
			_BMDSupportedVideoModeFlags flags = _BMDSupportedVideoModeFlags.bmdSupportedVideoModeDefault;

			if (supportsSDIOutput)
			{
				// Onyl SDI supports miltiple link connection, check flags against link configuration
				switch (linkConfiguration)
				{
					case _BMDLinkConfiguration.bmdLinkConfigurationSingleLink:
						flags = _BMDSupportedVideoModeFlags.bmdSupportedVideoModeSDISingleLink;
						break;

					case _BMDLinkConfiguration.bmdLinkConfigurationDualLink:
						flags = _BMDSupportedVideoModeFlags.bmdSupportedVideoModeSDIDualLink;
						break;

					case _BMDLinkConfiguration.bmdLinkConfigurationQuadLink:
						flags = _BMDSupportedVideoModeFlags.bmdSupportedVideoModeSDIQuadLink;
						break;
				}
			}
			return flags;
		}

		public static int BytesPerRow(_BMDPixelFormat pixelFormat, int frameWidth)
		{
			int bytesPerRow;

			// Refer to DeckLink SDK Manual - 2.7.4 Pixel Formats
			switch (pixelFormat)
			{
				case _BMDPixelFormat.bmdFormat8BitYUV:
					bytesPerRow = frameWidth * 2;
					break;

				case _BMDPixelFormat.bmdFormat10BitYUV:
					bytesPerRow = ((frameWidth + 47) / 48) * 128;
					break;

				case _BMDPixelFormat.bmdFormat10BitRGB:
					bytesPerRow = ((frameWidth + 63) / 64) * 256;
					break;

				case _BMDPixelFormat.bmdFormat8BitARGB:
				case _BMDPixelFormat.bmdFormat8BitBGRA:
				default:
					bytesPerRow = frameWidth * 4;
					break;
			}

			return bytesPerRow;
		}
	}
}
