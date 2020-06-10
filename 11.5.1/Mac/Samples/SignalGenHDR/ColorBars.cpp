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
// ColorBars.cpp : implementation file
//

#include <array>
#include <functional>
#include <utility>
#include <vector>
#include "ColorBars.h"

struct Color12BitRGB
{
	short Red;
	short Green;
	short Blue;
};

typedef std::array<Color12BitRGB, static_cast<size_t>(EOTFColorRange::Size)> EOTFColorArray;
typedef std::vector<std::pair<EOTFColorArray, uint32_t>> ColorBarsPattern;

static void FillLineBars(ColorBarsPattern& pattern, EOTFColorRange range, std::vector<Color12BitRGB>& lineBuffer);
static void FillLineRamp(ColorBarsPattern& pattern, EOTFColorRange range, std::vector<Color12BitRGB>& lineBuffer);


// Refer to BT.2111 specification
static const EOTFColorArray k100pcWhite			= { Color12BitRGB{ 3760, 3760, 3760 }, Color12BitRGB{ 3760, 3760, 3760 }, Color12BitRGB{ 4095, 4095, 4095 } };
static const EOTFColorArray k100pcYellow		= { Color12BitRGB{ 3760, 3760,  256 }, Color12BitRGB{ 3760, 3760,  256 }, Color12BitRGB{ 4095, 4095,    0 } };
static const EOTFColorArray k100pcCyan			= { Color12BitRGB{  256, 3760, 3760 }, Color12BitRGB{  256, 3760, 3760 }, Color12BitRGB{    0, 4095, 4095 } };
static const EOTFColorArray k100pcGreen			= { Color12BitRGB{  256, 3760,  256 }, Color12BitRGB{  256, 3760,  256 }, Color12BitRGB{    0, 4095,    0 } };
static const EOTFColorArray k100pcMagenta		= { Color12BitRGB{ 3760,  256, 3760 }, Color12BitRGB{ 3760,  256, 3760 }, Color12BitRGB{ 4095,    0, 4095 } };
static const EOTFColorArray k100pcRed			= { Color12BitRGB{ 3760,  256,  256 }, Color12BitRGB{ 3760,  256,  256 }, Color12BitRGB{ 4095,    0,    0 } };
static const EOTFColorArray k100pcBlue			= { Color12BitRGB{  256,  256, 3760 }, Color12BitRGB{  256,  256, 3760 }, Color12BitRGB{    0,    0, 4095 } };
static const EOTFColorArray k75pcWhite			= { Color12BitRGB{ 2884, 2884, 2884 }, Color12BitRGB{ 2288, 2288, 2288 }, Color12BitRGB{ 2375, 2375, 2375 } };
static const EOTFColorArray k75pcYellow			= { Color12BitRGB{ 2884, 2884,  256 }, Color12BitRGB{ 2288, 2288,  256 }, Color12BitRGB{ 2375, 2375,    0 } };
static const EOTFColorArray k75pcCyan			= { Color12BitRGB{  256, 2884, 2884 }, Color12BitRGB{  256, 2288, 2288 }, Color12BitRGB{    0, 2375, 2375 } };
static const EOTFColorArray k75pcGreen			= { Color12BitRGB{  256, 2884,  256 }, Color12BitRGB{  256, 2288,  256 }, Color12BitRGB{    0, 2375,    0 } };
static const EOTFColorArray k75pcMagenta		= { Color12BitRGB{ 2884,  256, 2884 }, Color12BitRGB{ 2288,  256, 2288 }, Color12BitRGB{ 2375,    0, 2375 } };
static const EOTFColorArray k75pcRed			= { Color12BitRGB{ 2884,  256,  256 }, Color12BitRGB{ 2288,  256,  256 }, Color12BitRGB{ 2375,    0,    0 } };
static const EOTFColorArray k75pcBlue			= { Color12BitRGB{  256,  256, 2884 }, Color12BitRGB{  256,  256, 2288 }, Color12BitRGB{    0,    0, 2375 } };
static const EOTFColorArray k40pcGrey			= { Color12BitRGB{ 1656, 1656, 1656 }, Color12BitRGB{ 1656, 1656, 1656 }, Color12BitRGB{ 1638, 1638, 1638 } };
static const EOTFColorArray kNeg7pcStep			= { Color12BitRGB{   16,   16,   16 }, Color12BitRGB{   16,   16,   16 }, Color12BitRGB{    0,    0,    0 } };
static const EOTFColorArray k0pcStep			= { Color12BitRGB{  256,  256,  256 }, Color12BitRGB{  256,  256,  256 }, Color12BitRGB{    0,    0,    0 } };
static const EOTFColorArray k10pcStep			= { Color12BitRGB{  608,  608,  608 }, Color12BitRGB{  608,  608,  608 }, Color12BitRGB{  410,  410,  410 } };
static const EOTFColorArray k20pcStep			= { Color12BitRGB{  956,  956,  956 }, Color12BitRGB{  956,  956,  956 }, Color12BitRGB{  819,  819,  819 } };
static const EOTFColorArray k30pcStep			= { Color12BitRGB{ 1308, 1308, 1308 }, Color12BitRGB{ 1308, 1308, 1308 }, Color12BitRGB{ 1229, 1229, 1229 } };
static const EOTFColorArray k40pcStep			= { Color12BitRGB{ 1656, 1656, 1656 }, Color12BitRGB{ 1656, 1656, 1656 }, Color12BitRGB{ 1638, 1638, 1638 } };
static const EOTFColorArray k50pcStep			= { Color12BitRGB{ 2008, 2008, 2008 }, Color12BitRGB{ 2008, 2008, 2008 }, Color12BitRGB{ 2048, 2048, 2048 } };
static const EOTFColorArray k60pcStep			= { Color12BitRGB{ 2360, 2360, 2360 }, Color12BitRGB{ 2360, 2360, 2360 }, Color12BitRGB{ 2457, 2457, 2457 } };
static const EOTFColorArray k70pcStep			= { Color12BitRGB{ 2708, 2708, 2708 }, Color12BitRGB{ 2708, 2708, 2708 }, Color12BitRGB{ 2867, 2867, 2867 } };
static const EOTFColorArray k80pcStep			= { Color12BitRGB{ 3060, 3060, 3060 }, Color12BitRGB{ 3060, 3060, 3060 }, Color12BitRGB{ 3276, 3276, 3276 } };
static const EOTFColorArray k90pcStep			= { Color12BitRGB{ 3408, 3408, 3408 }, Color12BitRGB{ 3408, 3408, 3408 }, Color12BitRGB{ 3686, 3686, 3686 } };
static const EOTFColorArray k100pcStep			= { Color12BitRGB{ 3760, 3760, 3760 }, Color12BitRGB{ 3760, 3760, 3760 }, Color12BitRGB{ 4095, 4095, 4095 } };
static const EOTFColorArray k109pcStep			= { Color12BitRGB{ 4076, 4076, 4076 }, Color12BitRGB{ 4076, 4076, 4076 }, Color12BitRGB{ 4095, 4095, 4095 } };
static const EOTFColorArray k75pcBT709Yellow	= { Color12BitRGB{ 2852, 2876, 1264 }, Color12BitRGB{ 2272, 2284, 1524 }, Color12BitRGB{ 2356, 2370, 1480 } };
static const EOTFColorArray k75pcBT709Cyan		= { Color12BitRGB{ 2152, 2836, 2872 }, Color12BitRGB{ 1936, 2264, 2284 }, Color12BitRGB{ 1964, 2345, 2368 } };
static const EOTFColorArray k75pcBT709Green		= { Color12BitRGB{ 2048, 2824, 1184 }, Color12BitRGB{ 1896, 2256, 1472 }, Color12BitRGB{ 1915, 2339, 1420 } };
static const EOTFColorArray k75pcBT709Magenta	= { Color12BitRGB{ 2604, 1144, 2820 }, Color12BitRGB{ 2144, 1444, 2256 }, Color12BitRGB{ 2206, 1389, 2336 } };
static const EOTFColorArray k75pcBT709Red		= { Color12BitRGB{ 2556, 1076,  656 }, Color12BitRGB{ 2120, 1400, 1024 }, Color12BitRGB{ 2178, 1337,  900 } };
static const EOTFColorArray k75pcBT709Blue		= { Color12BitRGB{  908,  588, 2808 }, Color12BitRGB{ 1268,  944, 2248 }, Color12BitRGB{ 1184,  805, 2328 } };
static const EOTFColorArray k0pcBlack			= { Color12BitRGB{  256,  256,  256 }, Color12BitRGB{  256,  256,  256 }, Color12BitRGB{    0,    0,    0 } };
static const EOTFColorArray kNeg2pcBlack		= { Color12BitRGB{  192,  192,  192 }, Color12BitRGB{  192,  192,  192 }, Color12BitRGB{    0,    0,    0 } };
static const EOTFColorArray kPlus2pcBlack		= { Color12BitRGB{  320,  320,  320 }, Color12BitRGB{  320,  320,  320 }, Color12BitRGB{   82,   82,   82 } };
static const EOTFColorArray kPlus4pcBlack		= { Color12BitRGB{  396,  396,  396 }, Color12BitRGB{  396,  396,  396 }, Color12BitRGB{  164,  164,  164 } };


