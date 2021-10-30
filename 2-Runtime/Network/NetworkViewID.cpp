#include "UnityPrefix.h"
#include "NetworkViewID.h"

#if ENABLE_NETWORK
#include "Runtime/Serialize/SwapEndianBytes.h"

enum {
	k4Bits = (1 << 4) - 1,
	k10Bits = (1 << 10) - 1,
	k14Bits = (1 << 14) - 1,
	k15Bits = (1 << 15) - 1,
	k29Bits = (1 << 29) - 1
};

void NetworkViewID::Write (RakNet::BitStream& stream)
{
	UInt32 endianSceneID = m_ID;
	UInt32 endianLevelPrefix = m_LevelPrefix;
	
	#if UNITY_BIG_ENDIAN
	SwapEndianBytes(endianSceneID);
	SwapEndianBytes(endianLevelPrefix);
	#endif

	if (m_Type == kAllocatedID)
	{
		// 16 bit mode
		if (m_ID <= k14Bits)
		{
			stream.Write0();// 16 bit mode
			stream.Write1();// allocated
			stream.WriteBits(reinterpret_cast<UInt8*>(&endianSceneID), 14);
		}
		// 32 bit mode
		else if (m_ID <= k29Bits)
		{
			stream.Write1();// 32 bit mode
			stream.Write0();// 32 bit mode
			stream.Write1();// allocated
			stream.WriteBits(reinterpret_cast<UInt8*>(&endianSceneID), 29);
		}
		// 64 bit mode
		else
		{
			AssertString("64 bit mode not supported yet");
		}
	}
	else
	{
		// 16 bit mode
		if (m_ID <= k10Bits && m_LevelPrefix <= k4Bits)
		{
			stream.Write0();// 16 bit mode
			stream.Write0();// allocated
			stream.WriteBits(reinterpret_cast<UInt8*>(&endianLevelPrefix), 4);
			stream.WriteBits(reinterpret_cast<UInt8*>(&endianSceneID), 10);
		}
		// 32 bit mode
		else if (m_ID <= k14Bits && m_LevelPrefix <= k15Bits)
		{
			stream.Write1();// 32 bit mode
			stream.Write0();// 32 bit mode
			stream.Write0();// allocated
			stream.WriteBits(reinterpret_cast<UInt8*>(&endianLevelPrefix), 15);
			stream.WriteBits(reinterpret_cast<UInt8*>(&endianSceneID), 14);
		}
		// 64 bit mode
		else
		{
			AssertString("64 bit mode not supported yet");
		}
	}
}

bool NetworkViewID::Read (RakNet::BitStream& stream)
{
	m_ID = 0;
	m_LevelPrefix = 0;
	m_Type = 0;
	
	if (stream.GetNumberOfUnreadBits() < 16)
		return false;

	int size =  16;
	int bitsRead = 1;
	if (stream.ReadBit())
	{
		size = 32;
		bitsRead++;
		if (stream.ReadBit())
			size = 64;

		if (stream.GetNumberOfUnreadBits() < size-bitsRead)
		{
			AssertString(Format("Only %d bits left, but expected %d\n", stream.GetNumberOfUnreadBits(), size-bitsRead));
			return false;
		}
	}

	bool isAllocated = stream.ReadBit();

	if (isAllocated)
	{
		m_Type = kAllocatedID;
		
		if (size == 16)
			stream.ReadBits(reinterpret_cast<UInt8*>(&m_ID), 14);	
		else if (size == 32)
			stream.ReadBits(reinterpret_cast<UInt8*>(&m_ID), 29);	
		else
			AssertString("Unsupported allocated view ID size");
	}
	else
	{
		m_Type = kSceneID;
		if (size == 16)
		{
			stream.ReadBits(reinterpret_cast<UInt8*>(&m_LevelPrefix), 4);
			stream.ReadBits(reinterpret_cast<UInt8*>(&m_ID), 10);
		}
		else if (size == 32)
		{
			stream.ReadBits(reinterpret_cast<UInt8*>(&m_LevelPrefix), 15);
			stream.ReadBits(reinterpret_cast<UInt8*>(&m_ID), 14);
		}
		else
		{
			AssertString("Unsupported scene view ID size");
		}
	}
	
	#if UNITY_BIG_ENDIAN
	SwapEndianBytes(m_LevelPrefix);
	SwapEndianBytes(m_ID);
	#endif
	
	return true;
}

void NetworkViewID::SetSceneID (UInt32 sceneID)
{
	m_Type = kSceneID;
	m_LevelPrefix = 0;
	m_ID = sceneID;
}

void NetworkViewID::SetAllocatedID (UInt32 sceneID)
{
	m_Type = kAllocatedID;
	m_LevelPrefix = 0;
	m_ID = sceneID;
}

void NetworkViewID::ReplaceLevelPrefix (UInt32 levelPrefix)
{
	AssertIf(m_Type != kSceneID);
	m_LevelPrefix = levelPrefix;
}

bool operator < (const NetworkViewID& lhs, const NetworkViewID& rhs)
{
	if (lhs.m_ID != rhs.m_ID)
		return lhs.m_ID < rhs.m_ID;
	else
	{
		if (lhs.m_LevelPrefix != rhs.m_LevelPrefix)
			return lhs.m_LevelPrefix < rhs.m_LevelPrefix;
		else
			return lhs.m_Type < rhs.m_Type;
	}
}

std::string NetworkViewID::ToString () const
{
	char buffer[128];
	if (m_Type == kSceneID)
		sprintf(buffer, "SceneID: %lu Level Prefix: %lu", m_ID, m_LevelPrefix);
	else
		sprintf(buffer, "AllocatedID: %ld", m_ID);
	return buffer;
}

bool operator == (const NetworkViewID& lhs, const NetworkViewID& rhs)
{
	return lhs.m_ID == rhs.m_ID && lhs.m_LevelPrefix == rhs.m_LevelPrefix && lhs.m_Type == rhs.m_Type;
}

bool operator != (const NetworkViewID& lhs, const NetworkViewID& rhs)
{
	return lhs.m_ID != rhs.m_ID || lhs.m_LevelPrefix != rhs.m_LevelPrefix || lhs.m_Type != rhs.m_Type;
}

#endif
