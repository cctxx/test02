#include "UnityPrefix.h"
#include "IntermediateUsers.h"
#include "IntermediateRenderer.h"

void IntermediateUsers::Notify(IntermediateNotify notify)
{
	IntermediateRendererList::iterator i;
	switch (notify)
	{
		case kImNotifyAssetDeleted:
			for (i = m_IntermediateUsers.begin(); i != m_IntermediateUsers.end(); ++i)
				(*i)->OnAssetDeleted();
			break;
		case kImNotifyBoundsChanged:
			for (i = m_IntermediateUsers.begin(); i != m_IntermediateUsers.end(); ++i)
				(*i)->OnAssetBoundsChanged();
			break;
		default:
			AssertString("unknown notification");
	}
}
