#include "UnityPrefix.h"

#ifdef ENABLE_UNIT_TESTS

#include "YAMLWrite.h"
#include "Runtime/Testing/Testing.h"
#include <map>

SUITE (YAMLWriteTests)
{
	struct Fixture
	{
		YAMLWrite instanceUnderTest;
		Fixture ()
			: instanceUnderTest (0) {}
	};

	#define ROOT (yaml_document_get_root_node (instanceUnderTest.GetDocument ()))
	#define FIRST_KEY_OF(node) (yaml_document_get_node (instanceUnderTest.GetDocument (), node->data.mapping.pairs.start->key))
	#define FIRST_VALUE_OF(node) (yaml_document_get_node (instanceUnderTest.GetDocument (), node->data.mapping.pairs.start->value))

	TEST_FIXTURE (Fixture, TransferSTLStyleMap_WithEmptyMap_ProducesMappingNode)
	{
		std::map<float, UnityStr> testMap;
		instanceUnderTest.TransferSTLStyleMap (testMap);
		CHECK (ROOT->type == YAML_MAPPING_NODE);
	}

	TEST_FIXTURE (Fixture, TransferSTLStyleMap_WithComplexKey_WritesDataChild)
	{
		// Arrange.
		std::map<PPtr<Object>, UnityStr> testMap;
		testMap[PPtr<Object> ()] = "bar";

		// Act.
		instanceUnderTest.TransferSTLStyleMap (testMap);

		// Assert.
		CHECK (FIRST_KEY_OF (ROOT)->type == YAML_SCALAR_NODE);
		CHECK (strcmp ((const char*) FIRST_KEY_OF (ROOT)->data.scalar.value, "data") == 0);
	}

	TEST_FIXTURE (Fixture, TransferSTLStyleMap_WithBasicTypeKey_DoesNotWriteDataChild)
	{
		// Arrange.
		std::map<int, UnityStr> testMap;
		testMap[1234] = "bar";

		// Act.
		instanceUnderTest.TransferSTLStyleMap (testMap);

		// Assert.
		CHECK (FIRST_KEY_OF (ROOT)->type == YAML_SCALAR_NODE);
		CHECK (strcmp ((const char*) FIRST_KEY_OF (ROOT)->data.scalar.value, "1234") == 0);
	}
}

#endif // ENABLE_UNIT_TESTS
