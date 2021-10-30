#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <vector>
#include <map>
#include <string>
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Misc/Allocator.h"
#include "MessageIdentifier.h"

/*
	DOCUMENT_______________________________________
*/

class MessageForwarder
{
	typedef void (*MessageCallback)(void* Receiver, int messageID, MessageData& data);
	typedef bool (*CanHandleMessageCallback)(void* Receiver, int messageID, MessageData& data);
	std::vector<MessageCallback> m_SupportedMessages;
	std::vector<int>             m_SupportedMessagesParameter;
	MessageCallback              m_GeneralMessage;
	CanHandleMessageCallback     m_CanHandleGeneralMessage;

	public:

	MessageForwarder ();

	// Returns true if a message callback exists for the class and the messageID
	bool HasMessageCallback (const MessageIdentifier& messageID);

	// Returns true a message callback exists and the message will actually be handled.
	// This is used to find out if a message will *actually* be forwared, eg. HasMessageCallback will always return true
	// for eg. ScriptBehaviours which checks at runtime if the message is supported by the script.
	bool WillHandleMessage (void* receiver, const MessageIdentifier& messageID);
	
	/// Calls the message
	/// the notification can be handled using CanHandleNotification
	void HandleMessage (void* receiver, int messageID, MessageData& notificationData);
	
	void RegisterMessageCallback (int messageID, MessageCallback message, int classId);
	void RegisterAllMessagesCallback (MessageCallback message, CanHandleMessageCallback canHandleMessage);
	
	/// Returns the parameter that the receiver expects from a message. If
	/// the method doesn't expect a parameter, is not supported, or uses a
	/// general message handler, 0 is returned.
	int GetExpectedParameter (int messageID);
		
	/// Adds all messages that baseMessages contains but this MessageReceiver does not handle yet.
	/// AddBaseNotifications is used to implement derivation by calling AddBaseNotifications for all base classes.
	void AddBaseMessages (const MessageForwarder& baseMessages);
};

typedef std::vector<MessageForwarder, STL_ALLOCATOR_ALIGNED(kMemPermanent, MessageForwarder, 8) > MessageForwarders;

class MessageHandler
{
	dynamic_bitset m_SupportedMessages;
	MessageForwarders m_Forwarder;
	int m_ClassCount;
	int m_MessageCount;
	
	typedef std::vector<MessageIdentifier> MessageIDToIdentifier;
	MessageIDToIdentifier m_MessageIDToIdentifier;
	typedef std::map<std::string, int> MessageNameToIndex;
	MessageNameToIndex m_MessageNameToIndex;
		
	public:
	
	///	Initializes all message forwarders and precalculates the supporetedmessages bit array
	void Initialize (const MessageForwarders& receivers);
	
	// Generates the messageIndices for all MessageIdentifier's
	// Gets the list of all message identifiers which are created by constructor of MessageIdentifier
	// Sorts the messages by name and builds the MessageNameToIndex and m_MessageIDToIdentifier maps.
	void InitializeMessageIdentifiers ();
	
	// Returns true if a message callback exists for the class and the messageID
	bool HasMessageCallback (int classID, int messageID) { return m_SupportedMessages.test (messageID * m_ClassCount + classID); }
	
	// Returns true a message callback exists and the message will actually be handled.
	// This is used to find out if a message will *actually* be forwared, eg. HasMessageCallback will always return true
	// for eg. ScriptBehaviours which checks at runtime if the message is supported by the script.
	bool WillHandleMessage (void* receiver, int classID, int messageID);
	
	/// Forwards a message to the appropriate MessageForwarder
	void HandleMessage (void* receiver, int classID, int messageID, MessageData& messageData)
	{
		AssertIf (classID >= m_ClassCount);
		SET_ALLOC_OWNER(NULL);
		m_Forwarder[classID].HandleMessage (receiver, messageID, messageData);
	}
	
	void SetMessageEnabled (int classID, int messageID, bool enabled)
	{
		// You are probably doing something wrong if you enable/disable a message twice
		DebugAssertIf(m_SupportedMessages[messageID * m_ClassCount + classID] == enabled);
		m_SupportedMessages[messageID * m_ClassCount + classID] = enabled;			
	}
	
	// Converts a message name to an ID if the name is not registered returns -1
	int MessageNameToID (const std::string& name);
	// Converts a messageID to its message name. The messageID has to exist
	const char* MessageIDToName (int messageID);
	// Converts a messageID to its parameter eg. ClassID (float). The messageID has to exist
	int MessageIDToParameter (int messageID);
	MessageIdentifier MessageIDToMessageIdentifier (int messageID);

	// Returns the number of registered messages
	int GetMessageCount () { return m_MessageCount; }
};

#endif
