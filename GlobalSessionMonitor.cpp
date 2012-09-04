#include "stdafx.h"
#include "GlobalSessionMonitor.h"

using namespace BazisLib;
using namespace GDBServerFoundation;

static GlobalSessionMonitor *s_pMainMonitor = NULL;

bool GDBServerFoundation::GlobalSessionMonitor::RegisterSession( ISyncGDBTarget *pTarget )
{
	MutexLocker lck(m_Mutex);
	if (pSession)
		return false;
	pSession = pTarget;
	return true;
}

void GDBServerFoundation::GlobalSessionMonitor::UnregisterSession( ISyncGDBTarget *pTarget )
{
	MutexLocker lck(m_Mutex);
	ASSERT(pSession == pTarget);
	pSession = NULL;
}

GDBServerFoundation::GlobalSessionMonitor::GlobalSessionMonitor()
	: pSession(NULL)
{
	SetConsoleCtrlHandler(CtrlHandler, TRUE);
	s_pMainMonitor = this;
}

BOOL CALLBACK GDBServerFoundation::GlobalSessionMonitor::CtrlHandler( DWORD dwType )
{
	if (dwType == CTRL_C_EVENT)
	{
		MutexLocker lck(s_pMainMonitor->m_Mutex);
		if (s_pMainMonitor->pSession)
			s_pMainMonitor->pSession->SendBreakInRequestAsync();
		else
			printf("No active session. To stop listening, press Ctrl+Break.\n");
		return TRUE;
	}
	if (dwType == CTRL_BREAK_EVENT|| dwType == CTRL_CLOSE_EVENT)
	{
		MutexLocker lck(s_pMainMonitor->m_Mutex);
		if (s_pMainMonitor->pSession)
			s_pMainMonitor->pSession->CloseSessionSafely();
		else
			printf("No active session. To stop listening, press Ctrl+Break.\n");
		return FALSE;
	}
	return FALSE;
}

GDBServerFoundation::GlobalSessionMonitor::~GlobalSessionMonitor()
{
	ASSERT(s_pMainMonitor == this);
	s_pMainMonitor = NULL;
}
