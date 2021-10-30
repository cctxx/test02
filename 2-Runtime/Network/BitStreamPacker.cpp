#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "BitStreamPacker.h"

#if ENABLE_NETWORK
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Vector3.h"
#include "External/RakNet/builds/include/StringCompressor.h"
#include "NetworkViewID.h"

using namespace std;

// WARNING: current impl did quite a few unaligned accesses, so we change it to use memcpy (no align restrictions)

template <typename T> static inline void ReadValueUnaligned(const UInt8* data, T* val)
{
	::memcpy(val, data, sizeof(T));
}
template <typename T> static inline void WriteValueUnaligned(UInt8* data, const T& val)
{
	::memcpy(data, &val, sizeof(T));
}


void BitstreamPacker::ReadPackState (Quaternionf& t)
{
	if (m_DeltaReadPos + sizeof(Quaternionf) <= m_DeltaReadSize)
	{
		ReadValueUnaligned(m_ReadDeltaData + m_DeltaReadPos, &t);
		m_DeltaReadPos += sizeof(Quaternionf);	
	}
	else
	{
		t = Quaternionf (0,0,0,1);
		m_DeltaReadPos += sizeof(Quaternionf);	
	}
}

void BitstreamPacker::ReadPackState (Vector3f& t)
{
	if (m_DeltaReadPos + sizeof(Vector3f) <= m_DeltaReadSize)
	{
		ReadValueUnaligned(m_ReadDeltaData + m_DeltaReadPos, &t);
		m_DeltaReadPos += sizeof(Vector3f);	
	}
	else
	{
		t = Vector3f(0,0,0);
		m_DeltaReadPos += sizeof(Vector3f);	
	}
}

void BitstreamPacker::WritePackState (Vector3f& t)
{
	std::vector<UInt8>& data = *m_WriteDeltaData;
	if (m_DeltaWritePos + sizeof(Vector3f) > data.size())
		data.resize(m_DeltaWritePos + sizeof(Vector3f));

	WriteValueUnaligned(&data[m_DeltaWritePos], t);
	m_DeltaWritePos += sizeof(Vector3f);
}

void BitstreamPacker::WritePackState (Quaternionf& t)
{
	std::vector<UInt8>& data = *m_WriteDeltaData;
	if (m_DeltaWritePos + sizeof(Quaternionf) > data.size())
		data.resize(m_DeltaWritePos + sizeof(Quaternionf));
	
	WriteValueUnaligned(&data[m_DeltaWritePos], t);
	m_DeltaWritePos += sizeof(Quaternionf);
}

void BitstreamPacker::WritePackState (string& t)
{
	std::vector<UInt8>& data = *m_WriteDeltaData;
	if (m_DeltaWritePos + t.size() > data.size())
		data.resize(m_DeltaWritePos + t.size() + sizeof(SInt32));
	
	WriteValueUnaligned<SInt32>(&data[m_DeltaWritePos], t.size());
	m_DeltaWritePos += sizeof(SInt32);

	memcpy(&data[m_DeltaWritePos], t.c_str(), t.size());
	m_DeltaWritePos += t.size();
}

void BitstreamPacker::ReadPackState (string& t)
{
	t = string();
	if (m_DeltaReadPos + sizeof(SInt32) <= m_DeltaReadSize)
	{
		SInt32 size = 0; ReadValueUnaligned<SInt32>(m_ReadDeltaData + m_DeltaReadPos, &size);
		m_DeltaReadPos += sizeof(SInt32);
		
		if (m_DeltaReadPos + size <= m_DeltaReadSize)
		{
			t.assign((char*)m_ReadDeltaData + m_DeltaReadPos, (char*)m_ReadDeltaData + m_DeltaReadPos + size);
		}
		m_DeltaReadPos += size;
	}
}

