#pragma once
#include <bzscore/string.h>

namespace GDBServerFoundation
{
	//! Contains various helper functions for parsing and generating hex-encoded strings
	namespace HexHelpers
	{
		const char hexTable[17] = "0123456789abcdef";

		//! Converts a single hex character (0-9,a-f,A-F) to its integral value
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

		//! Converts a single two-character hex value into a byte
		static inline unsigned ParseHexValue(const char *p)
		{
			return (hexToInt(p[0]) << 4) | hexToInt(p[1]);
		}

		//! Converts a single two-character hex value into a byte
		static inline unsigned ParseHexValue(char firstChar, char secondChar)
		{
			return (hexToInt(firstChar) << 4) | hexToInt(secondChar);
		}

		//! Converts a hex-encoded string into its integral value
		template<typename _Type> static inline _Type ParseHexString(const BazisLib::TempStringA &str)
		{
			_Type result = 0;
			size_t i = 0;
			bool negate = false;
			if (!str.empty() && str[0] == '-')
				negate = true, i++;

			for (;i < str.size(); i++)
				result = (result << 4) | hexToInt(str[i]);		

			if (negate)
				return 0 - result;
			return result;
		}
	}
}