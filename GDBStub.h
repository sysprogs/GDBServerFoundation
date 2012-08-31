#pragma once
#include "BasicGDBStub.h"
#include "IGDBTarget.h"

namespace GDBServerFoundation
{
	class GDBStub : public BasicGDBStub
	{
	private:
		ISyncGDBTarget *m_pTarget;
		bool m_bOwnStub;

		const PlatformRegisterList *m_pRegisters;

		std::vector<ThreadRecord> m_CachedThreadInfo;
		bool m_bThreadCacheValid, m_bThreadsSupported;

	public:
		GDBStub(ISyncGDBTarget *pTarget, bool own = true);

		~GDBStub()
		{
			if (m_bOwnStub)
				delete m_pTarget;
		}

		virtual void OnBreakInRequest()
		{
			if (m_pTarget)
				m_pTarget->SendBreakInRequestAsync();
		}

		virtual StubResponse Handle_QueryStopReason();
		virtual StubResponse Handle_g(int threadID);
		virtual StubResponse Handle_G(int threadID, const BazisLib::TempStringA &registerValueBlock);
		virtual StubResponse Handle_P(int threadID, const BazisLib::TempStringA &registerIndex, const BazisLib::TempStringA &registerValue);
		virtual StubResponse Handle_m(const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length);
		virtual StubResponse Handle_M(const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length, const BazisLib::TempStringA &data);
		virtual StubResponse Handle_X(const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length, const BazisLib::TempStringA &binaryData);

		virtual StubResponse Handle_qXfer(const BazisLib::TempStringA &object, const BazisLib::TempStringA &verb, const BazisLib::TempStringA &annex, const BazisLib::TempStringA &offset, const BazisLib::TempStringA &length);

		virtual StubResponse HandleRequest(const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData);
		virtual StubResponse Handle_c(int threadID);
		virtual StubResponse Handle_s(int threadID);
		virtual StubResponse Handle_qfThreadInfo();
		virtual StubResponse Handle_qsThreadInfo();
		virtual StubResponse Handle_qThreadExtraInfo(const BazisLib::TempStringA &strThreadID);
		virtual StubResponse Handle_T(const BazisLib::TempStringA &strThreadID);
		virtual StubResponse Handle_qC();
		virtual StubResponse Handle_vCont(const BazisLib::TempStringA &arguments);
		virtual StubResponse Handle_k();

	protected:
		virtual BazisLib::DynamicStringA BuildGDBReportByName(const BazisLib::TempStringA &name, const BazisLib::TempStringA &annex);

	protected:
		TargetRegisterValues InitializeTargetRegisterContainer();
		void ResetAllCachesWhenResumingTarget();

	protected:
		void ProvideThreadInfo();
	};
}