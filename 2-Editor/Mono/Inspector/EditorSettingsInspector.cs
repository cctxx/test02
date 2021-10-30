using UnityEditorInternal;
using UnityEngine;
using UnityEditor.VersionControl;

namespace UnityEditor
{
	[CustomEditor(typeof(EditorSettings))]
	internal class EditorSettingsInspector : Editor
	{
		struct PopupElement
		{
			public readonly bool requiresTeamLicense;
			public readonly bool requiresProLicense;
			public readonly GUIContent content;
			
			public bool Enabled
			{
				get { return (!requiresTeamLicense || InternalEditorUtility.HasMaint ()) && (!requiresProLicense || InternalEditorUtility.HasPro ()); }
			}
			
			public PopupElement (string content, bool requiresTeamLicense, bool requiresProLicense)
			{
				this.content = new GUIContent (content);
				this.requiresTeamLicense = requiresTeamLicense;
				this.requiresProLicense = requiresProLicense;
			}
		}
	
		private PopupElement[] vcDefaultPopupList =
		{ 
			new PopupElement (ExternalVersionControl.Disabled, false, false),
			new PopupElement (ExternalVersionControl.Generic, false, false),
			new PopupElement (ExternalVersionControl.AssetServer, true, false),
		};
		
		private PopupElement[] vcPopupList = null;

		private PopupElement[] serializationPopupList =
		{ 
			new PopupElement ("Mixed", false, false),
			new PopupElement ("Force Binary", false, false), 
			new PopupElement ("Force Text", false, false)
		};

#if ENABLE_SPRITES
		private PopupElement[] behaviorPopupList =
		{ 
			new PopupElement ("3D", false, false),
			new PopupElement ("2D", false, false)
		};

		private PopupElement[] spritePackerPopupList =
		{ 
			new PopupElement ("Disabled", false, false),
			new PopupElement ("Enabled For Builds", false, false),
			new PopupElement ("Always Enabled", false, false)
		};
#endif

		private string[] logLevelPopupList =
		{ 
			"Verbose", "Info", "Notice", "Fatal"
		};

		public void OnEnable ()
		{
			Plugin[] availvc = Plugin.availablePlugins;
			vcPopupList = new PopupElement[availvc.Length + vcDefaultPopupList.Length];
			System.Array.Copy(vcDefaultPopupList, vcPopupList, vcDefaultPopupList.Length);

			int j = 0;
			for ( int i = vcDefaultPopupList.Length; i < vcPopupList.Length; ++i, ++j)
			{
				vcPopupList[i] = new PopupElement (availvc[j].name, true, false);
			}
		}

