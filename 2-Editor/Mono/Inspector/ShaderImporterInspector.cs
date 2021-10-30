using UnityEngine;
using UnityEditor;
using System.Reflection;
using System.Collections.Generic;
using System;


namespace UnityEditor
{
	[CustomEditor(typeof(ShaderImporter))]
	internal class ShaderImporterInspector : AssetImporterInspector
	{
		private List<string> names = new List<string> (); 
		private List<Texture> textures = new List<Texture> ();
		private List<ShaderUtil.ShaderPropertyTexDim> dimensions = new List<ShaderUtil.ShaderPropertyTexDim> ();

		internal override void OnHeaderControlsGUI ()
		{
			var shaderAsset = assetEditor.target as Shader;
			GUILayout.FlexibleSpace ();
			
			if (GUILayout.Button("Open...", EditorStyles.miniButton))
			{
				AssetDatabase.OpenAsset(shaderAsset);	
				GUIUtility.ExitGUI ();
			}
		}

		public void OnEnable ()
		{
			ResetValues ();
		}

		private void ShowDefaultTextures (Shader shader, ShaderImporter importer)
		{
			for (var i = 0; i < names.Count; i++)
			{
				var propertyName = names[i];
				var displayName = ObjectNames.NicifyVariableName (propertyName);
				var oldTexture = textures[i];
				Texture newTexture = null;

				EditorGUI.BeginChangeCheck ();
				switch (dimensions[i])
				{
					case ShaderUtil.ShaderPropertyTexDim.TexDim2D:
					{
						newTexture = EditorGUILayout.ObjectField(displayName, oldTexture, typeof(Texture2D), false) as Texture2D;
						break;
					}
					case ShaderUtil.ShaderPropertyTexDim.TexDimCUBE:
					{
						newTexture = EditorGUILayout.ObjectField(displayName, oldTexture, typeof(Cubemap), false) as Cubemap;
						break;
					}
				}

				if (EditorGUI.EndChangeCheck ())
					textures[i] = newTexture;
			}
		}

		internal override bool HasModified ()
		{
			if (base.HasModified ())
				return true;

			var importer = target as ShaderImporter;
			if (importer == null)
				return false;

			var shader = importer.GetShader ();
			if (shader == null)
				return false;
				
			var propertyCount = ShaderUtil.GetPropertyCount (shader);
			for (int i = 0; i < propertyCount; i++)
			{
				var propertyName = ShaderUtil.GetPropertyName (shader, i);
				for (int k = 0; k < names.Count; k++)
				{
					if (names[k] == propertyName && textures[k] != importer.GetDefaultTexture (propertyName))
						return true;
				}
			}
			return false;
		}
		
		internal override void ResetValues ()
		{
			base.ResetValues ();

			names = new List<string> (); 
			textures = new List<Texture> ();
			dimensions = new List<ShaderUtil.ShaderPropertyTexDim> ();
			
			var importer = target as ShaderImporter;
			if (importer == null)
				return;

			var shader = importer.GetShader ();
			if (shader == null)
				return;
			
			var propertyCount = ShaderUtil.GetPropertyCount (shader);

			for (var i = 0; i < propertyCount; i++)
			{
				if (ShaderUtil.GetPropertyType(shader, i) != ShaderUtil.ShaderPropertyType.TexEnv)
					continue;

				var propertyName = ShaderUtil.GetPropertyName (shader, i);
				var texture = importer.GetDefaultTexture (propertyName);
				
				names.Add (propertyName);
				textures.Add (texture);
				dimensions.Add (ShaderUtil.GetTexDim (shader, i));
			}
		}

		internal override void Apply ()
		{
			base.Apply ();

			var importer = target as ShaderImporter;
			if (importer == null)
				return;

				importer.SetDefaultTextures(names.ToArray(), textures.ToArray());
				AssetDatabase.ImportAsset(AssetDatabase.GetAssetPath(importer));
		}

		private static int GetNumberOfTextures (Shader shader)
		{
			int numberOfTextures = 0;
			var propertyCount = ShaderUtil.GetPropertyCount(shader);

			for (var i = 0; i < propertyCount; i++)
			{
				if (ShaderUtil.GetPropertyType(shader, i) == ShaderUtil.ShaderPropertyType.TexEnv)
					numberOfTextures++;
			}
			return numberOfTextures;
		}
		
		public override void OnInspectorGUI()
		{
			var importer = target as ShaderImporter;
			if (importer == null)
				return;

			var shader = importer.GetShader ();
			if (shader == null)
				return;

			if (GetNumberOfTextures(shader) != names.Count)
				ResetValues ();

			ShowDefaultTextures (shader, importer);
			ApplyRevertGUI ();
		}
	}
}
