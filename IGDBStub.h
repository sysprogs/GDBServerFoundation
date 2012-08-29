#pragma once
#include <string>
#include <bzscore/buffer.h>

namespace GDBServerFoundation
{
	class StubResponse
	{
		BazisLib::BasicBuffer m_Buffer;

	public:
		StubResponse(const StubResponse &anotherResponse)
			: m_Buffer(anotherResponse.m_Buffer.GetSize())
		{
			memcpy(m_Buffer.GetData(), anotherResponse.m_Buffer.GetConstData(), anotherResponse.m_Buffer.GetSize());
			m_Buffer.SetSize(anotherResponse.m_Buffer.GetSize());
		}

		StubResponse()
		{
		}

		StubResponse(const char *pText)
			: m_Buffer(pText, strlen(pText))
		{
		}

		StubResponse(const void *pData, size_t length)
			: m_Buffer((const char *)pData, length)
		{
		}

		size_t GetSize() {return m_Buffer.GetSize();}
		const char *GetData() {return (const char *)m_Buffer.GetConstData();}

		void Append(const char *pStr)
		{
			m_Buffer.append(pStr, strlen(pStr), 4096);
		}

		void Append(const char *pStr, size_t length)
		{
			m_Buffer.append(pStr, length, 4096);
		}

		char *AllocateAppend(size_t length)
		{
			size_t oldSize = m_Buffer.GetSize();
			if (!m_Buffer.EnsureSize(oldSize + length))
				return NULL;
			m_Buffer.SetSize(oldSize + length);
			return (char *)m_Buffer.GetData(oldSize);
		}

		StubResponse &operator+=(const char *pStr)
		{
			Append(pStr);
			return *this;
		}
	};

	struct StandardResponses
	{
		static StubResponse CommandNotSupported;
		static StubResponse InvalidArgument;
		static StubResponse OK;
	};

	class IGDBStub
	{
	public:
		//! Handles a fully unescaped RLE-expanded request from GDB
		virtual StubResponse HandleRequest(const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData)=0;
		virtual void OnProtocolError(const TCHAR *errorDescription)=0;
		virtual ~IGDBStub(){}
	};
}