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

	size_t idx;

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
		break;
	case 'H':
		return Handle_H(requestType);
	case '?':
		return Handle_QueryStopReason();
	case 'g':
		return Handle_g(GetThreadIDForOp(requestType[0]));
	case 'G':
		return Handle_G(GetThreadIDForOp(requestType[0]), requestType.substr(1));
	case 'P':
		idx = requestType.find('=');
		if (idx == -1)
			break;
		return Handle_P(GetThreadIDForOp(requestType[0]), requestType.substr(1, idx - 1), requestType.substr(idx + 1));
	case 'm':
		idx = requestType.find(',');
		if (idx == -1)
			break;
		return Handle_m(requestType.substr(1, idx - 1), requestType.substr(idx + 1));
	case 'M':
		idx = requestType.find(',');
		if (idx == -1)
			break;
		return Handle_M(requestType.substr(1, idx - 1), requestType.substr(idx + 1), requestData);
	case 'X':
		idx = requestType.find(',');
		if (idx == -1)
			break;
		return Handle_X(requestType.substr(1, idx - 1), requestType.substr(idx + 1), requestData);
	case 'c':
		return Handle_c(GetThreadIDForOp(requestType[0]));
	case 's':
		return Handle_s(GetThreadIDForOp(requestType[0]));
	case 'T':
		return Handle_T(requestType.substr(1));
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
	unsigned char op = requestType[1];
	int threadId = HexHelpers::ParseHexString<unsigned>(requestType.substr(2));

	m_ThreadIDsForOps[op] = threadId;
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
	default:
		response.Append("T05");	//Default to SIGTRAP
		response.Append(szThread);
		break;
	}

	if (updateLastReportedThreadID)
		m_LastReportedCurrentThreadID = rec.ThreadID;

	return response;
}

int GDBServerFoundation::BasicGDBStub::GetThreadIDForOp( unsigned char op )
{
	int defaultThreadID = m_ThreadIDsForOps[op];
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
	if (status != kGDBSuccess)
		response.Append(BazisLib::DynamicStringA::sFormat("E%02x", status & 0xFF).c_str());
	else
		response.Append("OK");
	return response;
}
