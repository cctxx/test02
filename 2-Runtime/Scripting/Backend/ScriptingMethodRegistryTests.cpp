#include "UnityPrefix.h"
#if DEDICATED_UNIT_TEST_BUILD
#include "ScriptingMethodRegistry.h"
#include "External/UnitTest++/src/UnitTest++.h"
#include "ScriptingMethodFactory.h"
#include "Tests/ScriptingBackendApi_Tests.h"
#include <queue>
#include <set>
#include <vector>
using std::queue;
using std::vector;

class ScriptingMethodFactory_Dummy : public IScriptingMethodFactory
{
public:
	queue<ScriptingMethodPtr> produceThis;
	ScriptingTypePtr returnNullForRequestson;
	int produceCallCount;

	ScriptingMethodFactory_Dummy() : produceCallCount(0) {}

	virtual	ScriptingMethodPtr Produce(ScriptingTypePtr klass, const char* name, int searchFilter)
	{
		produceCallCount++;

		if (returnNullForRequestson == klass)
			return NULL;

		if (produceThis.size()==0)
			return NULL;

		ScriptingMethodPtr result = produceThis.front();
		
		if (result && !MethodDescriptionMatchesSearchFilter(searchFilter, result->instance, result->args))
			return NULL;

		produceThis.pop();
		return result;
	}

	virtual ScriptingMethodPtr Produce(void* nativePtr)
	{
		return Produce(NULL,NULL, 0);
	}

	virtual void Release(ScriptingMethodPtr method)
	{
	}
};

struct Fixture
{
    ScriptingMethodFactory_Dummy methodFactory;
	ScriptingMethodRegistry registry;
    
	ScriptingType normalType;

	ScriptingMethod method1;
	ScriptingMethod method2;

	Fixture() : registry(&methodFactory,NULL) 
	{
	}
};

TEST_FIXTURE(Fixture,RegistryProducesNullForNonExistingMethod)
{
	CHECK_EQUAL((ScriptingMethodPtr)NULL,registry.GetMethod(&normalType,"Method1"));
}

TEST_FIXTURE(Fixture,RegistryProducesMethod)
{
	methodFactory.produceThis.push(&method1);
	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1"));
}

TEST_FIXTURE(Fixture,RegistryReturnsSameMethodWhenQueriedTwiceWithSameArguments)
{
	methodFactory.produceThis.push(&method1);

	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1"));
	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1"));
}

TEST_FIXTURE(Fixture,RegistryReturnsDifferentMethodsWhenQueriedForTwoDifferentClasses)
{
	methodFactory.produceThis.push(&method1);
	methodFactory.produceThis.push(&method2);

	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1"));
	CHECK_EQUAL(&method2,registry.GetMethod((ScriptingTypePtr)124,"Method1"));
}

TEST_FIXTURE(Fixture,RegistryReturnsDifferentMethodsForDifferentSearchFilters)
{
	methodFactory.produceThis.push(&method1);
	methodFactory.produceThis.push(&method2);
	method1.instance = false;
	method2.instance = true;

	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1", ScriptingMethodRegistry::kStaticOnly));
	CHECK_EQUAL(&method2,registry.GetMethod(&normalType,"Method1", ScriptingMethodRegistry::kInstanceOnly));
}

TEST_FIXTURE(Fixture,RegistryDoesNotFindMethodsNotMatchingSearchFilter)
{
	methodFactory.produceThis.push(&method1);
	method1.instance = true;

	ScriptingMethodPtr found = registry.GetMethod(&normalType,"Method1", ScriptingMethodRegistry::kStaticOnly);
	if (found==NULL)
		CHECK_EQUAL(1,1);
	else
		CHECK_EQUAL(1,0);
}

TEST_FIXTURE(Fixture,RegistryRemembersNonFoundMethods)
{
	methodFactory.produceThis.push(NULL);
	methodFactory.produceThis.push(NULL);
	
	registry.GetMethod(&normalType,"NonExisting");
	registry.GetMethod(&normalType,"NonExisting");
	
	CHECK_EQUAL(1,methodFactory.produceCallCount);
}



