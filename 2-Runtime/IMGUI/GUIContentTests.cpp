#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "Runtime/IMGUI/GUIContent.h"
#include "Runtime/Testing/TestFixtures.h"
#include "Runtime/Testing/Testing.h"

SUITE (GUIContentTests)
{
	typedef TestFixtureBase Fixture;

	TEST_FIXTURE (Fixture, NullContentDoesNotCrash)
	{
		//Set expectations.
		EXPECT (Warning, "GUIContent is null. Use GUIContent.none.");

		// Do.
		GUIContent content = MonoGUIContentToTempNative (SCRIPTING_NULL);
		
		// Assert.
		CHECK ((UTF16String)"" == content.m_Text);
		CHECK ((UTF16String)"" == content.m_Tooltip);
		CHECK (content.m_Image.IsNull());
	}
}
#endif 
