using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor {

#if ENABLE_SPRITES
internal class MoveTool {
	public static void OnGUI(SceneView view)
	{
		if (!Selection.activeTransform || Tools.s_Hidden)
			return;
		Vector3 pos = Tools.handlePosition;

		bool gizmosAllowedForSpriteRenderer = false;
		bool isStatic = (!Tools.s_Hidden && EditorApplication.isPlaying && GameObjectUtility.ContainsStatic(Selection.gameObjects));
		if (isStatic)
			GUI.enabled = false;

		Vector3 pos2 = new Vector3(pos.x, pos.y, pos.z);
		bool use2DTool = false;

		if (view.in2DMode)
		{
			foreach (GameObject go in Selection.gameObjects)
			{
				SpriteRenderer spriteRenderer = go.transform.gameObject.GetComponent<SpriteRenderer> ();
				if (spriteRenderer != null && spriteRenderer.sprite != null)
				{
					gizmosAllowedForSpriteRenderer |= InternalEditorUtility.GetIsInspectorExpanded (spriteRenderer);
					use2DTool = true;
					break;
				}
			}
		}

		if (use2DTool)
		{
			if (!isStatic && gizmosAllowedForSpriteRenderer)
				TransformTool2D.OnGUI (view);
		}
		else
		{
			pos2 = Handles.PositionHandle(pos, Tools.handleRotation);
			if (GUI.changed && !isStatic)
			{
				Undo.RecordObjects (Selection.transforms, "Move");
					
				Vector3 delta = pos2 - pos;
				foreach (Transform t in Selection.transforms)
				{
					t.position += delta;
				}
			}
		}
		Handles.ShowStaticLabelIfNeeded(pos);
	}
}
#else
	internal class MoveTool
	{
		public static void OnGUI ()
		{
			if (!Selection.activeTransform || Tools.s_Hidden)
				return;
			Vector3 pos = Tools.handlePosition;
			bool isStatic = (!Tools.s_Hidden && EditorApplication.isPlaying && GameObjectUtility.ContainsStatic (Selection.gameObjects));
			if (isStatic)
				GUI.enabled = false;
			Vector3 pos2 = Handles.PositionHandle (pos, Tools.handleRotation);
			GUI.enabled = true;
			if (GUI.changed && !isStatic)
			{
				Undo.RecordObject (Selection.transforms, "Move");
				Vector3 delta = pos2 - pos;
				foreach (Transform t in Selection.transforms)
					t.position += delta;
			}
			Handles.ShowStaticLabelIfNeeded (pos);
		}
	}
#endif

internal class RotateTool {
	public static void OnGUI ()
	{
		if (!Selection.activeTransform || Tools.s_Hidden)
			return;
		bool isStatic = (!Tools.s_Hidden && EditorApplication.isPlaying && GameObjectUtility.ContainsStatic(Selection.gameObjects));
		if (isStatic)
			GUI.enabled = false;
		Vector3 point = Tools.handlePosition;
		Quaternion before = Tools.handleRotation;
		Quaternion after = Handles.RotationHandle (before, point);

		if (GUI.changed && !isStatic)
		{
			Quaternion delta = Quaternion.Inverse (before) * after;
			float angle;
			Vector3 axis;
			delta.ToAngleAxis (out angle, out axis);
			axis = before * axis;
			
			Undo.RecordObjects (Selection.transforms, "Rotate");
			foreach (Transform t in Selection.transforms)
			{
				t.RotateAround (point, axis, angle);
				if ( t.parent != null )
					t.SendTransformChangedScale(); // force scale update, needed if tr has non-uniformly scaled parent.
			}
			Tools.handleRotation = after;
		}
		GUI.enabled = true;
		Handles.ShowStaticLabelIfNeeded(point);
	}
}

internal class ScaleTool
{
	public static void OnGUI ()
	{
		if (!Selection.activeTransform || Tools.s_Hidden)
			return;
		bool isStatic = (!Tools.s_Hidden && EditorApplication.isPlaying && GameObjectUtility.ContainsStatic(Selection.gameObjects));
		if (isStatic)
			GUI.enabled = false;

		Vector3 scale = Selection.activeTransform.localScale;
		Vector3 newScale = Handles.ScaleHandle (scale, Tools.handlePosition, Tools.handleLocalRotation, HandleUtility.GetHandleSize (Tools.handlePosition));
		if (GUI.changed && !isStatic)
		{
			Vector3 dif;
			dif.x = (scale.x != 0.0f ? newScale.x / scale.x : 1);
			dif.y = (scale.y != 0.0f ? newScale.y / scale.y : 1);
			dif.z = (scale.z != 0.0f ? newScale.z / scale.z : 1);
			
			Undo.RecordObjects (Selection.transforms, "Scale");
			
			foreach (Transform t in Selection.transforms)
			{
				Vector3 oldTransformScale = t.localScale;
				Vector3 newTransformScale = Vector3.Scale (oldTransformScale, dif);
				//Don't allow setting non-Zero values to zero (otherwise we are stuck).
				if((newTransformScale.x != 0.0f || oldTransformScale.x == 0.0f) &&
					(newTransformScale.y != 0.0f || oldTransformScale.y == 0.0f) &&
					(newTransformScale.z != 0.0f || oldTransformScale.z == 0.0f) )
					t.localScale = newTransformScale;
			}
		}
		GUI.enabled = true;
		Handles.ShowStaticLabelIfNeeded(Tools.handlePosition);	
	}
}

internal class SnapSettings : EditorWindow {
	private static float s_MoveSnapX;
	private static float s_MoveSnapY;
	private static float s_MoveSnapZ;

	private static float s_ScaleSnap;
	private static float s_RotationSnap;

	private static bool s_Initialized;

	private static void Initialize ()
	{
		if (!s_Initialized)
		{
			s_MoveSnapX = EditorPrefs.GetFloat("MoveSnapX", 1f);
			s_MoveSnapY = EditorPrefs.GetFloat("MoveSnapY", 1f);
			s_MoveSnapZ = EditorPrefs.GetFloat("MoveSnapZ", 1f);

			s_ScaleSnap = EditorPrefs.GetFloat("ScaleSnap", .1f);
			s_RotationSnap = EditorPrefs.GetFloat("RotationSnap", 15);

			s_Initialized = true;
		}
	}

	public static Vector3 move {
		get
		{
			Initialize ();
			return new Vector3 (s_MoveSnapX, s_MoveSnapY, s_MoveSnapZ);
		}
		set
		{
			EditorPrefs.SetFloat ("MoveSnapX", value.x);
			s_MoveSnapX = value.x;
			EditorPrefs.SetFloat ("MoveSnapY", value.y);
			s_MoveSnapY = value.y;
			EditorPrefs.SetFloat ("MoveSnapZ", value.z);
			s_MoveSnapZ = value.z;
		}
	}

	public static float scale
	{
		get
		{
			Initialize ();
			return s_ScaleSnap;
		} 
		set
		{
			EditorPrefs.SetFloat ("ScaleSnap", value);
			s_ScaleSnap = value;
		}
	}

	public static float rotation
	{
		get
		{
			Initialize ();
			return s_RotationSnap;
		}
		set
		{
			EditorPrefs.SetFloat("RotationSnap", value);
			s_RotationSnap = value;
		}
	}
	
	[MenuItem ("Edit/Snap Settings...")]
	static void ShowSnapSettings () {
		EditorWindow.GetWindowWithRect<SnapSettings>(new Rect(100,100,230,130), true, "Snap settings");
	}

	class Styles
	{
		public GUIStyle buttonLeft = "ButtonLeft";
		public GUIStyle buttonMid = "ButtonMid";
		public GUIStyle buttonRight = "ButtonRight";
		public GUIContent snapAllAxes = EditorGUIUtility.TextContent("Snap.SnapAllAxes");
		public GUIContent snapX = EditorGUIUtility.TextContent("Snap.SnapX");
		public GUIContent snapY = EditorGUIUtility.TextContent("Snap.SnapY");
		public GUIContent snapZ = EditorGUIUtility.TextContent("Snap.SnapZ");
		public GUIContent moveX = EditorGUIUtility.TextContent("Snap.MoveX");
		public GUIContent moveY = EditorGUIUtility.TextContent("Snap.MoveY");
		public GUIContent moveZ = EditorGUIUtility.TextContent("Snap.MoveZ");
		public GUIContent scale = EditorGUIUtility.TextContent("Snap.Scale");
		public GUIContent rotation = EditorGUIUtility.TextContent("Snap.Rotation");
	}
	static Styles ms_Styles;

	void OnGUI()
	{
		if (ms_Styles == null) ms_Styles = new Styles();

		Vector3 m = move;
		m.x = EditorGUILayout.FloatField(ms_Styles.moveX, m.x);
		m.y = EditorGUILayout.FloatField(ms_Styles.moveY, m.y);
		m.z = EditorGUILayout.FloatField(ms_Styles.moveZ, m.z);

		if (GUI.changed) {
			if (m.x <= 0) m.x = move.x;
			if (m.y <= 0) m.y = move.y;
			if (m.z <= 0) m.z = move.z;
			move = m;
		}
		scale = EditorGUILayout.FloatField(ms_Styles.scale, scale);
		rotation = EditorGUILayout.FloatField(ms_Styles.rotation, rotation);

		GUILayout.Space(5);

		bool snapX = false, snapY = false, snapZ = false;
		GUILayout.BeginHorizontal();
		if (GUILayout.Button(ms_Styles.snapAllAxes, ms_Styles.buttonLeft)) { snapX = true; snapY = true; snapZ = true; }
		if (GUILayout.Button(ms_Styles.snapX, ms_Styles.buttonMid)) { snapX = true; }
		if (GUILayout.Button(ms_Styles.snapY, ms_Styles.buttonMid)) { snapY = true; }
		if (GUILayout.Button(ms_Styles.snapZ, ms_Styles.buttonRight)) { snapZ = true; }
		GUILayout.EndHorizontal();

		if (snapX | snapY | snapZ)
		{
			Vector3 scaleTmp = new Vector3(1.0f / move.x, 1.0f / move.y, 1.0f / move.z);

			Undo.RecordObjects(Selection.transforms, "Snap " + (Selection.transforms.Length == 1 ? Selection.activeGameObject.name : " selection") + " to grid");
			foreach (Transform t in Selection.transforms)
			{
				Vector3 pos = t.position;
				if (snapX) pos.x = Mathf.Round(pos.x * scaleTmp.x) / scaleTmp.x;
				if (snapY) pos.y = Mathf.Round(pos.y * scaleTmp.y) / scaleTmp.y;
				if (snapZ) pos.z = Mathf.Round(pos.z * scaleTmp.z) / scaleTmp.z;
				t.position = pos;
			}
		}

	}
}
} // namespace
