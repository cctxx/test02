#include "UnityPrefix.h"
#include "MessageIdentifier.h"
using namespace std;

MessageIdentifier::RegisteredMessages* gRegisteredMessageIdentifiers = NULL;

MessageIdentifier::MessageIdentifier (const char* name, Options opts, int classId, const char* scriptParamName)
{
	AssertIf (name == NULL);
		
	messageName = name;
	parameterClassId = classId;
	scriptParameterName = scriptParamName;
	messageID = -1;
	options = (int)opts;
	if (gRegisteredMessageIdentifiers == NULL)
		gRegisteredMessageIdentifiers = new RegisteredMessages();
	
	gRegisteredMessageIdentifiers->push_back (this);
}

MessageIdentifier::RegisteredMessages& MessageIdentifier::GetRegisteredMessages ()
{
	AssertIf (gRegisteredMessageIdentifiers == NULL);
	return *gRegisteredMessageIdentifiers;
}

MessageIdentifier::SortedMessages MessageIdentifier::GetSortedMessages (bool notificationMessages)
{
	// Build sorted Messages map. Which contains all messages sorted by name.
	SortedMessages sortedMessages;
	RegisteredMessages& messages = GetRegisteredMessages();
	RegisteredMessages::iterator j;
	for (j = messages.begin ();j != messages.end ();j++)
	{
		MessageIdentifier& identifier = **j;
		AssertIf (identifier.messageName == NULL);
		
		bool usesNotifications = identifier.options & kUseNotificationManager;
		if (usesNotifications != notificationMessages)
			continue;
		
		SortedMessages::iterator found = sortedMessages.find (identifier.messageName);
		if (found == sortedMessages.end ())
		{
			sortedMessages.insert (make_pair (string (identifier.messageName), identifier));
		}
		else
		{
			if (identifier.parameterClassId != found->second.parameterClassId)
			{
				string error = "There are conflicting definitions of the message: ";
				error += identifier.messageName;
				error += ". The parameter of one message has to be the same across all definitions of that message.";
				ErrorString (error);
			}

			if (identifier.scriptParameterName != found->second.scriptParameterName)
			{
				string error = "There are conflicting definitions of the message: ";
				error += identifier.messageName;
				error += ". The parameter of one message has to be the same across all definitions of that message.";
				ErrorString (error);
			}

			if (identifier.options != found->second.options)
			{
				string error = "There are conflicting options of the message: ";
				error += identifier.messageName;
				ErrorString (error);
			}
		}
	}
	return sortedMessages;
}

void MessageIdentifier::Cleanup ()
{
	delete gRegisteredMessageIdentifiers;
}

#include "Runtime/Dynamics/Collider.h"
#undef MESSAGE_IDENTIFIER
#define MESSAGE_IDENTIFIER(n,p) const EXPORT_COREMODULE MessageIdentifier n p
#include "MessageIdentifiers.h"

