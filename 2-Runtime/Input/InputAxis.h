#ifndef INPUTAXIS_H
#define INPUTAXIS_H

#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include <string>
using std::string;

enum AxisType {	
	kAxisButton, 
	kAxisMouse, 
	kAxisJoystick,
};

/// The class for one input axis
class InputAxis {
public:
	DECLARE_SERIALIZE (InputAxis)

	InputAxis ();
	InputAxis (const string &name);

	const UnityStr &GetName () 					{ return m_Name; }
	void SetName (const string &name)		{ m_Name = name; }

	float GetValue () const						{ return value; }
	float GetValueRaw () const;

	void Reset ()					{ value = 0.0F; rawValue = 0.0F; }
	/// Set the key codes for positiove and negative position.
	void SetKeys (int pos, int neg)			{ positiveButton = pos; negativeButton = neg; }
	void SetPosKey (int pos)					{ positiveButton = pos; }
	int GetPosKey () const						{ return positiveButton; }
	void SetNegKey (int neg)					{ negativeButton = neg; }
	int GetNegKey () const						{ return negativeButton; }

	void SetAltPosKey (int pos)					{ altPositiveButton = pos; }
	int GetAltPosKey () const						{ return altPositiveButton; }
	void SetAltNegKey (int neg)					{ altNegativeButton = neg; }
	int GetAltNegKey () const						{ return altNegativeButton; }

	/// Set the mouse or joystick axis for this input.
	/// 0 = x, 1 = y
	void SetAxis (int a) 						{ axis = a; }
	int GetAxis()									{ return axis; }
	/// for joysticks: Set the joystick number to read.
	void SetJoystickNumber (int input) 		{ joyNum = input; }
	int GetJoystickNumber () const 			{ return joyNum; }

	/// Make this axis return to neutral if left alone
	/// @param grav time it takes to return to neutral position
	/// @param pos neutral position
	void SetGravity (float grav) 				{ gravity = grav; }
	float GetGravity () const					{ return gravity; }

	/// How sensitive is this axis. 
	/// In mouse or joystick control, it maps the factor between mouse position and axis position.
	/// If keyboard control, how much we should move for each update. HINT: Use a value < .5
	void SetSensitivity (float sens)  		{ sensitivity = sens; }
	float GetSensitivity () const				{ return sensitivity; }

	/// Keyboard control only: should the axis snap to neutral position if the opposite key is pressed
	void SetKeySnap   (bool s)					{ snap = s; }
	bool GetKeySnap () const					{ return snap; }
	/// Set the size of the center.
	void SetDeadZone (float size) 			{ dead = size; }
	float GetDeadZone () const					{ return dead; }

	/// Should this axis be inverted ?
	void SetInvert	(bool invert)				{ this->invert = invert; }
	bool GetInvert () const						{ return invert; }
	
	/// Helper: Make a sensible analog key controller.
	void MakeAnalogKey (int keyPos, int keyNeg, int altKeyPos, int altKeyNeg);
	/// Helper: Make a sensible fire button.
	void MakeButton (int keyPos, int altKeyPos);
	void MakeMouse   (int axis);
	void MakeJoystick (int axis);
	
	int GetType () { return type; }

	virtual void Update ();
	
	UnityStr GetDescriptiveName(bool neg);
	
	float& GetValueRawRef ()	{ return rawValue; }
	float& GetValueRef ()     { return value; }
	
  private:
	UnityStr m_Name;
	UnityStr descriptiveName; ///< Name presented to the user for setup if present
	UnityStr descriptiveNegativeName; ///< Name for negative Button presented to the user for setup if present	
	int positiveButton;		///< Button to be pressed for movement in negative direction
	int negativeButton;		///< Button to be pressed for movement in positive direction
	int altPositiveButton;		///< alternative Button to be pressed for movement in negative direction
	int altNegativeButton;		///< alternative Button to be pressed for movement in positive direction
	int joyNum;					///< Joystick identifier index enum {Get Motion from all Joysticks = 0, Joystick 1,Joystick 2,Joystick 3,Joystick 4,Joystick 5,Joystick 6,Joystick 7,Joystick 8,Joystick 9,Joystick 10,Joystick 11 }
	int type;					///< enum { Key or Mouse Button = 0, Mouse Movement, Joystick Axis}
	float value;
	float rawValue;

	int axis;					///< Axis to use enum { X axis = 0, Y axis = 1, 3rd axis (Joysticks and Scrollwheel) = 2, 4th axis (Joysticks) = 3, 5th axis (Joysticks) = 4, 6th axis (Joysticks) = 5, 7th axis (Joysticks) = 6, 8th axis (Joysticks) = 7, 9th axis (Joysticks) = 8, 10th axis (Joysticks) = 9 }
	float gravity;				///< Speed (in units/sec) that the output value falls towards neutral when device at rest
	float dead;					///< Size of the analog dead zone. All analog device values within this range map to neutral
	float sensitivity;		///< Speed to move towards target value for digital devices (in units per sec) 
	bool snap;					///< If we have input in opposite direction of current, do we jump to neutral and continue from there?
	bool invert;				///< flip positive and negative?
	void DoGravity (float time);
};

std::string ConvertKeyToString (int key);
int ConvertStringToKey (const std::string& name);

string KeyToString (int key);
int StringToKey (const string& name);

template<class TransferFunc>
void InputAxis::Transfer (TransferFunc& transfer)
{
	transfer.SetVersion (3);

	TRANSFER(m_Name);
	TRANSFER(descriptiveName);
	TRANSFER(descriptiveNegativeName);

	TRANSFER_WITH_CUSTOM_GET_SET (UnityStr, "negativeButton",
		value = KeyToString (negativeButton),
		negativeButton = StringToKey (value), kSimpleEditorMask);
	TRANSFER_WITH_CUSTOM_GET_SET (UnityStr, "positiveButton",
		value = KeyToString (positiveButton),
		positiveButton = StringToKey (value), kSimpleEditorMask);
	TRANSFER_WITH_CUSTOM_GET_SET (UnityStr, "altNegativeButton",
		value = KeyToString (altNegativeButton),
		altNegativeButton = StringToKey (value), kSimpleEditorMask);
	TRANSFER_WITH_CUSTOM_GET_SET (UnityStr, "altPositiveButton",
		value = KeyToString (altPositiveButton),
		altPositiveButton = StringToKey (value), kSimpleEditorMask);
	
	TRANSFER(gravity);
	TRANSFER(dead);
	TRANSFER_SIMPLE(sensitivity);
	TRANSFER(snap);
	TRANSFER(invert);
	transfer.Align();

	TRANSFER_SIMPLE(type);
	TRANSFER(axis);
	TRANSFER(joyNum);
}

#endif
