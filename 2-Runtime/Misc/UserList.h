#pragma once

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Modules/ExportModules.h"

class MessageIdentifier;

// A UserList can be connected to other UserLists or UserListNodes.
// A UserListNode is simply the optimized case of a single-element list.
// The connected lists are symmetrical and can send messages in both directions.
// Deleting a node or list will disconnect it from the graph.

class EXPORT_COREMODULE UserListBase : public NonCopyable
{
public:
	Object* GetTarget () { return m_Target; }

protected:
	UserListBase (Object* target) : m_Target(target) {}
	struct Entry
	{
		Entry() : other(NULL), indexInOther(-1) {}
		UserListBase* other;
		int indexInOther;
	};	
	Object* m_Target;
};

class UserListNode : public UserListBase
{
public:
	UserListNode (Object* target) : UserListBase(target) {}
	~UserListNode () { Clear(); }

	void Clear ();
	bool IsConnected () const { return m_Entry.other != NULL; }

private:
	friend class UserList;
	Entry m_Entry;
};

class EXPORT_COREMODULE UserList : public UserListBase
{
public:
	UserList (Object* target) : UserListBase(target) {}
	~UserList () { Clear(); }
	
	void Clear ();
	void Reserve (size_t size);
	void AddUser (UserListNode& other);
	void AddUser (UserList& other);
	void SendMessage (const MessageIdentifier& msg);
	size_t GetSize () const { return m_Entries.size(); }

private:
	friend class UserListNode;
	Entry& GetEntryInOther (int index);
	void RemoveIndex (int index);

	dynamic_array<Entry> m_Entries;
};
