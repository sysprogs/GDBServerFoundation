#pragma once
#include <bzscore/string.h>
#include "signals.h"
#include "GDBRegisters.h"
#include <vector>
#include <string>

namespace GDBServerFoundation
{
	//! Denotes a status returned by all GDB Target operations
	enum GDBStatus
	{
		//! The operation has succeeded
		kGDBSuccess,
		//! The operation has failed now, but is generally supported and can be retried with different arguments
		kGDBUnknownError,
		//! The entire command is not supported. GDB should never try this command again in this session.
		kGDBNotSupported = 0x1000,
	};

	//! Specifies the mode in which separate threads can be put before resuming execution
	enum DebugThreadMode
	{
		//! Do not change anything, return either kGDBSuccess or kGDBNotSupported
		dtmProbe,
		//! Put the thread into single-stepping mode
		dtmSingleStep,
		//! Keep the thread stopped until next debug event from another thread
		dtmSuspend,
		//! Restore the thread state saved in pRestoreCookie
		dtmRestore,
	};

	//! Enumerates all breakpoint types supported by GDB
	enum BreakpointType
	{
		//! Normal software breakpoint set with \"break\" command. If unsupported, GDB will implement it by writing memory manually.
		bptSoftwareBreakpoint,
		//! A code breakpoint based on hardware registers set with \"hbreak\" command.
		bptHardwareBreakpoint,
		//! A data breakpoint sensitive to writing
		bptWriteWatchpoint,
		//! A data berakpoint sensitive to reading
		bptReadWatchpoint,
		//! A data breakpoint sensitive to either reading or writing
		bptAccessWatchpoint,
	};

	//! Tells GDB about the type of a certain memory range
	enum EmbeddedMemoryType
	{
		//! Normal read/write RAM memory. WriteTargetMemory() will be used to write it. 
		mtRAM,
		//! Read-only memory. GDB will not try to write to it and will not use software breakpoints.
		mtROM,
		//! FLASH memory. GDB will use the FLASH erasing/programming commands to write to this memory and will not use software breakpoints.
		mtFLASH,
	};

	//! Describes a shared library loaded in to the process address space
	struct DynamicLibraryRecord
	{
		//! Specifies the full path to the library on the debugged machine
		std::string FullPath;
		//! Specifies the load address of the library.
		/*
			\attention For Windows DLLs this should be the actual base address + 0x1000
		*/
		ULONGLONG LoadAddress;
	};

	//! Describes a single thread of the debugged program
	struct ThreadRecord
	{
		//! Specifies an arbitrary thread ID (that, however, should not be 0)
		int ThreadID;
		//! Specifies optional user-friendly description shown by GDB along with the ID
		std::string UserFriendlyName;
	};

	//! Specifies a single memory region inside an embedded device. Used to detect the FLASH memory that requires erasing.
	struct EmbeddedMemoryRegion
	{
		//! Specifies the type of the memory (RAM, ROM, FLASH)
		EmbeddedMemoryType Type;
		//! Specifies the starting address of the region
		ULONGLONG Start;
		//! Specifies the length in bytes of the region
		ULONGLONG Length;
		//! Specifies the size of a block that can be erased at a time
		/*!
			\remarks This member is only valid for FLASH memories and can be 0 if the entier FLASH should be erased.
		*/
		unsigned ErasureBlockSize;

		//! Creates an uninitialized memory region description
		EmbeddedMemoryRegion()
		{
		}

		//! Creates and initializes a memory region description
		EmbeddedMemoryRegion(EmbeddedMemoryType type, ULONGLONG start, ULONGLONG length, unsigned erasureBlockSize = 0)
			: Type(type)
			, Start(start)
			, Length(length)
			, ErasureBlockSize(erasureBlockSize)
		{
		}
	};

