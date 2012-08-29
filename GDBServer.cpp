#include "StdAfx.h"
#include "GDBServer.h"
#include "HexHelpers.h"

using namespace BazisLib;
using namespace BazisLib::Network;
using namespace GDBServerFoundation::HexHelpers;

static const char kACK = '+';
static const char kPacketStart = '$';
static const char kPacketEnd = '#';
static const char kEscapeChar = '}';

static const char kRLEMarker = '*';
static const char kEscapeMask = 0x20;
static const char kRLEBase = 29;


GDBServerFoundation::StubResponse GDBServerFoundation::StandardResponses::CommandNotSupported("");
GDBServerFoundation::StubResponse GDBServerFoundation::StandardResponses::InvalidArgument("E 16");	//0x16 = 22 == EINVAL
GDBServerFoundation::StubResponse GDBServerFoundation::StandardResponses::OK("OK");

#include <numeric>

static unsigned computeChecksum(void *p, size_t length)
{
	unsigned char *pCh = (unsigned char *)p;
	return std::accumulate(pCh, pCh + length, 0) & 0xFF;
}

void GDBServerFoundation::GDBServer::ConnectionHandler( TCPSocket &socket, const InternetAddress &addr )
{
	enum {kBytesToReceiveAtOnce = 65536};

	TCPSocketEx socketEx(&socket, false);
	bool ackEnabled = true, newAckEnabled = true;

	for (;;)
	{
		//We expect the following format: $<data>#<checksum>
		if (!FindPacketStart(socketEx, ackEnabled))
			break;

		ackEnabled = newAckEnabled;

		//Find end-of-packet symbol ('#') taking the escape symbol ('}') into account.
		size_t available = 0;
		char *pPacket = (char *)socketEx.PeekAbs(3, &available);
		if (!pPacket || !available)
			break;

		size_t searchRestartPosition = 0;
		size_t endOfPacket = -1;

		//Keep on trying until we find the end-of-packet
		for (;;)
		{
			for (size_t i = searchRestartPosition; i < available; i++)
			{
				searchRestartPosition = i;
				if (pPacket[i] == kEscapeChar)
				{
					i++;	//Simply skip the next character
					continue;
				}

				if (pPacket[i] == kPacketEnd)
				{
					endOfPacket = i;
					break;
				}
			}

			if (endOfPacket != -1)
				break;

			size_t newAvail = available;
			pPacket = (char *)socketEx.PeekRel(kBytesToReceiveAtOnce, &newAvail);
			if (!pPacket || newAvail == available)
				break;

			available = newAvail;
		}

		if (endOfPacket == -1)
			break;	//The connection has been closed

		size_t packetWithChecksumLength = endOfPacket + 3;
		if (available < packetWithChecksumLength)
		{
			pPacket = (char *)socketEx.PeekAbs(packetWithChecksumLength, &available);
			if (!pPacket || available < packetWithChecksumLength)
				break;
		}

		unsigned checksum = ParseHexValue(pPacket + endOfPacket + 1);
		unsigned expectedChecksum = computeChecksum(pPacket, endOfPacket);

		if (checksum != expectedChecksum)
		{
			OnPacketError(String::sFormat(_T("Invalid packet checksum. Expected 0x%02X, got 0x%02X"), expectedChecksum, checksum));
			continue;
		}

		size_t unescapedSize = UnescapePacket(pPacket, endOfPacket);
		if (ackEnabled)
		{
			char ch = kACK;
			socketEx.Send(&ch, 1);
		}

		HandleGDBPacketAndSendReply(pPacket, unescapedSize, socketEx, &newAckEnabled);

		socketEx.Discard(pPacket, packetWithChecksumLength);
	}
	socketEx.Close();
}

BazisLib::ActionStatus GDBServerFoundation::GDBServer::Start(unsigned port)
{
	return BasicTCPServer::Start(port);
}

bool GDBServerFoundation::GDBServer::FindPacketStart(TCPSocketEx &socket, bool expectingACK)
{
	for (;;)
	{
		size_t available = 0;
		//1. Find start of packet.
		char *pPacket = (char *)socket.PeekAbs(1, &available);
		if (!pPacket || !available)
			return false;

		if (expectingACK)
		{
			char ackChar = pPacket[0];
			socket.Discard(pPacket, 1);
			if (ackChar != kACK)
			{
				OnPacketError(String::sFormat(_T("Expected ACK ('+'), got 0x%02X (%c)"), ackChar & 0xFF, ackChar));
				continue;
			}
			pPacket = (char *)socket.PeekAbs(1, &available);
			if (!pPacket || !available)
				return false;
		}

		char firstChar = pPacket[0];
		socket.Discard(pPacket, 1);

		if (firstChar != kPacketStart)
		{
			OnPacketError(String::sFormat(_T("Expected start of packet ('$'), got 0x%02X (%c)"), firstChar & 0xFF, firstChar));
			continue;
		}

		return true;
	}
}

