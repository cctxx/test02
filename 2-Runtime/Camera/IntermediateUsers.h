#ifndef INTERMEDIATE_USERS_H
#define INTERMEDIATE_USERS_H

#include "BaseRenderer.h"
#include "Runtime/Utilities/LinkedList.h"

class IntermediateRenderer;

enum IntermediateNotify
{
	kImNotifyAssetDeleted,
	kImNotifyBoundsChanged,
};

class IntermediateUsers
{
public:
	void Notify(IntermediateNotify notify);
	void AddUser(ListNode<IntermediateRenderer>& node) { m_IntermediateUsers.push_back(node); }

protected:
	typedef List< ListNode<IntermediateRenderer> > IntermediateRendererList;
	IntermediateRendererList m_IntermediateUsers; // IntermediateRenderer users of this data
};

#endif