		public override void OnInspectorGUI()
		{
			// GUI.enabled hack because we don't want some controls to be disabled if the EditorSettings.asset is locked
			// since some of the controls are not dependent on the Editor Settings asset. Unfortunately, this assumes 
			// that the editor will only be disabled because of version control locking which may change in the future.
			var editorEnabled = GUI.enabled;
			
			GUI.enabled = true;
			GUILayout.Label ("Version Control", EditorStyles.boldLabel);
			GUI.enabled = editorEnabled;

			ExternalVersionControl selvc = EditorSettings.externalVersionControl;
			
			int popupIndex = System.Array.FindIndex(vcPopupList, (PopupElement cand) => (cand.content.text == selvc) );
			if (popupIndex < 0)
				popupIndex = 0;
			
			var content = new GUIContent (vcPopupList[popupIndex].content);
			var popupRect = GUILayoutUtility.GetRect (content, EditorStyles.popup);
			popupRect = EditorGUI.PrefixLabel (popupRect, 0, new GUIContent ("Mode"));
			if (EditorGUI.ButtonMouseDown (popupRect, content, FocusType.Passive, EditorStyles.popup))
				DoPopup (popupRect, vcPopupList, popupIndex, SetVersionControlSystem);
			
			if (VersionControlSystemHasGUI ())
			{
				GUI.enabled = true;
				bool hasRequiredFields = false;
				bool isUpdating = Provider.onlineState == OnlineState.Updating;

				if (EditorSettings.externalVersionControl == ExternalVersionControl.AssetServer)
				{
					EditorUserSettings.SetConfigValue("vcUsername", EditorGUILayout.TextField ("User", EditorUserSettings.GetConfigValue("vcUsername")));
					EditorUserSettings.SetConfigValue("vcPassword", EditorGUILayout.PasswordField ("Password", EditorUserSettings.GetConfigValue("vcPassword")));
				}
				else if (EditorSettings.externalVersionControl == ExternalVersionControl.Generic)
				{
					
				}
				else if (EditorSettings.externalVersionControl == ExternalVersionControl.Disabled)
				{
					
				}
				else
				{
					ConfigField[] configFields = Provider.GetActiveConfigFields();

					hasRequiredFields = true;
					GUI.enabled = !isUpdating;

					foreach (ConfigField field in configFields)
					{
						string oldVal = EditorUserSettings.GetConfigValue(field.name);
						string newVal = oldVal;

						if (field.isPassword)
						{
							newVal = EditorGUILayout.PasswordField (field.label, oldVal);
						}
						else
						{
							newVal = EditorGUILayout.TextField (field.label, oldVal);
						}
						if ( newVal != oldVal )
							EditorUserSettings.SetConfigValue(field.name, newVal);
						if (field.isRequired && string.IsNullOrEmpty(newVal))
						hasRequiredFields = false;
					}
				}

				// Log level popup
				string logLevel = EditorUserSettings.GetConfigValue("vcSharedLogLevel");
				int idx = System.Array.FindIndex(logLevelPopupList, (item) => item.ToLower() == logLevel);
				if (idx == -1)
				{
					logLevel = "info";
				}
				int newIdx = EditorGUILayout.Popup("Log Level", System.Math.Abs(idx), logLevelPopupList);
				if (newIdx != idx)
				{
					EditorUserSettings.SetConfigValue("vcSharedLogLevel", logLevelPopupList[newIdx].ToLower());
				}

				GUI.enabled = editorEnabled;

				string osState = "Connected";
				if (Provider.onlineState == OnlineState.Updating)
					osState = "Connecting...";
				else if (Provider.onlineState == OnlineState.Offline)
					osState = "Disconnected";

				EditorGUILayout.LabelField("Status", osState);

				if (Provider.onlineState != OnlineState.Online && !string.IsNullOrEmpty(Provider.offlineReason))
				{
					GUI.enabled = false;
					GUILayout.TextArea(Provider.offlineReason);
					GUI.enabled = editorEnabled;
				}

				GUILayout.BeginHorizontal ();
				GUILayout.FlexibleSpace ();
				GUI.enabled = hasRequiredFields && Provider.onlineState != OnlineState.Updating;
				if (GUILayout.Button ("Connect", EditorStyles.miniButton))
					Provider.UpdateSettings ();
				GUILayout.EndHorizontal ();

				EditorUserSettings.AutomaticAdd = EditorGUILayout.Toggle ("Automatic add", EditorUserSettings.AutomaticAdd);
				
				if (Provider.requiresNetwork)
				{
					bool workOfflineNew = EditorGUILayout.Toggle("Work Offline", EditorUserSettings.WorkOffline); // Enabled has a slightly different behaviour
					if (workOfflineNew != EditorUserSettings.WorkOffline)
					{
						// On toggling on show a warning
						if (workOfflineNew && !EditorUtility.DisplayDialog("Confirm working offline", "Working offline and making changes to your assets means that you will have to manually integrate changes back into version control using your standard version control client before you stop working offline in Unity. Make sure you know what you are doing.", "Work offline", "Cancel"))
						{
							workOfflineNew = false; // User cancelled working offline
						}
						EditorUserSettings.WorkOffline = workOfflineNew;
						EditorApplication.RequestRepaintAllViews();
					}
				}

				GUI.enabled = editorEnabled;

				//string DiffTool = EditorGUILayout.TextField ("Diff Tool", DiffTool);
				//string DiffArgs = EditorGUILayout.TextField ("Diff Args", DiffArgs);


				/*if (Unsupported.IsDeveloperBuild ())
				{
						EditorUserSettings.DebugCmd = EditorGUILayout.Toggle ("Debug Cmds", EditorUserSettings.DebugCmd);
					EditorUserSettings.DebugOut = EditorGUILayout.Toggle ("Debug Output", EditorUserSettings.DebugOut);
					EditorUserSettings.DebugCom = EditorGUILayout.Toggle ("Debug Coms", EditorUserSettings.DebugCom);
				}*/
				
				
				DrawOverlayDescriptions();
			}

			GUILayout.Space (10);
			GUILayout.Label ("WWW Security Emulation", EditorStyles.boldLabel);
			
			string url = EditorGUILayout.TextField("Host URL", EditorSettings.webSecurityEmulationHostUrl);
			if (url != EditorSettings.webSecurityEmulationHostUrl)
				EditorSettings.webSecurityEmulationHostUrl = url;

			GUILayout.Space(10);
			
			GUI.enabled = true;
			GUILayout.Label ("Asset Serialization", EditorStyles.boldLabel);
			GUI.enabled = editorEnabled;

			content = new GUIContent(serializationPopupList[(int)EditorSettings.serializationMode].content);
			popupRect = GUILayoutUtility.GetRect (content, EditorStyles.popup);
			popupRect = EditorGUI.PrefixLabel (popupRect, 0, new GUIContent ("Mode"));
			if (EditorGUI.ButtonMouseDown (popupRect, content, FocusType.Passive, EditorStyles.popup))
				DoPopup (popupRect, serializationPopupList, (int)EditorSettings.serializationMode, SetAssetSerializationMode);

#if ENABLE_SPRITES
			GUILayout.Space(10);

			GUI.enabled = true;
			GUILayout.Label ("Default Behavior Mode", EditorStyles.boldLabel);
			GUI.enabled = editorEnabled;

			int index = Mathf.Clamp ((int) EditorSettings.defaultBehaviorMode, 0, behaviorPopupList.Length - 1);
			content = new GUIContent (behaviorPopupList[index].content);
			popupRect = GUILayoutUtility.GetRect (content, EditorStyles.popup);
			popupRect = EditorGUI.PrefixLabel (popupRect, 0, new GUIContent ("Mode"));
			if (EditorGUI.ButtonMouseDown (popupRect, content, FocusType.Passive, EditorStyles.popup))
				DoPopup (popupRect, behaviorPopupList, index, SetDefaultBehaviorMode);

			
			GUILayout.Space(10);

			GUI.enabled = true;
			GUILayout.Label ("Sprite Packer", EditorStyles.boldLabel);
			GUI.enabled = editorEnabled;

			index = Mathf.Clamp ((int) EditorSettings.spritePackerMode, 0, spritePackerPopupList.Length - 1);
			content = new GUIContent (spritePackerPopupList[index].content);
			popupRect = GUILayoutUtility.GetRect (content, EditorStyles.popup);
			popupRect = EditorGUI.PrefixLabel (popupRect, 0, new GUIContent ("Mode"));
			if (EditorGUI.ButtonMouseDown (popupRect, content, FocusType.Passive, EditorStyles.popup))
				DoPopup (popupRect, spritePackerPopupList, index, SetSpritePackerMode);
#endif
		}
		
