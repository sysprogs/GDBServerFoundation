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
	The GDBServerFoundation framework provides a set of convenient C++ classes for implementing custom gdbserver-compatible stubs. The library depends on the 
	<a href="http://bazislib.sysprogs.org/">BazisLib library</a> for socket and synchronization primitive implementation.

	\section Quickstart
	In order to make your own GDB server with this framework, follow these steps:
	1. Look up the order in which GDB expects the register values for your target architecture by checking the &lt;target&gt;-tdep.c file in GDB directory.
	2. Define a global instance of the PlatformRegisterList struct describing the supported registers in the order GDB expects them. See \ref SimpleWin32Server/registers-i386.h "this example" for more details..
	3. Create a class implementing the \ref GDBServerFoundation::ISyncGDBTarget "ISyncGDBTarget" interface (including the members defined in \ref GDBServerFoundation::IStoppedGDBTarget "IStoppedGDBTarget").
	4. Create a class implementing the \ref GDBServerFoundation::IGDBStubFactory "IGDBStubFactory" interface. The \ref GDBServerFoundation::IGDBStubFactory::CreateStub "CreateStub" method should create
	   return an instance of \ref GDBServerFoundation::GDBStub class created with a pointer to your \ref GDBServerFoundation::ISyncGDBTarget "ISyncGDBTarget" implementation. E.g.:
	   \code
	   virtual IGDBStub *CreateStub(GDBServer *pServer)
	   {
		   return new GDBStub(new Win32GDBTarget(m_Process.hProcess, m_Process.hThread));
	   }
	   \endcode
	5. In the main() function create an instance of \ref GDBServerFoundation::GDBServer "GDBServer" class, call the \ref GDBServerFoundation::GDBServer::Start "Start()" method to start listening and then call
	   \ref GDBServerFoundation::GDBServer::WaitForTermination "WaitForTermination()" method to wait until the server is shut down.

	\section Example
	The GDBServerFoundation comes with a simple example - \ref SimpleWin32Server/SimpleWin32Server.cpp "a simple debug server based on the Win32 debug API" located in the Samples\\SimpleWin32Server directory.

	\section main_classes Main classes
	The main GDBServerFoundation classes are:
		1. \ref GDBServerFoundation::GDBServer "GDBServer" implements a TCP server supporting the packet layer of the gdbserver protocol (including RLE encoding and escaping).
		2. \ref GDBServerFoundation::GDBStub "GDBStub" handles common packets defined in the gdbserver protocol by calling the methods of the \ref GDBServerFoundation::ISyncGDBTarget "ISyncGDBTarget" interface.
	In most cases you need to implement the \ref GDBServerFoundation::ISyncGDBTarget "ISyncGDBTarget" interface and reuse the \ref GDBServerFoundation::GDBServer "GDBServer" and \ref GDBServerFoundation::GDBStub "GDBStub"
	classes. However, if you want to add support for more gdbserver packets, you can easily make your own stub implementation inheriting from the \ref GDBServerFoundation::GDBStub "GDBStub" class.

	\section License
	The GDBServerFoundation framework is dual-licensed:
		1. You can use it under the terms of the LGPL license (i.e. publish the source of all statically linked code).
		2. You can alternatively purchase a commercial license from <a href="http://sysprogs.com/contact/">Sysprogs</a> to use it in your closed-source stubs.

	\section Support
	We provide email support for the customers that purchased the commercial license.
*/

/*! 
	\example SimpleWin32Server/SimpleWin32Server.cpp
	This example is a simple gdbserver-compatible stub that can debug Win32 processes. It supports multi-threading and DLL load events.
*/