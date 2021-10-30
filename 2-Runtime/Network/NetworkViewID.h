#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK
#include "External/RakNet/builds/include/BitStream.h"
#include "Runtime/Serialize/SerializeUtility.h"

struct NetworkViewID
{
	private:
	
	UInt32 m_LevelPrefix;
	UInt32 m_ID;
	UInt32 m_Type;///< enum { Allocated = 0, Scene = 1 }
	
	public:
	
	DECLARE_SERIALIZE(NetworkViewID)
	
	NetworkViewID () : 
		m_LevelPrefix (0),
		m_ID (0),
		m_Type (0)
	{ }
	
	enum { kAllocatedID = 0, kSceneID = 1 };

	// Pack the view id into a 16 bit or 32 bit number based on how big the number is
	void Write (RakNet::BitStream& stream);
	bool Read (RakNet::BitStream& stream);
	
	friend bool operator < (const NetworkViewID& lhs, const NetworkViewID& rhs);
	friend bool operator != (const NetworkViewID& lhs, const NetworkViewID& rhs);
	friend bool operator == (const NetworkViewID& lhs, const NetworkViewID& rhs);
	
	void SetSceneID (UInt32 sceneID);
	void SetAllocatedID (UInt32 sceneID);
	bool IsSceneID() { return m_Type == kSceneID; }
	void ReplaceLevelPrefix (UInt32 levelPrefix);

	UInt32 GetIndex () const { return m_ID; }
	
	std::string ToString() const;

	static NetworkViewID GetUnassignedViewID() { NetworkViewID viewID; return viewID; }
};

template<class TransferFunc>
void NetworkViewID::Transfer (TransferFunc& transfer)
{	
	TRANSFER(m_ID);
	TRANSFER(m_Type);
}

#endif
