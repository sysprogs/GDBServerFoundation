#include "stdafx.h"
#include "GDBStub.h"
#include "HexHelpers.h"

using namespace GDBServerFoundation;

StubResponse GDBStub::Handle_QueryStopReason()
{
	if (!m_pTarget)
		return StandardResponses::CommandNotSupported;
	
	TargetStopRecord rec;
	if (m_pTarget->GetLastStopRecord(&rec) != kGDBSuccess)
		return StandardResponses::CommandNotSupported;

	TargetRegisterValues registers = InitializeTargetRegisterContainer();
	GDBStatus status = m_pTarget->ReadTargetRegisters(rec.ThreadID, registers);
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

BazisLib::DynamicStringA GDBServerFoundation::GDBStub::BuildGDBReportByName( const BazisLib::TempStringA &name, const BazisLib::TempStringA &annex )
{
	if (name == "libraries")
	{
		std::vector<DynamicLibraryRecord> libraries;
		BazisLib::DynamicStringA result = "<library-list>\n";
		GDBStatus status = m_pTarget->GetDynamicLibraryList(libraries);
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
		m_CachedThreadInfo.clear();
		GDBStatus status = m_pTarget->GetThreadList(m_CachedThreadInfo);
		if (status == kGDBSuccess)
			for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
			{
				result.AppendFormat("\t<thread id=\"%x\">", m_CachedThreadInfo[i].ThreadID);
				result.append(m_CachedThreadInfo[i].UserFriendlyName.c_str());
				result.append("</thread>\n");
			}
		result.AppendFormat("</threads>\n");
		return result;
	}
	return "";
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_c( int threadID )
{
	GDBStatus status = m_pTarget->ResumeAndWait(threadID);
	if (status != kGDBSuccess)
		return FormatGDBStatus(status);

	return Handle_QueryStopReason();
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_s( int threadID )
{
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
	m_CachedThreadInfo.clear();
	GDBStatus status = m_pTarget->GetThreadList(m_CachedThreadInfo);
	if (status != kGDBSuccess)
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
	int threadID = HexHelpers::ParseHexString<unsigned>(strThreadID);

	for (size_t i = 0; i < m_CachedThreadInfo.size(); i++)
		if (m_CachedThreadInfo[i].ThreadID == threadID)
			return "OK";

	return "ENOSUCHTHREAD";
}

GDBServerFoundation::StubResponse GDBServerFoundation::GDBStub::Handle_qC()
{
	TargetStopRecord rec;
	GDBStatus status = m_pTarget->GetLastStopRecord(&rec);
	if (status != kGDBSuccess)
		return StandardResponses::CommandNotSupported;

	char szResponse[64];
	snprintf(szResponse, sizeof(szResponse), "QC%x", rec.ThreadID);
	return szResponse;
}
