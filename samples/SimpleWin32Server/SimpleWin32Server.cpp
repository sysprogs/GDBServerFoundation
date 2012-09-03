/*
	A simple Win32 API-based gdbserver implementation.
	This server can run a Win32 EXE in debug mode and allow debugging it with GDB via the "target remote" command.

	The main functionality of the server is contained in the Win32GDBTarget class implementing the GDBServerFoundation::ISyncGDBTarget interface.
	
	The stub uses the following Win32 API to debug the started process:
		* DebugActiveProcess()
		* WaitForDebugEvent() / ContinueDebugEvent()
		* GetThreadContext() / SetThreadContext()
		* ReadProcessMemory() / WriteProcessMemory()

	To see the server in action, build a simple EXE with debug information using gcc:
		g++ -O0 -ggdb test.cpp -o test.exe
	Then start the server app:
		SimpleWin32Server test.exe
	and finally start debugging it with GDB:
		gdb test.exe
		target remote :2000
		b main
		continue

	The server is very simple, it does not handle all debug events and does not close all handles. It is provided only as an example for the GDBServerFoundation library.
*/

#include "stdafx.h"
#include <stdio.h>
#include "../../GDBServer.h"
#include "../../GDBStub.h"
#include "../../IGDBTarget.h"

#include "registers-i386.h"
#include <TlHelp32.h>

using namespace BazisLib;
using namespace GDBServerFoundation;

bool LaunchDebuggedProcess(LPCTSTR pszProcess, PROCESS_INFORMATION *pProcInfo)
{
	STARTUPINFO startupInfo = {sizeof(STARTUPINFO), };
	TCHAR tsz[MAX_PATH];
	_tcsncpy(tsz, pszProcess, __countof(tsz));

	if (!CreateProcess(NULL, tsz, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startupInfo, pProcInfo))
		return false;

	return true;
}

struct ContextEntry
{
	int RegisterIndex;
	unsigned ContextOffset;
};

static ContextEntry s_ContextRegisterOffsets[] = {
	{i386::rgEAX, FIELD_OFFSET(CONTEXT, Eax)},
	{i386::rgECX, FIELD_OFFSET(CONTEXT, Ecx)},
	{i386::rgEDX, FIELD_OFFSET(CONTEXT, Edx)},
	{i386::rgEBX, FIELD_OFFSET(CONTEXT, Ebx)},
	{i386::rgESP, FIELD_OFFSET(CONTEXT, Esp)},
	{i386::rgEBP, FIELD_OFFSET(CONTEXT, Ebp)},
	{i386::rgESI, FIELD_OFFSET(CONTEXT, Esi)},
	{i386::rgEDI, FIELD_OFFSET(CONTEXT, Edi)},

	{i386::rgEIP, FIELD_OFFSET(CONTEXT, Eip)},
	{i386::rgEFLAGS, FIELD_OFFSET(CONTEXT, EFlags)},

	{i386::rgCS, FIELD_OFFSET(CONTEXT, SegCs)},
	{i386::rgSS, FIELD_OFFSET(CONTEXT, SegSs)},
	{i386::rgDS, FIELD_OFFSET(CONTEXT, SegDs)},
	{i386::rgES, FIELD_OFFSET(CONTEXT, SegEs)},
	{i386::rgFS, FIELD_OFFSET(CONTEXT, SegFs)},
	{i386::rgGS, FIELD_OFFSET(CONTEXT, SegGs)},
};

C_ASSERT(__countof(s_ContextRegisterOffsets) == __countof(i386::_RawRegisterList));

//! Implements the GDBServerFoundation::ISyncGDBTarget interface using Win32 API functions to debug a local process 
class Win32GDBTarget : public MinimalTargetBase
{
private:
	virtual GDBStatus ResumeAndWait(int threadID)
	{
		if (!ContinueDebugEvent(m_DebugEvent.dwProcessId, m_DebugEvent.dwThreadId, DBG_CONTINUE))
			return kGDBUnknownError;
		if (!WaitForDebugEvent())
			return kGDBUnknownError;
		return kGDBSuccess;
	}

	bool EnableSingleStep(int threadID, bool enable)
	{
		CONTEXT context;
		if (!GetContextByThreadID(threadID, &context))
			return false;

		if (enable)
			context.EFlags |= 0x0100;
		else
			context.EFlags &= ~0x0100;

		if (!SetContextByThreadID(threadID, &context))
			return false;
		return true;
	}

