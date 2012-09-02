// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_WARNINGS

/*! \mainpage GDBServerFoundation framework
	
	\section Introduction
	The GDBServerFoundation framework provides a set of convenient C++ classes for implementing custom gdbserver-compatible stubs.
	The library provides the \ref GDBServerFoundation::GDBServer "GDBServer" class that implements the TCP/IP server handling the gdbserver protocol,
	the \ref GDBServerFoundation::GDBStub "GDBStub" class that provides implementations for the most common GDB packets and a simplified
	\ref GDBServerFoundation::ISyncGDBTarget "ISyncGDBTarget" interface that should be implemented by the users of the library to create a custom GDB stub.

	\section Quickstart
	In order to make your own GDB server with this framework, follow these steps:
	1. Look up the order in which GDB expects the register values for your target architecture by checking the &lt;target&gt;-tdep.c file in GDB directory.
	2. Define a global instance of the PlatformRegisterList struct describing the supported registers in the order GDB expects them. See \ref SimpleWin32Server/registers-i386.h "this example" for more details..
	3. Define a class implementing the \ref GDBServerFoundation::ISyncGDBTarget "ISyncGDBTarget" interface (including the members defined in \ref GDBServerFoundation::IStoppedGDBTarget "IStoppedGDBTarget").
	4. 

*/

/*! 
	\example SimpleWin32Server.cpp
	This example is a simple gdbserver-compatible stub that can debug Win32 processes. It supports multi-threading and DLL load events.
*/