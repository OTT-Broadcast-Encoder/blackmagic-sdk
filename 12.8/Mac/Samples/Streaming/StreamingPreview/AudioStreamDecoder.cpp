/* -LICENSE-START-
** Copyright (c) 2011 Blackmagic Design
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

#include "AudioStreamDecoder.h"
#include "Debug.h"

#include <errno.h>

const size_t kBufferSize = 32 << 10;
const size_t kBufferPacketDescs = 64;

AudioStreamDecoder::AudioStreamDecoder() :
	mStream(NULL),
	mQueue(NULL),
	mStarted(false),
	mFinished(false),
	mCurrentBuffer(NULL)
{
	memset(mBuffers, 0, sizeof(mBuffers));
}

AudioStreamDecoder::~AudioStreamDecoder()
{
	long err = Stop();
	CARP_IF(err, "Stop returned %ld\n", err);

	err = pthread_cond_destroy(&mCond);
	CARP_IF(err, "pthread_cond_destroy returned %ld\n", err);

	err = pthread_mutex_destroy(&mMutex);
	CARP_IF(err, "pthread_mutex_destroy returned %ld\n", err);
}

long AudioStreamDecoder::Init()
{
	long err = pthread_mutex_init(&mMutex, NULL);
	BAIL_IF(err, "pthread_mutex_init returned %ld\n", err);

	err = pthread_cond_init(&mCond, NULL);
	BAIL_IF(err, "pthread_cond_init returned %ld\n", err);

bail:
	return err;
}

long AudioStreamDecoder::Start()
{
	long err = AudioFileStreamOpen(this, StaticPropertyCallback, StaticPacketCallback, kAudioFileAAC_ADTSType, &mStream);
	BAIL_IF(err, "AudioFileStreamOpen returned %ld\n", err);

bail:
	return err;
}

long AudioStreamDecoder::Stop()
{
	long err = 0;

	if (mStream)
	{
		err = AudioFileStreamClose(mStream);
		CARP_IF(err, "AudioFileStreamClose returned %ld\n", err);

		mStream = NULL;
	}

	if (mStarted)
	{
		err = AudioQueueFlush(mQueue);
		BAIL_IF(err, "AudioQueueFlush returned %ld\n", err);

		err = AudioQueueStop(mQueue, true);
		BAIL_IF(err, "AudioQueueStop returned %ld\n", err);

		err = pthread_mutex_lock(&mMutex);
		BAIL_IF(err, "pthread_mutex_lock returned %ld\n", err);

		if (!mFinished)
		{
			err = pthread_cond_wait(&mCond, &mMutex);
			BAIL_IF(err, "pthread_cond_wait returned %ld\n", err);
		}

		err = pthread_mutex_unlock(&mMutex);
		BAIL_IF(err, "pthread_mutex_unlock returned %ld\n", err);

		mStarted = false;
		mFinished = false;
	}

	if (mQueue)
	{
		for (int i = 0; i < kBufferCount; i++)
		{
			if (mBuffers[i])
			{
				err = AudioQueueFreeBuffer(mQueue, mBuffers[i]);
				CARP_IF(err, "AudioQueueFreeBuffer returned %ld\n", err);
			}
		}

		err = AudioQueueDispose(mQueue, true);
		CARP_IF(err, "AudioQueueDispose returned %ld\n", err);

		mQueue = NULL;
	}

bail:
	return err;
}

long AudioStreamDecoder::EnqueueData(const void* data, unsigned int length, bool discontinuous)
{
	unsigned int flags = discontinuous ? kAudioFileStreamParseFlag_Discontinuity : 0;

	long err = mStream ? 0 : ENOENT;
	BAIL_IF(err, "AudioFileStream not initialised\n", err);

	err = data ? 0 : EINVAL;
	BAIL_IF(err, "Invalid data pointer\n");

	err = length ? 0 : EINVAL;
	BAIL_IF(err, "Invalid length\n");

	err = AudioFileStreamParseBytes(mStream, length, data, flags);
	BAIL_IF(err, "AudioFileStreamParseBytes returned %ld\n", err);

bail:
	return err;
}

void AudioStreamDecoder::StaticQueueRunningCallback(void* context, AudioQueueRef queue, AudioQueuePropertyID property)
{
	BAIL_IF(!context, "Invalid context pointer\n", context);
	((AudioStreamDecoder*)context)->QueueRunningCallback(queue, property);

bail:
	return;
}

void AudioStreamDecoder::StaticBufferCompleteCallback(void* context, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
	BAIL_IF(!context, "Invalid context pointer\n", context);
	((AudioStreamDecoder*)context)->BufferCompleteCallback(queue, buffer);

bail:
	return;
}

void AudioStreamDecoder::StaticPropertyCallback(void* context, AudioFileStreamID stream, AudioFileStreamPropertyID property, UInt32* flags)
{
	BAIL_IF(!context, "Invalid context pointer\n", context);
	((AudioStreamDecoder*)context)->PropertyCallback(stream, property, flags);

bail:
	return;
}

void AudioStreamDecoder::StaticPacketCallback(void* context, UInt32 byteCount, UInt32 packetCount, const void* data, AudioStreamPacketDescription* packetDescriptions)
{
	BAIL_IF(!context, "Invalid context pointer\n", context);
	((AudioStreamDecoder*)context)->PacketCallback(byteCount, packetCount, data, packetDescriptions);

bail:
	return;
}

void AudioStreamDecoder::QueueRunningCallback(AudioQueueRef queue, AudioQueuePropertyID property)
{
	long err;

	BAIL_IF(!queue || queue != mQueue, "Invalid queue %p\n", queue);
	BAIL_IF(!mStarted, "Queue not started\n");

	UInt32 running, size;
	size = sizeof(running);
	err = AudioQueueGetProperty(mQueue, kAudioQueueProperty_IsRunning, &running, &size);
	BAIL_IF(err, "AudioQueueGetProperty returned %ld\n", err);

	if (!running)
	{
		err = SetFinished();
		BAIL_IF(err, "Finished returned %ld\n", err);
	}

bail:
	return;
}

void AudioStreamDecoder::BufferCompleteCallback(AudioQueueRef queue, AudioQueueBufferRef buffer)
{
	long err;
	bool locked = false;

	BAIL_IF(!queue, "Invalid queue\n");
	BAIL_IF(!buffer, "Invalid buffer\n");

	err = pthread_mutex_lock(&mMutex);
	BAIL_IF(err, "pthread_mutex_lock returned %ld\n", err);

	locked = true;
	buffer->mUserData = NULL;

	err = pthread_cond_signal(&mCond);
	BAIL_IF(err, "pthread_cond_signal returned %ld\n", err);

bail:
	if (locked)
	{
		err = pthread_mutex_unlock(&mMutex);
		CARP_IF(err, "pthread_mutex_unlock returned %ld\n", err);
	}
}

void AudioStreamDecoder::PropertyCallback(AudioFileStreamID stream, AudioFileStreamPropertyID property, UInt32* flags)
{
	if (property != kAudioFileStreamProperty_ReadyToProducePackets)
		return;

	long err;
	void* buffer = NULL;
	unsigned char writable;
	AudioStreamBasicDescription desc = {0};
	UInt32 size = sizeof(desc);

	BAIL_IF(!stream || stream != mStream, "Invalid stream %p\n", stream);

	err = AudioFileStreamGetProperty(mStream, kAudioFileStreamProperty_DataFormat, &size, &desc);
	BAIL_IF(err, "AudioFileStreamGetProperty returned %ld\n", err);

	err = AudioQueueNewOutput(&desc, StaticBufferCompleteCallback, this, NULL, NULL, 0, &mQueue);
	BAIL_IF(err, "AudioQueueNewOutput returned %ld\n", err);

	err = AudioQueueAddPropertyListener(mQueue, kAudioQueueProperty_IsRunning, StaticQueueRunningCallback, this);
	BAIL_IF(err, "AudioQueueAddPropertyListener returned %ld\n", err);

	for (int i = 0; i < kBufferCount; i++)
	{
		err = AudioQueueAllocateBufferWithPacketDescriptions(mQueue, kBufferSize, kBufferPacketDescs, mBuffers + i);
		BAIL_IF(err, "AudioQueueAllocateBuffer returned %ld\n", err);
	}

	mCurrentBuffer = mBuffers;
	(*mCurrentBuffer)->mUserData = this;

	err = AudioFileStreamGetPropertyInfo(mStream, kAudioFileStreamProperty_MagicCookieData, &size, &writable);
	BAIL_IF(err, "AudioFileStreamGetPropertyInfo returned %ld\n", err);

	buffer = malloc(size);
	BAIL_IF(!buffer, "Failed to allocate %u byte buffer for cookie\n", (unsigned int)size);

	err = AudioFileStreamGetProperty(mStream, kAudioFileStreamProperty_MagicCookieData, &size, buffer);
	BAIL_IF(err, "AudioFileStreamGetProperty returned %ld\n", err);

	err = AudioQueueSetProperty(mQueue, kAudioQueueProperty_MagicCookie, buffer, size);
	BAIL_IF(err, "AudioQueueSetProperty returned %ld\n", err);

bail:
	free(buffer);
}

void AudioStreamDecoder::PacketCallback(UInt32 byteCount, UInt32 packetCount, const void* data, AudioStreamPacketDescription* packetDescriptions)
{
	long err;

	// Constant bit rate audio arrives without packet descriptions.
	while (!packetDescriptions && !mFinished && byteCount)
	{
		unsigned int copyCount = byteCount;
		if (copyCount > (*mCurrentBuffer)->mAudioDataBytesCapacity - (*mCurrentBuffer)->mAudioDataByteSize)
			copyCount = (*mCurrentBuffer)->mAudioDataBytesCapacity - (*mCurrentBuffer)->mAudioDataByteSize;

		memcpy((unsigned char*)(*mCurrentBuffer)->mAudioData + (*mCurrentBuffer)->mAudioDataByteSize, data, copyCount);
		byteCount -= copyCount;
		(*mCurrentBuffer)->mAudioDataByteSize += copyCount;

		if ((*mCurrentBuffer)->mAudioDataByteSize == (*mCurrentBuffer)->mAudioDataBytesCapacity)
		{
			err = EnqueueBuffer();
			BAIL_IF(err, "EnqueueBuffer returned %ld\n", err);
		}
	}

	if (!packetDescriptions)
		return;

	// Variable bit rate audio has a description for every packet.
	for (unsigned int i = 0; !mFinished && i < packetCount; i++)
	{
		if (packetDescriptions[i].mDataByteSize > (*mCurrentBuffer)->mAudioDataBytesCapacity - (*mCurrentBuffer)->mAudioDataByteSize)
		{
			err = EnqueueBuffer();
			BAIL_IF(err, "EnqueueBuffer returned %ld\n", err);
		}

		if (mFinished)
			break;

		memcpy((unsigned char*)(*mCurrentBuffer)->mAudioData + (*mCurrentBuffer)->mAudioDataByteSize,
			   (const unsigned char*)data + packetDescriptions[i].mStartOffset,
			   packetDescriptions[i].mDataByteSize);
		(*mCurrentBuffer)->mPacketDescriptions[(*mCurrentBuffer)->mPacketDescriptionCount] = packetDescriptions[i];
		(*mCurrentBuffer)->mPacketDescriptions[(*mCurrentBuffer)->mPacketDescriptionCount].mStartOffset = (*mCurrentBuffer)->mAudioDataByteSize;
		(*mCurrentBuffer)->mAudioDataByteSize += packetDescriptions[i].mDataByteSize;
		(*mCurrentBuffer)->mPacketDescriptionCount++;

		if ((*mCurrentBuffer)->mPacketDescriptionCount == (*mCurrentBuffer)->mPacketDescriptionCapacity)
		{
			err = EnqueueBuffer();
			BAIL_IF(err, "EnqueueBuffer returned %ld\n", err);
		}
	}

bail:
	return;
}

long AudioStreamDecoder::EnqueueBuffer()
{
	bool locked = false;

	if (mFinished)
		return 0;

	long err = AudioQueueEnqueueBuffer(mQueue, *mCurrentBuffer, 0, NULL);
	BAIL_IF(err, "AudioQueueEnqueueBuffer returned %ld\n", err);

	if (++mCurrentBuffer == mBuffers + kBufferCount)
		mCurrentBuffer = mBuffers;

	if (!mStarted && !mFinished)
	{
		mStarted = true;

		err = AudioQueueStart(mQueue, NULL);
		BAIL_IF(err, "AudioQueueStart returned %ld\n", err);
	}

	err = pthread_mutex_lock(&mMutex);
	BAIL_IF(err, "pthread_mutex_lock returned %ld\n", err);

	locked = true;

	while ((*mCurrentBuffer)->mUserData && !mFinished)
	{
		err = pthread_cond_wait(&mCond, &mMutex);
		BAIL_IF(err, "pthread_cond_wait returned %ld\n", err);
	}

	(*mCurrentBuffer)->mUserData = this;
	(*mCurrentBuffer)->mAudioDataByteSize = 0;
	(*mCurrentBuffer)->mPacketDescriptionCount = 0;

bail:
	long err2;

	if (locked)
	{
		err2 = pthread_mutex_unlock(&mMutex);
		CARP_IF(err2, "pthread_mutex_unlock returned %ld\n", err2);
	}

	if (err && mStarted)
	{
		err2 = SetFinished();
		CARP_IF(err2, "SetFinished returned %ld\n", err2);
	}

	return err;
}

long AudioStreamDecoder::SetFinished()
{
	bool locked = false;

	long err = pthread_mutex_lock(&mMutex);
	BAIL_IF(err, "pthread_mutex_lock returned %ld\n", err);

	locked = true;
	mFinished = true;

	err = pthread_cond_signal(&mCond);
	BAIL_IF(err, "pthread_cond_signal returned %ld\n", err);

bail:
	if (locked)
	{
		long err2 = pthread_mutex_unlock(&mMutex);
		CARP_IF(err2, "pthread_mutex_unlock returned %ld\n", err2);
	}

	return err;
}
