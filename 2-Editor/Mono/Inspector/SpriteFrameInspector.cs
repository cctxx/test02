#if ENABLE_SPRITES

using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor
{
	[CustomEditor (typeof (Sprite))]
	[CanEditMultipleObjects]
	internal class SpriteInspector : Editor
	{
		public readonly GUIContent[] spriteAlignmentOptions =
			{
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Center"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopLeft"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Top"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopRight"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Left"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Right"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomLeft"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Bottom"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomRight"),
				EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Custom"),
			};

		public readonly GUIContent spriteAlignment = EditorGUIUtility.TextContent ("SpriteInspector.Pivot");

		private Sprite sprite
		{
			get { return target as Sprite; }
		}

		private SpriteMetaData GetMetaData(string name)
		{
			string path = AssetDatabase.GetAssetPath(sprite);
			TextureImporter textureImporter = AssetImporter.GetAtPath(path) as TextureImporter;
			if (textureImporter != null)
			{
				SpriteMetaData[] spritesheet = textureImporter.spritesheet;
				for (int i = 0; i < spritesheet.Length; i++)
				{
					if (spritesheet[i].name.Equals (name))
						return spritesheet[i];
				}
			}

			return new SpriteMetaData();
		}

		public override void OnInspectorGUI ()
		{
			bool nameM;
			bool alignM;
			UnifiedValues(out nameM, out alignM);

			if (nameM)
				EditorGUILayout.LabelField("Name", sprite.name);
			else
				EditorGUILayout.LabelField("Name", "-");

			if (alignM)
			{
				int align = GetMetaData(sprite.name).alignment;
				EditorGUILayout.LabelField(spriteAlignment, spriteAlignmentOptions[align]);
			}
			else
				EditorGUILayout.LabelField(spriteAlignment.text, "-");
		}

		private void UnifiedValues(out bool name, out bool alignment)
		{
			name = true;
			alignment = true;
			if (targets.Length < 2)
				return;

			string path = AssetDatabase.GetAssetPath(sprite);
			TextureImporter textureImporter = AssetImporter.GetAtPath(path) as TextureImporter;
			SpriteMetaData[] spritesheet = textureImporter.spritesheet;

			string previousName = null;
			int previousAligment = -1;

			for (int targetsIndex = 0; targetsIndex < targets.Length; targetsIndex++)
			{
				Sprite curSprite = targets[targetsIndex] as Sprite;
				for (int spritesIndex = 0; spritesIndex < spritesheet.Length; spritesIndex++)
				{
					if (spritesheet[spritesIndex].name.Equals(curSprite.name))
					{
						if (spritesheet[spritesIndex].alignment != previousAligment && previousAligment > 0)
							alignment = false;
						else
							previousAligment = spritesheet[spritesIndex].alignment;

						if (spritesheet[spritesIndex].name != previousName && previousName != null)
							name = false;
						else
							previousName = spritesheet[spritesIndex].name;
					}
				}
			}
		}

		private static Texture2D BuildPreviewTexture (int width, int height, Sprite sprite, Material spriteRendererMaterial)
		{
			if (!ShaderUtil.hardwareSupportsRectRenderTexture)
			{
				return null;
			}
			Material copyMaterial = null;
			Texture2D texture = UnityEditor.Sprites.DataUtility.GetSpriteTexture(sprite, false);
			PreviewHelpers.AdjustWidthAndHeightForStaticPreview((int)sprite.rect.width, (int)sprite.rect.height, ref width, ref height);
			EditorUtility.SetTemporarilyAllowIndieRenderTexture (true);
			SavedRenderTargetState savedRTState = new SavedRenderTargetState ();

			RenderTexture tmp = RenderTexture.GetTemporary (
									width,
									height,
									0,
									RenderTextureFormat.Default,
									RenderTextureReadWrite.Default);

			RenderTexture.active = tmp;
			GL.sRGBWrite = (QualitySettings.activeColorSpace == ColorSpace.Linear);

			GL.Clear (true, true, new Color (0f, 0f, 0f, 0f));

			Texture _oldTexture = null;
			Vector4 _oldTexelSize = new Vector4(0, 0, 0, 0);
			bool _matHasTexture = false;
			bool _matHasTexelSize = false;
			if (spriteRendererMaterial != null)
			{
				_matHasTexture = spriteRendererMaterial.HasProperty("_MainTex");
				_matHasTexelSize = spriteRendererMaterial.HasProperty("_MainTex_TexelSize");
			}

			if (spriteRendererMaterial != null)
			{
				if (_matHasTexture)
				{
					_oldTexture = spriteRendererMaterial.GetTexture("_MainTex");
					spriteRendererMaterial.SetTexture("_MainTex", texture);
				}

				if (_matHasTexelSize)
				{
					_oldTexelSize = spriteRendererMaterial.GetVector("_MainTex_TexelSize");
					spriteRendererMaterial.SetVector("_MainTex_TexelSize", TextureUtil.GetTexelSizeVector(texture));
				}
				
				spriteRendererMaterial.SetPass (0);
			}
			else
			{
				copyMaterial = new Material (Shader.Find ("Hidden/BlitCopy"));

				copyMaterial.mainTexture = texture;
				copyMaterial.SetPass(0);
			}

			int glWidth = TextureUtil.GetGLWidth(texture);
			int glHeight = TextureUtil.GetGLHeight(texture);
			Rect rect = sprite.rect;
			
			GL.PushMatrix ();
			GL.LoadOrtho ();
			GL.Begin (GL.QUADS);
			GL.Color (new Color (1, 1, 1, 1));
			GL.TexCoord(new Vector3(rect.xMin / glWidth, rect.yMax / glHeight, 0));
			GL.Vertex3 (0f, 1f, 0);
			GL.TexCoord(new Vector3(rect.xMax / glWidth, rect.yMax / glHeight, 0));
			GL.Vertex3 (1f, 1f, 0);
			GL.TexCoord(new Vector3(rect.xMax / glWidth, rect.yMin / glHeight, 0));
			GL.Vertex3 (1f, 0f, 0);
			GL.TexCoord(new Vector3(rect.xMin / glWidth, rect.yMin / glHeight, 0));
			GL.Vertex3 (0f, 0f, 0);
			GL.End ();
			GL.PopMatrix ();
			GL.sRGBWrite = false;


			if (spriteRendererMaterial != null)
			{
				if (_matHasTexture)
					spriteRendererMaterial.SetTexture ("_MainTex", _oldTexture);
				if (_matHasTexelSize)
					spriteRendererMaterial.SetVector ("_MainTex_TexelSize", _oldTexelSize);
			}

			Texture2D copy = new Texture2D (width, height, TextureFormat.ARGB32, false);
			copy.hideFlags = HideFlags.HideAndDontSave;

			copy.ReadPixels (new Rect (0, 0, width, height), 0, 0);
			copy.Apply ();
			RenderTexture.ReleaseTemporary (tmp);

			savedRTState.Restore();

			EditorUtility.SetTemporarilyAllowIndieRenderTexture (false);

			if (copyMaterial != null)
				DestroyImmediate (copyMaterial);

			return copy;
		}

		public override Texture2D RenderStaticPreview (string assetPath, Object[] subAssets, int width, int height)
		{
			return BuildPreviewTexture (width, height, sprite, null);
		}

		public override bool HasPreviewGUI ()
		{
			return (target != null);
		}

		public override void OnPreviewGUI (Rect r, GUIStyle background)
		{
			if (target == null)
				return;

			if (Event.current.type != EventType.Repaint)
				return;

			//background.Draw (r, false, false, false, false);
			DrawPreview (r, sprite, null);
		}

		public static void DrawPreview (Rect r, Sprite frame, Material spriteRendererMaterial)
		{
			if (frame == null)
				return;

			float zoomLevel = Mathf.Min (r.width / frame.rect.width, r.height / frame.rect.height);
			Rect wantedRect = new Rect (r.x, r.y, frame.rect.width * zoomLevel, frame.rect.height * zoomLevel);
			wantedRect.center = r.center;

			Texture2D previewTexture = BuildPreviewTexture ((int)wantedRect.width, (int)wantedRect.height, frame, spriteRendererMaterial);
			EditorGUI.DrawTextureTransparent (wantedRect, previewTexture, ScaleMode.ScaleToFit);
			
			DestroyImmediate (previewTexture);
		}

		public override string GetInfoString ()
		{
			if (target == null)
				return "";

			Sprite sprite = target as Sprite;

			return string.Format ("({0}x{1})",
				(int)sprite.rect.width,
				(int)sprite.rect.height
				);
		}
	}
}

#endif
