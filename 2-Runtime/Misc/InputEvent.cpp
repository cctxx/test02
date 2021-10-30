#include "UnityPrefix.h"
#include "InputEvent.h"

InputInterface::~InputInterface () {
}
bool InputInterface::PerformDrag (InputEvent &event) {
	return false;
}
int InputInterface::UpdateDrag (InputEvent &event) {
	return 0;
}

InputEvent::~InputEvent(  )
{
	delete []commandString;
}

void InputEvent::Debug () const
{
#if ENABLE_NEW_EVENT_SYSTEM
	Vector2f mousePosition = touch.pos;
	Vector2f delta = touch.deltaPos;
#endif
	std::string buffer;
	const char *EventNames[] = {
		"kMouseDown", "kMouseUp", "kMouseMove", "kMouseDrag", "kKeyDown", "kKeyUp",
		"kScrollWheel", "kRepaint", "kLayout", "kDragUpdated", "kDragPerform", "kIgnore", "kUsed"
	};
	bool isMouse = false;
	bool isKeyb = false;
	switch (type) {
	case kKeyDown:
	case kKeyUp:
	case kScrollWheel:
		isKeyb = true;
		break;
	case kMouseDown:
	case kMouseUp:
	case kMouseMove:
	case kMouseDrag:
		isMouse = true;
	}
	buffer += Format("Event: %s\n", EventNames[type]);	
	if (isMouse) {
		buffer += Format("  Pos (%f, %f) \t   Delta: (%f, %f)\n", mousePosition.x, mousePosition.y, delta.x, delta.y);
		buffer += Format("  Button: %d", button);
		buffer += Format("  Click count: %d", clickCount);
	} else if (isKeyb) {
		AssertIf(clickCount != 0);
		if( character >= 32 && character <= 127 )
			buffer += Format("  character: '%c'\n", character);
		else
			buffer += Format("  character: code %d\n", character);
		buffer += Format("  keycode: '%d'\n", keycode);
	}
	buffer += "  Mod: ";
	if (modifiers & kShift) buffer += "shift ";
	if (modifiers & kControl) buffer += "ctrl ";
	if (modifiers & kAlt) buffer += "alt ";
	if (modifiers & kCommand) buffer += "cmd ";
	if (modifiers & kNumeric) buffer += "num ";
	if (modifiers & kCapsLock) buffer += "capslock ";
	if (modifiers & kFunctionKey) buffer += "fkey ";
	if (modifiers == 0) buffer += "none ";
	buffer += "\n";
	printf_console( "%s", buffer.c_str() );
}

void InputEvent::Init( )
{
#if !ENABLE_NEW_EVENT_SYSTEM
	mousePosition = Vector2f(0.0F, 0.0F);
	delta =  Vector2f(0.0F, 0.0F);
	#if UNITY_METRO
	touchType = kMouseTouch;
	#endif
#endif
	type = InputEvent::kIgnore;
	keycode = 0;
	character = 0;
	button = 0;
	clickCount = 0;
	pressure = 0;
	modifiers =  0;
	commandString = NULL;
}

InputEvent::InputEvent( const InputEvent& evt )
{
#if ENABLE_NEW_EVENT_SYSTEM
	touch = evt.touch;
#else
	mousePosition = evt.mousePosition;
	delta = evt.delta;
	#if UNITY_METRO
	touchType = evt.touchType;
	#endif
#endif
	type = evt.type;
	button = evt.button;
	modifiers = evt.modifiers;
	pressure = evt.pressure;
	clickCount = evt.clickCount;
	character = evt.character;
	keycode = evt.keycode;

	if (evt.commandString)
	{
		commandString = new char [strlen(evt.commandString) + 1];
		memcpy(commandString, evt.commandString, strlen(evt.commandString) + 1);
	}
	else
	{
		commandString = NULL;
	}
}


void InputEvent::operator = (const InputEvent& evt)
{
#if ENABLE_NEW_EVENT_SYSTEM
	touch = evt.touch;
#else
	mousePosition = evt.mousePosition;
	delta = evt.delta;
	#if UNITY_METRO
	touchType = evt.touchType;
	#endif
#endif
	type = evt.type;
	button = evt.button;
	modifiers = evt.modifiers;
	pressure = evt.pressure;
	clickCount = evt.clickCount;
	character = evt.character;
	keycode = evt.keycode;
	
	if (commandString)
	{
		delete[] commandString;
		commandString = NULL;
	}
	
	if (evt.commandString)
	{
		commandString = new char [strlen(evt.commandString) + 1];
		memcpy(commandString, evt.commandString, strlen(evt.commandString) + 1);
	}
}

InputEvent InputEvent::CommandStringEvent (const std::string& editorCommand, bool execute)
{
	InputEvent event;
	event.Init();
	if (execute)
		event.type = InputEvent::kExecuteCommand;
	else
		event.type = InputEvent::kValidateCommand;
	event.commandString = new char[editorCommand.size() + 1];
	memcpy(event.commandString, editorCommand.c_str(), editorCommand.size() + 1);
	return event;
}
