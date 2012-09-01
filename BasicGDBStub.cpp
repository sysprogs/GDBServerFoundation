#include "StdAfx.h"
#include "BasicGDBStub.h"
#include "HexHelpers.h"

using namespace BazisLib;
using namespace GDBServerFoundation;

StubResponse BasicGDBStub::HandleRequest( const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData )
{
	//requestData is the part of the request following the first ',', ';' or ':' character
	if (requestType.length() < 1)
		return StandardResponses::CommandNotSupported;

	size_t idx, idx2;

	switch(requestType[0])
	{
	case 'q':
		if (requestType == "qSupported")
			return Handle_qSupported(requestData);
		else if (requestType == "qfThreadInfo")
			return Handle_qfThreadInfo();
		else if (requestType == "qsThreadInfo")
			return Handle_qsThreadInfo();
		else if (requestType == "qThreadExtraInfo")
			return Handle_qThreadExtraInfo(requestData);
		else if (requestType == "qC")
			return Handle_qC();
		else if (requestType == "qCRC")
		{
			idx = requestData.find(',');
			if (idx == -1)
				break;
			return Handle_qCRC(requestData.substr(0, idx), requestData.substr(idx + 1));
		}
		else if (requestType == "qRcmd")
			return Handle_qRcmd(requestData);
		break;
	case 'H':
		return Handle_H(requestType);
	case '?':
		return Handle_QueryStopReason();
	case 'g':
		return Handle_g(GetThreadIDForOp(true));
	case 'G':
		return Handle_G(GetThreadIDForOp(true), requestType.substr(1));
	case 'P':
		idx = requestType.find('=');
		if (idx == -1)
			break;
		return Handle_P(GetThreadIDForOp(true), requestType.substr(1, idx - 1), requestType.substr(idx + 1));
	case 'm':
		return Handle_m(requestType.substr(1), requestData);
	case 'M':
		idx = requestData.find(':');
		if (idx == -1)
			break;
		return Handle_M(requestType.substr(1), requestData.substr(0, idx), requestData.substr(idx + 1));
	case 'X':
		idx = requestData.find(':');
		if (idx == -1)
			break;
		return Handle_X(requestType.substr(1), requestData.substr(0, idx), requestData.substr(idx + 1));
	case 'c':
		return Handle_c(GetThreadIDForOp(false));
	case 's':
		return Handle_s(GetThreadIDForOp(false));
	case 'T':
		return Handle_T(requestType.substr(1));
	case 'v':
		if (requestType == "vCont?")
			return Handle_vCont(requestType.substr(5));
		else if (requestType == "vCont")
			return Handle_vCont(requestData);
		else if (requestType == "vFlashErase")
		{
			idx = requestData.find(',');
			if (idx == -1)
				break;
			return Handle_vFlashErase(requestData.substr(0, idx), requestData.substr(idx + 1));
		}
		else if (requestType == "vFlashWrite")
		{
			idx = requestData.find(':');
			if (idx == -1)
				break;
			return Handle_vFlashWrite(requestData.substr(0, idx), requestData.substr(idx + 1));
		}
		else if (requestType == "vFlashDone")
			return Handle_vFlashDone();
		break;
	case 'k':
		return Handle_k();
	case 'Z':
	case 'z':
		if (requestType.length() != 2)
			break;
		idx2 = requestData.find(';');
		if (idx2 == -1)
			idx2 = requestData.length();
		idx = requestData.find(',');
		if (idx == -1)
			break;
		return Handle_Zz((requestType[0] == 'Z'), requestType[1], requestData.substr(0, idx), requestData.substr(idx + 1, idx2 - idx + 1), requestData.substr(idx2));
	}

	return StandardResponses::CommandNotSupported;
}

template <class _Splitter> static void FillMapFromSplitter(_Splitter &spl, std::map<std::string, std::string> &strMap)
{
	for(typename _Splitter::iterator it = spl.begin(); it != spl.end(); it++)
	{
		TempStringA str = *it;
		if (str.length() == 0)
			continue;
		int idx = str.find('=');
		if (idx == -1)
		{
			switch(str[str.length() - 1])
			{
			case '+':
			case '-':
			case '?':
				strMap[std::string(str.GetConstBuffer(), str.length() - 1)] = std::string(str.GetConstBuffer() + str.length() - 1, 1);
				break;
			default:
				strMap[std::string(str.GetConstBuffer(), str.length())] = "";
			}
		}
		else
			strMap[std::string(str.GetConstBuffer(), idx)] = std::string(str.GetConstBuffer() + idx + 1, str.length() - idx - 1);
	}
}


