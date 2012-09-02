#pragma once
#include <bzsnet/server.h>
#include <bzscore/status.h>
#include <bzsnet/BufferedSocket.h>
#include "IGDBStub.h"
#include "BreakInSocket.h"

namespace GDBServerFoundation
{
	//! Implements a TCP/IP server handling the gdbserver protocol
	/*!	The common use case for this class is to create the stub factory (implementing the IGDBStubFactory interface) and start listening for incoming connections:
		\code
		int main()
		{
			int kTCPPort = 2000;
			GDBServer srv(new MyStubFactory());
			srv.Start(kTCPPort);
			srv.WaitForTermination();

			return 0;
		}		
		\endcode
	*/
	class GDBServer : private BazisLib::Network::BasicTCPServer
	{
	private:
		virtual void ConnectionHandler(BazisLib::Network::TCPSocket &socket, const BazisLib::Network::InternetAddress &addr) override;

	private:
		IGDBStubFactory *m_pFactory;
		bool m_bOwnFactory;

	private:
		//! Reads the socket until the start-of-packet symbol ('$') is encountered. Returns false if the connection has been dropped.
		bool FindPacketStart(BreakInSocket::SocketWrapper &socket, bool expectingACK, IBreakInTarget *pTarget);

		void HandleGDBPacketAndSendReply(IGDBStub *pStub, const char *pPacketBody, size_t packetBodyLength, BreakInSocket &socket, bool *ackEnabled);

		static size_t UnescapePacket(const void *pPacket, size_t escapedSize, void *pTarget);

		//! Disables the +/- packet acknowledgment.
		StubResponse QStartNoAckMode(bool *ackEnabled);

	public:
		//! Creates a new instance of the GDB Server
		/*!
			\param pFactory Specifies the factory object that creates instances of GDBStub for the incoming connections.
			\param own If true, the pFactory will be depeted when the GDBServer is destroyed.
		*/
		GDBServer(IGDBStubFactory *pFactory, bool own = true)
			: m_pFactory(pFactory)
			, m_bOwnFactory(own)
		{
		}

		//! Starts listening for incoming connections
		BazisLib::ActionStatus Start(unsigned port);

		//! Waits till the server is stopped by calling StopListening() and the last connection is closed.
		void WaitForTermination()
		{
			return BasicTCPServer::WaitForTermination();
		}

		//! Stops listening for new incoming connections. The existing connections are not affected.
		void StopListening()
		{
			return Stop(false);
		}

	protected:
		void OnPacketError(const BazisLib::String &msg)
		{
			if (m_pFactory)
				m_pFactory->OnProtocolError(msg.c_str());
		}
	};
}