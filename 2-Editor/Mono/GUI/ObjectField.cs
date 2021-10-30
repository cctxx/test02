using UnityEngine;
using Object = UnityEngine.Object;

namespace UnityEditor
{
	public sealed partial class EditorGUI
	{
		internal delegate Object ObjectFieldValidator (Object[] references, System.Type objType, SerializedProperty property);
		
		internal static Object DoObjectField (Rect position, Rect dropRect, int id, Object obj, System.Type objType, SerializedProperty property, ObjectFieldValidator validator, bool allowSceneObjects)
		{
			return DoObjectField(position, dropRect, id, obj, objType, property, validator, allowSceneObjects, EditorStyles.objectField);
		}

		internal static Object DoObjectField (Rect position, Rect dropRect, int id, Object obj, System.Type objType, SerializedProperty property, ObjectFieldValidator validator, bool allowSceneObjects, GUIStyle style)
		{
			if (validator == null)
				validator = ValidateObjectFieldAssignment;
			Event evt = Event.current;
			EventType eventType = evt.type;

			// special case test, so we continue to ping/select objects with the object field disabled
			if (!GUI.enabled && GUIClip.enabled && (Event.current.rawType == EventType.MouseDown))
				eventType = Event.current.rawType;
			
			bool usePreview = EditorGUIUtility.HasObjectThumbnail (objType) && position.height > EditorGUI.kSingleLineHeight;
			Vector2 oldIconSize = EditorGUIUtility.GetIconSize ();
			if (!usePreview)
				EditorGUIUtility.SetIconSize (new Vector2 (12,12)); // Have to be this small to fit inside a single line height ObjectField
			
			switch (eventType) 
			{
			case EventType.DragExited: 
				if (GUI.enabled)
					HandleUtility.Repaint ();

				break;
			case EventType.DragUpdated:
			case EventType.DragPerform:

				if (dropRect.Contains(Event.current.mousePosition) && GUI.enabled)
				{
					Object[] references = DragAndDrop.objectReferences;
					Object validatedObject = validator (references, objType, property);
					
					if (validatedObject != null)
					{
						// If scene objects are not allowed and object is a scene object then clear
						if (!allowSceneObjects && !EditorUtility.IsPersistent (validatedObject))
							validatedObject = null;
					}

					if (validatedObject != null)
					{
						DragAndDrop.visualMode = DragAndDropVisualMode.Generic;
						if (eventType == EventType.DragPerform)
						{
							if (property != null)
								property.objectReferenceValue = validatedObject;
							else
								obj = validatedObject;
						
							if (validatedObject)
								Analytics.Event("ObjectSelection", "DragAndDrop", EditorUtility.IsPersistent(validatedObject) ? "Asset" : "Scene", 1);
							else
								Analytics.Event("ObjectSelection", "DragAndDrop", "null", 1);
								
							GUI.changed = true;
							DragAndDrop.AcceptDrag ();
							DragAndDrop.activeControlID = 0;
						}
						else
						{
							DragAndDrop.activeControlID = id;
						}
						Event.current.Use();
					}
				} 
				break;
			case EventType.MouseDown:
				// Ignore right clicks
				if (Event.current.button != 0)
					break;
				if (position.Contains(Event.current.mousePosition))
				{
					// Handle Object Selector
					Rect selectTarget;
					if (usePreview)
						selectTarget = new Rect (position.xMax - 32, position.yMax - 14, 32, 14);
					else 			
						selectTarget = new Rect (position.xMax - 15, position.y, 15, position.height);
					
					EditorGUIUtility.editingTextField = false;

					if (selectTarget.Contains(Event.current.mousePosition))
					{
						if (GUI.enabled)
						{
							GUIUtility.keyboardControl = id;
							ObjectSelector.get.Show(obj, objType, property, allowSceneObjects);
							ObjectSelector.get.objectSelectorID = id;
		
							evt.Use ();
							GUIUtility.ExitGUI();
						}
					}
					else
					{
						Object actualTargetObject = property != null ? property.objectReferenceValue : obj;
						Component com = actualTargetObject as Component;
						if (com)
							actualTargetObject = com.gameObject;
						if (showMixedValue)
							actualTargetObject = null;
						
						// One click shows where the referenced object is
						if (Event.current.clickCount == 1)
						{
							GUIUtility.keyboardControl = id;
							if (actualTargetObject)
								EditorGUIUtility.PingObject (actualTargetObject);
							evt.Use();
						}
						// Double click changes selection to referenced object
						else if (Event.current.clickCount == 2)
						{
							if (actualTargetObject)
							{
								AssetDatabase.OpenAsset(actualTargetObject);
								GUIUtility.ExitGUI();
							}
							evt.Use();
						}
					}
				}
				break;
			case EventType.ExecuteCommand:
				string commandName = evt.commandName;
				if (commandName == "ObjectSelectorUpdated" && ObjectSelector.get.objectSelectorID == id && GUIUtility.keyboardControl == id)
				{
					// Validate the assignment
					Object[] references = { ObjectSelector.GetCurrentObject() };
					Object assigned = validator (references, objType, property);

					// Assign the value
					if (property != null)
						property.objectReferenceValue = assigned;
					
					if (assigned)
						Analytics.Event("ObjectSelection", "ObjectPickerWindow", EditorUtility.IsPersistent(assigned) ? "Asset" : "Scene", 1);
					else
						Analytics.Event("ObjectSelection", "ObjectPickerWindow", "null", 1);
					
					GUI.changed = true;
					evt.Use ();
					return assigned;
				}
				
				break;
			case EventType.KeyDown:
				if (GUIUtility.keyboardControl == id) {
					if (evt.keyCode == KeyCode.Backspace || evt.keyCode == KeyCode.Delete)
					{
						if (property != null)
							property.objectReferenceValue = null;
						else
							obj = null;

						GUI.changed = true;
						evt.Use ();
					}
					
					// Apparently we have to check for the character being space instead of the keyCode,
					// otherwise the Inspector will maximize upon pressing space.
					if (evt.MainActionKeyForControl (id)) 
					{
						ObjectSelector.get.Show(obj, objType, property, allowSceneObjects);
						ObjectSelector.get.objectSelectorID = id; 
						evt.Use ();
						GUIUtility.ExitGUI();
					}
				}
				break;
			case EventType.Repaint:
				GUIContent temp;
				if (showMixedValue)
				{
					temp = s_MixedValueContent;
				}
				else if (property != null)
				{
					temp = EditorGUIUtility.TempContent (property.objectReferenceStringValue, AssetPreview.GetMiniThumbnail (property.objectReferenceValue));
					obj = property.objectReferenceValue;
					if (obj != null)
					{
						Object[] references = { obj };
						if (validator (references, objType, property) == null)
							temp = EditorGUIUtility.TempContent ("Type mismatch");
					}
				}
				else
				{
					temp = EditorGUIUtility.ObjectContent(obj, objType);
				}
				if (usePreview)
				{
					GUIStyle thumbStyle = EditorStyles.objectFieldThumb;
					thumbStyle.Draw (position, GUIContent.none, id, DragAndDrop.activeControlID == id);
					
					if (obj != null && !showMixedValue)
					{
						bool isCubemap = obj is Cubemap;
						Rect textureRect = thumbStyle.padding.Remove (position);
						if (isCubemap)
						{
							// Center
							textureRect.x += (textureRect.width - temp.image.width) / 2f;
							textureRect.y += (textureRect.height - temp.image.width) / 2f;
							// Draw texture with alpha blending
							GUIStyle.none.Draw (textureRect, temp.image, false, false, false, false);
						}
						else
						{
							// Draw texture
							Texture2D t2d = temp.image as Texture2D;
							if (t2d != null && t2d.alphaIsTransparency)
							{
								DrawTextureTransparent (textureRect, t2d);
							}
							else
							{
								DrawPreviewTexture (textureRect, temp.image);								
							}
						}
					}
					else 
					{
						GUIStyle s2 = thumbStyle.name + "Overlay";
						BeginHandleMixedValueContentColor ();
						s2.Draw (position, temp, id);
						EndHandleMixedValueContentColor ();
					}
					GUIStyle s3 = thumbStyle.name + "Overlay2";
					s3.Draw (position, EditorGUIUtility.TempContent ("Select"), id);
				}
				else
				{
					BeginHandleMixedValueContentColor ();
					style.Draw (position, temp, id, DragAndDrop.activeControlID == id);
					EndHandleMixedValueContentColor ();
				}
				break;
			}
			
			EditorGUIUtility.SetIconSize (oldIconSize);
			
			return obj;
		}
		
