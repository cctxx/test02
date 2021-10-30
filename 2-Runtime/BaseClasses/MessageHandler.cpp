#include "UnityPrefix.h"
#include "MessageHandler.h"
#include "Runtime/Utilities/LogAssert.h"
#include <map>
#include <list>

using namespace std;

MessageForwarder::MessageForwarder ()
{
	m_GeneralMessage = NULL;
	m_CanHandleGeneralMessage = NULL;
}

void MessageForwarder::HandleMessage (void* receiver, int messageID, MessageData& messageData)
{
	MessageCallback messagePtr = m_GeneralMessage;
	if (messageID < m_SupportedMessages.size () && m_SupportedMessages[messageID] != NULL)
		messagePtr = m_SupportedMessages[messageID];

	AssertIf (messagePtr == NULL);	
	messagePtr (receiver, messageID, messageData);
}

bool MessageForwarder::HasMessageCallback (const MessageIdentifier& identifier)
{
	if (identifier.messageID < m_SupportedMessages.size () && m_SupportedMessages[identifier.messageID])
		return true;
	else
		return m_GeneralMessage != NULL && (identifier.options & MessageIdentifier::kSendToScripts);
}

bool MessageForwarder::WillHandleMessage (void* receiver, const MessageIdentifier& identifier)
{
	if (identifier.messageID < m_SupportedMessages.size () && m_SupportedMessages[identifier.messageID])
		return true;

	if (m_GeneralMessage && (identifier.options & MessageIdentifier::kSendToScripts))
	{
		AssertIf (m_CanHandleGeneralMessage == NULL);
		MessageData data;
		return m_CanHandleGeneralMessage (receiver, identifier.messageID, data);
	}
	return false;	
}

int MessageForwarder::GetExpectedParameter (int messageID)
{
	if (messageID < m_SupportedMessages.size ())
		return m_SupportedMessagesParameter[messageID];
	else
		return 0;
}

void MessageForwarder::RegisterMessageCallback (int messageID, MessageCallback message, int classId)
{
	AssertIf (messageID == -1);
	if (messageID >= m_SupportedMessages.size ())
	{
		m_SupportedMessages.resize (messageID + 1, NULL);
		m_SupportedMessagesParameter.resize (messageID + 1, 0);
	}
	m_SupportedMessages[messageID] = message;
	m_SupportedMessagesParameter[messageID] = classId;
}

void MessageForwarder::RegisterAllMessagesCallback (MessageCallback message, CanHandleMessageCallback canHandle)
{
	m_GeneralMessage = message;
	m_CanHandleGeneralMessage = canHandle;
}

void MessageForwarder::AddBaseMessages (const MessageForwarder& baseClass)
{
	int maxsize = max (m_SupportedMessages.size (), baseClass.m_SupportedMessages.size ());
	m_SupportedMessages.resize (maxsize, NULL);
	m_SupportedMessagesParameter.resize (maxsize, 0);
	for (int i=0;i<m_SupportedMessages.size ();i++)
	{
		if (m_SupportedMessages[i] == NULL && i < baseClass.m_SupportedMessages.size ())
		{
			m_SupportedMessages[i] = baseClass.m_SupportedMessages[i];
			m_SupportedMessagesParameter[i] = baseClass.m_SupportedMessagesParameter[i];
		}
	}

	if (m_GeneralMessage == NULL)
		m_GeneralMessage = baseClass.m_GeneralMessage;
}

void MessageHandler::InitializeMessageIdentifiers ()
{
	MessageIdentifier::RegisteredMessages& identifiers = MessageIdentifier::GetRegisteredMessages ();
	MessageIdentifier::SortedMessages sortedMessages = MessageIdentifier::GetSortedMessages(false);
	
	m_MessageIDToIdentifier.clear ();
	m_MessageNameToIndex.clear ();
	
	// Build m_MessageIDToName and m_MessageNameToIndex
	// by copying the data from the sorted messages map
	m_MessageIDToIdentifier.resize (sortedMessages.size ());
	int index = 0; 
	MessageIdentifier::SortedMessages::iterator i;
	for (i = sortedMessages.begin ();i != sortedMessages.end ();i++)
	{
		m_MessageNameToIndex[i->first] = index;
		m_MessageIDToIdentifier[index] = i->second;
		m_MessageIDToIdentifier[index].messageID = index;
		index++;
	}
	
	
	// Setup the messageIDs of all registered message Identifiers
	MessageIdentifier::RegisteredMessages::iterator j;
	for (j = identifiers.begin ();j != identifiers.end ();j++)
	{
		AssertIf (*j == NULL);
		MessageIdentifier& identifier = **j;
		if (m_MessageNameToIndex.count (identifier.messageName))
		{
			identifier.messageID = m_MessageNameToIndex[identifier.messageName];
		}
	}
}

void MessageHandler::Initialize (const MessageForwarders& receivers)
{
	m_Forwarder = receivers;
	m_MessageCount = m_MessageNameToIndex.size ();
	m_ClassCount = receivers.size ();
	
	// Precalculate supported messages
	m_SupportedMessages.resize (m_ClassCount * m_MessageCount);
	for (int c=0;c<m_ClassCount;c++)
	{
		for (int m=0;m<m_MessageCount;m++)
		{
			bool hasCallback = m_Forwarder[c].HasMessageCallback (m_MessageIDToIdentifier[m]);
			if (hasCallback)
			{
				// Check if the parameter is correct and print an error if they dont match
				int wantedParameter = m_Forwarder[c].GetExpectedParameter (m);
				int messageParameter = m_MessageIDToIdentifier[m].parameterClassId;
				if (wantedParameter != 0 && messageParameter != wantedParameter)
				{
					char buffy[4096];
					char const format[] = "The message: %s in the class with "
										  "classID: %d uses a parameter type "
										  "that is different from the "
										  "message's parameter type: %d != %d.";
					sprintf (buffy, format, m_MessageIDToIdentifier[m].messageName,
							 c, wantedParameter, messageParameter);
					ErrorString (buffy);
					hasCallback = false;
				}
			}
			m_SupportedMessages[m * m_ClassCount + c] = hasCallback;
		}
	}
}

bool MessageHandler::WillHandleMessage (void* receiver, int classID, int messageID)
{
	AssertIf (!HasMessageCallback (classID, messageID));
	return m_Forwarder[classID].WillHandleMessage (receiver, m_MessageIDToIdentifier[messageID]);
}

int MessageHandler::MessageNameToID (const string& name)
{
	MessageNameToIndex::iterator i = m_MessageNameToIndex.find (name);
	if (i == m_MessageNameToIndex.end ())
		return -1;
	else
		return i->second;
}

const char* MessageHandler::MessageIDToName (int messageID)
{
	AssertIf (messageID < 0);
	AssertIf (messageID >= m_MessageIDToIdentifier.size ());
	return m_MessageIDToIdentifier[messageID].messageName;
}

int MessageHandler::MessageIDToParameter (int messageID)
{
	AssertIf (messageID < 0);
	AssertIf (messageID >= m_MessageIDToIdentifier.size ());
	return m_MessageIDToIdentifier[messageID].parameterClassId;
}

MessageIdentifier MessageHandler::MessageIDToMessageIdentifier (int messageID)
{
	AssertIf (messageID < 0);
	AssertIf (messageID >= m_MessageIDToIdentifier.size ());
	return m_MessageIDToIdentifier[messageID];
}
