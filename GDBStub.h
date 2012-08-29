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

	public:
		GDBStub(ISyncGDBTarget *pTarget, bool own = true)
		{
			m_pTarget = pTarget;
			m_bOwnStub = own;

			m_pRegisters = pTarget->GetRegisterList();
			
			RegisterStubFeature("qXfer:libraries:read");
		}

		~GDBStub()
		{
			if (m_bOwnStub)
				delete m_pTarget;
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

	protected:
		virtual BazisLib::DynamicStringA BuildGDBReportByName(const BazisLib::TempStringA &name, const BazisLib::TempStringA &annex);

	protected:
		TargetRegisterValues InitializeTargetRegisterContainer();
	};
}