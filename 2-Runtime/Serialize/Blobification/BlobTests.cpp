#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS && !(UNITY_LINUX && UNITY_64)
#include "External/UnitTest++/src/UnitTest++.h"
#include <limits>
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Allocator/MemoryManager.h"
#include "Runtime/Serialize/SerializeTraits.h"
#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/Blobification/BlobWrite.h"

template<typename TYPE> TYPE* Construct()
{
	#if ENABLE_MEMORY_MANAGER
	void* _where = GetMemoryManager().Allocate (sizeof(TYPE), ALIGN_OF(TYPE), MemLabelId(kMemNewDeleteId, NULL), kAllocateOptionNone, "Construct");
	#else
	void* _where = malloc(sizeof(TYPE));
	#endif
	return new(_where) TYPE;
}

template<typename TYPE> TYPE* ConstructArray(size_t num)
{
	#if ENABLE_MEMORY_MANAGER
	void* _where = GetMemoryManager().Allocate (sizeof(TYPE)*num, ALIGN_OF(TYPE), MemLabelId(kMemNewDeleteId, NULL), kAllocateOptionNone, "ConstructArray");
	#else
	void* _where = malloc(sizeof(TYPE)*num);
	#endif
	return new(_where) TYPE[num];
}

template<typename TYPE> void Destruct(TYPE* p)
{
	#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().Deallocate (p, MemLabelId(kMemNewDeleteId, NULL));
	#else
	free(p);
	#endif
}


struct SampleDataA
{
	enum {
		kLastIndex = 20
	};

	int                       intValue1;
	math::float4              float4Value; // (intentially unaligned)
	Vector3f                  vector3;

	mecanim::uint32_t         index[kLastIndex];

	OffsetPtr<float> nullPtr;

	OffsetPtr<float> floatPtr;
	
	mecanim::uint32_t         arraySize;
	OffsetPtr<float> array;

	mecanim::uint32_t       emptyArraySize;
	OffsetPtr<math::float4> emptyArray;
	
	int                      intValue2;

	DECLARE_SERIALIZE(SampleData)
};

struct SampleData
{
	int                       intValue1;						// 0
	math::float4              float4Value;						// 16 (intentially unaligned) 
	Vector3f                  vector3;							// 32

	OffsetPtr<float> nullPtr;									// 44
	
	OffsetPtr<float> floatPtr;									// 52
	
	mecanim::uint32_t arraySize;								// 60
	OffsetPtr<double> array;									// 64

	mecanim::uint32_t       emptyArraySize;						// 72
	OffsetPtr<math::float4> emptyArray;							// 76

	mecanim::uint32_t       sampleDataASize;					// 84
	OffsetPtr<SampleDataA>  sampleDataA;						// 88

	mecanim::uint32_t                   sampleDataAHandleSize;	// 96
	OffsetPtr<OffsetPtr<SampleDataA> >  sampleDataAHandle;		// 100
	
	int                     intValue2;							// 108

	DECLARE_SERIALIZE(SampleData)
};



template<class TransferFunction> inline
void SampleDataA::Transfer(TransferFunction& transfer)
{
	TRANSFER(intValue1);
	TRANSFER(float4Value);
	TRANSFER(vector3);
	
	STATIC_ARRAY_TRANSFER(mecanim::uint32_t, index, kLastIndex);
	
	TRANSFER(nullPtr);
	TRANSFER(floatPtr);

	TRANSFER_BLOB_ONLY(arraySize);
	MANUAL_ARRAY_TRANSFER2(float, array, arraySize);
	

	TRANSFER_BLOB_ONLY(emptyArraySize);
	MANUAL_ARRAY_TRANSFER2(math::float4, emptyArray, emptyArraySize);
	
	TRANSFER(intValue2);
}