void BitstreamPacker::WritePackState (char* t, int& length)
{
	std::vector<UInt8>& data = *m_WriteDeltaData;
	if (m_DeltaWritePos + length > data.size())
		data.resize(m_DeltaWritePos + length + sizeof(SInt32));
	
	WriteValueUnaligned<SInt32>(&data[m_DeltaWritePos], length);
	m_DeltaWritePos += sizeof(SInt32);
	memcpy(&data[m_DeltaWritePos], t, length);
	m_DeltaWritePos += length;
}

void BitstreamPacker::ReadPackState (char*& t, int& length)
{
	if (m_DeltaReadPos + sizeof(SInt32) <= m_DeltaReadSize)
	{
		SInt32 size = 0; ReadValueUnaligned<SInt32>(m_ReadDeltaData + m_DeltaReadPos, &size);
		m_DeltaReadPos += sizeof(SInt32);
		
		// Allocate new char array with correct size
		t = new char[size];
		length = size;
		
		// Again check data bounds
		if (m_DeltaReadPos + size <= m_DeltaReadSize)
		{
			// Assign char pointer to the correct location in memory

			// WhatAFuckingHell is that
			// should be memcpy probably ;-)
			t = reinterpret_cast<char*>(m_ReadDeltaData + m_DeltaReadPos);
		}
		m_DeltaReadPos += size;
	}
}

void BitstreamPacker::ReadPackState (NetworkViewID& t)
{
	if (m_DeltaReadPos + sizeof(NetworkViewID) <= m_DeltaReadSize) \
	{
		ReadValueUnaligned(m_ReadDeltaData + m_DeltaReadPos, &t);
		m_DeltaReadPos += sizeof(NetworkViewID);
	}
	else
	{
		t = NetworkViewID::GetUnassignedViewID();
		m_DeltaReadPos += sizeof(NetworkViewID);
	}
}

void BitstreamPacker::WritePackState (NetworkViewID& t)
{
	std::vector<UInt8>& data = *m_WriteDeltaData;
	if (m_DeltaWritePos + sizeof(NetworkViewID) > data.size())
		data.resize(m_DeltaWritePos + sizeof(NetworkViewID));
	WriteValueUnaligned(&data[m_DeltaWritePos], t);
	m_DeltaWritePos += sizeof(NetworkViewID);
}

#define READ_WRITE_PACKSTATE(TYPE) \
void BitstreamPacker::ReadPackState (TYPE& t) \
{ \
	if (m_DeltaReadPos + sizeof(TYPE) <= m_DeltaReadSize) \
	{ \
		ReadValueUnaligned<TYPE>(m_ReadDeltaData + m_DeltaReadPos, &t); \
		m_DeltaReadPos += sizeof(TYPE);	 \
	} \
	else \
	{ \
		t = TYPE(); \
		m_DeltaReadPos += sizeof(TYPE);	\
	} \
} \
void BitstreamPacker::WritePackState (TYPE t) \
{ \
	std::vector<UInt8>& data = *m_WriteDeltaData; \
	if (m_DeltaWritePos + sizeof(TYPE) > data.size()) \
		data.resize(m_DeltaWritePos + sizeof(TYPE)); \
	WriteValueUnaligned<TYPE>(&data[m_DeltaWritePos], t); \
	m_DeltaWritePos += sizeof(TYPE); \
}

READ_WRITE_PACKSTATE(UInt32)
READ_WRITE_PACKSTATE(float)
READ_WRITE_PACKSTATE(short)
READ_WRITE_PACKSTATE(UInt8)
READ_WRITE_PACKSTATE(bool)

void BitstreamPacker::Serialize (NetworkViewID& value)
{
	if (m_IsReading)
	{
		if (m_WriteDeltaData != NULL)
		{
			NetworkViewID oldData;
			ReadPackState(oldData);
			
			bool readValue = false;
			m_NoOutOfBounds &= m_BitStream->Read(readValue);
			if (readValue)
				m_NoOutOfBounds &= value.Read(*m_BitStream);
			else
				value = oldData;
			
			WritePackState (value);
		}
		else
		{
			m_NoOutOfBounds &= value.Read(*m_BitStream);
		}
	}
	else
	{
		if (m_WriteDeltaData != NULL)
		{
			NetworkViewID oldData;
			ReadPackState(oldData);

			if (value != oldData)
			{
				m_BitStream->Write1();
				value.Write(*m_BitStream);
				WritePackState (value);
				m_IsDifferent |= true;
			}
			else
			{
				m_BitStream->Write0();
				WritePackState (oldData);
			}

		}
		else
		{
			value.Write(*m_BitStream);
			m_IsDifferent |= true;
		}
	}

}


