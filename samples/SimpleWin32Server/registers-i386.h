#pragma once
#include "../../GDBRegisters.h"

namespace GDBServerFoundation
{
	namespace i386
	{
		//! Enumerates i386 registers in the order that is expected by GDB (and is hardcoded in i386-tdep.c in GDB
		enum RegisterIndex
		{
			rgEAX,
			rgECX,
			rgEDX,
			rgEBX,
			rgESP,
			rgEBP,
			rgESI,
			rgEDI,

			rgEIP,
			rgEFLAGS,
			rgCS,
			rgSS,
			rgDS,
			rgES,
			rgFS,
			rgGS,
		};

		static RegisterEntry _RawRegisterList[] = {
			{rgEAX, "eax", 32},
			{rgECX, "ecx", 32},
			{rgEDX, "edx", 32},
			{rgEBX, "ebx", 32},
			{rgESP, "esp", 32},
			{rgEBP, "ebp", 32},
			{rgESI, "esi", 32},
			{rgEDI, "edi", 32},

			{rgEIP, "eip", 32},
			{rgEFLAGS, "eflags", 32},
			{rgCS, "cs", 32},
			{rgCS, "ss", 32},
			{rgCS, "ds", 32},
			{rgCS, "es", 32},
			{rgCS, "fs", 32},
			{rgCS, "gs", 32},
		};

		static PlatformRegisterList RegisterList = 
		{
			sizeof(_RawRegisterList) / sizeof(_RawRegisterList[0]),
			_RawRegisterList,
		};

	}
}