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

#include <wincodec.h>		// For handing bitmap files
#include <atlstr.h>
#include <algorithm>
#include "ImageLoader.h"

namespace ImageLoader
{
	IWICImagingFactory*	g_wicFactory = NULL;
}

HRESULT ImageLoader::Initialize()
{
	// Create WIC Imaging factory to read image stills
	HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wicFactory));
	if (FAILED(result))
	{
		fprintf(stderr, "A WIC imaging factory could not be created.\n");
	}

	return result;
}

HRESULT ImageLoader::UnInitialize()
{
	if (g_wicFactory != NULL)
	{
		g_wicFactory->Release();
		g_wicFactory = NULL;
	}
	return S_OK;
}

HRESULT ImageLoader::GetPNGFilesFromDir(const std::string& path, std::vector<std::string>& fileList)
{
	HRESULT			result = E_FAIL;
	WIN32_FIND_DATA	fileData;
	CString			fileNameMask = CString(path.c_str()) + _T("\\*.png");
	HANDLE			fileSearch = FindFirstFile(fileNameMask, &fileData);

	if (fileSearch != INVALID_HANDLE_VALUE)
	{
		do
		{
			CString fileNameString = CString(path.c_str()) + _T('\\') + CString(fileData.cFileName);
			fileList.push_back(std::string(CT2CA(fileNameString.GetString())));
		} while (FindNextFile(fileSearch, &fileData) != 0);

		FindClose(fileSearch);
		result = S_OK;
	}

	return result;
}