size_t GDBServerFoundation::GDBServer::UnescapePacket( void *pPacket, size_t escapedSize )
{
	size_t w = 0;
	char *pCh = (char *)pPacket;

	for (size_t r = 0; r < escapedSize; r++)
	{
		if (pCh[r] == kEscapeChar && r != (escapedSize - 1))
			pCh[w++] = pCh[++r] ^ kEscapeMask;
		else
			pCh[w++] = pCh[r];
	}

	return w;
}

void GDBServerFoundation::GDBServer::HandleGDBPacketAndSendReply( const char *pPacketBody, size_t packetBodyLength, BazisLib::Network::TCPSocketEx &socket, bool *ackEnabled )
{
	if (!m_pStub)
		return;

	static const char splitterChars[] = ";: ";
	size_t splitter = packetBodyLength;
	char splitterChar = 0;
	for (size_t i = 0; i < packetBodyLength; i++)
	{
		if (strchr(splitterChars, pPacketBody[i]))
		{
			splitter = i;
			splitterChar = pPacketBody[i];
			break;
		}
	}

	BazisLib::TempStrPointerWrapperA cmd(pPacketBody, splitter), args(pPacketBody + splitter + 1, (splitter == packetBodyLength) ? 0 : packetBodyLength - splitter - 1);

	bool isStartNoAck = (cmd == "QStartNoAckMode");
	StubResponse response = isStartNoAck ? QStartNoAckMode(ackEnabled) : m_pStub->HandleRequest(cmd, splitterChar, args);

	char internalBuf[4996];
	size_t internalBufSize = 1;

	internalBuf[0] = kPacketStart;

	const char *pReply = response.GetData();

	static const char charsToEscape[] = "#$}*";
	unsigned char checksum = 0;

	for (size_t i = 0; i < response.GetSize(); i++)
	{
		char charToSend = pReply[i];
		size_t runLength = 1;

		size_t remaining = response.GetSize() - i;
		while(runLength < remaining)
			if (pReply[i + runLength] == charToSend)
				runLength++;
			else
				break;		
		
		//Maximum amount of chars appended per cycle - 3 (RLE, format "<char>*<cnt>" where <char> is escaped)
		if (internalBufSize >= (sizeof(internalBuf) - 4))
		{
			socket.Send(internalBuf, internalBufSize);
			internalBufSize = 0;
		}

		if (strchr(charsToEscape, charToSend))
		{
			internalBuf[internalBufSize++] = kEscapeChar;
			internalBuf[internalBufSize++] = charToSend ^ kEscapeMask;
			checksum += kEscapeChar + (charToSend ^ kEscapeMask);
			runLength = 1;	//RLE-encoding escaped characters seems to be unsupported by gdb
		}
		else
		{
			internalBuf[internalBufSize++] = charToSend;
			checksum += charToSend;
		}
		
		if (runLength > 3)
		{
			size_t moreCharacters = runLength - 1;

			if (moreCharacters >= (126 - kRLEBase))
				moreCharacters = (126 - kRLEBase);

			char runLengthChar = kRLEBase + moreCharacters;
			if (runLengthChar == kPacketStart || runLengthChar == kPacketEnd || runLengthChar == kEscapeChar)
				moreCharacters = 0;
			else
			{
				internalBuf[internalBufSize++] = kRLEMarker;
				internalBuf[internalBufSize++] = runLengthChar;

				checksum += kRLEMarker + runLengthChar;

				i += moreCharacters;
			}
		}
	}

	if (internalBufSize >= (sizeof(internalBuf) - 3))
	{
		socket.Send(internalBuf, internalBufSize);
		internalBufSize = 0;
	}

	internalBuf[internalBufSize++] = kPacketEnd;
	internalBuf[internalBufSize++] = hexTable[(checksum >> 4) & 0x0F];
	internalBuf[internalBufSize++] = hexTable[checksum & 0x0F];
	
	socket.Send(internalBuf, internalBufSize);
	internalBufSize = 0;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBServer::QStartNoAckMode(bool *ackEnabled)
{
	ASSERT(ackEnabled);
	ASSERT(*ackEnabled);
	*ackEnabled = false;
	return StandardResponses::OK;
}