		private void DrawOverlayDescriptions()
		{
			Texture2D atlas = Provider.overlayAtlas;
			if (atlas == null)
				return;

			GUILayout.Space (10);
			GUILayout.Label ("Overlay legends", EditorStyles.boldLabel);
				
			DrawOverlayDescription(Asset.States.Local);
			DrawOverlayDescription(Asset.States.OutOfSync);
			DrawOverlayDescription(Asset.States.CheckedOutLocal);
			DrawOverlayDescription(Asset.States.CheckedOutRemote);
			DrawOverlayDescription(Asset.States.DeletedLocal);
			DrawOverlayDescription(Asset.States.DeletedRemote);
			DrawOverlayDescription(Asset.States.AddedLocal);
			DrawOverlayDescription(Asset.States.AddedRemote);
			DrawOverlayDescription(Asset.States.Conflicted);
			DrawOverlayDescription(Asset.States.LockedLocal);
			DrawOverlayDescription(Asset.States.LockedRemote);
		}

		void DrawOverlayDescription(Asset.States state)
		{
			Rect atlasUV = Provider.GetAtlasRectForState((int)state);
			if (atlasUV.width == 0f)
				return; // no overlay
		
			Texture2D atlas = Provider.overlayAtlas;
			if (atlas == null)
				return;

			GUILayout.Label ("    " + Asset.StateToString(state), EditorStyles.miniLabel);
			Rect r = GUILayoutUtility.GetLastRect();
			r.width = 16f;
			GUI.DrawTextureWithTexCoords (r, atlas, atlasUV);
		}

