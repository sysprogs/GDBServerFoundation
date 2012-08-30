#pragma once
#include "IGDBStub.h"
#include <map>
#include <string>
#include "IGDBTarget.h"

namespace GDBServerFoundation
{
	class BasicGDBStub : public IGDBStub
	{
	private:
		//! Contains features reported by GDB. Each feature is split into a key/value pair either by looking for '=', or checking if the last character is '+', '-' or '?'
		std::map<std::string, std::string> m_GDBFeatures;

		//! Contains featuers supported by the stub. E.g. [{PacketSize, 65536},{multiprocess,-}]
		std::map<std::string, std::string> m_StubFeatures;

	protected:
		class ThreadIDStore
		{
			int m_IDs[256];

		public:
			ThreadIDStore()
			{
				memset(&m_IDs, 0, sizeof(m_IDs));
			}

			int &operator[](unsigned char op)
			{
				return m_IDs[op];
			}
		};

		//! Stores the data received from the 'H op thread-id' packet
		ThreadIDStore m_ThreadIDsForOps;
		//Should be updated from the code returning stop records
		int m_LastReportedCurrentThreadID;

	public:
		virtual StubResponse HandleRequest(const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData);

		BasicGDBStub();

	protected:
		//! Stores features supported by GDB and reports features supported by the stub
		virtual StubResponse Handle_qSupported(const BazisLib::TempStringA &requestData);

		//! Sets thread ID for subsequent thread-related commands
		virtual StubResponse Handle_H(const BazisLib::TempStringA &requestType);

		virtual StubResponse Handle_QueryStopReason()=0;
		
		//! Returns the values of all target registers in one block
		virtual StubResponse Handle_g(int threadID)=0;

		//! Sets the value of all target registers
		virtual StubResponse Handle_G(int threadID, const BazisLib::TempStringA &registerValueBlock)=0;

		//! Sets the value of exactly one register
		virtual StubResponse Handle_P(int threadID, const BazisLib::TempStringA &registerIndex, const BazisLib::TempStringA &registerValue)=0;

		//! Reads target memory
		virtual StubResponse Handle_m(const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length)=0;

		//! Writes target memory
		virtual StubResponse Handle_M(const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length, const BazisLib::TempStringA &data)=0;

		//! Writes target memory, data is transmitted in binary format
		virtual StubResponse Handle_X(const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length, const BazisLib::TempStringA &binaryData)=0;

		//! Continue executing selected thread
		virtual StubResponse Handle_c(int threadID)=0;

		//! Single-step selected thread
		virtual StubResponse Handle_s(int threadID)=0;

		//!Return a list of all thread IDs
		virtual StubResponse Handle_qfThreadInfo()=0;
		virtual StubResponse Handle_qsThreadInfo()=0;

		//!Return user-friendly thread description
		virtual StubResponse Handle_qThreadExtraInfo(const BazisLib::TempStringA &strThreadID)=0;

		//!Check whether the specified thread is alive
		virtual StubResponse Handle_T(const BazisLib::TempStringA &strThreadID)=0;

		//! Returns the current thread ID
		virtual StubResponse Handle_qC()=0;


	protected:
		StubResponse StopRecordToStopReply(const TargetStopRecord &rec, const char *pReportedRegisterValues = NULL, bool updateLastReportedThreadID = true);
		int GetThreadIDForOp(unsigned char op);
		
		void AppendRegisterValueToString(const RegisterValue &val, size_t sizeInBytes, BazisLib::DynamicStringA &str, const char *pSuffix = NULL);	

		StubResponse FormatGDBStatus(GDBStatus status);

		void RegisterStubFeature(const char *pFeature) {m_StubFeatures[pFeature] = "+";}
	};
}