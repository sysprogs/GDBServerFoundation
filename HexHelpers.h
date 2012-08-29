#pragma once
#include <bzscore/string.h>

namespace GDBServerFoundation
{
	namespace HexHelpers
	{
		const char hexTable[17] = "0123456789abcdef";

		static inline unsigned hexToInt(char hexChar)
		{
			//TODO: convert to a table-based version
			if (hexChar >= '0' && hexChar <= '9')
				return hexChar - '0';
			if (hexChar >= 'A' && hexChar <= 'F')
				return hexChar + 0x0A - 'A';
			if (hexChar >= 'a' && hexChar <= 'f')
				return hexChar + 0x0A - 'a';
			return 0;
		}

		static inline unsigned ParseHexValue(const char *p)
		{
			return (hexToInt(p[0]) << 4) | hexToInt(p[1]);
		}

		static inline unsigned ParseHexValue(char firstChar, char secondChar)
		{
			return (hexToInt(firstChar) << 4) | hexToInt(secondChar);
		}

		template<typename _Type> static inline _Type ParseHexString(const BazisLib::TempStringA &str)
		{
			_Type result = 0;
			for (size_t i = 0; i < str.size(); i++)
				result = (result << 4) | hexToInt(str[i]);		
			return result;
		}
	}
}