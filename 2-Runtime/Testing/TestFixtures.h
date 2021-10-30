#pragma once

#include <memory>
#include <algorithm>
#include <map>

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/WriteTypeToBuffer.h"
#include "Runtime/BaseClasses/ObjectDefines.h"


/// Test fixture that allows accumulating objects that are cleaned up automatically
/// when the test finishes.
class TestFixtureBase
{
public:

	~TestFixtureBase()
	{
		std::for_each (m_Objects.begin (), m_Objects.end (), DestroySingleObject);
	}

	template<typename T>
	T* AddObjectToCleanup (T* object)
	{
		if (object != 0)
		{
			m_Objects.push_back (object);
		}
		return object;
	}

	template<typename X>
	X* NewTestObject ()
	{
		X* result = NEW_OBJECT_RESET_AND_AWAKE (X);
		AddObjectToCleanup (result);
		return result;
	}

private:
	std::vector<Object*> m_Objects;
};


/// Fixture that automatically creates an object of type "T".
template<typename T>
class ObjectTestFixture : public TestFixtureBase
{
public:
	ObjectTestFixture ()
	{
		m_ObjectUnderTest = NewTestObject<T> ();
	}

protected:

	T* m_ObjectUnderTest;
};


/// Fixture that simplifies serializing and deserializing an object and provides various
/// helpers to simplify setting up tests for transfers.
template<class T>
struct SerializationTestFixture : public TestFixtureBase, public GenerateIDFunctor
{
	T m_ObjectUnderTest;
	TypeTree m_TypeTree;
	dynamic_array<UInt8> m_Buffer;
	int m_TransferOptions;

	SerializationTestFixture ()
		: m_TransferOptions (0)
	{
	}

	void GenerateTypeTree ()
	{
		ProxyTransfer proxyTransfer (m_TypeTree, m_TransferOptions, &m_ObjectUnderTest, sizeof (T));
		proxyTransfer.Transfer (m_ObjectUnderTest, "Base");
	}

	void WriteObjectToBuffer ()
	{
		WriteTypeToVector (m_ObjectUnderTest, &m_Buffer, m_TransferOptions);
	}

	void DoSafeBinaryTransfer ()
	{
		#if SUPPORT_SERIALIZED_TYPETREES

		GenerateTypeTree();
		WriteObjectToBuffer ();

		SafeBinaryRead m_Transfer;
		CachedReader& reader = m_Transfer.Init (m_TypeTree, 0, m_Buffer.size (), 0);
		MemoryCacheReader memoryCache (m_Buffer);

		reader.InitRead (memoryCache, 0, m_Buffer.size ());
		m_Transfer.Transfer (m_ObjectUnderTest, "Base");

		reader.End ();

		#endif
	}

	void DoTextTransfer ()
	{
		#if SUPPORT_TEXT_SERIALIZATION
		YAMLWrite write (m_TransferOptions);
		write.Transfer (m_ObjectUnderTest, "Base");

		string text;
		write.OutputToString(text);

		YAMLRead read (text.c_str (), text.size (), m_TransferOptions);
		read.Transfer (m_ObjectUnderTest, "Base");
		#endif
	}

	/// @name RemapPPtrTransfer Helpers
	/// @{

	typedef std::map<SInt32, SInt32> PPtrRemapTable;
	PPtrRemapTable m_PPtrRemapTable;

	void DoRemapPPtrTransfer (bool readPPtrs = true)
	{
		RemapPPtrTransfer transfer (m_TransferOptions, readPPtrs);
		transfer.SetGenerateIDFunctor (this);
		transfer.Transfer (m_ObjectUnderTest, "Base");
	}

	void AddPPtrRemap (SInt32 oldInstanceID, SInt32 newInstanceID)
	{
		m_PPtrRemapTable[oldInstanceID] = newInstanceID;
	}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag)
	{
		PPtrRemapTable::const_iterator iter = m_PPtrRemapTable.find (oldInstanceID);
		if (iter == m_PPtrRemapTable.end ())
			return oldInstanceID;

		return iter->second;
	}

	/// @}
};


/// Define a "<Name>Test" class with a transfer function.
/// @note The class is not derived from Object.
#define DEFINE_TRANSFER_TEST_FIXTURE(NAME) \
	struct NAME ## Test \
	{ \
		DECLARE_SERIALIZE (NAME ## Test) \
	}; \
	typedef SerializationTestFixture<NAME ## Test> NAME ## TestFixture; \
	template<typename TransferFunction> \
	void NAME ## Test::Transfer (TransferFunction& transfer)