TEST_FIXTURE(Fixture,InvalidateCacheCausesNewMethodToBeProduced)
{
	methodFactory.produceThis.push(&method1);
	methodFactory.produceThis.push(&method2);
	
	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1"));
	registry.InvalidateCache();
	CHECK_EQUAL(&method2,registry.GetMethod(&normalType,"Method1"));
}

struct FixtureWithBaseTypeHavingMethod : public Fixture
{
	ScriptingType normalType;
	ScriptingType baseType;

	FixtureWithBaseTypeHavingMethod()
	{
		normalType.baseType = &baseType;
		methodFactory.produceThis.push(&method1);
		methodFactory.returnNullForRequestson = &normalType;
	}
};

TEST_FIXTURE(FixtureWithBaseTypeHavingMethod,RegistryWillSearchBaseTypesIfRequested)
{
	CHECK_EQUAL(&method1,registry.GetMethod(&normalType,"Method1"));
}

TEST_FIXTURE(FixtureWithBaseTypeHavingMethod, RegistryWillNotSearchBaseTypesIfNotRequested)
{
	CHECK_EQUAL((ScriptingMethodPtr)NULL,registry.GetMethod(&normalType,"Method1", ScriptingMethodRegistry::kDontSearchBaseTypes));
}

TEST_FIXTURE(Fixture, CanGetMethodFromNativeMethodPointer)
{
	int nativeMethod;
	methodFactory.produceThis.push(&method1);
	CHECK_EQUAL(&method1,registry.GetMethod( &nativeMethod));
}

TEST_FIXTURE(Fixture, CanGetSameMethodFromNativeMethodTwice)
{
	int nativeMethod;
	methodFactory.produceThis.push(&method1);
	CHECK_EQUAL(&method1,registry.GetMethod(&nativeMethod));
	CHECK_EQUAL(&method1,registry.GetMethod(&nativeMethod));
}

TEST_FIXTURE(Fixture, InvalidateCacheClearsNativeMethodWrappers)
{
	int nativeMethod;
	methodFactory.produceThis.push(&method1);
	methodFactory.produceThis.push(&method2);
	CHECK_EQUAL(&method1,registry.GetMethod(&nativeMethod));
	registry.InvalidateCache();
	CHECK_EQUAL(&method2,registry.GetMethod(&nativeMethod));
}

TEST_FIXTURE(Fixture, AllMethodsInType)
{
	normalType.methods.push_back(&method1);

	vector<ScriptingMethodPtr> result;
	registry.AllMethodsIn(&normalType, result);
	CHECK_EQUAL(1, result.size());
	CHECK_EQUAL(&method1, result[0]);
}

TEST_FIXTURE(Fixture, AllMethodsInTypeWillSearchBaseTypes)
{
	ScriptingType baseType;
	baseType.methods.push_back(&method1);
	normalType.baseType = &baseType;

	vector<ScriptingMethodPtr> result;
	registry.AllMethodsIn(&normalType, result);
	CHECK_EQUAL(1, result.size());
	CHECK_EQUAL(&method1, result[0]);
}

TEST_FIXTURE(Fixture, AllMethodsInTypeCanSkipStaticMethods)
{
	normalType.methods.push_back(&method1);
	normalType.methods.push_back(&method2);

	method2.instance = true;

	vector<ScriptingMethodPtr> result;
	registry.AllMethodsIn(&normalType, result, ScriptingMethodRegistry::kInstanceOnly);
	CHECK_EQUAL(1, result.size());
	CHECK_EQUAL(&method2, result[0]);
}

TEST_FIXTURE(Fixture, SearchFilterCanSkipMethodsWithArguments)
{
	normalType.methods.push_back(&method1);
	normalType.methods.push_back(&method2);

	method1.args=0;
	method2.args=2;

	vector<ScriptingMethodPtr> result;
	registry.AllMethodsIn(&normalType, result, ScriptingMethodRegistry::kWithoutArguments );
	CHECK_EQUAL(1, result.size());
	CHECK_EQUAL(&method1, result[0]);
}

#endif
