#include "UnityPrefix.h"
#include "UserList.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/BaseClasses/GameObject.h"

void UserListNode::Clear ()
{
	if (IsConnected())
	{
		UserList* other = static_cast<UserList*>(m_Entry.other);
		other->RemoveIndex(m_Entry.indexInOther);
		m_Entry = Entry();
	}
}

void UserList::Clear ()
{
	for (int index=0; index<m_Entries.size(); index++)
	{
		Entry& user = m_Entries[index];
		if (user.indexInOther == -1)
		{
			// Other is a node
			UserListNode* other = static_cast<UserListNode*>(user.other);
			DebugAssert(other->m_Entry.other == this);
			DebugAssert(other->m_Entry.indexInOther == index);
			other->m_Entry = Entry();
		}
		else
		{
			// Other is a another list
			UserList* other = static_cast<UserList*>(user.other);
			DebugAssert(other->m_Entries[user.indexInOther].other == this);
			DebugAssert(other->m_Entries[user.indexInOther].indexInOther == index);
			other->RemoveIndex(user.indexInOther);
		}
	}
	m_Entries.clear();
}

void UserList::Reserve (size_t size)
{
	m_Entries.reserve(size);
}

void UserList::AddUser (UserListNode& other)
{
	other.Clear();
	other.m_Entry.other = this;
	other.m_Entry.indexInOther = m_Entries.size();
	Entry& user = m_Entries.push_back();
	user.other = &other;
	user.indexInOther = -1;
}

void UserList::AddUser (UserList& other)
{
	DebugAssert(&other != this);
	int index = m_Entries.size();
	int indexInOther = other.m_Entries.size();
	Entry& user = m_Entries.push_back();
	user.other = &other;
	user.indexInOther = indexInOther;
	Entry& otherUser = other.m_Entries.push_back();
	otherUser.other = this;
	otherUser.indexInOther = index;
}

void UserList::SendMessage (const MessageIdentifier& msg)
{	
	ASSERT_RUNNING_ON_MAIN_THREAD
	
	MessageData data;
	// Traverse list backwards, makes it easier if an element removes itself
	int index = (int)m_Entries.size() - 1;
	while (index >= 0)
	{
		UserListBase* other = m_Entries[index].other;
#if DEBUGMODE
		// Verify that the link back is correct
		GetEntryInOther(index);
#endif
		SendMessageDirect(*other->GetTarget(), msg, data);
		// Make sure index is within range
		index = std::min(index, (int)m_Entries.size());
		index--;
	}
}

UserList::Entry& UserList::GetEntryInOther (int index)
{
	Entry& user = m_Entries[index];
	Entry* entryInOther;
	if (user.indexInOther == -1)
	{
		// Other is a node
		UserListNode* other = static_cast<UserListNode*>(user.other);
		entryInOther = &other->m_Entry;
	}
	else
	{
		// Other is a another list
		UserList* other = static_cast<UserList*>(user.other);
		entryInOther = &other->m_Entries[user.indexInOther];
	}
	DebugAssert(entryInOther->other == this);
	DebugAssert(entryInOther->indexInOther == index);
	return *entryInOther;
}

void UserList::RemoveIndex (int index)
{
	int lastIndex = m_Entries.size() - 1;
	if (index != lastIndex)
	{
		// Move last entry to index
		Entry& lastUser = m_Entries[lastIndex];
		m_Entries[index] = lastUser;
		Entry& entryInOther = GetEntryInOther(lastIndex);
		entryInOther.indexInOther = index;
	}
	m_Entries.pop_back();
}

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

class TestUserList : public UserList
{
public:
	TestUserList() : UserList(NULL) {}
};

class TestUserListNode : public UserListNode
{
public:
	TestUserListNode() : UserListNode(NULL) {}
};

SUITE (UserListTests)
{
TEST(UserList_BasicUserList)
{
	const int kNumA = 10;
	const int kNumB = 20;
	const int kNumC = 30;
	TestUserList listsA[kNumA];
	TestUserList listsB[kNumB];
	TestUserList listsC[kNumC];
	TestUserListNode nodesD[kNumC];
	for (int c = 0; c < kNumC; c++)
		if ((c % 3) == 1)
			listsC[c].AddUser(nodesD[c]);
	for (int a = 0; a < kNumA; a++)
		for (int b = 0; b < kNumB; b++)
			if (((a + b) % 3) == 0)
				listsA[a].AddUser(listsB[b]);
	for (int b = 0; b < kNumB; b++)
		for (int c = 0; c < kNumC; c++)
			if (((b + c) % 5) == 0)
				listsB[b].AddUser(listsC[c]);
	for (int c = 0; c < kNumC; c++)
		if ((c % 3) == 2)
			listsC[c].AddUser(nodesD[c]);
	for (int a = kNumA - 1; a >= 0; a--)
		if ((a % 2) == 0)
			listsA[a].Clear();
	for (int c = 0; c < kNumC; c++)
		if ((c % 4) == 0)
			nodesD[c].Clear();
	for (int b = 0; b < kNumB; b++)
		listsB[b].Clear();
	for (int a = 0; a < kNumA; a++)
	{
		CHECK_EQUAL (0, listsA[a].GetSize());
	}
	for (int c = 0; c < kNumC; c++)
	{
		if ((c % 3) != 0 && (c % 4) != 0)
		{
			CHECK_EQUAL (1, listsC[c].GetSize());
			CHECK_EQUAL (true, nodesD[c].IsConnected());
		}
		else
		{
			CHECK_EQUAL (0, listsC[c].GetSize());
			CHECK_EQUAL (false, nodesD[c].IsConnected());
		}
	}
}
}

#endif
