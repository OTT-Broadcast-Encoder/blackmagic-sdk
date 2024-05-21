/* -LICENSE-START-
 ** Copyright (c) 2015 Blackmagic Design
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

#include "VideoFrameTransfer.h"


#define DVP_CHECK(cmd) {						\
    DVPStatus hr = (cmd);						\
    if (DVP_STATUS_OK != hr) {                  \
		OutputDebugStringA( #cmd " failed\n" ); \
		ExitProcess(hr);						\
		    }                                       \
}

#define MEM_RD32(a) (*(const volatile unsigned int *)(a))
#define MEM_WR32(a, d) do { *(volatile unsigned int *)(a) = (d); } while (0)

// Initialise static members
bool								VideoFrameTransfer::mInitialized = false;
bool								VideoFrameTransfer::mUseDvp = false;
unsigned							VideoFrameTransfer::mWidth = 0;
unsigned							VideoFrameTransfer::mHeight = 0;
void*								VideoFrameTransfer::mCaptureTexture = 0;

// NVIDIA specific static members
DVPBufferHandle						VideoFrameTransfer::mDvpCaptureTextureHandle = 0;
DVPBufferHandle						VideoFrameTransfer::mDvpPlaybackTextureHandle = 0;
uint32_t							VideoFrameTransfer::mBufferAddrAlignment = 0;
uint32_t							VideoFrameTransfer::mBufferGpuStrideAlignment = 0;
uint32_t							VideoFrameTransfer::mSemaphoreAddrAlignment = 0;
uint32_t							VideoFrameTransfer::mSemaphoreAllocSize = 0;
uint32_t							VideoFrameTransfer::mSemaphorePayloadOffset = 0;
uint32_t							VideoFrameTransfer::mSemaphorePayloadSize = 0;


bool VideoFrameTransfer::isNvidiaDvpAvailable()
{
	return true;
}

bool VideoFrameTransfer::checkFastMemoryTransferAvailable()
{
	return isNvidiaDvpAvailable();
}

bool VideoFrameTransfer::initialize(ID3D11Device* pD3DDevice, unsigned width, unsigned height, void *captureTexture, void *playbackTexture)
{
	if (mInitialized)
		return false;

	if (!checkFastMemoryTransferAvailable())
		return false;

	mUseDvp = isNvidiaDvpAvailable();
	mWidth = width;
	mHeight = height;
	mCaptureTexture = captureTexture;

	if (!initializeMemoryLocking(mWidth * mHeight * 4))		// BGRA uses 4 bytes per pixel
		return false;

	if (mUseDvp)
	{
		// DVP initialisation
		DVP_CHECK(dvpInitD3D11Device(pD3DDevice, 0));
		DVP_CHECK(dvpGetRequiredConstantsD3D11Device(&mBufferAddrAlignment, &mBufferGpuStrideAlignment,
			&mSemaphoreAddrAlignment, &mSemaphoreAllocSize,
			&mSemaphorePayloadOffset, &mSemaphorePayloadSize,
			pD3DDevice));

		// Register textures with DVP
		DVP_CHECK(dvpCreateGPUD3D11Resource((ID3D11Resource *)captureTexture, &mDvpCaptureTextureHandle));
		DVP_CHECK(dvpCreateGPUD3D11Resource((ID3D11Resource *)playbackTexture, &mDvpPlaybackTextureHandle));
	}

	mInitialized = true;

	return true;
}

bool VideoFrameTransfer::destroy(ID3D11Device* pD3DDevice)
{
	if (!mInitialized)
		return false;

	if (mUseDvp)
	{
		DVP_CHECK(dvpFreeBuffer(mDvpCaptureTextureHandle));
		DVP_CHECK(dvpFreeBuffer(mDvpPlaybackTextureHandle));

		DVP_CHECK(dvpCloseD3D11Device(pD3DDevice));
	}

	mInitialized = false;

	return true;
}

bool VideoFrameTransfer::initializeMemoryLocking(unsigned memSize)
{
	// Increase the process working set size to allow pinning of memory.
	static SIZE_T	dwMin = 0, dwMax = 0;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
	if (!hProcess)
		return false;

	// Retrieve the working set size of the process.
	if (!dwMin && !GetProcessWorkingSetSize(hProcess, &dwMin, &dwMax))
		return false;

	// Allow for 80 frames to be locked
	BOOL res = SetProcessWorkingSetSize(hProcess, memSize * 80 + dwMin, memSize * 80 + (dwMax - dwMin));
	if (!res)
		return false;

	CloseHandle(hProcess);
	return true;
}

// SyncInfo sets up a semaphore which is shared between the GPU and CPU and used to
// synchronise access to DVP buffers.
struct SyncInfo
{
	SyncInfo(uint32_t semaphoreAllocSize, uint32_t semaphoreAddrAlignment);
	~SyncInfo();

	volatile uint32_t*	mSem;
	volatile uint32_t	mReleaseValue;
	volatile uint32_t	mAcquireValue;
	DVPSyncObjectHandle	mDvpSync;
};

SyncInfo::SyncInfo(uint32_t semaphoreAllocSize, uint32_t semaphoreAddrAlignment)
{
	mSem = (uint32_t*)_aligned_malloc(semaphoreAllocSize, semaphoreAddrAlignment);

	// Initialise
	mSem[0] = 0;
	mReleaseValue = 0;
	mAcquireValue = 0;

	// Setup DVP sync object and import it
	DVPSyncObjectDesc syncObjectDesc;
	syncObjectDesc.externalClientWaitFunc = NULL;
	syncObjectDesc.sem = (uint32_t*)mSem;

	DVP_CHECK(dvpImportSyncObject(&syncObjectDesc, &mDvpSync));
}

SyncInfo::~SyncInfo()
{
	DVP_CHECK(dvpFreeSyncObject(mDvpSync));
	_aligned_free((void*)mSem);
}

VideoFrameTransfer::VideoFrameTransfer(ID3D11Device* pD3DDevice, unsigned long memSize, void* address, Direction direction) :
	mBuffer(address),
	mMemSize(memSize),
	mDirection(direction),
	mExtSync(NULL),
	mGpuSync(NULL),
	mDvpSysMemHandle(0)
{
	if (mUseDvp)
	{
		// Pin the memory
		if (!VirtualLock(mBuffer, mMemSize))
			throw std::runtime_error("Error pinning memory with VirtualLock");

		// Create necessary sysmem and gpu sync objects
		mExtSync = new SyncInfo(mSemaphoreAllocSize, mSemaphoreAddrAlignment);
		mGpuSync = new SyncInfo(mSemaphoreAllocSize, mSemaphoreAddrAlignment);

		// Cache the device
		mpD3DDevice = pD3DDevice;

		// Register system memory buffers with DVP
		DVPSysmemBufferDesc sysMemBuffersDesc;
		sysMemBuffersDesc.width = mWidth;
		sysMemBuffersDesc.height = mHeight;
		sysMemBuffersDesc.stride = mWidth * 4;
		sysMemBuffersDesc.format = DVP_BGRA;
		sysMemBuffersDesc.type = DVP_UNSIGNED_BYTE;
		sysMemBuffersDesc.size = mMemSize;
		sysMemBuffersDesc.bufAddr = mBuffer;

		if (mDirection == CPUtoGPU)
		{
			// A UYVY 4:2:2 frame is transferred to the GPU, rather than RGB 4:4:4, so width is halved
			sysMemBuffersDesc.width /= 2;
			sysMemBuffersDesc.stride /= 2;
		}

		DVP_CHECK(dvpCreateBuffer(&sysMemBuffersDesc, &mDvpSysMemHandle));
		DVP_CHECK(dvpBindToD3D11Device(mDvpSysMemHandle, mpD3DDevice));
	}
}

VideoFrameTransfer::~VideoFrameTransfer()
{
	if (mUseDvp)
	{
		DVP_CHECK(dvpUnbindFromD3D11Device(mDvpSysMemHandle, mpD3DDevice));
		DVP_CHECK(dvpDestroyBuffer(mDvpSysMemHandle));

		delete mExtSync;
		delete mGpuSync;

		VirtualUnlock(mBuffer, mMemSize);
	}
}

bool VideoFrameTransfer::performFrameTransfer()
{
	if (mUseDvp)
	{
		// NVIDIA DVP transfers
		DVPStatus status;

		mGpuSync->mReleaseValue++;

		dvpBegin();
		if (mDirection == CPUtoGPU)
		{
			// Copy from system memory to GPU texture
			dvpMapBufferWaitDVP(mDvpCaptureTextureHandle);
			status = dvpMemcpyLined(mDvpSysMemHandle, mExtSync->mDvpSync, mExtSync->mAcquireValue, DVP_TIMEOUT_IGNORED,
				mDvpCaptureTextureHandle, mGpuSync->mDvpSync, mGpuSync->mReleaseValue, 0, mHeight);
			dvpMapBufferEndDVP(mDvpCaptureTextureHandle);
		}
		else
		{
			// Copy from GPU texture to system memory
			dvpMapBufferWaitDVP(mDvpPlaybackTextureHandle);

			status = dvpMemcpyLined(mDvpPlaybackTextureHandle, mExtSync->mDvpSync, mExtSync->mReleaseValue, DVP_TIMEOUT_IGNORED,
				mDvpSysMemHandle, mGpuSync->mDvpSync, mGpuSync->mReleaseValue, 0, mHeight);
			dvpMapBufferEndDVP(mDvpPlaybackTextureHandle);
		}
		dvpEnd();

		return (status == DVP_STATUS_OK);
	}

	return true;
}

void VideoFrameTransfer::waitSyncComplete()
{
	if (!mUseDvp)
		return;

	// Acquire the GPU semaphore
	mGpuSync->mAcquireValue++;

	// Increment the release value
	mExtSync->mReleaseValue++;

	// Block until buffer has completely transferred from GPU to CPU buffer
	if (mDirection == GPUtoCPU) {
		dvpBegin();
		DVPStatus status = dvpSyncObjClientWaitPartial(mGpuSync->mDvpSync, mGpuSync->mAcquireValue, DVP_TIMEOUT_IGNORED);
		dvpEnd();
	}
}

void VideoFrameTransfer::endSyncComplete()
{
	if (!mUseDvp)
		return;

	// Update semaphore
	MEM_WR32(mExtSync->mSem, mExtSync->mReleaseValue);
}

void VideoFrameTransfer::waitAPI(Direction direction)
{
	if (!mUseDvp)
		return;

	if (direction == CPUtoGPU)
		dvpMapBufferWaitAPI(mDvpCaptureTextureHandle);
	else
		dvpMapBufferWaitAPI(mDvpPlaybackTextureHandle);
}

void VideoFrameTransfer::endAPI(Direction direction)
{
	if (!mUseDvp)
		return;

	if (direction == CPUtoGPU)
		dvpMapBufferEndAPI(mDvpCaptureTextureHandle);
	else
		dvpMapBufferEndAPI(mDvpPlaybackTextureHandle);
}
