#pragma once
#include <bzsnet/BufferedSocket.h>
#include <bzscore/sync.h>
#include <bzscore/thread.h>

namespace GDBServerFoundation
{
	//! Receives asynchronous break-in requests from a BreakInSocket
	class IBreakInTarget
	{
	public:
		virtual void OnBreakInRequest()=0;
	};

	//! Encapsulates a socket with asynchronous break-in support
	/*! This class should be used to receive packets from GDB. The main packet handling loop should look this way:
		1. Create an instance of BreakInSocket::SocketWrapper
		2. Use SocketWrapper to get read the packet. If the first byte received from the socket is 0x03, raise the break-in event.
		3. If TCPSocketEx::Peek() was called, call TCPSocketEx::Discard()
		4. Delete the BreakInSocket::SocketWrapper instance
		5. Process the packet, send reply, etc.

		The BreakInSocket class ensures that if a break-in request (0x03 byte) arrives while the packet is being processed 
		(i.e. BreakInSocket::SocketWrapper not existing), a IBreakInTarget::OnBreakInRequest() will be called from a worker thread.

		This allows the packet handlers to run blocking requests (e.g. 'continue') and still being able to react to the asynchronous
		break-in requests coming from GDB.

		\remarks When an instance of BreakInSocket::SocketWrapper is active, the worker thread is suspended and does not interfere
				 with the socket. As soon as the BreakInSocket::SocketWrapper instance is deleted, the worker thread starts monitoring
				 the socket. If it receives anything except the 0x03 byte (i.e. start of a packet), it suspends itself until the packet
				 is handled (i.e. an instance of BreakInSocket::SocketWrapper is created and deleted).

	*/
	class BreakInSocket
	{
	public:
		enum {kBreakInByte = 0x03};

	private:
		BazisLib::Network::TCPSocketEx *m_pSocket;

	private:
		BazisLib::MemberThread m_WorkerThread;
		BazisLib::Mutex m_RecvMutex;
		BazisLib::Semaphore m_Semaphore;
		bool m_bTerminating;

		IBreakInTarget *m_pTarget;

	private:
		int WorkerThreadBody()
		{
			BazisLib::MutexLocker lck(m_RecvMutex);
			while (!m_bTerminating)
			{
				size_t total = 0;
				void *pData = m_pSocket->PeekAbs(1, &total);
				if (!pData || !total || *((char *)pData) == kBreakInByte)
				{
					if (total)
						m_pSocket->Discard(pData, 1);

					IBreakInTarget *pTarget = m_pTarget;
					if (pTarget)
					{
						m_RecvMutex.Unlock();
						pTarget->OnBreakInRequest();
						m_RecvMutex.Lock();
					}

					continue;
				}

				m_RecvMutex.Unlock();
				m_Semaphore.Wait();
				m_RecvMutex.Lock();
			}
			return 0;
		}

	public:
		BreakInSocket(BazisLib::Network::TCPSocketEx *pSock)
			: m_pSocket(pSock)
			, m_WorkerThread(this, &BreakInSocket::WorkerThreadBody)
			, m_bTerminating(false)
			, m_pTarget(NULL)
		{
			m_WorkerThread.Start();
		}

		~BreakInSocket()
		{
			m_bTerminating = true;
			m_Semaphore.Signal();
			m_WorkerThread.Join();
		}

		size_t Send(const void *pBuffer, size_t size)
		{
			return m_pSocket->Send(pBuffer, size);
		}

		void SetTarget(IBreakInTarget *pTarget)
		{
			m_pTarget = pTarget;
		}

		//! An instance of this class should be obtained and held for the entire time when a packet is received. After it is destroyed, the break-in detector thread becomes active again.
		class SocketWrapper
		{
		private:
			BreakInSocket &m_Socket;

		public:
			SocketWrapper(BreakInSocket &sock)
				: m_Socket(sock)
			{
				m_Socket.m_RecvMutex.Lock();
			}

			BazisLib::Network::TCPSocketEx *operator->()
			{
				return m_Socket.m_pSocket;
			}

			~SocketWrapper()
			{
				m_Socket.m_RecvMutex.Unlock();
				m_Socket.m_Semaphore.Signal();
			}
		};
	};
}