StubResponse BasicGDBStub::Handle_qSupported( const BazisLib::TempStringA &requestData )
{
	m_GDBFeatures.clear();
	FillMapFromSplitter(requestData.SplitByMarker(';'), m_GDBFeatures);

	StubResponse response;

	for(std::map<std::string, std::string>::iterator it = m_StubFeatures.begin(); it != m_StubFeatures.end(); it++)
	{
		if (response.GetSize())
			response += ";";
		response += it->first.c_str();
		if (it->second.length() != 1 || ((it->second[0] != '+') && (it->second[0] != '-') && (it->second[0] != '?')))
			response += "=";
		response += it->second.c_str();
	}
	
	return response;
}

GDBServerFoundation::BasicGDBStub::BasicGDBStub()
{
	//m_StubFeatures["PacketSize"] = "4000";	//0x4000
	m_StubFeatures["QStartNoAckMode"] = "+";
}

GDBServerFoundation::StubResponse GDBServerFoundation::BasicGDBStub::Handle_H( const BazisLib::TempStringA &requestType )
{
	if (requestType.length() < 3)
		return StandardResponses::InvalidArgument;

	int threadId = HexHelpers::ParseHexString<unsigned>(requestType.substr(2));
	if (threadId > 0)
	{
		//If the thread does not exist, abort the command
		StubResponse response = Handle_T(requestType.substr(2));
		if (response.GetSize() != 2 || memcmp(response.GetData(), "OK", 2))
			return response;
	}

	unsigned char op = requestType[1];

	switch(op)
	{
	case 'c':
		m_ThreadIDForCont = threadId;
		break;
	case 'g':
		m_ThreadIDForReg = threadId;
		break;
	default:
		return StandardResponses::InvalidArgument;
	}

	return StandardResponses::OK;
}

#if _MSC_VER
#define snprintf _snprintf
#endif

GDBServerFoundation::StubResponse GDBServerFoundation::BasicGDBStub::StopRecordToStopReply( const TargetStopRecord &rec, const char *pReportedRegisterValues, bool updateLastReportedThreadID )
{
	StubResponse response;
	
	char szReasonBase[32];
	char szThread[32];
	if (rec.ThreadID)
		snprintf(szThread, sizeof(szThread), "thread:%x;", rec.ThreadID);
	else
		szThread[0] = 0;

	switch(rec.Reason)
	{
	case kProcessExited:
		if (rec.ProcessID)
			snprintf(szReasonBase, sizeof(szReasonBase), "W%x;process:%x", rec.Extension.ExitCode, rec.ProcessID);
		else
			snprintf(szReasonBase, sizeof(szReasonBase), "W%x", rec.Extension.ExitCode);
		response.Append(szReasonBase);
		break;
	case kSignalReceived:
		snprintf(szReasonBase, sizeof(szReasonBase), "T%02x", rec.Extension.SignalNumber & 0xFF);
		response.Append(szReasonBase);
		if (pReportedRegisterValues)
			response.Append(pReportedRegisterValues);
		response.Append(szThread);
		break;
	case kLibraryEvent:
	default:
		response.Append("T05");	//Default to SIGTRAP
		if (pReportedRegisterValues)
			response.Append(pReportedRegisterValues);
		response.Append(szThread);
		if (rec.Reason == kLibraryEvent)
			response.Append("library:;");
		break;
	}

	if (updateLastReportedThreadID)
		m_LastReportedCurrentThreadID = rec.ThreadID;

	return response;
}

int GDBServerFoundation::BasicGDBStub::GetThreadIDForOp( bool isRegOp )
{
	int defaultThreadID = isRegOp ? m_ThreadIDForReg : m_ThreadIDForCont;
	if (defaultThreadID <= 0)
		return m_LastReportedCurrentThreadID;
	return defaultThreadID;
}

void GDBServerFoundation::BasicGDBStub::AppendRegisterValueToString( const RegisterValue &val, size_t sizeInBytes, BazisLib::DynamicStringA &str, const char *pSuffix /*= NULL*/ )
{
	if (!sizeInBytes)
		sizeInBytes = val.SizeInBytes;

	for (size_t j = 0; j < sizeInBytes ; j++)
		if (val.Valid)
			str.AppendFormat("%02x", val.Value[j] & 0xFF);
		else
			str.append("xx");

	if (pSuffix)
		str.AppendFormat(pSuffix);
}

GDBServerFoundation::StubResponse GDBServerFoundation::BasicGDBStub::FormatGDBStatus( GDBStatus status )
{
	StubResponse response;
	if (status == kGDBNotSupported)
		return StandardResponses::CommandNotSupported;
	else if (status != kGDBSuccess)
		response.Append(BazisLib::DynamicStringA::sFormat("E%02x", status & 0xFF).c_str());
	else
		response.Append("OK");

	return response;
}

void GDBServerFoundation::BasicGDBStub::ResetAllCachesWhenResumingTarget()
{
	m_ThreadIDForCont = m_ThreadIDForReg = 0;
}
