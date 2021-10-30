#ifndef UNITY_ONSCREEN_KEYBOARD_
#define UNITY_ONSCREEN_KEYBOARD_

#include "Runtime/Math/Color.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Graphics/ScreenManager.h"

class KeyboardOnScreen
{
public:

	static Rectf GetRect();
	static bool IsVisible();

	static void Hide();
	static void setInputHidden(bool flag);
	static bool isInputHidden();

public:
	KeyboardOnScreen(std::string const& text, UInt32 keyboardType = 0,
	                 bool autocorrection = true, bool multiline = false,
	                 bool secure = false, bool alert = false,
	                 std::string const& textPlaceholder = "");
	~KeyboardOnScreen();
	bool isActive() const;
	bool isDone() const;
	bool wasCanceled() const;
	std::string getText() const;
	void setText(std::string text);
	void setActive(bool flag = true);

private:
	#if UNITY_WP8
	static BridgeInterface::IWP8Keyboard^ ms_Keyboard;
	BridgeInterface::IWP8Keyboard^ m_Keyboard;
	#endif
	ColorRGBA32 textColor;
	ColorRGBA32 backgroundColor;
	UInt32 keyboardType;
	std::string textPlaceholder;
	bool autocorrection;
	bool multiline;
	bool secure;
	bool alert;
};

#endif