	//! Implements methods required to program the FLASH memory
	/*! If your GDB target contains FLASH memory that requires special block erase commands, GDB will issue them automatically if
		you implement this interface and return a pointer to it in IStoppedGDBTarget::GetFLASHProgrammer().

		\remarks If you do not implement this interface, GDB will try to write to the memory using the IStoppedGDBTarget::WriteTargetMemory() method.
	*/
	class IFLASHProgrammer
	{
	public:
		//! Returns a list of all memory regions inside the device
		virtual GDBStatus GetEmbeddedMemoryRegions(std::vector<EmbeddedMemoryRegion> &regions)=0;
		//! Erases a given region of FLASH memory
		/*!
			\param Address Specifies the starting address of the region to be erased.
			\param length Specifies the length in bytes of the region to be erased.
			\remarks The erase region size is determined from the data returned by GetEmbeddedMemoryRegions() method.
		*/
		virtual GDBStatus EraseFLASH(ULONGLONG Address, size_t length)=0;
		//! Writes data into the FLASH memory
		/*!
			\remarks Note that IFlashProgrammer::CommitFLASHWrite() method will be called after all FLASH writing operations are completed.
					 Thus, the actual writing can be delayed until that call if it increases performance.
		*/
		virtual GDBStatus WriteFLASH(ULONGLONG Address, const void *pBuffer, size_t length)=0;

		//! Called when the FLASH write operations are done.
		virtual GDBStatus CommitFLASHWrite()=0;
	};

	//! Defines methods called when the target is stopped.
	/*! This interface defines the methods of the GDB target that are called when the target is stopped. 
		Actual targets should implement the ISyncGDBTarget interface instead.
	*/
	class IStoppedGDBTarget
	{
	public:	//General target info
		//! Returns a pointer to a persistent list of the registers supported by the target
		/*! This method should return a list to a constant global PlatformRegisterList instance
			containing the descriptions of all the registers present in the target.

			The method is called during initialization and the table returned by it should
			be present during the entire target lifetime.
		*/
		virtual const PlatformRegisterList *GetRegisterList()=0;

	public:	//Register accessing API
		//! Reads program counter, stack pointer and stack base pointer registers
		/*!
			\return See the GDBStatus description for a list of valid return codes.
			\param threadID Specifies the ID of the thread to query
			\param registers Contains the storage for the register values. The storage is initialized with RegisterValue
							 instances flagged as invalid and sizes set based on the GetRegisterList() call.
			\remarks If this method returns an error, GDB will read all registers using the
					 ReadTargetRegisters() method. Thus it only makes sense to implement this
					 method if reading frame-related registers instead of all registers saves time.

			Here's an example implementation of this method:
			\code
			virtual GDBStatus ReadFrameRelatedRegisters(int threadID, RegisterSetContainer &registers)
			{
				// (Fill the context structure here)

				registers[i386::rgEIP] = RegisterValue(context.Eip, 4);
				registers[i386::rgESP] = RegisterValue(context.Esp, 4);
				registers[i386::rgEBP] = RegisterValue(context.Ebp, 4);

				return kGDBSuccess;
			}
			\endcode
		*/
		virtual GDBStatus ReadFrameRelatedRegisters(int threadID, RegisterSetContainer &registers)=0;

		//! Reads the values of all registers from the target
		/*! This method should read all registers from the target (or a single target thread). See the ReadFrameRelatedRegisters()
			method description for implementation examples.
		*/
		virtual GDBStatus ReadTargetRegisters(int threadID, RegisterSetContainer &registers)=0;

		//! Writes one or more target registers
		/*! This method should write the target registers that have valid values provided.
			\param threadID Specifies the ID of the thread to query
			\param registers Provides the values for the registers to write. The size of the registers collection
				   can be determined by calling the RegisterSetContainer::RegisterCount() method. If an i-th
				   register needs to be modified, the registers[i].Valid field is set.

			\remarks Here's an example implementation:
			\code
				virtual GDBStatus WriteTargetRegisters(int threadID, const RegisterSetContainer &registers)
				{
					for (size_t i = 0; i < registers.RegisterCount(); i++)
					{
						if (!registers[i].Valid)
							continue;

						m_pTargetImpl->WriteRegister(i, registers[i].ToUInt32());
					}
				return kGDBSuccess;
			}
			\endcode
		*/
		virtual GDBStatus WriteTargetRegisters(int threadID, const RegisterSetContainer &registers)=0;

	public: //Memory accessing API
		//! Reads the memory of the underlying target
		virtual GDBStatus ReadTargetMemory(ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes)=0;
		//! Writes the memory of the underlying target
		virtual GDBStatus WriteTargetMemory(ULONGLONG Address, const void *pBuffer, size_t sizeInBytes)=0;