	virtual GDBStatus Step(int threadID)
	{
		if (!EnableSingleStep(threadID, true))
			return kGDBUnknownError;
		return ResumeAndWait(threadID);
	}

	virtual GDBStatus SendBreakInRequestAsync()
	{
		if (!DebugBreakProcess(m_hProcess))
			return kGDBUnknownError;
		return kGDBSuccess;
	}

private:
	DWORD m_dwPID;
	DEBUG_EVENT m_DebugEvent;
	HANDLE m_hProcess;
	std::set<int> m_Threads;

private:
	bool WaitForDebugEvent(bool ignoreUnsupportedEvents = true)
	{
		for (;;)
		{
			if (!::WaitForDebugEvent(&m_DebugEvent, INFINITE))
				return false;

			printf("Debug event #%d, thread 0x%x\n", m_DebugEvent.dwDebugEventCode, m_DebugEvent.dwThreadId);

			switch(m_DebugEvent.dwDebugEventCode)
			{
			case EXCEPTION_DEBUG_EVENT:
				if (m_DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
					if (!EnableSingleStep(m_DebugEvent.dwThreadId, false))
						break;
				break;
			case CREATE_THREAD_DEBUG_EVENT:
				if (m_DebugEvent.dwThreadId)
					m_Threads.insert(m_DebugEvent.dwThreadId);
				break;
			case EXIT_THREAD_DEBUG_EVENT:
				if (m_DebugEvent.dwThreadId);
					m_Threads.erase(m_DebugEvent.dwThreadId);
				break;
			}

			if (ignoreUnsupportedEvents && !IsDebugEventSupportedByGDB(m_DebugEvent.dwDebugEventCode))
			{
				if (!ContinueDebugEvent(m_DebugEvent.dwProcessId, m_DebugEvent.dwThreadId, DBG_CONTINUE))
					return false;
				continue;
			}

			return true;
		}
	}

	bool IsDebugEventSupportedByGDB(DWORD dwEvent)
	{
		switch(dwEvent)
		{
		case EXCEPTION_DEBUG_EVENT:
		case LOAD_DLL_DEBUG_EVENT:
		case UNLOAD_DLL_DEBUG_EVENT:
		case OUTPUT_DEBUG_STRING_EVENT:
		case EXIT_PROCESS_DEBUG_EVENT:
		case RIP_EVENT:
			return true;
		default:
			return false;
		}
	}

public:
	Win32GDBTarget(HANDLE hProcess, HANDLE hThreadToResume = INVALID_HANDLE_VALUE)
		: m_hProcess(hProcess)
	{
		m_dwPID = GetProcessId(hProcess);
		if (!DebugActiveProcess(m_dwPID))
			m_dwPID = 0;
		
		if (hThreadToResume != INVALID_HANDLE_VALUE)
			ResumeThread(hThreadToResume);

		WaitForDebugEvent(false);
	}

	~Win32GDBTarget()
	{
		if (m_dwPID)
			DebugActiveProcessStop(m_dwPID);
	}

protected:
	virtual GDBStatus GetLastStopRecord(TargetStopRecord *pRec)
	{
		pRec->ProcessID = m_DebugEvent.dwProcessId;
		pRec->ThreadID = m_DebugEvent.dwThreadId;
		pRec->Reason = kUnspecified;

		switch(m_DebugEvent.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:
			pRec->Reason = kSignalReceived;
			switch(m_DebugEvent.u.Exception.ExceptionRecord.ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION:
				pRec->Extension.SignalNumber = SIGSEGV;
				break;
			case EXCEPTION_BREAKPOINT:
			case EXCEPTION_SINGLE_STEP:
				pRec->Extension.SignalNumber = SIGTRAP;
				break;
			default:
				pRec->Extension.SignalNumber = SIGINT;
				break;
			}
			break;
		case LOAD_DLL_DEBUG_EVENT:
		case UNLOAD_DLL_DEBUG_EVENT:
			pRec->Reason = kLibraryEvent;
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			pRec->Reason = kProcessExited;
			pRec->Extension.ExitCode = m_DebugEvent.u.ExitProcess.dwExitCode;
			break;
		default:
			pRec->Reason = kUnspecified;
			break;
		}
		return kGDBSuccess;
	}
	
private:
	bool GetContextByThreadID(int threadID, CONTEXT *pContext)
	{
		HANDLE hThread = OpenThread(THREAD_GET_CONTEXT, FALSE, threadID);
		if (hThread == INVALID_HANDLE_VALUE)
			return false;

		pContext->ContextFlags = CONTEXT_ALL;
		BOOL successful = GetThreadContext(hThread, pContext);
		CloseHandle(hThread);
		return successful != 0;
	}

	bool SetContextByThreadID(int threadID, const CONTEXT *pContext)
	{
		HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, threadID);
		if (hThread == INVALID_HANDLE_VALUE)
			return false;

		BOOL successful = SetThreadContext(hThread, pContext);
		CloseHandle(hThread);
		return successful != 0;
	}

public:	//Register API
	virtual const PlatformRegisterList *GetRegisterList()
	{
		return &i386::RegisterList;
	}