void BitstreamPacker::Serialize (float& value, float maxDelta)
{
	if (m_IsReading)
	{
		if (m_WriteDeltaData != NULL)
		{
			float oldData;
			ReadPackState(oldData);
			
			bool readValue = false;
			m_NoOutOfBounds &= m_BitStream->Read(readValue);
			if (readValue)
				m_NoOutOfBounds &= m_BitStream->Read(value);
			else
				value = oldData;
			
			WritePackState (value);
		}
		else
		{
			m_NoOutOfBounds &= m_BitStream->Read(value);
		}
	}
	else
	{
		if (m_WriteDeltaData != NULL)
		{
			float oldData;
			ReadPackState(oldData);

			if (!CompareApproximately(value, oldData, maxDelta))
			{
				m_BitStream->Write1();
				m_BitStream->Write(value);
				WritePackState (value);
				m_IsDifferent |= true;
			}
			else
			{
				m_BitStream->Write0();
				WritePackState (oldData);
			}

		}
		else
		{
			m_BitStream->Write(value);
			m_IsDifferent |= true;
		}
	}
}

void BitstreamPacker::Serialize (bool& value)
{
	if (m_IsReading)
	{
		if (m_WriteDeltaData != NULL)
		{
			bool oldData;
			ReadPackState(oldData);
			m_NoOutOfBounds &= m_BitStream->Read(value);
			WritePackState (value);
		}
		else
		{
			m_NoOutOfBounds &= m_BitStream->Read(value);
		}
	}
	else
	{
		if (m_WriteDeltaData != NULL)
		{
			bool oldData;
			ReadPackState(oldData);

			if (value != oldData)
			{
				m_BitStream->Write(value);
				WritePackState (value);
				m_IsDifferent |= true;
			}
			else
			{
				m_BitStream->Write(value);
				WritePackState (oldData);
			}
		}
		else
		{
			m_BitStream->Write(value);
			m_IsDifferent |= true;
		}
	}
}

#define SERIALIZE(TYPE) void BitstreamPacker::Serialize (TYPE& value) {\
	if (m_IsReading)\
	{\
		if (m_WriteDeltaData != NULL)\
		{\
			TYPE oldData;\
			ReadPackState(oldData);\
			\
			bool readValue = false;\
			m_NoOutOfBounds &= m_BitStream->Read(readValue);\
			if (readValue)\
				m_NoOutOfBounds &= m_BitStream->Read(value);\
			else\
				value = oldData;\
			\
			WritePackState (value);\
		}\
		else\
		{\
			m_NoOutOfBounds &= m_BitStream->Read(value);\
		}\
	}\
	else\
	{\
		if (m_WriteDeltaData != NULL)\
		{\
			TYPE oldData;\
			ReadPackState(oldData);\
			\
			if (value != oldData)\
			{\
				m_BitStream->Write1();\
				m_BitStream->Write(value);\
				WritePackState (value);\
				m_IsDifferent |= true;\
			}\
			else\
			{\
				m_BitStream->Write0();\
				WritePackState (oldData);\
			}\
		}\
		else\
		{\
			m_BitStream->Write(value);\
			m_IsDifferent |= true;\
		}\
	}\
}

SERIALIZE(UInt32)
SERIALIZE(short)
SERIALIZE(UInt8)


void BitstreamPacker::Serialize (Vector3f& value, float maxDelta)
{
	Serialize(value.x, maxDelta);
	Serialize(value.y, maxDelta);
	Serialize(value.z, maxDelta);
}

