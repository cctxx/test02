using UnityEngine;
using UnityEditor;
using System.Reflection;
using System.Collections.Generic;
using System;


namespace UnityEditor
{
	[CustomEditor(typeof(MonoImporter))]
	internal class MonoScriptImporterInspector : AssetImporterInspector
	{
		const int m_RowHeight = 16;
		
		static GUIContent s_HelpIcon, s_TitleSettingsIcon;
		
		private SerializedObject m_TargetObject;
		private SerializedProperty m_Icon;
		
		internal override void OnHeaderControlsGUI ()
		{
			TextAsset textAsset = assetEditor.target as TextAsset;
			
			GUILayout.FlexibleSpace ();
			
			if (GUILayout.Button("Open...", EditorStyles.miniButton))
			{
				AssetDatabase.OpenAsset(textAsset);	
				GUIUtility.ExitGUI ();
			}
			
			if (textAsset as MonoScript)
			{
				if (GUILayout.Button("Execution Order...", EditorStyles.miniButton))//GUILayout.Width(150)))
				{
					EditorApplication.ExecuteMenuItem("Edit/Project Settings/Script Execution Order");
					GUIUtility.ExitGUI ();
				}
			}
		}
		
		internal override void OnHeaderIconGUI (Rect iconRect)
		{
			if (m_Icon == null)
			{
				m_TargetObject = new SerializedObject (assetEditor.targets);
				m_Icon = m_TargetObject.FindProperty ("m_Icon");
			}
			EditorGUI.ObjectIconDropDown (iconRect, assetEditor.targets, true, null, m_Icon);
		}
		
		// Clear default references
		[MenuItem ("CONTEXT/MonoImporter/Reset")]
		static void ResetDefaultReferences (MenuCommand command)
		{
			MonoImporter importer = command.context as MonoImporter;
			importer.SetDefaultReferences(new string[0], new UnityEngine.Object[0]);
			AssetDatabase.ImportAsset(AssetDatabase.GetAssetPath(importer));
		}
		
		void ShowFieldInfo (Type type, MonoImporter importer, List<string> names, List<UnityEngine.Object> objects, ref bool didModify)
		{
			// Only show default properties for types that support it (so far only MonoBehaviour derived types)
			if (type == null || !type.IsSubclassOf (typeof (MonoBehaviour)))
				return;
			
			ShowFieldInfo (type.BaseType, importer, names, objects, ref didModify);
			
			FieldInfo[] infos = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly);
			foreach (FieldInfo field in infos)	
			{
				if (!field.IsPublic)
				{
					object[] attr = field.GetCustomAttributes(typeof(SerializeField), true);
					if (attr == null || attr.Length == 0)
						continue;
				}
				
				if (field.FieldType.IsSubclassOf(typeof(UnityEngine.Object)) || field.FieldType == typeof(UnityEngine.Object))
				{
					UnityEngine.Object oldTarget = importer.GetDefaultReference(field.Name);
					UnityEngine.Object newTarget = EditorGUILayout.ObjectField(ObjectNames.NicifyVariableName(field.Name), oldTarget, field.FieldType, false);
					
					names.Add(field.Name);
					objects.Add(newTarget);
					
					if (oldTarget != newTarget)
						didModify = true;
				}
			}
		}
		
		public override void OnInspectorGUI()
		{
			Vector2 wasIconSize = EditorGUIUtility.GetIconSize();
			EditorGUIUtility.SetIconSize(new Vector2(m_RowHeight, m_RowHeight));
			
			MonoImporter importer = target as MonoImporter;
			MonoScript script = importer.GetScript();
			
			if (script)
			{
				List<string> names = new List<string> (); 
				List<UnityEngine.Object> objects = new List<UnityEngine.Object> ();
				bool didModify = false;
				Type type = script.GetClass();
				ShowFieldInfo(type, importer, names, objects, ref didModify);
				
				if (didModify)
				{
					importer.SetDefaultReferences(names.ToArray(), objects.ToArray());
					AssetDatabase.ImportAsset(AssetDatabase.GetAssetPath(importer));
				}
			}
			
			EditorGUIUtility.SetIconSize(wasIconSize);
		}
	}

	[CustomEditor(typeof(TextAsset))]
	[CanEditMultipleObjects]
	internal class TextAssetInspector : Editor
	{
		private const int kMaxChars = 7000;
		private GUIStyle m_TextStyle;
		
		public override void OnInspectorGUI()
		{
			if (m_TextStyle == null)
				m_TextStyle = "ScriptText";
			
			bool enabledTemp = GUI.enabled;
			GUI.enabled = true;
			TextAsset textAsset = target as TextAsset;
			if (textAsset != null)
			{
				string text;
				if (targets.Length > 1)
				{
					text = targetTitle;
				}
				else
				{
					text = textAsset.ToString();
					if (text.Length > kMaxChars)
						text = text.Substring (0, kMaxChars) + "...\n\n<...etc...>";
				}
				Rect rect = GUILayoutUtility.GetRect (EditorGUIUtility.TempContent (text), m_TextStyle);
				rect.x = 0;
				rect.y -= 3;
				rect.width = GUIClip.visibleRect.width + 1;
				GUI.Box (rect, text, m_TextStyle);
			}
			GUI.enabled = enabledTemp;
		}
	}
	
	[CustomEditor(typeof(MonoScript))]
	[CanEditMultipleObjects]
	internal class MonoScriptInspector : TextAssetInspector
	{
		protected override void OnHeaderGUI () { }
	}
}