		internal static Object DoDropField (Rect position, int id, System.Type objType, ObjectFieldValidator validator, bool allowSceneObjects, GUIStyle style)
		{
			if (validator == null)
				validator = ValidateObjectFieldAssignment;
			Event evt = Event.current;
			EventType eventType = evt.type;

			// special case test, so we continue to ping/select objects with the object field disabled
			if (!GUI.enabled && GUIClip.enabled && (Event.current.rawType == EventType.MouseDown))
				eventType = Event.current.rawType;
			
			switch (eventType) 
			{
			case EventType.DragExited: 
				if (GUI.enabled)
					HandleUtility.Repaint ();
				break;
			case EventType.DragUpdated:
			case EventType.DragPerform:

				if (position.Contains(Event.current.mousePosition) && GUI.enabled)
				{
					Object[] references = DragAndDrop.objectReferences;
					Object validatedObject = validator (references, objType, null);
					
					if (validatedObject != null)
					{
						// If scene objects are not allowed and object is a scene object then clear
						if (!allowSceneObjects && !EditorUtility.IsPersistent (validatedObject))
							validatedObject = null;
					}

					if (validatedObject != null)
					{
						DragAndDrop.visualMode = DragAndDropVisualMode.Generic;
						if (eventType == EventType.DragPerform)
						{
							if (validatedObject)
								Analytics.Event("ObjectSelection", "DragAndDrop", EditorUtility.IsPersistent(validatedObject) ? "Asset" : "Scene", 1);
							else
								Analytics.Event("ObjectSelection", "DragAndDrop", "null", 1);
							
							GUI.changed = true;
							DragAndDrop.AcceptDrag ();
							DragAndDrop.activeControlID = 0;
							Event.current.Use();
							return validatedObject;
						}
						else
						{
							DragAndDrop.activeControlID = id;
							Event.current.Use();
						}
						
					}
				} 
				break;
			case EventType.Repaint:
				style.Draw (position, GUIContent.none, id, DragAndDrop.activeControlID == id);
				break;
			}
			return null;
		}
	}
}
