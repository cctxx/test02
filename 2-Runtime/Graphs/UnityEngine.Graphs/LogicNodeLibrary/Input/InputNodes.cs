using UnityEngine;

namespace UnityEngine.Graphs.LogicGraph
{
	public partial class InputNodes
	{
		public delegate void AxisDelegate (float value);
		public delegate void MouseDelegate (Vector3 mousePosition);

		[Logic]
		[Title("Input/Get Button")]
		public static void GetButton(string buttonName, Action onDown, Action onUp, Action down, Action up)
		{
			if (onDown != null && Input.GetButtonDown (buttonName))
				onDown ();

			if (onUp != null && Input.GetButtonUp (buttonName))
				onUp ();

			if (down != null || up != null)
			{
				var stateDelegate = Input.GetButton (buttonName) ? down : up;
				if (stateDelegate != null)
					stateDelegate ();
			}
		}

		[Logic]
		[Title("Input/Get Mouse Button")]
		public static void GetMouseButton (int mouseButton, MouseDelegate onDown, MouseDelegate onUp, MouseDelegate down, MouseDelegate up)
		{
			if (onDown != null && Input.GetMouseButtonDown(mouseButton))
				onDown(Input.mousePosition);

			if (onUp != null && Input.GetMouseButtonUp(mouseButton))
				onUp(Input.mousePosition);

			if (down != null || up != null)
			{
				MouseDelegate stateDelegate = Input.GetMouseButton(mouseButton) ? down : up;
				if (stateDelegate != null)
					stateDelegate(Input.mousePosition);
			}
		}

		[Logic]
		[Title("Input/Get Key")]
		public static void GetKey(KeyCode key, Action onDown, Action onUp, Action down, Action up)
		{
			if (onDown != null && Input.GetKeyDown (key))
				onDown ();

			if (onUp != null && Input.GetKeyUp (key))
				onUp ();

			if (down != null || up != null)
			{
				var stateDelegate = Input.GetKey (key) ? down : up;
				if (stateDelegate != null)
					stateDelegate ();
			}
		}

		[Logic]
		[Title("Input/Get Axis")]
		public static void GetAxis(string axisName, AxisDelegate down, AxisDelegate up)
		{
			AxisDelegate stateDelegate = Input.GetButton (axisName) ? down : up;
			if (stateDelegate != null)
				stateDelegate (Input.GetAxis (axisName));
		}

		[LogicEval]
		[Title("Input/Mouse Position")]
		[return: Title("Mouse Position")]
		public static Vector3 MousePosition ()
		{
			return Input.mousePosition;
		}
	}
}

