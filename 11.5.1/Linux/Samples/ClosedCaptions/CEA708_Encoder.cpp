/* -LICENSE-START-
 ** Copyright (c) 2016 Blackmagic Design
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

#include "CEA708_Encoder.h"
#include <cstring>

namespace CEA708
{

// SMPTE 334-2 Table 3 - CDP Frame Rate
// CEA-708 4.4.1 Captioning Data Semantics
static uint8_t FrameRateToCDPCCCount(int64_t frameDuration, int64_t timeScale)
{
	static const unsigned kCDPPayloadRate = 600;	// 9600bps / 16 bits per cc_data
	double cc_count = kCDPPayloadRate / (static_cast<double>(timeScale) / frameDuration);
	return static_cast<uint8_t>(cc_count);
}
static CDPFrameRate FrameRateToCDPFrameRate(int64_t frameDuration, int64_t timeScale)
{
	double fps = static_cast<double>(timeScale)/frameDuration;
	switch (static_cast<unsigned>(fps * 100.0))
	{
		case 2397:
			return cdpFrameRate_2397;
		case 2400:
			return cdpFrameRate_24;
		case 2500:
			return cdpFrameRate_25;
		case 2997:
			return cdpFrameRate_2997;
		case 3000:
			return cdpFrameRate_30;
		case 5000:
			return cdpFrameRate_50;
		case 5994:
			return cdpFrameRate_5994;
		case 6000:
			return cdpFrameRate_60;
	}
	
	return cdpFrameRate_Forbidden;
}

//=====================================================================

ServiceBlockEncoder::ServiceBlockEncoder(CaptionChannelPacketEncoder& packetEncoder, uint8_t serviceNumber)
: m_packetEncoder(packetEncoder), m_serviceNumber(serviceNumber)
{
	reset();
}

void ServiceBlockEncoder::reset()
{
	std::memset(m_block, 0, sizeof(m_block));
	m_blockSize = 0;
}

void ServiceBlockEncoder::updateHeader()
{
	if (m_blockSize == 0)
		m_block[0] = 0;	// NULL Service Block Header
	else
		m_block[0] = (m_serviceNumber << 5) | (m_blockSize & 0x1F);
}

void ServiceBlockEncoder::push(const uint8_t* buffer, uint8_t count)
{
	if (count > kMaximumData)
		return;	// Ignore oversize indivisible unit
	
	if (m_blockSize + count > kMaximumData)
	{
		updateHeader();
		m_packetEncoder.push(m_block, kHeaderSize + m_blockSize);
		reset();
	}
	
	std::memcpy(m_block + kHeaderSize + m_blockSize, buffer, count);
	m_blockSize += count;
}

void ServiceBlockEncoder::flush()
{
	updateHeader();
	m_packetEncoder.push(m_block, kHeaderSize + m_blockSize);
	reset();
	
	m_packetEncoder.flush();
}

//=====================================================================

CaptionChannelPacketEncoder::CaptionChannelPacketEncoder(CaptionDistributionPacketEncoder& cdpEncoder)
: m_CDPEncoder(cdpEncoder), m_sequence(0)
{
	reset();
}

void CaptionChannelPacketEncoder::reset()
{
	std::memset(m_packet, 0, sizeof(m_packet));
	m_packetSize = 0;
}

uint8_t CaptionChannelPacketEncoder::paddedSize()
{
	uint8_t fullPacketSize = kHeaderSize + m_packetSize;
	if (fullPacketSize % 2 == 0)
		return fullPacketSize;
	else
		return fullPacketSize + 1;
}

void CaptionChannelPacketEncoder::updateHeader()
{
	uint8_t paddedPacketSize = paddedSize();
	uint8_t sizeCode = paddedPacketSize == 128 ? 0 : paddedPacketSize / 2;
	
	m_packet[0] |= (m_sequence & 0x3) << 6;
	m_packet[0] |= sizeCode & 0x3F;

	++m_sequence;
	m_sequence %= 4;
}

void CaptionChannelPacketEncoder::push(const uint8_t* block, uint8_t blockLength)
{
	/* As service block data and caption packets cannot be fragmented,
	   the maximum data which can be packed depends on the cc_count of the 
	   cdp packet into which this caption packet will be encoded. */
	std::size_t maxDataLength = std::min(m_CDPEncoder.maxPayloadSize(), static_cast<std::size_t>(kMaximumData));
	
	if (blockLength > maxDataLength)
		return;	// service block cannot be larger than caption channel packet.
	
	if (m_packetSize + blockLength > maxDataLength)
	{
		updateHeader();
		m_CDPEncoder.push(m_packet, paddedSize());
		reset();
	}
	
	std::memcpy(m_packet + kHeaderSize + m_packetSize, block, blockLength);
	m_packetSize += blockLength;
}