	//This method can be left unimplemented. In this case GDB will issue a 'g' packet to read all target registers when the target is stopped.
	virtual GDBStatus ReadFrameRelatedRegisters(int threadID, RegisterSetContainer &registers)
	{
		//return kGDBNotSupported;

		CONTEXT context;
		if (!GetContextByThreadID(threadID, &context))
			return kGDBUnknownError;

		registers[i386::rgEIP] = RegisterValue(context.Eip, 4);
		registers[i386::rgESP] = RegisterValue(context.Esp, 4);
		registers[i386::rgEBP] = RegisterValue(context.Ebp, 4);
		
		return kGDBSuccess;
	}

	virtual GDBStatus ReadTargetRegisters(int threadID, RegisterSetContainer &registers)
	{
		CONTEXT context;
		if (!GetContextByThreadID(threadID, &context))
			return kGDBUnknownError;

		for (size_t i = 0; i < i386::RegisterList.RegisterCount; i++)
		{
			ASSERT(registers[i].SizeInBytes == 4);
			ASSERT(s_ContextRegisterOffsets[i].RegisterIndex == i);
			registers[i].Valid = true;
			memcpy(registers[i].Value, ((char *)&context) + s_ContextRegisterOffsets[i].ContextOffset, registers[i].SizeInBytes);
		}
		return kGDBSuccess;
	}

	virtual GDBStatus WriteTargetRegisters(int threadID, const RegisterSetContainer &registers)
	{
		CONTEXT context;
		if (!GetContextByThreadID(threadID, &context))
			return kGDBUnknownError;

		for (size_t i = 0; i < i386::RegisterList.RegisterCount; i++)
		{
			if (!registers[i].Valid)
				continue;
			
			ASSERT(registers[i].SizeInBytes == 4);
			ASSERT(s_ContextRegisterOffsets[i].RegisterIndex == i);
			memcpy(((char *)&context) + s_ContextRegisterOffsets[i].ContextOffset, registers[i].Value, registers[i].SizeInBytes);
		}

		if (!SetContextByThreadID(threadID, &context))
			return kGDBUnknownError;
		return kGDBSuccess;
	}

public:	//Memory API
	virtual GDBStatus ReadTargetMemory(ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes)
	{
		C_ASSERT(sizeof(SIZE_T) == sizeof(size_t));
		if (!ReadProcessMemory(m_hProcess, (LPCVOID)Address, pBuffer, *pSizeInBytes, (SIZE_T *)pSizeInBytes))
			return kGDBUnknownError;
		return kGDBSuccess;
	}