		private void DoPopup (Rect popupRect, PopupElement[] elements, int selectedIndex, GenericMenu.MenuFunction2 func)
		{
			GenericMenu menu = new GenericMenu ();
			for (int i = 0; i < elements.Length; i++)
			{
				var element = elements[i];
			
				if (element.Enabled)
					menu.AddItem (element.content, i == selectedIndex, func, i);
				else
					menu.AddDisabledItem (element.content);
			}
			menu.DropDown (popupRect);
		}

		private bool VersionControlSystemHasGUI ()
		{
			ExternalVersionControl system = EditorSettings.externalVersionControl;
			return 
				system != ExternalVersionControl.Disabled &&
				system != ExternalVersionControl.AutoDetect &&
				system != ExternalVersionControl.AssetServer &&
				system != ExternalVersionControl.Generic;
		}

		private void SetVersionControlSystem (object data)
		{
			int popupIndex = (int)data;
			if (popupIndex < 0 && popupIndex >= vcPopupList.Length)
				return;
			
			PopupElement el = vcPopupList[popupIndex];
            string oldVC = EditorSettings.externalVersionControl;

            EditorSettings.externalVersionControl = el.content.text;
            Provider.UpdateSettings();
            AssetDatabase.Refresh();

            if (oldVC != el.content.text)
            {
                if (el.content.text == ExternalVersionControl.AssetServer ||
                    el.content.text == ExternalVersionControl.Disabled ||
                    el.content.text == ExternalVersionControl.Generic
                    )
                {
                    // Close the normal version control window
                    WindowPending.CloseAllWindows();
                }
                else
                {
                    var wins = Resources.FindObjectsOfTypeAll(typeof(ASMainWindow)) as ASMainWindow[];
                    ASMainWindow win = wins.Length > 0 ? wins[0] : null;
                    if (win != null)
                    {
                        win.Close();
                    }
                }
            }
		}
		
		private void SetAssetSerializationMode (object data)
		{
			int popupIndex = (int)data;
			
			EditorSettings.serializationMode = (SerializationMode)popupIndex;
		}

#if ENABLE_SPRITES
		private void SetDefaultBehaviorMode (object data)
		{
			int popupIndex = (int)data;

			EditorSettings.defaultBehaviorMode = (EditorBehaviorMode)popupIndex;
		}

		private void SetSpritePackerMode(object data)
		{
			int popupIndex = (int)data;

			EditorSettings.spritePackerMode = (SpritePackerMode)popupIndex;
		}
#endif
	}
}
