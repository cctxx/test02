#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "ScriptingTypeRegistry.h"
#include "ScriptingBackendApi.h"

ScriptingTypeRegistry::ScriptingTypeRegistry(IScriptingTypeProvider* scriptingTypeProvider)
	: m_ScriptingTypeProvider(scriptingTypeProvider)
{
}

ScriptingTypePtr ScriptingTypeRegistry::GetType(const char* namespaze, const char* name)
{
	NameSpaceAndNamePair key = std::make_pair(namespaze,name);

	Cache::iterator i = m_Cache.find(key);
	if (i != m_Cache.end())
		return i->second;

	BackendNativeType nativetype = m_ScriptingTypeProvider->NativeTypeFor(namespaze, name);
	ScriptingTypePtr type = m_ScriptingTypeProvider->Provide(nativetype);

	m_Cache.insert(std::make_pair(key,type));
	return type;
}

ScriptingTypePtr ScriptingTypeRegistry::GetType(BackendNativeType nativeType)
{
	NativeTypeCache::iterator i = m_NativeTypeCache.find(ScriptingObjectToComPtr(nativeType));
	if (i != m_NativeTypeCache.end())
		return i->second;

	ScriptingTypePtr type = m_ScriptingTypeProvider->Provide(nativeType);
	
	m_NativeTypeCache.insert(std::make_pair(ScriptingObjectToComPtr(nativeType), type));
	return type;
}

ScriptingTypePtr ScriptingTypeRegistry::GetType(ScriptingObjectPtr systemTypeInstance)
{
	return scripting_class_from_systemtypeinstance(systemTypeInstance, *this);
}

void ScriptingTypeRegistry::InvalidateCache()
{
	m_Cache.clear();
	m_NativeTypeCache.clear();
}

#if DEDICATED_UNIT_TEST_BUILD
#include "External/UnitTest++/src/UnitTest++.h"
#include <queue>
#include "Tests/ScriptingBackendApi_Tests.h"
using std::queue;

class ScriptingTypeProvider_test : public IScriptingTypeProvider
{
public:
	BackendNativeType NativeTypeFor(const char* namespaze, const char* name)
	{
		BackendNativeType ptr = provideThis.front();
		provideThis.pop();
		return ptr;
	}

	ScriptingTypePtr Provide(BackendNativeType nativeType)
	{
		return (ScriptingTypePtr)nativeType;
	}

	void Release(ScriptingTypePtr ptr) {}

	queue<BackendNativeType> provideThis;
};

struct MyFixture
{
	ScriptingTypeProvider_test typeProvider;
	
	ScriptingType type1;
	ScriptingType type2;
	int nativeType1;
	int nativeType2;
	FakeSystemTypeInstance systemTypeInstance1;
	FakeSystemTypeInstance systemTypeInstance2;
};

TEST_FIXTURE(MyFixture,ScriptingTypeRegistryReturnsSameTypeWhenAskedForSameName)
{
	typeProvider.provideThis.push(&type1);
	
	ScriptingTypeRegistry registry(&typeProvider);
	ScriptingTypePtr t1 = registry.GetType("mynamespace","mytype");
	ScriptingTypePtr t2 = registry.GetType("mynamespace","mytype");
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type1,t2);
}

TEST_FIXTURE(MyFixture,ScriptingTypeRegistryReturnsDifferentTypeWhenAskedForDifferentName)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);
	
	ScriptingTypeRegistry registry(&typeProvider);
	ScriptingTypePtr t1 = registry.GetType("mynamespace", "mytype");
	ScriptingTypePtr t2 = registry.GetType("mynamespace", "myothertype");
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}

TEST_FIXTURE(MyFixture,ScriptingTypeRegistryReturnsDifferentTypeWhenAskedForTypesWithDifferentNamespace)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);
	
	ScriptingTypeRegistry registry(&typeProvider);
	ScriptingTypePtr t1 = registry.GetType("mynamespace", "mytype");
	ScriptingTypePtr t2 = registry.GetType("myothernamespace", "mytype");
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}

TEST_FIXTURE(MyFixture,ScriptingTypeRegistryInvalidateCacheCausesNewTypeBeingProduced)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);
	
	ScriptingTypeRegistry registry(&typeProvider);
	ScriptingTypePtr t1 = registry.GetType("mynamespace","mytype");
	registry.InvalidateCache();
	ScriptingTypePtr t2 = registry.GetType("mynamespace", "mytype");
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}

TEST_FIXTURE(MyFixture,AskingForTheSameNativeTypeTwiceReturnsSameScriptingTypeTwice)
{
	typeProvider.provideThis.push(&type1);

	ScriptingTypeRegistry registry(&typeProvider);
	
	ScriptingTypePtr t1 = registry.GetType(&nativeType1);
	ScriptingTypePtr t2 = registry.GetType(&nativeType1);
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type1,t2);
}

TEST_FIXTURE(MyFixture,AskingForTwoDifferentNativeTypesReturnsDifferentScriptingTypes)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);

	ScriptingTypeRegistry registry(&typeProvider);
	ScriptingTypePtr t1 = registry.GetType(&nativeType1);
	ScriptingTypePtr t2 = registry.GetType(&nativeType2);
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}

TEST_FIXTURE(MyFixture,InvalidateCacheCausesNewTypeToBeReturned)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);

	ScriptingTypeRegistry registry(&typeProvider);
	
	ScriptingTypePtr t1 = registry.GetType(&nativeType1);
	registry.InvalidateCache();
	ScriptingTypePtr t2 = registry.GetType(&nativeType1);
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}

TEST_FIXTURE(MyFixture,AskingForTypeOfSystemTypeInstanceTwiceReturnsSameType)
{
	typeProvider.provideThis.push(&type1);
	ScriptingTypeRegistry registry(&typeProvider);
	
	systemTypeInstance1.nativeType = &nativeType1;
	ScriptingTypePtr t1 = registry.GetType((ScriptingObjectPtr) &systemTypeInstance1);
	ScriptingTypePtr t2 = registry.GetType((ScriptingObjectPtr) &systemTypeInstance1);
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type1,t2);
}
/*
TEST_FIXTURE(MyFixture,AskingForDifferentSystemTypeInstancesReturnsDifferentTypes)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);
	ScriptingTypeRegistry registry(&typeProvider);
	
	systemTypeInstance1.nativeType = &nativeType1;
	systemTypeInstance1.nativeType = &nativeType2;
	ScriptingTypePtr t1 = registry.GetType((ScriptingObjectPtr) &systemTypeInstance1);
	ScriptingTypePtr t2 = registry.GetType((ScriptingObjectPtr) &systemTypeInstance2);
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}
*/
TEST_FIXTURE(MyFixture,InvalidateCacheCausesSystemTypeInstanceQueryToProvideNewType)
{
	typeProvider.provideThis.push(&type1);
	typeProvider.provideThis.push(&type2);
	ScriptingTypeRegistry registry(&typeProvider);
	
	systemTypeInstance1.nativeType = &nativeType1;
	ScriptingTypePtr t1 = registry.GetType((ScriptingObjectPtr) &systemTypeInstance1);
	registry.InvalidateCache();
	ScriptingTypePtr t2 = registry.GetType((ScriptingObjectPtr) &systemTypeInstance1);
	CHECK_EQUAL(&type1,t1);
	CHECK_EQUAL(&type2,t2);
}

#endif
#endif
