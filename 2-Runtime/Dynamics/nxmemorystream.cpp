#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "nxmemorystream.h"
#include "Runtime/Allocator/MemoryMacros.h"


#define BLOCKSIZE 4096

MemoryStream::MemoryStream(NxU8* data,NxU32 len)
{
	if ( data )
	{
		mBuffer = data;
		mMyAlloc = false;
	}
	else
	{
		mBuffer = (NxU8 *) UNITY_MALLOC(kMemFile, sizeof(NxU8)*len);
		mMyAlloc = true;
	}

	mReadLoc  = 0;
	mWriteLoc = 0;
	mLen      = len;
}

MemoryStream::~MemoryStream(void)
{
	if ( mMyAlloc )
	{
		UNITY_FREE(kMemFile, mBuffer);
	}
}

NxU8			MemoryStream::readByte(void) const
{
	NxU8 ret = 0;
	readBuffer(&ret, sizeof(NxU8) );
	return ret;
}

NxU16			MemoryStream::readWord(void) const
{
	NxU16 ret = 0;
	readBuffer(&ret,sizeof(NxU16));
	return ret;
}

NxU32			MemoryStream::readDword(void)	const
{
	NxU32 ret = 0;
	readBuffer(&ret,sizeof(NxU32));
	return ret;
}

float			MemoryStream::readFloat(void)	const
{
	float ret = 0;
	readBuffer(&ret,sizeof(float));
	return ret;
}

double		MemoryStream::readDouble(void) const
{
	double ret = 0;
	readBuffer(&ret,sizeof(double));
	return ret;
}

void			MemoryStream::readBuffer(void* buffer, NxU32 size)	const
{
	if ( (mReadLoc+size) <= mLen )
	{
		memcpy( buffer, &mBuffer[mReadLoc], size );
		mReadLoc+=size;
	}
}

NxStream&		MemoryStream::storeByte(NxU8 b)
{
	storeBuffer(&b, sizeof(NxU8) );
	return *this;
}

NxStream&		MemoryStream::storeWord(NxU16 w)
{
	storeBuffer(&w,sizeof(NxU16));
	return *this;
}

NxStream&		MemoryStream::storeDword(NxU32 d)
{
	storeBuffer(&d,sizeof(NxU32));
	return *this;
}

NxStream&		MemoryStream::storeFloat(NxReal f)
{
	storeBuffer(&f,sizeof(NxReal));
	return *this;
}

NxStream&		MemoryStream::storeDouble(NxF64 f)
{
	storeBuffer(&f,sizeof(NxF64));
	return *this;
}

NxStream&		MemoryStream::storeBuffer(const void* buffer, NxU32 size)
{

	if ( (mWriteLoc+size) >= mLen )
	{
		unsigned int blocksize = BLOCKSIZE;

		if ( size > BLOCKSIZE ) blocksize = size*2;

		NxU8 *buf = (NxU8 *)UNITY_MALLOC(kMemFile, mLen+blocksize);
		memcpy(buf,mBuffer,mWriteLoc);
		UNITY_FREE(kMemFile, mBuffer);
		mBuffer = buf;
		mLen+=blocksize;

	}
  memcpy(&mBuffer[mWriteLoc], buffer, size );
	mWriteLoc+=size;

	return *this;
}

#endif //ENABLE_PHYSICS