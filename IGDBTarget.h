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
		kGDBNotSupported = 22,	//EINVAL
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

	public:	//Optional methods
		virtual GDBStatus GetDynamicLibraryList(std::vector<DynamicLibraryRecord> &libraries)=0;
		virtual GDBStatus GetThreadList(std::vector<ThreadRecord> &threads)=0;
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