	virtual GDBStatus WriteTargetMemory(ULONGLONG Address, const void *pBuffer, size_t sizeInBytes)
	{
		SIZE_T done = 0;
		if (!WriteProcessMemory(m_hProcess, (LPVOID)Address, pBuffer, sizeInBytes, &done))
			return kGDBUnknownError;
		if (done != sizeInBytes)
			return kGDBUnknownError;
		return kGDBSuccess;
	}

public:	//Optional API
	virtual GDBStatus GetDynamicLibraryList(std::vector<DynamicLibraryRecord> &libraries)
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, m_dwPID);
		if (hSnapshot == INVALID_HANDLE_VALUE)
			return kGDBUnknownError;
		MODULEENTRY32 module = {sizeof(module), };
		if (Module32First(hSnapshot, &module))
			while(Module32Next(hSnapshot, &module))		//Skip the first module, as it's the actual EXE
			{
				DynamicLibraryRecord rec;
				rec.FullPath = BazisLib::StringToANSISTLString(TempStrPointerWrapper(module.szExePath));
				rec.LoadAddress = (ULONGLONG)module.modBaseAddr + 0x1000;	//GDB requires this offset to map symbols correctly
				libraries.push_back(rec);
			} 
		CloseHandle(hSnapshot);
		return kGDBSuccess;
	}

	virtual GDBStatus GetThreadList(std::vector<ThreadRecord> &threads)
	{
		for each(int id in m_Threads)
		{
			ThreadRecord rec;
			rec.UserFriendlyName = "No additional info";
			rec.ThreadID = id;
			threads.push_back(rec);
		}
		return kGDBSuccess;
	}

	virtual GDBStatus SetThreadModeForNextCont(int threadID, DebugThreadMode mode, OUT bool *pNeedRestoreCall, IN OUT INT_PTR *pRestoreCookie)
	{
		switch(mode)
		{
		case dtmProbe:
			return kGDBSuccess;
		case dtmSingleStep:
			if (!EnableSingleStep(threadID, true))
				return kGDBUnknownError;
			return kGDBSuccess;
		case dtmSuspend:
			*pNeedRestoreCall = true;
			//Intentionally no break
		case dtmRestore:
			{
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadID);
				if (hThread == INVALID_HANDLE_VALUE)
					return kGDBUnknownError;

				bool ok = ((mode == dtmSuspend) ? SuspendThread(hThread) : ResumeThread(hThread)) != -1;

				CloseHandle(hThread);
				return ok ? kGDBSuccess : kGDBUnknownError;
			}
		default:
			return kGDBNotSupported;
		}
	}

	virtual GDBStatus Terminate()
	{
		return TerminateProcess(m_hProcess, -1) ? kGDBSuccess : kGDBUnknownError;
	}

	virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie)
	{
		return kGDBNotSupported;
	}

	virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie)
	{
		return kGDBNotSupported;
	}
};

class StubWithLogging : public GDBStub
{
	FILE *pLogFile;

	virtual StubResponse HandleRequest(const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData)
	{
		printf(">> %s%c%s\n", DynamicStringA(requestType).c_str(), splitterChar, DynamicStringA(requestData).c_str());
		fprintf(pLogFile, ">> %s%c%s\n", DynamicStringA(requestType).c_str(), splitterChar, DynamicStringA(requestData).c_str());
		StubResponse response = __super::HandleRequest(requestType, splitterChar, requestData);
		printf("<< %s\n", std::string(response.GetData(), response.GetSize()).c_str());
		fprintf(pLogFile, "<< %s\n", std::string(response.GetData(), response.GetSize()).c_str());
		fflush(pLogFile);
		return response;
	}

public:
	StubWithLogging(ISyncGDBTarget *pTarget)
		: GDBStub(pTarget, true)
	{
		pLogFile = fopen("gdbserver.log", "w");
	}

	~StubWithLogging()
	{
		fclose(pLogFile);
	}
};

class Win32StubFactory : public IGDBStubFactory
{
	PROCESS_INFORMATION m_Process;
	bool m_bVerbose;

public:
	Win32StubFactory(const PROCESS_INFORMATION &proc, bool verbose)
		: m_Process(proc)
		, m_bVerbose(verbose)
	{
	}

	virtual IGDBStub *CreateStub(GDBServer *pServer)
	{
		printf("GDB connected.\n");
		pServer->StopListening();
		Win32GDBTarget *pTarget = new Win32GDBTarget(m_Process.hProcess, m_Process.hThread);

		if (m_bVerbose)
			return new StubWithLogging(pTarget);
		else
			return new GDBStub(pTarget);
	}

	virtual void OnProtocolError(const TCHAR *errorDescription)
	{
		_tprintf(_T("Protocol error: %s\n"), errorDescription);
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		printf("Usage: SimpleWin32Server <exe name> [--verbose]");
		return 1;
	}

	bool verbose = false;
	if (argc >= 3 && !_tcscmp(argv[2], _T("--verbose")))
		verbose = true;

	PROCESS_INFORMATION info;
	if (!LaunchDebuggedProcess(argv[1], &info))
	{
		_tprintf(_T("Cannot launch %s\n"), argv[1]);
		return 1;
	}

	GDBServer srv(new Win32StubFactory(info, verbose));
	printf("Listening in port 2000. You can connect GDB now\n");
	srv.Start(2000);
	srv.WaitForTermination();

	return 0;
}

