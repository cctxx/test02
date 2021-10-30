#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "Runtime/Testing/Testing.h"
#include "Runtime/Testing/TestFixtures.h"

SUITE (RemapPPtrTransferTests)
{
	//-------------------------------------------------------------------------

	DEFINE_TRANSFER_TEST_FIXTURE (DoesNotTouchNonPPtrProperty)
	{
		UnityStr m_NonPPtrProperty = "test";
		TRANSFER (m_NonPPtrProperty);
		CHECK (m_NonPPtrProperty == "test");
	}

	TEST_FIXTURE (DoesNotTouchNonPPtrPropertyTestFixture, Transfer_WithNonPPtrProperty_DoesNotChangeProperty)
	{
		DoRemapPPtrTransfer ();
	}

	//-------------------------------------------------------------------------

	DEFINE_TRANSFER_TEST_FIXTURE (RemapsPPtrProperty)
	{
		PPtr<Object> m_PPtrProperty (1234);
		TRANSFER (m_PPtrProperty);
		CHECK (m_PPtrProperty.GetInstanceID () == 4321);
	}

	TEST_FIXTURE (RemapsPPtrPropertyTestFixture, Transfer_WithPPtrProperty_MapsToNewInstanceID)
	{
		AddPPtrRemap (1234, 4321);
		DoRemapPPtrTransfer ();
	}

	//-------------------------------------------------------------------------

	DEFINE_TRANSFER_TEST_FIXTURE (DidReadLastPPtrProperty)
	{
		PPtr<Object> m_PPtrProperty;
		TRANSFER (m_PPtrProperty);
		CHECK (transfer.DidReadLastPPtrProperty ());
	}

	TEST_FIXTURE (DidReadLastPPtrPropertyTestFixture, DidReadLastPPtrProperty_WithPPtrProperty_IsTrue)
	{
		DoRemapPPtrTransfer ();
	}

} //TEST

#endif // ENABLE_UNIT_TESTS