// Pattern 1 - 100% Bars - Color and HD-width pairs
static const ColorBarsPattern kColorBarsPattern1 = {
	std::make_pair(k40pcGrey, 240),
	std::make_pair(k100pcWhite, 206),
	std::make_pair(k100pcYellow, 206),
	std::make_pair(k100pcCyan, 206),
	std::make_pair(k100pcGreen, 204),
	std::make_pair(k100pcMagenta, 206),
	std::make_pair(k100pcRed, 206),
	std::make_pair(k100pcBlue, 206),
	std::make_pair(k40pcGrey, 240),
};

// Pattern 2 - 75% bars (HLG)/58% bars (PQ) - Color and HD-width pairs
static const ColorBarsPattern kColorBarsPattern2 = {
	std::make_pair(k40pcGrey, 240),
	std::make_pair(k75pcWhite, 206),
	std::make_pair(k75pcYellow, 206),
	std::make_pair(k75pcCyan, 206),
	std::make_pair(k75pcGreen, 204),
	std::make_pair(k75pcMagenta, 206),
	std::make_pair(k75pcRed, 206),
	std::make_pair(k75pcBlue, 206),
	std::make_pair(k40pcGrey, 240),
};

// Pattern 3 - 10% Step - Color and HD-width pairs
static const ColorBarsPattern kColorBarsPattern3Limited = {
	std::make_pair(k75pcWhite, 240),
	std::make_pair(kNeg7pcStep, 206),
	std::make_pair(k0pcStep, 103),
	std::make_pair(k10pcStep, 103),
	std::make_pair(k20pcStep, 103),
	std::make_pair(k30pcStep, 103),
	std::make_pair(k40pcStep, 102),
	std::make_pair(k50pcStep, 102),
	std::make_pair(k60pcStep, 103),
	std::make_pair(k70pcStep, 103),
	std::make_pair(k80pcStep, 103),
	std::make_pair(k90pcStep, 103),
	std::make_pair(k100pcStep, 103),
	std::make_pair(k109pcStep, 103),
	std::make_pair(k75pcWhite, 240),
};
static const ColorBarsPattern kColorBarsPattern3Full = {
	std::make_pair(k75pcWhite, 240),
	std::make_pair(k0pcStep, 309),
	std::make_pair(k10pcStep, 103),
	std::make_pair(k20pcStep, 103),
	std::make_pair(k30pcStep, 103),
	std::make_pair(k40pcStep, 102),
	std::make_pair(k50pcStep, 102),
	std::make_pair(k60pcStep, 103),
	std::make_pair(k70pcStep, 103),
	std::make_pair(k80pcStep, 103),
	std::make_pair(k90pcStep, 103),
	std::make_pair(k100pcStep, 206),
	std::make_pair(k75pcWhite, 240),
};

