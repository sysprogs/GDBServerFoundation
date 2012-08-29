// VBoxGDB.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include "../../GDBServer.h"
#include "../../GDBStub.h"
#include "../../IGDBTarget.h"

#include "registers-i386.h"
#include <TlHelp32.h>

using namespace BazisLib;
using namespace GDBServerFoundation;

bool LaunchDebuggedProcess(PROCESS_INFORMATION *pProcInfo)
{
	STARTUPINFO startupInfo = {sizeof(STARTUPINFO), };
	TCHAR tsz[] = L"C:\\MinGW\\bin\\0.exe";

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

#include <map>

class Win32GDBTarget : public ISyncGDBTarget
{
	virtual GDBStatus ResumeAndWait()
	{
		return kGDBUnknownError;
	}
	virtual GDBStatus Step()
	{
		return kGDBUnknownError;
	}

	virtual GDBStatus SendBreakInRequestAsync()
	{
		return kGDBUnknownError;
	}

private:
	DWORD m_dwPID;
	DEBUG_EVENT m_DebugEvent;
	HANDLE m_hProcess;

//	std::map<ULONGLONG, DynamicLibraryRecord> m_LoadedDLLs;

private:
	bool WaitForDebugEvent()
	{
		if (!::WaitForDebugEvent(&m_DebugEvent, INFINITE))
			return false;

	/*	switch(m_DebugEvent.dwDebugEventCode)
		{
		case LOAD_DLL_DEBUG_EVENT:
			{
				DynamicLibraryRecord rec;
				rec.LoadAddress = (ULONGLONG)m_DebugEvent.u.LoadDll.lpBaseOfDll;
// 				if (m_DebugEvent.u.LoadDll.fUnicode)
// 					rec.FullPath = BazisLib::StringToANSISTLString(TempStrPointerWrapper((wchar_t *)m_DebugEvent.u.LoadDll.lpImageName));
// 				else
// 					rec.FullPath = (char *)m_DebugEvent.u.LoadDll.lpImageName;

				m_LoadedDLLs[rec.LoadAddress] = rec;
			}
			break;
		case UNLOAD_DLL_DEBUG_EVENT:
			m_LoadedDLLs.erase((ULONGLONG)m_DebugEvent.u.UnloadDll.lpBaseOfDll);
			break;
		}*/

		return true;
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

		WaitForDebugEvent();
		while (m_DebugEvent.dwDebugEventCode != EXCEPTION_DEBUG_EVENT)
		{
			ContinueDebugEvent(m_DebugEvent.dwProcessId, m_DebugEvent.dwThreadId, DBG_CONTINUE);
			WaitForDebugEvent();
		}
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
				pRec->Extension.SignalNumber = SIGTRAP;
				break;
			default:
				pRec->Extension.SignalNumber = SIGINT;
				break;
			}
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
	virtual GDBStatus ReadFrameRelatedRegisters(int threadID, TargetRegisterValues &registers)
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

	virtual GDBStatus ReadTargetRegisters(int threadID, TargetRegisterValues &registers)
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

	virtual GDBStatus WriteTargetRegisters(int threadID, const TargetRegisterValues &registers)
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
// 		for each(const std::pair<ULONGLONG, DynamicLibraryRecord> &kv in m_LoadedDLLs)
// 			libraries.push_back(kv.second);
//		return kGDBSuccess;

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

};

class SimpleStub : public GDBStub
{
	virtual StubResponse HandleRequest(const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData)
	{
		printf(">> %s%c%s\n", DynamicStringA(requestType).c_str(), splitterChar, DynamicStringA(requestData).c_str());
		StubResponse response = __super::HandleRequest(requestType, splitterChar, requestData);
		printf("<< %s\n", std::string(response.GetData(), response.GetSize()).c_str());
		return response;
	}

	virtual void OnProtocolError(const TCHAR *errorDescription)
	{
		_tprintf(_T("Protocol error: %s\n"), errorDescription);
	}

public:
	SimpleStub(ISyncGDBTarget *pTarget)
		: GDBStub(pTarget, true)
	{
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	PROCESS_INFORMATION info;
	if (!LaunchDebuggedProcess(&info))
		return 1;

	Win32GDBTarget *pTarget = new Win32GDBTarget(info.hProcess, info.hThread);

	GDBServer srv(new SimpleStub(pTarget));
	srv.Start(2000);
	Sleep(INFINITE);

	return 0;
}

