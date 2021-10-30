using UnityEditor;
using UnityEngine;

namespace UnityEditorInternal
{
	internal class Slider1D
	{
		private static Vector2 s_StartMousePosition, s_CurrentMousePosition;
		private static Vector3 s_StartPosition;

#if ENABLE_SPRITES
		internal static Vector3 Do (int id, Vector3 position, Vector3 direction, float size, Handles.DrawCapFunction drawFunc, float snap)
		{
			return Do(id, position, direction, direction, size, drawFunc, snap);
		}

		internal static Vector3 Do (int id, Vector3 position, Vector3 handleDirection, Vector3 slideDirection, float size, Handles.DrawCapFunction drawFunc, float snap)
#else
		internal static Vector3 Do (int id, Vector3 position, Vector3 direction, float size, Handles.DrawCapFunction drawFunc, float snap)
#endif
		{
			Event evt = Event.current;
			switch (evt.GetTypeForControl(id))
			{
				case EventType.layout:
					// This is an ugly hack. It would be better if the drawFunc can handle it's own layout.
					if (drawFunc == Handles.ArrowCap)
					{
#if ENABLE_SPRITES
						HandleUtility.AddControl(id, HandleUtility.DistanceToLine(position, position + slideDirection * size));
						HandleUtility.AddControl(id, HandleUtility.DistanceToCircle(position + slideDirection * size, size * .2f));
#else
						HandleUtility.AddControl(id, HandleUtility.DistanceToLine(position, position + direction * size));
						HandleUtility.AddControl(id, HandleUtility.DistanceToCircle(position + direction * size, size * .2f));
#endif
					}
					else
					{
						HandleUtility.AddControl (id, HandleUtility.DistanceToCircle (position, size * .2f));
					}
					break;
				case EventType.mouseDown:
					// am I closest to the thingy?
					if (((HandleUtility.nearestControl == id && evt.button == 0) || (GUIUtility.keyboardControl == id && evt.button == 2)) && GUIUtility.hotControl == 0)
					{
						GUIUtility.hotControl = GUIUtility.keyboardControl = id;	// Grab mouse focus
						s_CurrentMousePosition = s_StartMousePosition = evt.mousePosition;
						s_StartPosition = position;
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(1);
					}

					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						s_CurrentMousePosition += evt.delta;
#if ENABLE_SPRITES
						float dist = HandleUtility.CalcLineTranslation(s_StartMousePosition, s_CurrentMousePosition, s_StartPosition, slideDirection);
						
						dist = Handles.SnapValue(dist, snap);
						
						Vector3 worldDirection = Handles.matrix.MultiplyVector(slideDirection);
						Vector3 worldPosition = Handles.s_Matrix.MultiplyPoint(s_StartPosition) + worldDirection * dist;
						position = Handles.s_InverseMatrix.MultiplyPoint(worldPosition);
#else
						float dist = HandleUtility.CalcLineTranslation (s_StartMousePosition, s_CurrentMousePosition, s_StartPosition, direction);
						dist = Handles.SnapValue (dist, snap);

						Vector3 worldDirection = Handles.matrix.MultiplyVector (direction);
						Vector3 worldPosition = Handles.s_Matrix.MultiplyPoint (s_StartPosition) + worldDirection * dist;
						position = Handles.s_InverseMatrix.MultiplyPoint (worldPosition);
#endif
						GUI.changed = true;
						evt.Use();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						GUIUtility.hotControl = 0;
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(0);
					}
					break;
				case EventType.repaint:
					Color temp = Color.white;
					if (id == GUIUtility.keyboardControl && GUI.enabled)
					{
						temp = Handles.color;
						Handles.color = Handles.selectedColor;
					}
#if ENABLE_SPRITES
					drawFunc(id, position, Quaternion.LookRotation(handleDirection), size);
#else
					drawFunc (id, position, Quaternion.LookRotation (direction), size);
#endif

					if (id == GUIUtility.keyboardControl)
						Handles.color = temp;
					break;
			}

			//		bool TempGUIChanged = GUI.changed;
			//		float t = Typein (id, HandleUtility.WorldToGUIPoint (position + direction * size),
			//				  Vector3.Dot (position - s_StartPosition, direction));
			//		if (!TempGUIChanged && GUI.changed) 
			//			position = s_StartPosition + t * direction;


			return position;
		}

	}
}