// Pattern 4 - Ramp pattern - Describe as Color and HD-width points
static const ColorBarsPattern kColorBarsPattern4Full = {
	std::make_pair(k0pcBlack, 0),
	std::make_pair(k0pcBlack, 790),
	std::make_pair(k100pcWhite, 1813),
	std::make_pair(k100pcWhite, 1919),
};

static const ColorBarsPattern kColorBarsPattern4Limited = {
	std::make_pair(k0pcBlack, 0),
	std::make_pair(k0pcBlack, 239),
	std::make_pair(kNeg7pcStep, 240),
	std::make_pair(kNeg7pcStep, 798),
	std::make_pair(k109pcStep, 1813),
	std::make_pair(k109pcStep, 1919),
};

// Pattern 5 - BT.709 + Black signal - Color and HD-width
static const ColorBarsPattern kColorBarsPattern5Limited = {
	std::make_pair(k75pcBT709Yellow, 80),
	std::make_pair(k75pcBT709Cyan, 80),
	std::make_pair(k75pcBT709Green, 80),
	std::make_pair(k0pcBlack, 136),
	std::make_pair(kNeg2pcBlack, 70),
	std::make_pair(k0pcBlack, 68),
	std::make_pair(kPlus2pcBlack, 70),
	std::make_pair(k0pcBlack, 68),
	std::make_pair(kPlus4pcBlack, 70),
	std::make_pair(k0pcBlack, 238),
	std::make_pair(k75pcWhite, 438),
	std::make_pair(k0pcBlack, 282),
	std::make_pair(k75pcBT709Magenta, 80),
	std::make_pair(k75pcBT709Red, 80),
	std::make_pair(k75pcBT709Blue, 80),
};
static const ColorBarsPattern kColorBarsPattern5Full = {
	std::make_pair(k75pcBT709Yellow, 80),
	std::make_pair(k75pcBT709Cyan, 80),
	std::make_pair(k75pcBT709Green, 80),
	std::make_pair(k0pcBlack, 274),
	std::make_pair(kPlus2pcBlack, 70),
	std::make_pair(k0pcBlack, 68),
	std::make_pair(kPlus4pcBlack, 70),
	std::make_pair(k0pcBlack, 238),
	std::make_pair(k75pcWhite, 438),
	std::make_pair(k0pcBlack, 282),
	std::make_pair(k75pcBT709Magenta, 80),
	std::make_pair(k75pcBT709Red, 80),
	std::make_pair(k75pcBT709Blue, 80),
};

