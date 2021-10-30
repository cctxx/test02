#ifndef MDFOURGENERATOR_H
#define MDFOURGENERATOR_H

#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Math/Color.h"
#include <string>

#if UNITY_OSX || UNITY_LINUX
#include <openssl/md4.h>
#elif UNITY_WIN
#include <WinCrypt.h>
#else
#error Unknown platform
#endif

struct MdFour
{
	unsigned char bytes[16];

	//MdFour (UInt32 a, UInt32 b, UInt32 c, UInt32 d) { data[0] = a; data[1] = b; data[2] = c; data[3] = d; }
	MdFour (const char* src, size_t len=16);	
	MdFour ();
	void Reset();

	int CompareTo(const MdFour& other) const ;
	
	bool operator == (const MdFour& other) const { return CompareTo(other) == 0;}
	bool operator != (const MdFour& other) const { return CompareTo(other) != 0; }
	bool operator < (const MdFour& other) const { return CompareTo(other) < 0; }

	operator const char*() { return reinterpret_cast<char*>(bytes); }

	UInt32 PackToUInt32 () const {
		UInt32 const* pu = reinterpret_cast<UInt32 const*> (bytes);
		return pu[0] ^ pu[1] ^ pu[2] ^ pu[3];
	}

	DECLARE_SERIALIZE_NO_PPTR (MdFour)
};

std::string MdFourToString(const MdFour& digest) ;
MdFour StringToMdFour(const std::string& input) ;


template<class TransferFunction>
void MdFour::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion(2);
	#if UNITY_EDITOR
	if (transfer.IsCurrentVersion())
	{
	#endif 
		unsigned size = 16;
		transfer.TransferTypeless (&size, "md4 hash", kHideInEditorMask);
		transfer.TransferTypelessData (size, bytes);
	#if UNITY_EDITOR
	}
	else
	{
		UInt32* data=reinterpret_cast<UInt32*>(bytes);
		// When transferring md4 hashes we shouldn't swap bytes.
		// UInt32 already convert endianess by default so we convert it two times to keep it the same :)
		if (transfer.ConvertEndianess ())
		{
			if (transfer.IsReading())
			{
				TRANSFER (data[0]);	
				TRANSFER (data[1]);	
				TRANSFER (data[2]);	
				TRANSFER (data[3]);	
				SwapEndianBytes (data[0]);
				SwapEndianBytes (data[1]);
				SwapEndianBytes (data[2]);
				SwapEndianBytes (data[3]);
			}
			else
			{
				UInt32 temp = data[0];
				SwapEndianBytes (temp);
				transfer.Transfer (temp, "data[0]");

				temp = data[1];
				SwapEndianBytes (temp);
				transfer.Transfer (temp, "data[1]");

				temp = data[2];
				SwapEndianBytes (temp);
				transfer.Transfer (temp, "data[2]");

				temp = data[3];
				SwapEndianBytes (temp);
				transfer.Transfer (temp, "data[3]");	
			}
		}
		else
		{
			TRANSFER (data[0]);	
			TRANSFER (data[1]);	
			TRANSFER (data[2]);	
			TRANSFER (data[3]);	
		}
	}
	#endif
}

class MdFourGenerator {
public:
	MdFourGenerator();

	void Feed(const char* bytes, int length);
	void Feed(SInt32 data); 
	void Feed(UInt32 data); 
	void Feed(UInt64 data); 
	void Feed(SInt64 data); 
	void Feed(ColorRGBA32 data); // used to feed ColorRGBA32s to the hash, which are stored as *unswapped* UINT32s
	void Feed(float data); 
	void Feed(double data);
	void Feed(std::string data); 
	void Feed(char const* string); 
	void FeedFromFile(std::string pathName)	;
	MdFour Finish();

private:
	#if UNITY_OSX || UNITY_LINUX
	MD4_CTX m_Acc;
	#elif UNITY_WIN
	HCRYPTPROV	m_CryptProvider;
	HCRYPTHASH	m_CryptHash;
	#else
	#error Unknown platform
	#endif
	bool m_Done;
};

MdFour MdFourFile(std::string filename);

#endif