	public:	//Optional methods, return kGDBNotSupported if not implemented
		//! Fills the list of the dynamic libraries currently loaded in the target
		/*! If the target supports dynamic libraries (a.k.a DLLs, a.k.a. shared libraries, a.k.a. shared objects),
			this method should provide the information about them by filling the libraries vector.
			\param libraries Contains an empty vector that should be filled with instances of DynamicLibraryRecord
			\return If the target does not support dynamic libraries, the method should return kGDBNotSupported.
					Refer to GDBStatus documentation for more information.
		*/
		virtual GDBStatus GetDynamicLibraryList(std::vector<DynamicLibraryRecord> &libraries)=0;
		
		//! Fills the list of the threads currently present in the target
		/*! If the target supports multiple threads, this method should provide the information about them by filling the threads vector.
			\param threads Contains an empty vector that should be filled with instances of ThreadRecord
			\return If the target does not support multiple threads, the method should return kGDBNotSupported.
					Refer to GDBStatus documentation for more information.
		*/
		virtual GDBStatus GetThreadList(std::vector<ThreadRecord> &threads)=0;

		//! Sets the mode in which an individual thread will continue before the next debug event
		/*! This method allows setting different continuation modes (e.g. single-step, free run or halt) for different threads of a multi-threaded program.
			If the target does not support it, the method should return kGDBNotSupported.
			\param threadID Specifies the ID of the thread that is being controlled
			\param mode Specifies the mode in which the thread should be put until the next debug event
			\param pNeedRestoreCall Set the value pointed by this argument to TRUE if this method needs to be called again with mode == dtmRestore
				   once the next debug event completes.
			\param pRestoreCookie If mode is not dtmRestore, you set pNeedRestoreCall to TRUE, this argument can receive an arbitrary INT_PTR value
				   that will be passed to the method later during the dtmRestore call. If mode is dtmRestore, this argument points to the previously
				   saved value.

			\remarks Before the method is actually used, it is called with mode == dtmProbe and an invalid threadID to determine whether the target
					 supports it. Thus, if mode is dtmProbe, the threadID argument should be ignored and the method should immediately return either
					 kGDBNotSupported or kGDBSuccess.
		*/
		virtual GDBStatus SetThreadModeForNextCont(int threadID, DebugThreadMode mode, OUT bool *pNeedRestoreCall, IN OUT INT_PTR *pRestoreCookie)=0;

		//! Terminates the target
		virtual GDBStatus Terminate()=0;

		//! Sets a breakpoint at a given address
		/*!
			\param type Specifies the type of the supported breakpoint.
			\param Address Specifies the address where the breakpoint should be set.
			\param kind Specifies the additional information provided by GDB. For data breakpoints this is the length of the watched region.
			\param pCookie receives an arbitrary INT_PTR value that will be passed to RemoveBreakpoint() once this breakpoint is removed.
			\remarks It is not necessary to implement this method for software breakpoints. If GDB encounters a "not supported" reply,
					 it will set the breakpoint using the WriteTargetMemory() call. It is only recommended to implement this method for
					 the software breakpoints if it can set them better than GDB itself.
		*/
		virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie)=0;

