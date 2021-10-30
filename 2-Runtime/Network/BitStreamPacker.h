#pragma once

#if ENABLE_NETWORK
#include "External/RakNet/builds/include/BitStream.h"
class Vector3f;
class Quaternionf;
struct NetworkViewID;


class BitstreamPacker
{
	RakNet::BitStream*  m_BitStream;

	int                 m_DeltaReadPos;
	UInt8*				m_ReadDeltaData;
	int                 m_DeltaReadSize;

	std::vector<UInt8>* m_WriteDeltaData;
	int                 m_DeltaWritePos;
	
	bool                m_IsDifferent;
	bool                m_IsReading;
	bool                m_NoOutOfBounds;
	
	void Init(RakNet::BitStream& stream, std::vector<UInt8>* delta, UInt8* readDeltaData, int readDeltaSize, bool reading);
	
	public:
	
	BitstreamPacker (RakNet::BitStream& stream, std::vector<UInt8>* delta, UInt8* readDeltaData, int readDeltaSize, bool reading);
	BitstreamPacker (RakNet::BitStream& stream, bool reading);

	bool IsWriting () { return !m_IsReading; }
	bool IsReading () { return m_IsReading; }
	bool HasChanged () { return m_IsDifferent; }
	bool HasReadOutOfBounds () { return !m_NoOutOfBounds; }
	
	void Serialize (float& value, float maxDelta = -1.0F);
	void Serialize (SInt32& value) { Serialize((UInt32&)value); }
	void Serialize (UInt32& value);
	void Serialize (short& value);
	void Serialize (char& value) { Serialize((unsigned char&)value); }
	void Serialize (unsigned char& value);
	void Serialize (bool& value);
	void Serialize (NetworkViewID& value);
	void Serialize (std::string& value);
	void Serialize (Vector3f& value, float maxDelta = -1.0F);
	void Serialize (Quaternionf& value, float maxDelta = -1.0F);
	void Serialize (char* value, int& valueLength);
	
	private:
		
	void ReadPackState (Quaternionf& t);
	void ReadPackState (Vector3f& t);
	void ReadPackState (float& t);
	void ReadPackState (UInt32& t);
	void ReadPackState (short& t);
	void ReadPackState (unsigned char& t);
	void ReadPackState (bool& t);
	void ReadPackState (std::string& t);
	void ReadPackState (char*& t, int& length);
	void ReadPackState (NetworkViewID& t);

	void WritePackState (Vector3f& t);
	void WritePackState (Quaternionf& t);
	void WritePackState (float t);
	void WritePackState (UInt32 t);
	void WritePackState (short t);
	void WritePackState (unsigned char t);
	void WritePackState (bool t);
	void WritePackState (std::string& t);
	void WritePackState (char* t, int& length);
	void WritePackState (NetworkViewID& t);

};

#endif
