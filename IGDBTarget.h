#pragma once
#include <bzscore/string.h>
#include "signals.h"
#include "GDBRegisters.h"
#include <vector>
#include <string>

namespace GDBServerFoundation
{
	enum GDBStatus
	{
		kGDBSuccess,
		kGDBUnknownError,
		kGDBNotSupported = 0x1000,	//The command not supported. GDB should remember it and never try it again.
	};

	enum DebugThreadMode
	{
		dtmProbe,	//Do not change anything, just return kGDBSuccess if the method is supported
		dtmSingleStep,
		dtmSuspend,
		dtmRestore,
	};

	enum BreakpointType
	{
		bptSoftwareBreakpoint,
		bptHardwareBreakpoint,
		bptWriteWatchpoint,
		bptReadWatchpoint,
		bptAccessWatchpoint,
	};

	enum EmbeddedMemoryType
	{
		mtRAM,
		mtROM,
		mtFLASH,
	};

	struct DynamicLibraryRecord
	{
		std::string FullPath;
		ULONGLONG LoadAddress;
	};

	struct ThreadRecord
	{
		int ThreadID;
		std::string UserFriendlyName;
	};

	struct EmbeddedMemoryRegion
	{
		EmbeddedMemoryType Type;
		ULONGLONG Start;
		ULONGLONG Length;
		unsigned ErasureBlockSize;	//Valid for FLASH only. Should be 0 if not supported

		EmbeddedMemoryRegion()
		{
		}

		EmbeddedMemoryRegion(EmbeddedMemoryType type, ULONGLONG start, ULONGLONG length, unsigned erasureBlockSize = 0)
			: Type(type)
			, Start(start)
			, Length(length)
			, ErasureBlockSize(erasureBlockSize)
		{
		}
	};

	class IFLASHProgrammer
	{
	public:
		virtual GDBStatus GetEmbeddedMemoryRegions(std::vector<EmbeddedMemoryRegion> &regions)=0;
		virtual GDBStatus EraseFLASH(ULONGLONG addr, size_t length)=0;
		virtual GDBStatus WriteFLASH(ULONGLONG addr, const void *pBuffer, size_t length)=0;
		virtual GDBStatus CommitFLASHWrite()=0;
	};

	//! Defines methods called when the target is stopped
	class IStoppedGDBTarget
	{
	public:	//General target info
		virtual const PlatformRegisterList *GetRegisterList()=0;

	public:	//Register accessing API
		virtual GDBStatus ReadFrameRelatedRegisters(int threadID, TargetRegisterValues &registers)=0;
		virtual GDBStatus ReadTargetRegisters(int threadID, TargetRegisterValues &registers)=0;
		virtual GDBStatus WriteTargetRegisters(int threadID, const TargetRegisterValues &registers)=0;

	public: //Memory accessing API
		virtual GDBStatus ReadTargetMemory(ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes)=0;
		virtual GDBStatus WriteTargetMemory(ULONGLONG Address, const void *pBuffer, size_t sizeInBytes)=0;

	public:	//Optional methods, return kGDBNotSupported if not implemented
		virtual GDBStatus GetDynamicLibraryList(std::vector<DynamicLibraryRecord> &libraries)=0;
		virtual GDBStatus GetThreadList(std::vector<ThreadRecord> &threads)=0;
		virtual GDBStatus SetThreadModeForNextCont(int threadID, DebugThreadMode mode, OUT bool *pNeedRestoreCall, IN OUT INT_PTR *pRestoreCookie)=0;
		virtual GDBStatus Terminate()=0;

		//! Sets a breakpoint at a given address
		/*!
			\remarks It is not necessary to implement this method for software breakpoints. If GDB encounters a "not supported" reply,
					 it will set the breakpoint using the WriteTargetMemory() call. It is only recommended to implement this method for
					 the software breakpoints if it can set them better than GDB itself.
		*/
		virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie)=0;
		virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie)=0;

		//! This handler is invoked when user sends an arbitrary command to the GDB stub ("mon <command>" in GDB).
		virtual GDBStatus ExecuteRemoteCommand(const std::string &command, std::string &output)=0;

		//! Returns a pointer to an IFLASHProgrammer instance, or NULL if not supported. The returned instance should be persistent (e.g. the same object that implements IStoppedGDBTarget).
		virtual IFLASHProgrammer *GetFLASHProgrammer()=0;
	};

	enum TargetStopReason
	{
		kUnspecified,
		kSignalReceived,
		kProcessExited,
		kLibraryEvent,
	};

	struct TargetStopRecord
	{
		TargetStopReason Reason;
		int ProcessID;
		int ThreadID;	//0 if not specified

		union
		{
			UnixSignal SignalNumber;
			int ExitCode;
		} Extension;
	};

	class ISyncGDBTarget : public IStoppedGDBTarget
	{
	public:
		virtual GDBStatus GetLastStopRecord(TargetStopRecord *pRec)=0;
		virtual GDBStatus ResumeAndWait(int threadID)=0;
		virtual GDBStatus Step(int threadID)=0;

		virtual GDBStatus SendBreakInRequestAsync()=0;
	};

}