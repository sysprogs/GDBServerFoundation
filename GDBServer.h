#pragma once
#include <bzsnet/server.h>
#include <bzscore/status.h>
#include <bzsnet/BufferedSocket.h>
#include "IGDBStub.h"

namespace GDBServerFoundation
{
	class GDBServer : private BazisLib::Network::BasicTCPServer
	{
	private:
		virtual void ConnectionHandler(BazisLib::Network::TCPSocket &socket, const BazisLib::Network::InternetAddress &addr) override;
		typedef BazisLib::Network::TCPSocketEx TCPSocketEx;

	private:
		IGDBStubFactory *m_pFactory;
		bool m_bOwnFactory;

	private:
		//! Reads the socket until the start-of-packet symbol ('$') is encountered. Returns false if the connection has been dropped.
		bool FindPacketStart(TCPSocketEx &socket, bool expectingACK);

		void HandleGDBPacketAndSendReply(IGDBStub *pStub, const char *pPacketBody, size_t packetBodyLength, BazisLib::Network::TCPSocketEx &socket, bool *ackEnabled);

		static size_t UnescapePacket(void *pPacket, size_t escapedSize);

		//! Disables the +/- packet acknowledgment.
		StubResponse QStartNoAckMode(bool *ackEnabled);

	public:
		GDBServer(IGDBStubFactory *pFactory, bool own = true)
			: m_pFactory(pFactory)
			, m_bOwnFactory(own)
		{
		}

		BazisLib::ActionStatus Start(unsigned port);

	protected:
		void OnPacketError(const BazisLib::String &msg)
		{
			if (m_pFactory)
				m_pFactory->OnProtocolError(msg.c_str());
		}

	};
}