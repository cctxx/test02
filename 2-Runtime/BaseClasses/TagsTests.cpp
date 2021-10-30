#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "Tags.h"
#include "Runtime/Testing/Testing.h"

SUITE (TagsTests)
{
	TEST (StringToTag_TagToString_WithEmptyString_IsIdentityOperation)
	{
		CHECK_EQUAL ("", TagToString (StringToTag ("")));
	}

	TEST (StringToTag_TagToString_WithDefaultTag_IsIdentityOperation)
	{
		CHECK_EQUAL ("Untagged", TagToString (StringToTag ("Untagged")));
	}

#	if UNITY_EDITOR
	TEST (SortingLayer_UserID_Works)
	{
		CHECK_EQUAL (1, GetSortingLayerCount()); // only default layer initially

		// add 3 layers
		AddSortingLayer();
		AddSortingLayer();
		AddSortingLayer();
		SetSortingLayerName(1, "A");
		SetSortingLayerName(2, "B");
		SetSortingLayerName(3, "C");
		const int idA = GetSortingLayerUniqueID(1);
		const int idB = GetSortingLayerUniqueID(2);
		const int idC = GetSortingLayerUniqueID(3);

		// now the order is: Default, A, B, C

		// they should get 1,2,3 user IDs assigned
		CHECK_EQUAL(1, GetSortingLayerUserID(1));
		CHECK_EQUAL(2, GetSortingLayerUserID(2));
		CHECK_EQUAL(3, GetSortingLayerUserID(3));
		CHECK_EQUAL(0, GetSortingLayerUserIDFromValue(0));
		CHECK_EQUAL(1, GetSortingLayerUserIDFromValue(1));
		CHECK_EQUAL(2, GetSortingLayerUserIDFromValue(2));
		CHECK_EQUAL(3, GetSortingLayerUserIDFromValue(3));

		// find their values by user IDs
		CHECK_EQUAL(1, GetSortingLayerValueFromUserID(1));
		CHECK_EQUAL(2, GetSortingLayerValueFromUserID(2));
		CHECK_EQUAL(3, GetSortingLayerValueFromUserID(3));

		// find their values by names
		CHECK_EQUAL(1, GetSortingLayerValueFromName("A"));
		CHECK_EQUAL(2, GetSortingLayerValueFromName("B"));
		CHECK_EQUAL(3, GetSortingLayerValueFromName("C"));

		// check all the above for default layer
		CHECK_EQUAL(0, GetSortingLayerUserID(0));
		CHECK_EQUAL(0, GetSortingLayerValueFromUserID(0));
		CHECK_EQUAL(0, GetSortingLayerValueFromName(""));
		CHECK_EQUAL(0, GetSortingLayerValueFromName("Default"));
		CHECK_EQUAL(0, GetSortingLayerValueFromUniqueID(0));

		// reorder layers into: B, A, Default, C
		SwapSortingLayers (0, 2);

		// check user IDs
		CHECK_EQUAL(2, GetSortingLayerUserID(0)); // B
		CHECK_EQUAL(1, GetSortingLayerUserID(1));
		CHECK_EQUAL(3, GetSortingLayerUserID(3));

		CHECK_EQUAL(2, GetSortingLayerUserIDFromValue(-2)); // B
		CHECK_EQUAL(1, GetSortingLayerUserIDFromValue(-1)); // A
		CHECK_EQUAL(0, GetSortingLayerUserIDFromValue(0)); // Default
		CHECK_EQUAL(3, GetSortingLayerUserIDFromValue(1)); // C

		// find values by names
		CHECK_EQUAL(-2, GetSortingLayerValueFromName("B"));
		CHECK_EQUAL(-1, GetSortingLayerValueFromName("A"));
		CHECK_EQUAL(1, GetSortingLayerValueFromName("C"));

		// check all the above for default layer
		CHECK_EQUAL(0, GetSortingLayerUserID(2));
		CHECK_EQUAL(0, GetSortingLayerValueFromUserID(0));
		CHECK_EQUAL(0, GetSortingLayerValueFromName(""));
		CHECK_EQUAL(0, GetSortingLayerValueFromName("Default"));
		CHECK_EQUAL(0, GetSortingLayerValueFromUniqueID(0));

		RegisterDefaultTagsAndLayerMasks (); // cleanup
	}
#	endif // if UNITY_EDITOR

#if UNITY_EDITOR

	TEST (StringToTagAddIfUnavailable_WithNewTag_SetsUpMappings)
	{
		UInt32 tag = StringToTagAddIfUnavailable ("foobar");

		CHECK_EQUAL (tag, StringToTag ("foobar"));
		CHECK_EQUAL ("foobar", TagToString (tag));

		// Cleanup.
		RegisterDefaultTagsAndLayerMasks ();
	}

#endif // UNITY_EDITOR
}

#endif // ENABLE_UNIT_TESTS
