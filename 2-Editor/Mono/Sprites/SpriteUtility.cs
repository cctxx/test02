#if ENABLE_SPRITES
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditorInternal;
using UnityEngine;
using System.Collections.Generic;
using System;
using Object = UnityEngine.Object;

namespace UnityEditor
{
	internal static class SpriteUtility
	{
		public static void OnSceneDrag (SceneView sceneView)
		{
			Event evt = Event.current;

			if (evt.type != EventType.DragUpdated && evt.type != EventType.DragPerform && evt.type != EventType.DragExited)
				return;

			Sprite[] assets = GetSpriteFromDraggedPathsOrObjects ();
			if (assets.Length == 0)
				return;

			Sprite sprite = assets[0];
			if (sprite == null)
				return;

			DragAndDrop.visualMode = DragAndDropVisualMode.Copy;

			switch (evt.type)
			{
				case (EventType.DragPerform):
					Vector3 position = HandleUtility.GUIPointToWorldRay (evt.mousePosition).GetPoint (10);
					position.z = 0f;

					GameObject go = DropFramesToSceneToCreateGO(sprite.name, assets, position);
					Undo.RegisterCreatedObjectUndo (go, "Create Sprite");
					evt.Use ();
					break;
			}
		}

		static void CreateAnimation (GameObject gameObject, Sprite[] frames)
		{
			// Use same name compare as when we sort in the backend: See AssetDatabase.cpp: SortChildren
			System.Array.Sort(frames, (a, b) => EditorUtility.SemiNumericCompare(a.name, b.name));

			// Go forward with presenting user a save clip dialog			
			string message = string.Format ("Create a new animation for the game object '{0}':", gameObject.name);
			string path = Path.GetDirectoryName (AssetDatabase.GetAssetPath (frames[0]));
			
			string newClipPath = EditorUtility.SaveFilePanelInProject ("Create New Animation", "New Animation", "anim", message, path);

			// If user canceled or save path is invalid, we can't create a clip
			if (newClipPath == "")
				return;

			// At this point we know that we can create a clip
			AnimationClip newClip = AnimationSelection.AllocateAndSetupClip (true);
			AssetDatabase.CreateAsset (newClip, newClipPath);

			AnimationSelection.AddClipToAnimatorComponent (gameObject, newClip);

			// TODO Default framerate be exposed to user?
			newClip.frameRate = 12;

			// Add keyframes
			ObjectReferenceKeyframe[] keyframes = new ObjectReferenceKeyframe[frames.Length];

			for (int i = 0; i < keyframes.Length; i++)
			{
				keyframes[i] = new ObjectReferenceKeyframe();
				keyframes[i].value = frames[i];
				keyframes[i].time = i / newClip.frameRate;
			}

			// Create binding
			EditorCurveBinding curveBinding = EditorCurveBinding.PPtrCurve ("", typeof (SpriteRenderer), "m_Sprite");

			// Save curve to clip
			AnimationUtility.SetObjectReferenceCurve (newClip, (EditorCurveBinding)curveBinding, keyframes);
		}
		
		public static Sprite[] GetSpriteFromDraggedPathsOrObjects ()
		{
			List<Sprite> sprites = new List<Sprite> ();

			foreach (Object obj in DragAndDrop.objectReferences)
			{
				if (AssetDatabase.Contains (obj))
				{
					if (obj is Sprite)
						sprites.Add (obj as Sprite);
					else if (obj is Texture2D)
						sprites.Add (TextureToSprite (obj as Texture2D));
				}
			}

			if (sprites.Count > 0)
				return sprites.ToArray();

			return new Sprite[] {HandleExternalDrag (Event.current.type == EventType.DragPerform)};
		}

		public static Sprite[] GetSpritesFromDraggedObjects ()
		{
			List<Sprite> result = new List<Sprite>();

			foreach (Object obj in DragAndDrop.objectReferences)
			{
				if (obj.GetType () == typeof (Sprite))
					result.Add (obj as Sprite);
				else if (obj.GetType() == typeof (Texture2D))
				{
					Sprite sprite = TextureToSprite (obj as Texture2D);
					if (sprite != null)
						result.Add (sprite);
				}
			}

			return result.ToArray();
		}

		private static Sprite HandleExternalDrag (bool perform)
		{
			if (DragAndDrop.paths.Length == 0)
				return null;

			string path = DragAndDrop.paths[0];
			if (!ValidPathForTextureAsset (path))
				return null;

			DragAndDrop.visualMode = DragAndDropVisualMode.Copy;

			if (!perform)
				return null;

			var newPath = AssetDatabase.GenerateUniqueAssetPath (Path.Combine ("Assets",FileUtil.GetLastPathNameComponent (path)));
			if (newPath.Length <= 0)
				return null;
			
			FileUtil.CopyFileOrDirectory (path, newPath);
			ForcedImportFor (newPath);

			return GenerateDefaultSprite (AssetDatabase.LoadMainAssetAtPath (newPath) as Texture2D);
		}

