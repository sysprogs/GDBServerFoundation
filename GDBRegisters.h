#pragma once
#include <vector>

namespace GDBServerFoundation
{
	struct RegisterEntry
	{
		int RegisterIndex;
		const char *RegisterName;
		int SizeInBits;
	};

	struct PlatformRegisterList
	{
		size_t RegisterCount;
		RegisterEntry *Registers;
	};

	struct RegisterValue
	{
		bool Valid;
		unsigned char SizeInBytes;
		unsigned char Value[64];

		RegisterValue()
			: Valid(false)
			, SizeInBytes(0)
		{
		}

		RegisterValue(unsigned integralValue, unsigned char sizeInBytes)
			: Valid(true)
			, SizeInBytes(sizeInBytes)
		{
			if (SizeInBytes > sizeof(Value))
				SizeInBytes = sizeof(Value);
			memcpy(Value, &integralValue, SizeInBytes);
		}

		unsigned ToUInt32() const
		{
			return *((unsigned *)Value);
		}
	};

	//! Stores values of some or all target registers.
	/*! This class should be used in conjunction with the target-specific register index enumeration.
		E.g. 
			<pre>values[rgEAX] = RegisterValue(context.Eax, 4)</pre>

		To test whether a register value is provided, the following construct should be used:
			<pre>if(values[rgEAX].Valid) { ... }</pre>

		Note that it's safe to use the [] operator as long as its argument is below the RegisterCount().
	*/
	class TargetRegisterValues
	{
	private:
		std::vector<RegisterValue> m_Values;

	public:
		TargetRegisterValues(size_t registerCount)
		{
			m_Values.resize(registerCount);
		}

		RegisterValue &operator[](size_t index)
		{
			ASSERT(index < m_Values.size());
			return m_Values[index];
		}

		const RegisterValue &operator[](size_t index) const
		{
			ASSERT(index < m_Values.size());
			return m_Values[index];
		}

		size_t RegisterCount()
		{
			return m_Values.size();
		}
	};
}