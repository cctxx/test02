#ifndef MEMORY_STREAM_H
#define MEMORY_STREAM_H

#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "External/PhysX/builds/SDKs/Foundation/include/NxStream.h"

class MemoryStream : public NxStream
	{
	public:
								MemoryStream(NxU8* data,NxU32 len);
	virtual				~MemoryStream();

	virtual		NxU8			readByte()								const;
	virtual		NxU16			readWord()								const;
	virtual		NxU32			readDword()								const;
	virtual		float			readFloat()								const;
	virtual		double			readDouble()							const;
	virtual		void			readBuffer(void* buffer, NxU32 size)	const;

	virtual		NxStream&		storeByte(NxU8 b);
	virtual		NxStream&		storeWord(NxU16 w);
	virtual		NxStream&		storeDword(NxU32 d);
	virtual		NxStream&		storeFloat(NxReal f);
	virtual		NxStream&		storeDouble(NxF64 f);
	virtual		NxStream&		storeBuffer(const void* buffer, NxU32 size);

	bool                    mMyAlloc;
	mutable NxU32           mReadLoc;
	NxU32                   mWriteLoc;
	NxU32                   mLen;
	NxU8*		                mBuffer;         // buffer
	};

#endif
