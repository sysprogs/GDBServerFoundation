#include "stdafx.h"
#include "GDBStub.h"
#include "HexHelpers.h"

using namespace GDBServerFoundation;

StubResponse GDBStub::Handle_QueryStopReason()
{
	if (!m_pTarget)
		return StandardResponses::CommandNotSupported;
	
	TargetStopRecord rec;
	memset(&rec, 0, sizeof(rec));
	if (m_pTarget->GetLastStopRecord(&rec) != kGDBSuccess)
		return StandardResponses::CommandNotSupported;

	TargetRegisterValues registers = InitializeTargetRegisterContainer();
	GDBStatus status = m_pTarget->ReadFrameRelatedRegisters(rec.ThreadID, registers);
	BazisLib::DynamicStringA strRegisters;
	if (status == kGDBSuccess)
	{
		for(size_t i = 0; i < registers.RegisterCount(); i++)
		{
			const RegisterValue &val = registers[i];
			if (val.Valid)
			{
				strRegisters.AppendFormat("%02x:", m_pRegisters->Registers[i].RegisterIndex);
				AppendRegisterValueToString(val, (m_pRegisters->Registers[i].SizeInBits + 7) / 8, strRegisters, ";");
			}
		}
	}
	return StopRecordToStopReply(rec, strRegisters.c_str());
}


GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_g(int threadID)
{
	TargetRegisterValues registers = InitializeTargetRegisterContainer();

	StubResponse response;
	GDBStatus status = m_pTarget->ReadTargetRegisters(threadID, registers);
	if (status != kGDBSuccess)
		response.Append(BazisLib::DynamicStringA::sFormat("E%02x", status & 0xFF).c_str());
	else
	{
		BazisLib::DynamicStringA str;
		for(size_t i = 0; i < registers.RegisterCount(); i++)
			AppendRegisterValueToString(registers[i], (m_pRegisters->Registers[i].SizeInBits + 7) / 8, str);

		response.Append(str.c_str());
	}
	return response;
}