// Color bar patterns, heights:
enum { kColorBarsPatternFillFunction = 0, kColorBarsPatternHeight };
static const std::vector<std::tuple<std::function<void(EOTFColorRange, std::vector<Color12BitRGB>&)>, uint32_t>> kColorBarPatternsNarrow = {
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern1, std::placeholders::_1, std::placeholders::_2), 90),
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern2, std::placeholders::_1, std::placeholders::_2), 540),
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern3Limited, std::placeholders::_1, std::placeholders::_2), 90),
	std::make_tuple(std::bind(FillLineRamp, kColorBarsPattern4Limited, std::placeholders::_1, std::placeholders::_2), 90),
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern5Limited, std::placeholders::_1, std::placeholders::_2), 270),
};
static const std::vector<std::tuple<std::function<void(EOTFColorRange, std::vector<Color12BitRGB>&)>, uint32_t>> kColorBarPatternsFull = {
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern1, std::placeholders::_1, std::placeholders::_2), 90),
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern2, std::placeholders::_1, std::placeholders::_2), 540),
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern3Full, std::placeholders::_1, std::placeholders::_2), 90),
	std::make_tuple(std::bind(FillLineRamp, kColorBarsPattern4Full, std::placeholders::_1, std::placeholders::_2), 90),
	std::make_tuple(std::bind(FillLineBars, kColorBarsPattern5Full, std::placeholders::_1, std::placeholders::_2), 270),
};

