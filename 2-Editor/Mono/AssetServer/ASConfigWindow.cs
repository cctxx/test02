using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
	internal class ASConfigWindow
	{
		const int listLenghts = 20;

		static ASMainWindow.Constants constants = null;

		ListViewState serversLv = new ListViewState(0);
		ListViewState projectsLv = new ListViewState(0);

		string serverAddress = string.Empty;
		string projectName = string.Empty;
		string userName = string.Empty;
		string password = string.Empty;
		string port = string.Empty;

		bool resetKeyboardControl = false;

		string[] projectsList, serversList;

		PListConfig plc;

		ASMainWindow parentWin;

		void LoadConfig()
		{
			PListConfig plc = new PListConfig(ASEditorBackend.kServerSettingsFile);

			serverAddress = plc[ASEditorBackend.kServer];
			userName = plc[ASEditorBackend.kUserName];
			port = plc[ASEditorBackend.kPortNumber];
			projectName = plc[ASEditorBackend.kProjectName];
			password = ASEditorBackend.GetPassword(serverAddress, userName);

			if (port != string.Empty && port != ASEditorBackend.kDefaultServerPort.ToString())
				serverAddress += ":" + port;

			serversList = InternalEditorUtility.GetEditorSettingsList("ASServer", listLenghts);

			serversLv.totalRows = serversList.Length;

			if (ArrayUtility.Contains(serversList, serverAddress))
				serversLv.row = ArrayUtility.IndexOf(serversList, serverAddress);
		}

		public ASConfigWindow(ASMainWindow parent)
		{
			parentWin = parent;
			LoadConfig();
		}

		void GetUserAndPassword()
		{
			string str = ASEditorBackend.GetUser(serverAddress);

			if (str != string.Empty)
			{
				userName = str;
			}

			str = ASEditorBackend.GetPassword(serverAddress, str);

			if (str != string.Empty)
				password = str;
		}

		void GetDefaultPListConfig()
		{
			plc = new PListConfig(ASEditorBackend.kServerSettingsFile);

			plc[ASEditorBackend.kServer] = "";
			plc[ASEditorBackend.kUserName] = "";
			plc[ASEditorBackend.kDatabaseName] = "";
			plc[ASEditorBackend.kPortNumber] = "";
			plc[ASEditorBackend.kProjectName] = "";
			plc[ASEditorBackend.kPassword] = ""; // don't store this in project config

			if (plc[ASEditorBackend.kSettingsType] == string.Empty)
				plc[ASEditorBackend.kSettingsType] = "manual";

			if (plc[ASEditorBackend.kTimeout] == string.Empty)
				plc[ASEditorBackend.kTimeout] = "5";

			if (plc[ASEditorBackend.kConnectionSettings] == string.Empty)
				plc[ASEditorBackend.kConnectionSettings] = "";
		}

		void DoShowProjects()
		{
			int port = ASEditorBackend.kDefaultServerPort;
			string server = serverAddress;

			if (server.IndexOf(":") > 0)
			{
				int.TryParse(server.Substring(server.IndexOf(":") + 1), out port);
				server = server.Substring(0, server.IndexOf(":"));
			}

			AssetServer.AdminSetCredentials(server, port, userName, password);

			MaintDatabaseRecord[] rec = AssetServer.AdminRefreshDatabases();

			if (rec != null)
			{
				projectsList = new string[rec.Length];

				for (int i = 0; i < rec.Length; i++)
					projectsList[i] = rec[i].name;

				projectsLv.totalRows = rec.Length;

				GetDefaultPListConfig();

				plc[ASEditorBackend.kServer] = server;
				plc[ASEditorBackend.kUserName] = userName;
				plc[ASEditorBackend.kPortNumber] = this.port;

				plc.Save();

				ASEditorBackend.SetPassword(server, userName, password);
				ASEditorBackend.AddUser(serverAddress, userName);
			}
			else
				projectsLv.totalRows = 0;
		}

		void ClearConfig()
		{
			if (EditorUtility.DisplayDialog("Clear Configuration", "Are you sure you want to disconnect from Asset Server project and clear all configuration values?", "Clear", "Cancel"))
			{
				plc = new PListConfig(ASEditorBackend.kServerSettingsFile);
				plc.Clear();
				plc.Save();
				LoadConfig();
				projectsLv.totalRows = 0;
				ASEditorBackend.InitializeMaintBinding();
				resetKeyboardControl = true;
			}
		}

		void DoConnect()
		{
			AssetServer.RemoveMaintErrorsFromConsole();

			int port = ASEditorBackend.kDefaultServerPort;
			string server = serverAddress;

			if (server.IndexOf(":") > 0)
			{
				int.TryParse(server.Substring(server.IndexOf(":") + 1), out port);
				server = server.Substring(0, server.IndexOf(":"));
			}

			this.port = port.ToString();

			string dbName = AssetServer.GetDatabaseName(server, userName, password, this.port, projectName);

			GetDefaultPListConfig();

			plc[ASEditorBackend.kServer] = server;
			plc[ASEditorBackend.kUserName] = userName;
			plc[ASEditorBackend.kDatabaseName] = dbName;
			plc[ASEditorBackend.kPortNumber] = this.port;
			plc[ASEditorBackend.kProjectName] = projectName;

			plc.Save();

			if (ArrayUtility.Contains(serversList, serverAddress))
				ArrayUtility.Remove(ref serversList, serverAddress);

			ArrayUtility.Insert(ref serversList, 0, serverAddress);

			ASEditorBackend.AddUser(serverAddress, userName);
			ASEditorBackend.SetPassword(serverAddress, userName, password);

			InternalEditorUtility.SaveEditorSettingsList("ASServer", serversList, listLenghts);

			if (dbName != string.Empty)
			{
				ASEditorBackend.InitializeMaintBinding();

				parentWin.Reinit();
				GUIUtility.ExitGUI();
			}
			else
			{
				parentWin.NeedsSetup = true;
				parentWin.Repaint();
			}
		}

		void ServersPopup()
		{
			if (serversList.Length > 0)
			{
				int i = EditorGUILayout.Popup(-1, serversList, constants.dropDown, GUILayout.MaxWidth(18));

				if (i >= 0)
				{
					GUIUtility.keyboardControl = 0;
					GUIUtility.hotControl = 0;
					resetKeyboardControl = true;
					serverAddress = serversList[i];
					parentWin.Repaint();
				}
			}
		}

		private void DoConfigGUI()
		{
			bool wasGUIEnabled = GUI.enabled;
			bool wasChanged = GUI.changed;
			GUI.changed = false;

			bool passwordReturn = false;
			bool projectReturn = false;

			Event evt = Event.current;

			if (evt.type == EventType.KeyDown)
			{
				// on mac this is one single event, on windows two separate ones
				bool returnPressed;
				
				if (Application.platform == RuntimePlatform.OSXEditor)
				{
					returnPressed = (evt.character == '\n' || (int)evt.character == 3) || evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter;
				}
				else
				{
					if (evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter)
						evt.Use();
						
					returnPressed = (evt.character == '\n' || (int)evt.character == 3);
				}
				
				if (returnPressed)
				{
					switch (GUI.GetNameOfFocusedControl())
					{
						case "password":
							passwordReturn = true;
							break;
						case "project":
							projectReturn = true;
							break;
						default:
							evt.Use(); // return key should not do anything here
							break;
					}
				}
			}

			GUILayout.BeginHorizontal();
			serverAddress = EditorGUILayout.TextField("Server", serverAddress);

			ServersPopup();

			GUILayout.EndHorizontal();

			if (GUI.changed)
				GetUserAndPassword();

			GUI.changed |= wasChanged;

			userName = EditorGUILayout.TextField("User Name", userName);

			GUI.SetNextControlName("password");
			password = EditorGUILayout.PasswordField("Password", password); // has max length of 50 - no longer supported

			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();

			GUI.enabled = userName != string.Empty && password != string.Empty && serverAddress != string.Empty && wasGUIEnabled;

			if (GUILayout.Button("Show Projects", GUILayout.MinWidth(100)) || (passwordReturn && GUI.enabled))
			{
				DoShowProjects();
				projectName = "";
				EditorGUI.FocusTextInControl("project");
			}

			bool wasEnabled = GUI.enabled;
			GUI.enabled = wasGUIEnabled;

			if (GUILayout.Button("Clear Configuration"))
			{
				ClearConfig();
			}

			GUI.enabled = wasEnabled;

			GUILayout.EndHorizontal();

			GUILayout.Space(5);

			wasChanged = GUI.changed;
			GUI.changed = false;

			GUI.SetNextControlName("project");
			projectName = EditorGUILayout.TextField("Project Name", projectName);

			GUI.changed |= wasChanged;

			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();

			GUI.enabled = userName != string.Empty && password != string.Empty && serverAddress != string.Empty && projectName != string.Empty && wasGUIEnabled;

			if (GUILayout.Button("Connect", constants.bigButton, GUILayout.MinWidth(100)) || (projectReturn && GUI.enabled))
			{
				DoConnect();
			}

			GUI.enabled = wasGUIEnabled;

			GUILayout.EndHorizontal();
		}

		private void DoProjectsGUI()
		{
			GUILayout.BeginVertical(constants.groupBox);

			GUILayout.Label("Projects on Server", constants.title);

			foreach (ListViewElement el in ListViewGUILayout.ListView(projectsLv, constants.background))
			{
				if (el.row == projectsLv.row && Event.current.type == EventType.Repaint)
					constants.entrySelected.Draw(el.position, false, false, false, false);

				GUILayout.Label((string)projectsList[el.row], constants.element);
			}

			if (projectsLv.selectionChanged)
			{
				projectName = (string)projectsList[projectsLv.row];
			}

			GUILayout.EndVertical();
		}

		public bool DoGUI()
		{
			if (constants == null)
			{
				constants = new ASMainWindow.Constants();
			}

			if (resetKeyboardControl)
			{
				resetKeyboardControl = false;
				GUIUtility.keyboardControl = 0;
			}

			GUILayout.BeginHorizontal();
			
			GUILayout.BeginVertical(constants.groupBox);
			GUILayout.Box("Server Connection", constants.title);
			GUILayout.BeginVertical(constants.contentBox);

			DoConfigGUI();

			if (AssetServer.GetAssetServerError() != string.Empty)
			{
				GUILayout.Space(10);
				GUILayout.Label(AssetServer.GetAssetServerError(), constants.errorLabel);
				GUILayout.Space(10);
			}
			
			GUILayout.EndVertical();
			GUILayout.EndVertical();

			DoProjectsGUI();

			GUILayout.EndHorizontal();

			return true;
		}
	}
}