// update header, push() && flush()
void CaptionChannelPacketEncoder::flush()
{
	updateHeader();
	m_CDPEncoder.push(m_packet, paddedSize());
	reset();
	
	m_CDPEncoder.flush();
}

//=====================================================================

CaptionDistributionPacketEncoder::CaptionDistributionPacketEncoder(CDPQueue& cdpQueue, int64_t frameDuration, int64_t timeScale)
: m_cdpQueue(cdpQueue), m_sequence(0), m_CCCount(FrameRateToCDPCCCount(frameDuration, timeScale)), m_frameRate(FrameRateToCDPFrameRate(frameDuration, timeScale))
{

}

void CaptionDistributionPacketEncoder::encode_ccdata(uint8_t*& buffer)
{
	static const uint8_t CCDATA_ID = 0x72;
	
	enum cc_type
	{
		cc_type_608_1 = 0,
		cc_type_608_2,
		cc_type_708_data,
		cc_type_708_start
	};
	
	unsigned payloadPackets = static_cast<unsigned>(m_payload.size() / 2);
	unsigned padPackets = m_CCCount - payloadPackets;
	
	if (payloadPackets > m_CCCount)
		return;
	
	uint8_t* ccdata_header = buffer;
	ccdata_header[0] = CCDATA_ID;
	ccdata_header[1] |= 0x7 << 5;		// marker
	ccdata_header[1] |= m_CCCount & 0x1F;
	buffer += 2;
	
	uint8_t* cc_data_x = m_payload.data();
	for (unsigned i = 0; i < payloadPackets; ++i)
	{
		uint8_t* ccdata = buffer;
		ccdata[0] = 0x1F << 3;			// marker
		ccdata[0] |= (1 << 2);			// cc_valid
		ccdata[0] |= i == 0 ? cc_type_708_start : cc_type_708_data;
		ccdata[1] = *cc_data_x++;
		ccdata[2] = *cc_data_x++;
		buffer += 3;
	}
	for (unsigned i = 0; i < padPackets; ++i)
	{
		uint8_t* ccdata = buffer;
		ccdata[0] = 0x1F << 3;			// marker
		ccdata[0] |= cc_type_708_data;	// cc_valid == 0
		ccdata[1] = 0;
		ccdata[2] = 0;
		buffer += 3;
	}
}

void CaptionDistributionPacketEncoder::encode_svcinfo(uint8_t*& buffer)
{
	static const uint8_t  CDP_SERVICE_INFO_ID = 0x73;

	enum
	{
		kServiceInfoHeaderLength = 2,
		kServiceDataLength = 7
	};
	
	// Required as per CEA-708 4.5 Caption Service Metadata
	uint8_t* svcinfo_header = buffer;
	svcinfo_header[0] = CDP_SERVICE_INFO_ID;
	svcinfo_header[1] = 0x80;			// reserved
	svcinfo_header[1] |= 1 << 4;		// svc_info_complete
	svcinfo_header[1] |= 1;				// svc_count
	buffer += kServiceInfoHeaderLength;
	
	// ATSC A/65 Table 6.26
	uint8_t* svcinfo = buffer;
	svcinfo[0] = 0x80;					// reserved | csn_size == 0
	svcinfo[0] |= (serviceNumber_PrimaryCaptionService & 0x3F);

	// ISO 639-2 language code
	svcinfo[1] = 'e';
	svcinfo[2] = 'n';
	svcinfo[3] = 'g';

	svcinfo[4] = (1 << 7);				// digital_cc
	svcinfo[4] |= serviceNumber_PrimaryCaptionService;
	svcinfo[5] = 0x7F;					// !easy_reader, 16:9 aspect ratio, 6 reserved bits
	svcinfo[6] = 0xFF;					// reserved
	buffer += kServiceDataLength;
}