static const uint32_t kHD1080Width	= 1920;
static const uint32_t kHD1080Height	= 1080;

void FillBT2111ColorBars(com_ptr<IDeckLinkMutableVideoFrame>& colorBarsFrame, EOTFColorRange range)
{
	uint32_t*		nextWord;
	unsigned long	width;
	unsigned long	height;
	unsigned long	rowBytes;
	std::vector<Color12BitRGB> colorBarsLine;

	colorBarsFrame->GetBytes((void**)&nextWord);
	width = colorBarsFrame->GetWidth();
	height = colorBarsFrame->GetHeight();
	rowBytes = colorBarsFrame->GetRowBytes();

	colorBarsLine.reserve(width);

	for (auto& iter : kColorBarPatternsNarrow)
	{
		uint32_t* refLine = nextWord;

		// Scale pattern for UHD frame height
		uint32_t patternHeight = std::get<kColorBarsPatternHeight>(iter) * (height / kHD1080Height);

		// If 2K/4K/8K DCI mode, then pad with 40% grey bars
		unsigned padWidth = (width % kHD1080Width) / 2;
		
		for (unsigned i = 0; i < padWidth; i++)
			colorBarsLine.push_back(k40pcGrey[(int)range]);

		std::get<kColorBarsPatternFillFunction>(iter)(range, colorBarsLine);
		
		for (unsigned i = 0; i < padWidth; i++)
			colorBarsLine.push_back(k40pcGrey[(int)range]);

		for (unsigned j = 0; j < patternHeight; j++)
		{
			uint8_t* lineStart = (uint8_t*)nextWord;

			if (j == 0)
			{
				if (range == EOTFColorRange::PQFullRange)
				{
					// Write out data in full-range 12-bit RGB
					for (unsigned i = 0; i < width; i += 8)
					{
						// Refer to DeckLink SDK Manual, section 2.7.4 for packing structure
						*nextWord++ = ((colorBarsLine[i].Blue & 0x0FF) << 24) | ((colorBarsLine[i].Green & 0xFFF) << 12) | (colorBarsLine[i].Red & 0xFFF);
						*nextWord++ = ((colorBarsLine[i + 1].Blue & 0x00F) << 28) | ((colorBarsLine[i + 1].Green & 0xFFF) << 16) | ((colorBarsLine[i + 1].Red & 0xFFF) << 4) | ((colorBarsLine[i].Blue & 0xF00) >> 8);
						*nextWord++ = ((colorBarsLine[i + 2].Green & 0xFFF) << 20) | ((colorBarsLine[i + 2].Red & 0xFFF) << 8) | ((colorBarsLine[i + 1].Blue & 0xFF0) >> 4);
						*nextWord++ = ((colorBarsLine[i + 3].Green & 0x0FF) << 24) | ((colorBarsLine[i + 3].Red & 0xFFF) << 12) | (colorBarsLine[i + 2].Blue & 0xFFF);
						*nextWord++ = ((colorBarsLine[i + 4].Green & 0x00F) << 28) | ((colorBarsLine[i + 4].Red & 0xFFF) << 16) | ((colorBarsLine[i + 3].Blue & 0xFFF) << 4) | ((colorBarsLine[i + 3].Green & 0xF00) >> 8);
						*nextWord++ = ((colorBarsLine[i + 5].Red & 0xFFF) << 20) | ((colorBarsLine[i + 4].Blue & 0xFFF) << 8) | ((colorBarsLine[i + 4].Green & 0xFF0) >> 4);
						*nextWord++ = ((colorBarsLine[i + 6].Red & 0x0FF) << 24) | ((colorBarsLine[i + 5].Blue & 0xFFF) << 12) | (colorBarsLine[i + 5].Green & 0xFFF);
						*nextWord++ = ((colorBarsLine[i + 7].Red & 0x00F) << 28) | ((colorBarsLine[i + 6].Blue & 0xFFF) << 16) | ((colorBarsLine[i + 6].Green & 0xFFF) << 4) | ((colorBarsLine[i + 6].Red & 0xF00) >> 8);
						*nextWord++ = ((colorBarsLine[i + 7].Blue & 0xFFF) << 20) | ((colorBarsLine[i + 7].Green & 0xFFF) << 8) | ((colorBarsLine[i + 7].Red & 0xFF0) >> 4);
					}
				}
				else
				{
					// Write out data with video-range r210
					for (unsigned i = 0; i < width; i++)
					{
						// Refer to DeckLink SDK Manual, section 2.7.4 for packing structure
						*nextWord++ = ((colorBarsLine[i].Blue & 0x3FC) << 22) | ((colorBarsLine[i].Green & 0x0FC) << 16) | ((colorBarsLine[i].Blue & 0xC00) << 6)
										| ((colorBarsLine[i].Red & 0x03C) << 10) | (colorBarsLine[i].Green & 0xF00) | ((colorBarsLine[i].Red & 0xFC0) >> 6);
					}
				}
			}
			else
			{
				memcpy(nextWord, refLine, rowBytes);
			}
			nextWord = (uint32_t*)(lineStart + rowBytes);
		}

		colorBarsLine.clear();
	}
}