		public static bool HandleMultipleSpritesDragIntoHierarchy (IHierarchyProperty property, Sprite[] sprites, bool perform)
		{
			GameObject targetGO = null;

			if (property == null || property.pptrValue == null)
			{
				if (perform)
				{
					Analytics.Event ("Sprite Drag and Drop", "Drop multiple sprites to empty hierarchy", "null", 1);

					targetGO = new GameObject ();
					targetGO.name = sprites[0].name;
					targetGO.transform.position = GetDefaultInstantiatePosition ();
				}
			}
			else
			{
				Object obj = property.pptrValue;
				targetGO = obj as GameObject;

				if(perform)
					Analytics.Event ("Sprite Drag and Drop", "Drop multiple sprites to gameobject", "null", 1);

			}
			
			if (perform)
			{
				SpriteRenderer spriteRenderer = targetGO.GetComponent<SpriteRenderer> ();
				if (spriteRenderer == null)
					spriteRenderer = targetGO.AddComponent<SpriteRenderer> ();

				// Sometimes adding spriteRenderer fails (for example if it has other renderers already)
				// We still return true, because we don't want Texture2D->mesh drop codepath to execute. Drags of Texture2D that is in spritemode should always be handled as a Sprite.
				if (spriteRenderer == null)
					return true;
				
				if (spriteRenderer.sprite == null)
					spriteRenderer.sprite = sprites[0];

				CreateAnimation (targetGO, sprites);

				Selection.activeGameObject = targetGO;
			}
			return true;
		}

		public static bool HandleSingleSpriteDragIntoHierarchy (IHierarchyProperty property, Sprite sprite, bool perform)
		{
			GameObject targetGO = null;

			if (property != null && property.pptrValue != null)
			{
				Object obj = property.pptrValue;
				targetGO = obj as GameObject;
			}

			if (perform)
			{
				Vector3 position = GetDefaultInstantiatePosition ();
				GameObject go = SpriteUtility.DropSpriteToSceneToCreateGO (sprite.name, sprite, position);

				if (targetGO != null)
				{
					Analytics.Event ("Sprite Drag and Drop", "Drop single sprite to existing gameobject", "null", 1);
					go.transform.parent = targetGO.transform;
					go.transform.localPosition = Vector3.zero;
				}
				else
				{
					Analytics.Event ("Sprite Drag and Drop", "Drop single sprite to empty hierarchy", "null", 1);
				}

				Selection.activeGameObject = go;
			}
			return true;
		}

		private static Vector3 GetDefaultInstantiatePosition()
		{
			Vector3 result = Vector3.zero;
			if (SceneView.lastActiveSceneView)
			{
				if (SceneView.lastActiveSceneView.in2DMode)
				{
					result = SceneView.lastActiveSceneView.camera.transform.position;
					result.z = 0f;
				}
				else
				{
					result = SceneView.lastActiveSceneView.cameraTargetPosition;
				}
			}
			return result;
		}
		
		private static void ForcedImportFor (string newPath)
		{
			try
			{
				AssetDatabase.StartAssetEditing();
				AssetDatabase.ImportAsset(newPath);
			}
			finally
			{
				AssetDatabase.StopAssetEditing();
			}
		}

		private static Sprite GenerateDefaultSprite (Texture2D texture)
		{
			string assetPath = AssetDatabase.GetAssetPath (texture);
			TextureImporter textureImporter = AssetImporter.GetAtPath (assetPath) as TextureImporter;
			if (textureImporter.textureType != TextureImporterType.Sprite &&
				textureImporter.textureType != TextureImporterType.Advanced)
			{
				return null;
			}

			if (textureImporter.spriteImportMode == SpriteImportMode.None)
			{
				if (textureImporter.textureType == TextureImporterType.Advanced)
					return null;

				textureImporter.spriteImportMode = SpriteImportMode.Single;
				AssetDatabase.WriteImportSettingsIfDirty (assetPath);
				ForcedImportFor (assetPath);
			}

			Object firstSprite = null;
			try
			{
				firstSprite = AssetDatabase.LoadAllAssetsAtPath(assetPath).First(t => t is Sprite);
			}
			catch (System.Exception)
			{
				Debug.LogWarning("Texture being dragged has no Sprites.");
			}
					
			return firstSprite as Sprite;
		}

		public static GameObject DropFramesToSceneToCreateGO(string name, Sprite[] frames, Vector3 position)
		{
			if (frames.Length > 0)
			{
				Sprite frame = frames[0];
				GameObject go = new GameObject(name);

				SpriteRenderer spriteRenderer = go.AddComponent<SpriteRenderer>();
				spriteRenderer.sprite = frame;

				go.transform.position = position;
				Selection.activeObject = go;

				// Create animation
				if (frames.Length > 1)
				{
					Analytics.Event ("Sprite Drag and Drop", "Drop multiple sprites to scene", "null", 1);
					CreateAnimation (go, frames);
				}
				else
				{
					Analytics.Event ("Sprite Drag and Drop", "Drop single sprite to scene", "null", 1);
				}
				return go;
			}

			return null;
		}

		public static GameObject DropSpriteToSceneToCreateGO (string name, Sprite sprite, Vector3 position)
		{
			GameObject go = new GameObject (name);
			go.name = sprite.name;
			SpriteRenderer spriteRenderer = go.AddComponent<SpriteRenderer> ();
			spriteRenderer.sprite = sprite;
			go.transform.position = position;
			Selection.activeObject = go;

			return go;
		}

		public static Sprite TextureToSprite (Texture2D tex)
		{
			Object[] assets = AssetDatabase.LoadAllAssetsAtPath(AssetDatabase.GetAssetPath(tex));

			for(int i=0; i<assets.Length; i++)
			{
				if (assets[i].GetType () == typeof (Sprite))
				{
					return assets[i] as Sprite;
				}
			}

			return GenerateDefaultSprite(tex);
		}

		private static bool ValidPathForTextureAsset (string path)
		{
			string ext = FileUtil.GetPathExtension (path).ToLower ();
			return ext == "jpg" || ext == "jpeg" || ext == "tif" || ext == "tiff" || ext == "tga" || ext == "gif" || ext == "png" || ext == "psd" || ext == "bmp" || ext == "iff" || ext == "pict" || ext == "pic" || ext == "pct" || ext == "exr";
		}
	}
}
#endif