EncodedCaptionDistributionPacket CaptionDistributionPacketEncoder::encode()
{
	static const uint16_t	CDP_IDENTIFIER = 0x9669;
	static const uint8_t	CDP_FOOTER_ID = 0x74;
	
	enum
	{
		kCDPHeaderLength = 7,
		kServiceInfoLength = 9,
		kCDPFooterLength = 4
	};
	
	if (m_frameRate == cdpFrameRate_Forbidden)
		return EncodedCaptionDistributionPacket();
	
	uint8_t cc_data_length = 2 + 3 * m_CCCount;
	uint8_t cdp_length = kCDPHeaderLength + cc_data_length + kServiceInfoLength + kCDPFooterLength;
	
	EncodedCaptionDistributionPacket encoded;
	encoded.resize(cdp_length);
	uint8_t* buffer = encoded.data();
	
	enum cdp_flags
	{
		time_code_present = 1 << 7,
		ccdata_present = 1 << 6,
		svcinfo_present = 1 << 5,
		svc_info_start = 1 << 4,
		svc_info_change = 1 << 3,
		svc_info_complete = 1 << 2,
		caption_service_active = 1 << 1
	};
	
	uint8_t* cdp_header = buffer;
	cdp_header[0] = (CDP_IDENTIFIER & 0xFF00) >> 8;
	cdp_header[1] = (CDP_IDENTIFIER & 0x00FF);
	cdp_header[2] = cdp_length;
	cdp_header[3] |= m_frameRate << 4;
	cdp_header[3] |= 0x0F;		// reserved
	cdp_header[4] |= ccdata_present | caption_service_active | svcinfo_present | svc_info_complete;
	cdp_header[4] |= 1 << 0;	// reserved
	cdp_header[5] = (m_sequence & 0xFF00) >> 8;
	cdp_header[6] = (m_sequence & 0x00FF);
	buffer += kCDPHeaderLength;
	
	encode_ccdata(buffer);
	encode_svcinfo(buffer);
	
	uint8_t* cdp_footer = buffer;
	cdp_footer[0] = CDP_FOOTER_ID;
	cdp_footer[1] = (m_sequence & 0xFF00) >> 8;
	cdp_footer[2] = (m_sequence & 0x00FF);
	cdp_footer[3] = 0 /* checksum filled below */;
	
	for (unsigned i = 0; i < encoded.size()-1; ++i)
		cdp_footer[3] += encoded[i];
	cdp_footer[3] = cdp_footer[3] ? 256 - cdp_footer[3] : 0;
	
	buffer += kCDPFooterLength;
	
	++m_sequence;
	
	return encoded;
}

void CaptionDistributionPacketEncoder::reset()
{
	m_payload.clear();
}

void CaptionDistributionPacketEncoder::push(const uint8_t* packet, uint8_t packetLength)
{
	if (m_payload.size() + packetLength > maxPayloadSize())
	{
		EncodedCaptionDistributionPacket packet = encode();
		m_cdpQueue.push(packet);
		reset();
	}
	
	m_payload.insert(m_payload.end(), packet, packet + packetLength);
}

void CaptionDistributionPacketEncoder::flush()
{
	EncodedCaptionDistributionPacket packet = encode();
	m_cdpQueue.push(packet);
	reset();
}

//=====================================================================

Encoder::Encoder(int64_t frameDuration, int64_t timeScale)
: m_cdpQueue(), m_cdpEncoder(m_cdpQueue, frameDuration, timeScale), m_packetEncoder(m_cdpEncoder), m_serviceBlockEncoder(m_packetEncoder, serviceNumber_PrimaryCaptionService)
{
}

Encoder& Encoder::operator<<(const SyntacticElement& command)
{
	m_serviceBlockEncoder.push(command.data(), command.size());
	return *this;
}
Encoder& Encoder::operator<<(const char* captionText)
{
	size_t len = strlen(captionText);
	for (size_t i = 0; i < len; ++i)
	{
		// captionText can only consist of characters in CEA-708's code space (7.1 Code Space Organization), which incorporates
		// at least ASCII. For other locales an extended encoding scheme may be requried. How to achieve this is beyond the
		// scope of this sample.
		const uint8_t ch = captionText[i];
		m_serviceBlockEncoder.push(&ch, 1);
	}
	return *this;
}

bool Encoder::empty() const
{
	return m_cdpQueue.empty();
}

bool Encoder::pop(EncodedCaptionDistributionPacket* packet)
{
	if (!m_cdpQueue.empty())
	{
		*packet = m_cdpQueue.front();
		m_cdpQueue.pop();
		return true;
	}
	return false;
}

void Encoder::flush()
{
	m_serviceBlockEncoder.flush();
}

}