void BitstreamPacker::Serialize (Quaternionf& value, float maxDelta)
{
	Serialize(value.x, maxDelta);
	Serialize(value.y, maxDelta);
	Serialize(value.z, maxDelta);
	Serialize(value.w, maxDelta);
}

void BitstreamPacker::Serialize (std::string& value)
{
	if (m_IsReading)
	{
		if (m_WriteDeltaData != NULL)
		{
			std::string oldData;
			ReadPackState(oldData);
			
			bool readValue = false;
			m_BitStream->Read(readValue);
			if (readValue)
			{
				char rawOut[4096];
				if (StringCompressor::Instance()->DecodeString(rawOut, 4096, m_BitStream))
				{
					value = rawOut;
				}
				else
				{
					value = oldData;
				}
			}
			else
				value = oldData;
			
			WritePackState (value);
		}
		else
		{
			char rawOut[4096];
			if (StringCompressor::Instance()->DecodeString(rawOut, 4096, m_BitStream))
			{
				value = rawOut;
			}
			else
				value = string();
		}
	}
	else
	{
		if (m_WriteDeltaData != NULL)
		{
			string oldData;
			ReadPackState(oldData);
			
			if (value != oldData)
			{
				m_BitStream->Write1();
				StringCompressor::Instance()->EncodeString(value.c_str(), 4096, m_BitStream);
				WritePackState (value);
				m_IsDifferent |= true;
			}
			else
			{
				m_BitStream->Write0();
			}
			WritePackState (value);
		}
		else
		{
			m_BitStream->Write(value);
			m_IsDifferent |= true;
		}
	}
}

void BitstreamPacker::Serialize (char* value, int& valueLength)
{
	if (m_IsReading)
	{
		if (m_WriteDeltaData != NULL)
		{
			char* oldData = NULL;
			ReadPackState(oldData, valueLength);
			
			// Check if this contains a new state or if we should reuse the old one
			bool readValue = false;
			m_BitStream->Read(readValue);
			if (readValue)
				m_NoOutOfBounds &= m_BitStream->Read(value, valueLength);
			else
				value = oldData;
			WritePackState (value, valueLength);
			if (oldData != NULL) 
				delete[] oldData;
		}
		else
		{
			m_NoOutOfBounds &= m_BitStream->Read(value, valueLength);
		}
	}
	else
	{
		if (m_WriteDeltaData != NULL)
		{
			char* oldData = NULL;
			ReadPackState(oldData, valueLength);
			
			// If the state has changed, then pack it, if not then reuse old state
			if (strcmp(value,oldData) != 0)
			{
				m_BitStream->Write1();	// New state
				m_BitStream->Write(value, valueLength);
				WritePackState (value, valueLength);
				m_IsDifferent |= true;
			}
			else
			{
				m_BitStream->Write0();	// No change
				WritePackState (oldData, valueLength);
			}
			if (oldData != NULL)
				delete[] oldData;
		}
		else
		{
			m_BitStream->Write(value, valueLength);
			m_IsDifferent |= true;
		}
	}
}


BitstreamPacker::BitstreamPacker (RakNet::BitStream& stream, std::vector<UInt8>* delta, UInt8* readData, int readSize, bool reading)
{	
	Init(stream, delta, readData, readSize, reading);
}

BitstreamPacker::BitstreamPacker (RakNet::BitStream& stream, bool reading)
{
	Init(stream, NULL, NULL, 0, reading);
}

void BitstreamPacker::Init(RakNet::BitStream& stream, std::vector<UInt8>* delta, UInt8* readData, int readSize, bool reading)
{
	m_BitStream = &stream;
	m_DeltaReadPos = 0;
	m_DeltaWritePos = 0;
	m_WriteDeltaData = delta;
	m_DeltaReadSize = readSize;
	m_ReadDeltaData = readData;
	m_IsDifferent = false;
	m_IsReading = reading;
	m_NoOutOfBounds = true;	
}

#endif