		//! Removes a previously set breakpoint.
		/*! This method should remove a breakpoint previously set with CreateBreakpoint().
			\param type Equal to the type argument of the corresponding CreateBreakpoint() call.
			\param Address Equal to the Address argument of the corresponding CreateBreakpoint() call.
			\param Cookie contains the arbitrary INT_PTR value provided by the CreateBreakpoint() method.
		*/
		virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie)=0;

		//! This handler is invoked when user sends an arbitrary command to the GDB stub ("mon <command>" in GDB).
		virtual GDBStatus ExecuteRemoteCommand(const std::string &command, std::string &output)=0;

		//! Returns a pointer to an IFLASHProgrammer instance, or NULL if not supported. The returned instance should be persistent (e.g. the same object that implements IStoppedGDBTarget).
		virtual IFLASHProgrammer *GetFLASHProgrammer()=0;
		virtual ~IStoppedGDBTarget(){}
	};

	//! Specifies the type last debug event (i.e. the reason why the target was stopped)
	enum TargetStopReason
	{
		//! The stop reason is unknown
		kUnspecified,
		//! The target has received a signal. The TargetStopRecord::Extension::SignalNumber should contain the signal (e.g. SIGTRAP for a breakpoint).
		kSignalReceived,
		//! The process has exited
		kProcessExited,
		//! A dynamic library has loaded or unloaded
		kLibraryEvent,
	};

	//! Defines the last debug event (i.e. the reason why the target was stopped)
	/*! The following list summarizes how the most common debug events are reported:
		* When a breakpoint is hit, the Reason should be kSignalReceived and Extension.SignalNumber should be SIGTRAP.
		* When a single step is complete, the TargetStopRecord should be the same as for a breakpoint.
		* When an invalid pointer is referenced, the Reason should be kSignalReceived and Extension.SignalNumber should be SIGSEGV.
	*/
	struct TargetStopRecord
	{
		//! The reason why the target was stopped (e.g. breakpoint or library load event)
		TargetStopReason Reason;

		//! Specifies the process ID or 0 if not applicable.
		int ProcessID;
		//! Specifies the thread ID or 0 if not applicable.
		int ThreadID;

		//! Contains additional information dependent on the Reason field
		union
		{
			//! If Reason == kSignalReceived, contains the UNIX signal number that was received (e.g. SIGTRAP for a breakpoint)
			UnixSignal SignalNumber;
			//! If Reason == kProcessExited, contains the exit code of the process
			int ExitCode;
		} Extension;
	};

	//! Defines a GDB target
	/*!
		\remarks Note that the target is assumed to be already stopped when an object implementing IStoppedGDBTarget is created.
				 Thus, the first method of this interface to be called will be GetLastStopRecord().
	*/
	class ISyncGDBTarget : public IStoppedGDBTarget
	{
	public:
		//! Returns the information explaining why the target has stopped (e.g. stopped at a breakpoint).
		/*!
			\remarks This method can be called multiple times for the same stop event. Thus, it should not clear any caches or flags.
		*/
		virtual GDBStatus GetLastStopRecord(TargetStopRecord *pRec)=0;

		//! Resumes the target and indefinitely waits till a next debug event occurs
		/*!	This method should resume the target and wait for the next debug event to occur. If SetThreadModeForNextCont() was previously
			called for one or more threads, the threadID will be 0 and the method should ensure that the thread previously set to single-step
			or halt mode will remain in that mode until the next debug event.
		*/
		virtual GDBStatus ResumeAndWait(int threadID)=0;

		//! Resumes the target in a single-step mode
		/*! This method is called to do single-stepping if SetThreadModeForNextCont() is not supported. It should perform a single step on the
			specified thread and wait until the step continues.
		*/
		virtual GDBStatus Step(int threadID)=0;

		//! Requests the target to stop executing (i.e. forces a breakpoint)
		/*!
			\remarks This method can be executed from an arbitrary thread (either an internal GDBServer worker thread, or the main thread) and thus
					 should be fully thread-safe, should not wait for any event to complete and should return immediately. 
		*/
		virtual GDBStatus SendBreakInRequestAsync()=0;

		//! Close the session when the server is about to terminate
		/*! This method is called by the GlobalSessionMonitor class when the user presses Ctrl+Break to terminate the program.
		*/
		virtual void CloseSessionSafely()=0;
	};

	//! Provides default "not supported" implementations for optional methods of IStoppedGDBTarget
	class MinimalTargetBase : public ISyncGDBTarget
	{
	public:
		virtual GDBStatus ReadFrameRelatedRegisters(int threadID, RegisterSetContainer &registers)
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus GetDynamicLibraryList(std::vector<DynamicLibraryRecord> &libraries)
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus GetThreadList(std::vector<ThreadRecord> &threads)
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus SetThreadModeForNextCont(int threadID, DebugThreadMode mode, OUT bool *pNeedRestoreCall, IN OUT INT_PTR *pRestoreCookie)
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus Terminate()
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie)
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie)
		{
			return kGDBNotSupported;
		}

		virtual GDBStatus ExecuteRemoteCommand(const std::string &command, std::string &output)
		{
			return kGDBNotSupported;
		}

		virtual IFLASHProgrammer *GetFLASHProgrammer()
		{
			return NULL;
		}

		virtual void CloseSessionSafely()
		{
		}

	};

}