void FillLineBars(ColorBarsPattern& pattern, EOTFColorRange colorRange, std::vector<Color12BitRGB>& lineBuffer)
{
	for (auto& iter : pattern)
	{
		// Column widths are based on HD, scale for 4K/8K
		uint32_t barWidth = iter.second * (lineBuffer.capacity() / kHD1080Width);

		for (unsigned i = 0; i < barWidth; i++)
		{
			lineBuffer.push_back(iter.first[(int)colorRange]);
		}

	}
}

void FillLineRamp(ColorBarsPattern& pattern, EOTFColorRange colorRange, std::vector<Color12BitRGB>& lineBuffer)
{
	Color12BitRGB	refColor = k0pcBlack[(int)colorRange];
	uint32_t		refColumn = 0;

	for (auto& iter : pattern)
	{
		if (iter.second == 0)
		{
			// Store reference color
			refColor = iter.first[(int)colorRange];
			lineBuffer.push_back(refColor);
			refColumn = 0;
		}
		else 
		{
			// Column widths are based on HD, scale for 4K/8K
			uint32_t endColumn = (iter.second + 1) * (lineBuffer.capacity() / kHD1080Width) - 1;

			if (endColumn > refColumn)
			{
				Color12BitRGB endColor = iter.first[(int)colorRange];
				for (unsigned i = refColumn + 1; i <= endColumn; i++)
				{
					// Interpolate ramp color
					Color12BitRGB rampColor;
					rampColor.Red = refColor.Red + (i - refColumn) * (endColor.Red - refColor.Red) / (endColumn - refColumn);
					rampColor.Green = refColor.Green + (i - refColumn) * (endColor.Green - refColor.Green) / (endColumn - refColumn);
					rampColor.Blue = refColor.Blue + (i - refColumn) * (endColor.Blue - refColor.Blue) / (endColumn - refColumn);
					lineBuffer.push_back(rampColor);
				}

				refColor = endColor;
				refColumn = endColumn;
			}
		}
	}
}