GDBServerFoundation::TargetRegisterValues GDBServerFoundation::GDBStub::InitializeTargetRegisterContainer()
{
	TargetRegisterValues registers(m_pRegisters->RegisterCount);
	for (size_t i = 0; i < m_pRegisters->RegisterCount; i++)
		registers[i].SizeInBytes = (m_pRegisters->Registers[i].SizeInBits + 7) / 8;
	return registers;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_G( int threadID, const BazisLib::TempStringA &registerValueBlock )
{
	TargetRegisterValues registers = InitializeTargetRegisterContainer();

	size_t offset = 0;
	for (size_t i = 0; i < registers.RegisterCount(); i++)
	{
		size_t avail = registerValueBlock.size() - offset;
		if (avail < 2U * registers[i].SizeInBytes)
			break;

		for (size_t j = 0; j < registers[i].SizeInBytes; j++)
		{
			registers[i].Value[j] = HexHelpers::ParseHexValue(registerValueBlock[offset], registerValueBlock[offset + 1]);
			offset += 2;
		}

		registers[i].Valid = true;
	}

	GDBStatus status = m_pTarget->WriteTargetRegisters(threadID, registers);
	return FormatGDBStatus(status);
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_P( int threadID, const BazisLib::TempStringA &registerIndex, const BazisLib::TempStringA &registerValue )
{
	unsigned registerNumber = 0;
	for (size_t i = 0; i < registerIndex.size(); i++)
		registerNumber = (registerNumber << 4) | HexHelpers::hexToInt(registerIndex[i]);

	if (registerNumber >= m_pRegisters->RegisterCount)
		return "EINVAL";

	TargetRegisterValues registers = InitializeTargetRegisterContainer();

	if (registerValue.size() < 2U * registers[registerNumber].SizeInBytes)
		return "EINVAL";

	for (size_t j = 0; j < registers[registerNumber].SizeInBytes; j++)
		registers[registerNumber].Value[j] = HexHelpers::ParseHexValue(registerValue[j * 2], registerValue[j * 2 + 1]);

	registers[registerNumber].Valid = true;

	GDBStatus status = m_pTarget->WriteTargetRegisters(threadID, registers);
	return FormatGDBStatus(status);
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_m( const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length )
{
	ULONGLONG ullAddr = HexHelpers::ParseHexString<ULONGLONG>(addr);
	size_t uLength = HexHelpers::ParseHexString<unsigned>(length);
	size_t done = uLength;

	void *pBuf = malloc(uLength);
	if (!pBuf)
		return "ENOMEM";

	StubResponse response;
	GDBStatus status = m_pTarget->ReadTargetMemory(ullAddr, pBuf, &done);
	if (status != kGDBSuccess)
		response.Append(BazisLib::DynamicStringA::sFormat("E%02x", status & 0xFF).c_str());
	else
	{
		ASSERT(done <= uLength);
		if (done > uLength)
			done = uLength;

		char *pNewText = response.AllocateAppend(done * 2);
		for (size_t i = 0, j = 0; i < done; i++)
		{
			unsigned char val = ((unsigned char *)pBuf)[i];
			pNewText[j++] = HexHelpers::hexTable[(val >> 4) & 0x0F];
			pNewText[j++] = HexHelpers::hexTable[val & 0x0F];
		}
	}
	free(pBuf);
	return response;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_M( const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length, const BazisLib::TempStringA &data )
{
	ULONGLONG ullAddr = HexHelpers::ParseHexString<ULONGLONG>(addr);
	size_t uLength = HexHelpers::ParseHexString<unsigned>(length);

	if (data.length() != uLength * 2)
		return "EINVAL";

	void *pBuf = malloc(uLength);
	if (!pBuf)
		return "ENOMEM";

	for (size_t i = 0; i < uLength; i++)
		((char *)pBuf)[i] = HexHelpers::ParseHexValue(data[i*2], data[i*2+1]);

	GDBStatus status = m_pTarget->WriteTargetMemory(ullAddr, pBuf, uLength);
	free(pBuf);
	return FormatGDBStatus(status);
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_X( const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length, const BazisLib::TempStringA &binaryData )
{
	if (length.size() == 1 && length[0] == '0')
		return "OK";	//GDB is probing whether the command is supported

	ULONGLONG ullAddr = HexHelpers::ParseHexString<ULONGLONG>(addr);
	size_t uLength = HexHelpers::ParseHexString<unsigned>(length);

	if (binaryData.length() != uLength)
		return "EINVAL";

	GDBStatus status = m_pTarget->WriteTargetMemory(ullAddr, binaryData.GetConstBuffer(), uLength);
	return FormatGDBStatus(status);
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qXfer( const BazisLib::TempStringA &object, const BazisLib::TempStringA &verb, const BazisLib::TempStringA &annex, const BazisLib::TempStringA &strOffset, const BazisLib::TempStringA &strLength )
{
	if (verb != "read")
		return StandardResponses::CommandNotSupported;

	BazisLib::DynamicStringA str = BuildGDBReportByName(object, annex);
	if (str.size() == 0)
		return StandardResponses::CommandNotSupported;

	size_t offset = HexHelpers::ParseHexString<unsigned>(strOffset);
	size_t length = HexHelpers::ParseHexString<unsigned>(strLength);

	bool moreData = false;

	if (offset > str.length())
		offset = str.length();

	if (length >= (str.length() - offset))
		length = str.length() - offset;
	else
		moreData = true;

	StubResponse response;
	response.Append(moreData ? "m" : "l");
	response.Append(str.GetConstBuffer() + offset, length);
	return response;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::HandleRequest( const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData )
{
	if (requestType.length() < 1)
		return BasicGDBStub::HandleRequest(requestType, splitterChar, requestData);
	switch(requestType[0])
	{
	case 'q':
		if (requestType == "qXfer")
		{
			int idxVerb = requestData.find(':');
			if (idxVerb == -1)
				break;
			int idxAnnex = requestData.find(':', idxVerb + 1);
			if (idxAnnex == -1)
				break;
			int idxOffset = requestData.find(':', idxAnnex + 1);
			if (idxOffset == -1)
				break;
			int idxLength = requestData.find(',', idxOffset + 1);
			if (idxLength == -1)
				break;
			return Handle_qXfer(requestData.substr(0, idxVerb), 
				requestData.substr(idxVerb + 1, idxAnnex - idxVerb - 1),
				requestData.substr(idxAnnex + 1, idxOffset - idxAnnex - 1),
				requestData.substr(idxOffset + 1, idxLength- idxOffset - 1),
				requestData.substr(idxLength + 1));
		}
		break;
	}
	return BasicGDBStub::HandleRequest(requestType, splitterChar, requestData);
}

static BazisLib::DynamicStringA HTMLEncode(const char *pStr)
{
	BazisLib::DynamicStringA result;

	for (size_t i = 0; pStr[i]; i++)
	{
		char ch = pStr[i];
		switch(ch)
		{
		case '<':
			result.append("&lt;");
			break;
		case '>':
			result.append("&gt;");
			break;
		case '&':
			result.append("&amp;");
			break;
		case '\"':
			result.append("&quot;");
			break;
		default:
			result.append(1, ch);
		}
	}

	return result;
}

BazisLib::DynamicStringA GDBServerFoundation::GDBStub::BuildGDBReportByName( const BazisLib::TempStringA &name, const BazisLib::TempStringA &annex )
{
	if (name == "libraries")
	{
		std::vector<DynamicLibraryRecord> libraries;
		BazisLib::DynamicStringA result = "<library-list>\n";
		GDBStatus status = m_pTarget->GetDynamicLibraryList(libraries);
		if (status == kGDBNotSupported)
			return "";
		if (status == kGDBSuccess)
		{
			for (size_t i = 0; i < libraries.size(); i++)
				result.AppendFormat("\t<library name=\"%s\"><segment address=\"0x%I64x\"/></library>\n", libraries[i].FullPath.c_str(), libraries[i].LoadAddress);
		}

		result += "</library-list>\n";
		return result;
	}
	else if (name == "threads")
	{
		BazisLib::DynamicStringA result = "<?xml version=\"1.0\"?>\n<threads>\n";
		ProvideThreadInfo();
		if (!m_bThreadsSupported)
			return "";
		for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
		{
			result.AppendFormat("\t<thread id=\"%x\">", m_CachedThreadInfo[i].ThreadID);
			result.append(HTMLEncode(m_CachedThreadInfo[i].UserFriendlyName.c_str()).c_str());
			result.append("</thread>\n");
		}
		result.AppendFormat("</threads>\n");
		return result;
	}
	else if (name == "memory-map")
	{
		static const char *MemoryTypes[3] = {"ram", "rom", "flash"};

		BazisLib::DynamicStringA result = "<?xml version=\"1.0\"?>\n<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \"http://sourceware.org/gdb/gdb-memory-map.dtd\">\n";
		result += "<memory-map>\n";
		for (size_t i = 0; i < m_EmbeddedMemoryRegions.size(); i++)
		{
			const EmbeddedMemoryRegion &region = m_EmbeddedMemoryRegions[i];
			if (region.Type >= __countof(MemoryTypes))
				continue;

			if (region.Type == mtFLASH)
			{
				unsigned blockSize = region.ErasureBlockSize;
				if (!blockSize)
					blockSize = region.Length;

				result.AppendFormat("\t<memory type=\"%s\" start=\"0x%I64x\" length = \"0x%I64x\">\n", MemoryTypes[region.Type], region.Start, region.Length);
				result.AppendFormat("\t\t<property name=\"blocksize\">0x%x</property>\n", blockSize);
				result.AppendFormat("\t</memory>\n");
			}
			else
				result.AppendFormat("\t<memory type=\"%s\" start=\"0x%I64x\" length = \"0x%I64x\"/>\n", MemoryTypes[region.Type], region.Start, region.Length);
		}
		result += "</memory-map>\n";
		return result;
	}
	return "";
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_c( int threadID )
{
	ResetAllCachesWhenResumingTarget();
	GDBStatus status = m_pTarget->ResumeAndWait(threadID);
	if (status != kGDBSuccess)
		return FormatGDBStatus(status);

	return Handle_QueryStopReason();
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_s( int threadID )
{
	ResetAllCachesWhenResumingTarget();
	GDBStatus status = m_pTarget->Step(threadID);
	if (status != kGDBSuccess)
		return FormatGDBStatus(status);

	return Handle_QueryStopReason();
}

#if _MSC_VER
#define snprintf _snprintf
#endif

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qfThreadInfo()
{
	ProvideThreadInfo();
	if (!m_bThreadsSupported)
		return StandardResponses::CommandNotSupported;

	StubResponse response;
	if (m_CachedThreadInfo.size())
	{
		char szID[64];
		response.Append("m");
		for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
		{
			snprintf(szID, sizeof(szID), "%x", m_CachedThreadInfo[i].ThreadID);
			if (i)
				response.Append(",");
			response.Append(szID);
		}
	}
	else
		response.Append("l");
	return response;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qsThreadInfo()
{
	return "l";
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qThreadExtraInfo( const BazisLib::TempStringA &strThreadID )
{
	ProvideThreadInfo();
	int threadID = HexHelpers::ParseHexString<unsigned>(strThreadID);
	for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
	{
		if (m_CachedThreadInfo[i].ThreadID == threadID)
		{
			StubResponse response;
			const std::string &desc = m_CachedThreadInfo[i].UserFriendlyName;

			char *pNewText = response.AllocateAppend(desc.length() * 2);
			for (size_t i = 0, j = 0; i < desc.length(); i++)
			{
				unsigned char val = desc[i];
				pNewText[j++] = HexHelpers::hexTable[(val >> 4) & 0x0F];
				pNewText[j++] = HexHelpers::hexTable[val & 0x0F];
			}
			return response;
		}
	}

	return "";
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_T( const BazisLib::TempStringA &strThreadID )
{
	ProvideThreadInfo();
	int threadID = HexHelpers::ParseHexString<unsigned>(strThreadID);

	for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
		if (m_CachedThreadInfo[i].ThreadID == threadID)
			return "OK";

	return "ENOSUCHTHREAD";
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qC()
{
	TargetStopRecord rec;
	memset(&rec, 0, sizeof(rec));
	GDBStatus status = m_pTarget->GetLastStopRecord(&rec);
	if (status != kGDBSuccess)
		return StandardResponses::CommandNotSupported;

	char szResponse[64];
	snprintf(szResponse, sizeof(szResponse), "QC%x", rec.ThreadID);
	return szResponse;
}

GDBServerFoundation::GDBStub::GDBStub( ISyncGDBTarget *pTarget, bool own /*= true*/ )
{
	m_pTarget = pTarget;
	m_bOwnStub = own;
	m_bThreadCacheValid = false;
	m_bThreadsSupported = true;

	m_pRegisters = pTarget->GetRegisterList();

	std::vector<DynamicLibraryRecord> libraries;
	if (m_pTarget->GetDynamicLibraryList(libraries) != kGDBNotSupported)
		RegisterStubFeature("qXfer:libraries:read");

	if (m_pTarget->GetThreadList(m_CachedThreadInfo) != kGDBNotSupported)
		RegisterStubFeature("qXfer:threads:read");

	IFLASHProgrammer *pProg = m_pTarget->GetFLASHProgrammer();
	if (pProg && pProg->GetEmbeddedMemoryRegions(m_EmbeddedMemoryRegions) == kGDBSuccess && !m_EmbeddedMemoryRegions.empty())
		RegisterStubFeature("qXfer:memory-map:read");
}

void GDBServerFoundation::GDBStub::ResetAllCachesWhenResumingTarget()
{
	BasicGDBStub::ResetAllCachesWhenResumingTarget();
	m_bThreadCacheValid = false;
}

void GDBServerFoundation::GDBStub::ProvideThreadInfo()
{
	if (m_bThreadCacheValid)
		return;
	m_bThreadCacheValid = true;
	m_CachedThreadInfo.clear();
	m_bThreadsSupported = (m_pTarget->GetThreadList(m_CachedThreadInfo) != kGDBNotSupported);
}

static DebugThreadMode modeFromAction(char action)
{
	switch(action)
	{
	case 'c':
	case 'C':
		return dtmProbe;
	case 's':
	case 'S':
		return dtmSingleStep;
	case 't':
		return dtmSuspend;
	default:
		return dtmProbe;
	}
}

#include <list>

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_vCont( const BazisLib::TempStringA &arguments )
{
	bool needRestore;
	INT_PTR cookie;

	if (arguments == "?")	//Query supported vCont modes
	{
		if (m_pTarget->SetThreadModeForNextCont(0, dtmProbe, &needRestore, &cookie) == kGDBSuccess)
			return "vCont;c;C;s;S;t";	//If only a subset is specified, GDB won't use vCont
		return StandardResponses::CommandNotSupported;
	}

	ProvideThreadInfo();
	std::map<unsigned, DebugThreadMode> threadMap;
	for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
		threadMap[m_CachedThreadInfo[i].ThreadID] = dtmProbe;	//We use this value as a default one for 'no action'
	DebugThreadMode defaultMode = dtmProbe;

	off_t start = 0, end = 0;
	bool last = false;
	for (;;)
	{
		end = arguments.find(';', start);
		if (end == -1)
			end = arguments.length(), last = true;

		BazisLib::TempStringA action = arguments.substr(start, end - start);
		unsigned threadID = 0;
		off_t idx = action.find(':');
		if (idx != -1)
		{
			threadID = HexHelpers::ParseHexString<unsigned>(action.substr(idx + 1));
			action = action.substr(0, idx);
		}

		if (action.length() < 1)
			return "EINVALIDARG";

		DebugThreadMode mode = modeFromAction(action[0]);
		if (threadID)
			threadMap[threadID] = mode;
		else
			defaultMode = mode;

		if (last)
			break;
		start = end + 1;
	}

	std::list<std::pair<unsigned, INT_PTR>> restoreQueue;
	GDBStatus status = kGDBSuccess;

	for (std::map<unsigned, DebugThreadMode>::iterator it = threadMap.begin(); it != threadMap.end(); it++)
	{
		DebugThreadMode mode = it->second;
		if (mode == dtmProbe)
			mode = defaultMode;

		if (mode == dtmProbe)
			continue;	//Nothing to do

		needRestore = false;
		cookie = 0;

		status = m_pTarget->SetThreadModeForNextCont(it->first, mode, &needRestore, &cookie);
		if (status != kGDBSuccess)
			break;


		if (needRestore)
			restoreQueue.push_back(std::pair<unsigned, INT_PTR>(it->first, cookie));
	}

	if (status == kGDBSuccess)
		status = m_pTarget->ResumeAndWait(0);

	for(std::list<std::pair<unsigned, INT_PTR>>::iterator it = restoreQueue.begin(); it != restoreQueue.end(); it++)
	{
		needRestore = true;
		m_pTarget->SetThreadModeForNextCont(it->first, dtmRestore, &needRestore, &it->second);
	}

	if (status != kGDBSuccess)
		return FormatGDBStatus(status);

	return Handle_QueryStopReason();
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_k()
{
	return FormatGDBStatus(m_pTarget->Terminate());
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_Zz( bool setBreakpoint, char type, const BazisLib::TempStringA &addr, const BazisLib::TempStringA &kind, const BazisLib::TempStringA &conditions )
{
	BreakpointType bpType;
	switch(type)
	{
	case '0':
		bpType = bptSoftwareBreakpoint;
		break;
	case '1':
		bpType = bptHardwareBreakpoint;
		break;
	case '2':
		bpType = bptWriteWatchpoint;
		break;
	case '3':
		bpType = bptReadWatchpoint;
		break;
	case '4':
		bpType = bptAccessWatchpoint;
		break;
	default:
		return StandardResponses::CommandNotSupported;
	}

	if (!conditions.empty())
		return "ENOTSUPPORTED";

	ULONGLONG ullAddr = HexHelpers::ParseHexString<ULONGLONG>(addr);
	unsigned uKind = HexHelpers::ParseHexString<unsigned>(kind);

	GDBStatus status;
	INT_PTR cookie = 0;

	std::pair<ULONGLONG, BreakpointType> key(ullAddr, bpType);

	if (setBreakpoint)
	{
		status = m_pTarget->CreateBreakpoint(bpType, ullAddr, uKind, &cookie);
		if (status == kGDBSuccess)
			m_BreakpointMap[key] = cookie;
	}
	else
	{
		std::map<std::pair<ULONGLONG, BreakpointType>, INT_PTR>::iterator it = m_BreakpointMap.find(key);
		if (it != m_BreakpointMap.end())
			cookie = it->second;
		status = m_pTarget->RemoveBreakpoint(bpType, ullAddr, cookie);
		if (it != m_BreakpointMap.end())
			m_BreakpointMap.erase(it);
	}

	return FormatGDBStatus(status);
}

#include "CRC32.h"

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qCRC( const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length )
{
	ULONGLONG ullAddr = HexHelpers::ParseHexString<ULONGLONG>(addr);
	unsigned uLength = HexHelpers::ParseHexString<unsigned>(length);

	BazisLib::BasicBuffer buf;
	if (!buf.EnsureSize(65536))
		return "ENOMEMORY";

	unsigned crcValue = -1U;

	while (uLength)
	{
		unsigned todo = uLength, done;
		if (todo > buf.GetAllocated())
			todo = buf.GetAllocated();

		done = todo;

		GDBStatus status = m_pTarget->ReadTargetMemory(ullAddr, buf.GetData(), &done);
		if (status != kGDBSuccess)
			return FormatGDBStatus(status);

		if (done != todo)
			return "EFAULT";

		crcValue = CRC32(crcValue, buf.GetData(), done);

		uLength -= done;
		ullAddr += done;
	}

	char szResult[128];
	snprintf(szResult, sizeof(szResult), "C%08X", crcValue);

	return szResult;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qRcmd( const BazisLib::TempStringA &command )
{
	std::string str, reply;
	str.reserve(command.length() / 2);

	if (command.length() % 2)
		return "EINVAL";

	for (size_t i = 0; i < command.length(); i+=2)
	{
		unsigned char byte = HexHelpers::ParseHexValue(command[i], command[i + 1]);
		str.append(1, byte);
	}

	GDBStatus status = m_pTarget->ExecuteRemoteCommand(str, reply);
	if (status != kGDBSuccess)
		return FormatGDBStatus(status);

	if (reply.empty())
		return "OK";

	StubResponse response;
	char *pNewText = response.AllocateAppend(reply.length() * 2);
	for (size_t i = 0, j = 0; i < reply.length(); i++)
	{
		unsigned char val = reply[i];
		pNewText[j++] = HexHelpers::hexTable[(val >> 4) & 0x0F];
		pNewText[j++] = HexHelpers::hexTable[val & 0x0F];
	}
	return response;
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_vFlashErase( const BazisLib::TempStringA &addr, const BazisLib::TempStringA &length )
{
	IFLASHProgrammer *pProg = m_pTarget->GetFLASHProgrammer();
	if (!pProg)
		return StandardResponses::CommandNotSupported;
	GDBStatus status = pProg->EraseFLASH(HexHelpers::ParseHexString<ULONGLONG>(addr), HexHelpers::ParseHexString<unsigned>(length));
	return FormatGDBStatus(status);
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_vFlashWrite( const BazisLib::TempStringA &addr, const BazisLib::TempStringA &binaryData )
{
	IFLASHProgrammer *pProg = m_pTarget->GetFLASHProgrammer();
	if (!pProg)
		return StandardResponses::CommandNotSupported;
	if (!binaryData.length())
		return "OK";
	GDBStatus status = pProg->WriteFLASH(HexHelpers::ParseHexString<ULONGLONG>(addr), binaryData.GetConstBuffer(), binaryData.size());
	return FormatGDBStatus(status);
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_vFlashDone()
{
	IFLASHProgrammer *pProg = m_pTarget->GetFLASHProgrammer();
	if (!pProg)
		return StandardResponses::CommandNotSupported;
	GDBStatus status = pProg->CommitFLASHWrite();
	return FormatGDBStatus(status);
}
