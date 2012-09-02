#pragma once
#include <vector>

namespace GDBServerFoundation
{
	//! Describes a single register of the target platform
	struct RegisterEntry
	{
		//! A zero-based register index. All indicies should be sequential.
		int RegisterIndex;
		//! A user-friendly register name.
		const char *RegisterName;
		//! The size of the register in bits
		int SizeInBits;
	};

	/*!
		\example SimpleWin32Server/registers-i386.h
		This example shows how i386 registers are defined.
	*/

	//! Contains a fixed list of registers defined at compile time
	/*! An global instance of PlatformRegisterList should be initialized and provided via the IStoppedGDBTarget::GetRegisterList() method.
		See \ref SimpleWin32Server/registers-i386.h "this example" for more details.
	*/
	struct PlatformRegisterList
	{
		//! Specifies the amount of the registers
		size_t RegisterCount;
		//! Points to an array containing register definitions
		RegisterEntry *Registers;
	};

	//! Contains the value of a single register. Register values are normally passed via RegisterSetContainer objects. 
	struct RegisterValue
	{
		//! Specifies whether the value is valid
		bool Valid;
		//! Specifies the size in bytes of the register
		unsigned char SizeInBytes;
		//! Contains the register value in the target byte order
		unsigned char Value[64];

		//! Creates a new instance of RegisterValue and flags it as invalid
		RegisterValue()
			: Valid(false)
			, SizeInBytes(0)
		{
		}

		//! Creates a new instance of RegisterValue containing the actual value
		RegisterValue(unsigned integralValue, unsigned char sizeInBytes)
			: Valid(true)
			, SizeInBytes(sizeInBytes)
		{
			if (SizeInBytes > sizeof(Value))
				SizeInBytes = sizeof(Value);
			memcpy(Value, &integralValue, SizeInBytes);
		}

		//! Converts the little-endian value to a 32-bit integer
		unsigned ToUInt32() const
		{
			return *((unsigned *)Value);
		}

		//! Converts a little-endian value to a 16-bit integer
		unsigned short ToUInt16() const
		{
			return *((unsigned short *)Value);
		}
	};

	//! Stores values of some or all target registers.
	/*! This class should be used in conjunction with the target-specific register index enumeration.
		E.g. 
		\code values[rgEAX] = RegisterValue(context.Eax, 4) \endcode

		To test whether a register value is provided, the following construct should be used:
		\code if(values[rgEAX].Valid) { ... } \endcode

		Note that it's safe to use the [] operator as long as its argument is below the RegisterCount().
	*/
	class RegisterSetContainer
	{
	private:
		std::vector<RegisterValue> m_Values;

	public:
		//! Creates an instance of RegisterSetContainer given the number of registers
		RegisterSetContainer(size_t registerCount)
		{
			m_Values.resize(registerCount);
		}

		//! Gets or sets a register by its index
		RegisterValue &operator[](size_t index)
		{
			ASSERT(index < m_Values.size());
			return m_Values[index];
		}

		//! Gets a register value by its index
		const RegisterValue &operator[](size_t index) const
		{
			ASSERT(index < m_Values.size());
			return m_Values[index];
		}

		//! Returns the total amount of registers
		size_t RegisterCount()
		{
			return m_Values.size();
		}
	};
}