HRESULT ImageLoader::ConvertPNGToDeckLinkVideoFrame(const std::string& pngFilename, IDeckLinkVideoFrame* deckLinkVideoFrame)
{
	HRESULT					result = S_OK;
	IWICBitmapDecoder*		bitmapDecoder = NULL;
	IWICBitmapFrameDecode*	bitmapFrameDecode = NULL;
	IWICBitmapClipper*		bitmapClipper = NULL;
	IWICBitmapSource*		bitmapFrameSource = NULL;
	IWICBitmap*				bitmapFrame = NULL;
	IWICBitmapLock*			bitmapLock = NULL;
	IWICFormatConverter*	formatConverter = NULL;
	WICPixelFormatGUID		pixelFormat = GUID_WICPixelFormat32bppBGRA;

	UINT					imageWidth;
	UINT					imageHeight;
	UINT					bitmapLockStride;

	long					videoFrameWidth;
	long					videoFrameHeight;
	long					videoFrameRowBytes;
	long					videoFrameOffsetX;
	long					videoFrameOffsetY;

	UINT					bufferSize = 0;
	BYTE*					bitmapData = NULL;
	WICRect					rectLock;
	BYTE*					deckLinkBuffer = NULL;

	CString filename = pngFilename.c_str();
	result = g_wicFactory->CreateDecoderFromFilename(filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &bitmapDecoder);
	if (FAILED(result))
		goto bail;

	// Retrieve the image frame from the decoder
	result = bitmapDecoder->GetFrame(0, &bitmapFrameDecode);
	if (FAILED(result))
		goto bail;

	// Get and set the size.
	result = bitmapFrameDecode->GetSize(&imageWidth, &imageHeight);
	if (FAILED(result))
		goto bail;

	// Get the pixel format
	result = bitmapFrameDecode->GetPixelFormat(&pixelFormat);
	if (FAILED(result))
		goto bail;

	result = bitmapFrameDecode->QueryInterface(IID_IWICBitmapSource, (void**)&bitmapFrameSource);
	if (FAILED(result))
		goto bail;

	// If pixel format is not a BGRA32 compatible format, then convert source
	if (!IsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGRA) && !IsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGR))
	{
		IWICBitmapSource *bitmapFrameSourceConverted = NULL;

		result = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGR, bitmapFrameSource, &bitmapFrameSourceConverted);
		if (FAILED(result))
			goto bail;

		bitmapFrameSource->Release();
		bitmapFrameSource = bitmapFrameSourceConverted;
	}

	videoFrameWidth = deckLinkVideoFrame->GetWidth();
	videoFrameHeight = deckLinkVideoFrame->GetHeight();
	videoFrameRowBytes = deckLinkVideoFrame->GetRowBytes();

	// If the image is larger than frame size, then crop it 
	if ((imageWidth > (UINT)videoFrameWidth) || (imageHeight > (UINT)videoFrameHeight))
	{
		WICRect clipRect;

		clipRect.X = imageWidth > (UINT)videoFrameWidth ? (imageWidth - (UINT)videoFrameWidth) / 2 : (UINT)0;
		clipRect.Y = imageHeight > (UINT)videoFrameHeight ? (imageHeight - (UINT)videoFrameHeight) / 2 : (UINT)0;
		clipRect.Width = (std::min)(imageWidth, (UINT)videoFrameWidth);
		clipRect.Height = (std::min)(imageHeight, (UINT)videoFrameHeight);

		result = g_wicFactory->CreateBitmapClipper(&bitmapClipper);
		if (FAILED(result))
			goto bail;

		result = bitmapClipper->Initialize(bitmapFrameSource, &clipRect);
		if (FAILED(result))
			goto bail;

		result = g_wicFactory->CreateBitmapFromSource(bitmapClipper, WICBitmapCacheOnDemand, &bitmapFrame);
		if (FAILED(result))
			goto bail;

		imageWidth = clipRect.Width;
		imageHeight = clipRect.Height;
	}
	else
	{
		result = g_wicFactory->CreateBitmapFromSource(bitmapFrameSource, WICBitmapCacheOnDemand, &bitmapFrame);
		if (FAILED(result))
			goto bail;
	}

	rectLock = { 0, 0, static_cast<INT>(imageWidth), static_cast<INT>(imageHeight) };

	// Create bitmap lock so we can access the pixel data directly.
	result = bitmapFrame->Lock(&rectLock, WICBitmapLockRead, &bitmapLock);
	if (FAILED(result))
		goto bail;

	result = bitmapLock->GetDataPointer(&bufferSize, &bitmapData);
	if (FAILED(result))
		goto bail;

	result = bitmapLock->GetStride(&bitmapLockStride);
	if (FAILED(result))
		goto bail;

	result = deckLinkVideoFrame->GetBytes((void**)&deckLinkBuffer);
	if (FAILED(result))
		goto bail;

	// Clear buffer in video frame, so we can display image smaller than the video frame size without artifacts
	memset(deckLinkBuffer, 0, videoFrameRowBytes * videoFrameHeight);

	// Determine X and Y offsets for when image is smaller than output video frame
	videoFrameOffsetX = (UINT)videoFrameWidth > imageWidth ? ((UINT)videoFrameWidth - imageWidth) / 2 : (UINT)0;
	videoFrameOffsetY = (UINT)videoFrameHeight > imageHeight ? ((UINT)videoFrameHeight - imageHeight) / 2 : (UINT)0;

	deckLinkBuffer += videoFrameOffsetY * deckLinkVideoFrame->GetRowBytes();

	for (UINT j = 0; j < imageHeight; j++)
	{
		memcpy(deckLinkBuffer + (videoFrameOffsetX * 4), bitmapData, bitmapLockStride);
		deckLinkBuffer += deckLinkVideoFrame->GetRowBytes();
		bitmapData += bitmapLockStride;
	}

bail:
	if (formatConverter)
		formatConverter->Release();

	if (bitmapLock)
		bitmapLock->Release();

	if (bitmapFrame)
		bitmapFrame->Release();

	if (bitmapFrameSource)
		bitmapFrameSource->Release();

	if (bitmapClipper)
		bitmapClipper->Release();

	if (bitmapFrameDecode)
		bitmapFrameDecode->Release();

	if (bitmapDecoder)
		bitmapDecoder->Release();

	return result;
}