template<class TransferFunction> inline
void SampleData::Transfer(TransferFunction& transfer)
{
	TRANSFER(intValue1);
	TRANSFER(float4Value);
	TRANSFER(vector3);
	
	TRANSFER(nullPtr);
	TRANSFER(floatPtr);

	TRANSFER_BLOB_ONLY(arraySize);
	MANUAL_ARRAY_TRANSFER2(double, array, arraySize);
	

	TRANSFER_BLOB_ONLY(emptyArraySize);
	MANUAL_ARRAY_TRANSFER2(math::float4, emptyArray, emptyArraySize);

	TRANSFER_BLOB_ONLY(sampleDataASize);
	MANUAL_ARRAY_TRANSFER2(SampleDataA, sampleDataA, sampleDataASize);

	TRANSFER_BLOB_ONLY(sampleDataAHandleSize);
	MANUAL_ARRAY_TRANSFER2(OffsetPtr<SampleDataA>, sampleDataAHandle, sampleDataAHandleSize);

	TRANSFER(intValue2);
}

static void SetupTestDataA (SampleDataA& sourceData)
{
	sourceData.intValue1 = 1;
	sourceData.float4Value = math::float4(1, 2, 3, 4);
	sourceData.vector3 = Vector3f(1,2,3);

	mecanim::uint32_t i;
	for(i=0; i<SampleDataA::kLastIndex;i++)
		sourceData.index[i] = i;

	sourceData.nullPtr = NULL;
	sourceData.floatPtr = new float;
	*sourceData.floatPtr = 5.5F;
	
	sourceData.emptyArraySize = 0;
	sourceData.emptyArray = NULL;
	
	sourceData.arraySize = 3;
	sourceData.array = new float[3];
	sourceData.array[0] = 6.5f;
	sourceData.array[1] = 7.5f;
	sourceData.array[2] = 8.5f;
	sourceData.intValue2 = 2;
}

static void DeleteTestDataA (SampleDataA& sourceData)
{
	delete[] sourceData.array.Get();
	delete sourceData.floatPtr.Get();
}


static void SetupTestData (SampleData& sourceData)
{
	sourceData.intValue1 = 1;
	sourceData.float4Value = math::float4(1, 2, 3, 4);
	sourceData.vector3 = Vector3f(1,2,3);
	sourceData.nullPtr = NULL;
	sourceData.floatPtr = new float;
	*sourceData.floatPtr = 5.5F;
	
	sourceData.emptyArraySize = 0;
	sourceData.emptyArray = NULL;
	
	sourceData.arraySize = 3;
	sourceData.array = new double[3];
	sourceData.array[0] = 6.5;
	sourceData.array[1] = 7.5;
	sourceData.array[2] = 8.5;
	sourceData.intValue2 = 2;

	sourceData.sampleDataASize = 4;

	sourceData.sampleDataA = ConstructArray<SampleDataA>(sourceData.sampleDataASize);
	for(int i=0;i<sourceData.sampleDataASize;i++)
	{
		SetupTestDataA(sourceData.sampleDataA[i]);
	}
	
	sourceData.sampleDataAHandleSize = 2;
	sourceData.sampleDataAHandle = new OffsetPtr<SampleDataA> [2];
	sourceData.sampleDataAHandle[0] = Construct<SampleDataA>();
	SetupTestDataA(*sourceData.sampleDataAHandle[0]);
	sourceData.sampleDataAHandle[1] = Construct<SampleDataA>();
	SetupTestDataA(*sourceData.sampleDataAHandle[1]);
}


static void DeleteTestData (SampleData& sourceData)
{
	for(int i=0;i<sourceData.sampleDataASize;i++)
		DeleteTestDataA (sourceData.sampleDataA[i]);
	
	Destruct (sourceData.sampleDataA.Get());
	Destruct (sourceData.sampleDataAHandle[0].Get());
	Destruct (sourceData.sampleDataAHandle[1].Get());

	delete sourceData.floatPtr.Get();
	delete[] sourceData.array.Get();
	delete[] sourceData.sampleDataAHandle.Get();
}


static void TestDataA (SampleDataA& deserialized)
{
	CHECK (reinterpret_cast<UInt32>(&deserialized) % ALIGN_OF(SampleDataA) == 0 );
	CHECK (deserialized.intValue1 == 1);
	CHECK (dot(deserialized.float4Value - math::float4(1, 2, 3, 4)) == math::float1::zero());
	
	mecanim::uint32_t i;
	for(i=0; i<SampleDataA::kLastIndex;i++)
		CHECK (deserialized.index[i] == i);
	
	CHECK (deserialized.nullPtr.IsNull());
	
	float ptr = *deserialized.floatPtr;
	CHECK (ptr == 5.5F);
	
	float* array = deserialized.array.Get();
	CHECK (array[0] == 6.5F);
	CHECK (array[1] == 7.5F);
	CHECK (array[2] == 8.5F);
	
	CHECK (deserialized.emptyArray.IsNull());
	CHECK (deserialized.emptyArraySize == 0);
	
	
	CHECK (deserialized.vector3 == Vector3f(1,2,3));
	CHECK (deserialized.intValue2 == 2);
}


static void TestData (SampleData& deserialized)
{
	CHECK (reinterpret_cast<UInt32>(&deserialized) % ALIGN_OF(SampleData) == 0);
	CHECK (deserialized.intValue1 == 1);
	CHECK (dot(deserialized.float4Value - math::float4(1, 2, 3, 4)) == math::float1::zero() );
	
	CHECK (deserialized.nullPtr.IsNull());
	
	float ptr = *deserialized.floatPtr;
	CHECK (ptr == 5.5F);
	
	double* array = deserialized.array.Get();
	CHECK (array[0] == 6.5);
	CHECK (array[1] == 7.5);
	CHECK (array[2] == 8.5);
	
	CHECK (deserialized.emptyArray.IsNull());
	CHECK (deserialized.emptyArraySize == 0);
	
	CHECK (deserialized.vector3 == Vector3f(1,2,3));
	CHECK (deserialized.intValue2 == 2);

	CHECK (deserialized.sampleDataASize == 4);
	for(int i=0;i<deserialized.sampleDataASize;i++)
	{
		TestDataA(deserialized.sampleDataA[i]);
	}

	CHECK (deserialized.sampleDataAHandleSize == 2);
	for(int i=0;i<deserialized.sampleDataAHandleSize;i++)
	{
		TestDataA(*deserialized.sampleDataAHandle[i]);
	}
}

// This test crashes the linux-amd64 editor right now
SUITE (BlobTests)
{
	TEST (Blobification_BlobPtrs)
	{
		SampleData sourceData;
		SetupTestData (sourceData);
		TestData(sourceData);

		// Generate blob
		BlobWrite::container_type data;
		BlobWrite blobWrite (data, kNoTransferInstructionFlags, kBuildNoTargetPlatform);
		blobWrite.Transfer(sourceData, "Base");
		TestData(*reinterpret_cast<SampleData*> (data.begin()));

		// Generate blob with reduce copy
		BlobWrite::container_type dataReduced;
		BlobWrite blobWriteReduce (dataReduced, kNoTransferInstructionFlags, kBuildNoTargetPlatform);
		blobWriteReduce.SetReduceCopy(true);
		blobWriteReduce.Transfer(sourceData, "Base");
		TestData(*reinterpret_cast<SampleData*> (dataReduced.begin()));
	
		// Ensure reduced blob is actually smaller.
		CHECK (dataReduced.size() < data.size());
	
		// Ensure that 64 bit data is larger than non-64 bit data
		BlobWrite::container_type data64;
		BlobWrite blobWrite64 (data64, kNoTransferInstructionFlags, kBuildStandaloneWin64Player);
		blobWrite64.Transfer(sourceData, "Base");

		BlobWrite::container_type data32;
		BlobWrite blobWrite32 (data32, kNoTransferInstructionFlags, kBuildStandaloneWinPlayer);
		blobWrite32.Transfer(sourceData, "Base");
		CHECK (data64.size() > data32.size());
	
		DeleteTestData (sourceData);
	}

	TEST (Blobification_OffsetPtr)
	{
		OffsetPtr<size_t>* ptrHigh = new OffsetPtr<size_t>;
		OffsetPtr<size_t>* ptrLow = new OffsetPtr<size_t>;

		size_t* ptrH = reinterpret_cast<size_t*>(std::numeric_limits<size_t>::max()-4);
		size_t* ptrL = reinterpret_cast<size_t*>(4);

		ptrHigh->reset(ptrH);
		ptrLow->reset(ptrL);

		size_t h = reinterpret_cast<size_t>(ptrHigh->Get());
		size_t l = reinterpret_cast<size_t>(ptrLow->Get());


		CHECK (h == std::numeric_limits<size_t>::max()-4);
		CHECK (l == 4);

		delete ptrHigh;
		delete ptrLow;
	}
}

#endif //ENABLE_UNIT_TESTS
