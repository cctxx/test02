using UnityEngine;
using UnityEditor;
using System.Collections;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
	internal class ASServerAdminWindow
	{
		const int listLenghts = 20;

		static ASMainWindow.Constants constants = null;

		ListViewState lv;
		ListViewState lv2;
		SplitterState lvSplit = new SplitterState(new float[] { 5, 50, 50, 150 }, new int[] { 20, 70, 70, 100 }, null);

		MaintDatabaseRecord[] databases;
		MaintUserRecord[] users;

		ASMainWindow parentWin;
		bool splittersOk = false;

		bool resetKeyboardControl = false;

		enum Action { Main, CreateUser, SetPassword, CreateProject, ModifyUser };

		Action currAction = Action.Main;

		string[] servers;

		string server = string.Empty;
		string user = string.Empty;
		string password = string.Empty;

		string nPassword1 = string.Empty;
		string nPassword2 = string.Empty;
		string nProjectName = string.Empty;
		string nTemplateProjectName = string.Empty;
		string nFullName = string.Empty;
		string nUserName = string.Empty;
		string nEmail = string.Empty;

		bool projectSelected = false;
		bool userSelected = false;
		bool isConnected = false;

		public ASServerAdminWindow(ASMainWindow parentWin)
		{
			lv = new ListViewState(0);
			lv2 = new ListViewState(0);

			this.parentWin = parentWin;

			servers = InternalEditorUtility.GetEditorSettingsList("ASServer", listLenghts);

			server = EditorPrefs.GetString("ASAdminServer");
			user = "admin";
		}

		void ServersPopup()
		{
			if (servers.Length > 0)
			{
				int i = EditorGUILayout.Popup(-1, servers, constants.dropDown, GUILayout.MaxWidth(18));

				if (i >= 0)
				{
					GUIUtility.keyboardControl = 0;
					GUIUtility.hotControl = 0;
					resetKeyboardControl = true;
					server = servers[i];
					parentWin.Repaint();
				}
			}
		}

		bool WordWrappedLabelButton(string label, string buttonText)
		{
			GUILayout.BeginHorizontal();
			GUILayout.Label(label, EditorStyles.wordWrappedLabel);
			bool retval = GUILayout.Button(buttonText, GUILayout.Width(100));
			GUILayout.EndHorizontal();
			return retval;
		}

		bool CanPerformCurrentAction()
		{
			switch (currAction)
			{
				case Action.Main:
					return (server != string.Empty) && (user != string.Empty);
				case Action.CreateUser:
					bool goodName = true;

					// force only simple characters
					for (int i = 0; i < nUserName.Length; i++)
					{
						char c = nUserName[i];

						//"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
						if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '-') || (c == '_')))
						{
							goodName = false;
							break;
						}
					}

					return nFullName != string.Empty && nUserName != string.Empty && nPassword1 != string.Empty &&
						nPassword1 == nPassword2 && goodName;
				case Action.SetPassword:
					return (nPassword1 != string.Empty) && (nPassword1 == nPassword2);
				case Action.CreateProject:
					return nProjectName != string.Empty;
				case Action.ModifyUser:
					return nFullName != string.Empty;
				default:
					return false;
			}
		}

		void PerformCurrentAction()
		{
			switch (currAction)
			{
				case Action.Main:
					currAction = Action.Main;
					DoConnect();
					Event.current.Use();
					break;
				case Action.CreateUser:
					AssetServer.AdminCreateUser(nUserName, nFullName, nEmail, nPassword1);
					currAction = Action.Main;

					if (lv.row > -1)
						DoGetUsers();

					Event.current.Use();
					break;
				case Action.SetPassword:
					AssetServer.AdminChangePassword(users[lv2.row].userName, nPassword1);
					currAction = Action.Main;

					Event.current.Use();
					break;
				case Action.CreateProject:
					if (AssetServer.AdminCreateDB(nProjectName, nTemplateProjectName) != 0)
					{
						DoRefreshDatabases();

						// select newly created project
						for (int i = 0; i < databases.Length; i++)
						{
							if (databases[i].name == nProjectName)
							{
								lv.row = i;
								DoGetUsers();
								break;
							}
						}
					}
					currAction = Action.Main;

					Event.current.Use();
					break;
				case Action.ModifyUser:
					AssetServer.AdminModifyUserInfo(databases[lv.row].dbName, users[lv2.row].userName, nFullName, nEmail);
					currAction = Action.Main;

					if (lv.row > -1)
						DoGetUsers();

					Event.current.Use();
					break;
			}
		}

		void ActionBox()
		{
			bool wasGUIEnabled = GUI.enabled;
			switch (currAction)
			{
				case Action.Main:
					if (!isConnected)
						GUI.enabled = false;

					if (WordWrappedLabelButton("Want to create a new project?", "Create"))
					{
						nProjectName = "";
						nTemplateProjectName = "";
						currAction = Action.CreateProject;
					}

					if (WordWrappedLabelButton("Want to create a new user?", "New User"))
					{
						nPassword1 = nPassword2 = string.Empty;
						nFullName = nUserName = nEmail = string.Empty;
						currAction = Action.CreateUser;
					}

					GUI.enabled = isConnected && userSelected && wasGUIEnabled;

					if (WordWrappedLabelButton("Need to change user password?", "Set Password"))
					{
						nPassword1 = nPassword2 = string.Empty;
						currAction = Action.SetPassword;
					}

					if (WordWrappedLabelButton("Need to change user information?", "Edit"))
					{
						nFullName = users[lv2.row].fullName;
						nEmail = users[lv2.row].email;
						currAction = Action.ModifyUser;
					}

					GUI.enabled = isConnected && projectSelected && wasGUIEnabled;

					if (WordWrappedLabelButton("Duplicate selected project", "Copy Project"))
					{
						nProjectName = "";
						nTemplateProjectName = databases[lv.row].name;
						currAction = Action.CreateProject;
					}
					
					if (WordWrappedLabelButton("Delete selected project", "Delete Project"))
					{
						if (EditorUtility.DisplayDialog("Delete project", "Are you sure you want to delete project " +
							databases[lv.row].name + "? This operation cannot be undone!", "Delete", "Cancel"))
						{
							if (AssetServer.AdminDeleteDB(databases[lv.row].name) != 0)
							{
								DoRefreshDatabases();
								GUIUtility.ExitGUI();
							}
						}
					}

					GUI.enabled = isConnected && userSelected && wasGUIEnabled;

					if (WordWrappedLabelButton("Delete selected user", "Delete User"))
					{
						if (EditorUtility.DisplayDialog("Delete user", "Are you sure you want to delete user " +
							users[lv2.row].userName + "? This operation cannot be undone!", "Delete", "Cancel"))
						{
							if (AssetServer.AdminDeleteUser(users[lv2.row].userName) != 0)
							{
								if (lv.row > -1)
								{
									DoGetUsers();
								}

								GUIUtility.ExitGUI();
							}
						}
					}

					GUI.enabled = wasGUIEnabled;

					break;
				case Action.CreateUser:
					nFullName = EditorGUILayout.TextField("Full Name:", nFullName);
					nEmail = EditorGUILayout.TextField("Email Address:", nEmail);
					GUILayout.Space(5);
					nUserName = EditorGUILayout.TextField("User Name:", nUserName);
					GUILayout.Space(5);
					nPassword1 = EditorGUILayout.PasswordField("Password:", nPassword1);
					nPassword2 = EditorGUILayout.PasswordField("Repeat Password:", nPassword2);

					GUILayout.BeginHorizontal();
					GUILayout.FlexibleSpace();

					GUI.enabled = CanPerformCurrentAction() && wasGUIEnabled;

					if (GUILayout.Button("Create User", constants.smallButton))
					{
						PerformCurrentAction();
					}

					GUI.enabled = wasGUIEnabled;

					if (GUILayout.Button("Cancel", constants.smallButton))
						currAction = Action.Main;
					GUILayout.EndHorizontal();
					break;
				case Action.SetPassword:
					GUILayout.Label("Setting password for user: " + users[lv2.row].userName, constants.title);

					GUILayout.Space(5);

					nPassword1 = EditorGUILayout.PasswordField("Password:", nPassword1);
					nPassword2 = EditorGUILayout.PasswordField("Repeat Password:", nPassword2);

					GUILayout.Space(5);

					GUILayout.BeginHorizontal();
					GUILayout.FlexibleSpace();

					GUI.enabled = CanPerformCurrentAction() && wasGUIEnabled;

					if (GUILayout.Button("Change Password", constants.smallButton))
					{
						PerformCurrentAction();
					}

					GUI.enabled = wasGUIEnabled;

					if (GUILayout.Button("Cancel", constants.smallButton))
						currAction = Action.Main;
					GUILayout.EndHorizontal();
					break;
				case Action.CreateProject:
					nProjectName = EditorGUILayout.TextField("Project Name:", nProjectName);

					GUILayout.BeginHorizontal();
					GUILayout.FlexibleSpace();

					GUI.enabled = CanPerformCurrentAction() && wasGUIEnabled;

					if (GUILayout.Button(nTemplateProjectName==""?"Create Project":"Copy "+nTemplateProjectName, constants.smallButton))
					{
						PerformCurrentAction();
					}

					GUI.enabled = wasGUIEnabled;

					if (GUILayout.Button("Cancel", constants.smallButton))
						currAction = Action.Main;
					GUILayout.EndHorizontal();
					break;
				case Action.ModifyUser:
					nFullName = EditorGUILayout.TextField("Full Name:", nFullName);
					nEmail = EditorGUILayout.TextField("Email Address:", nEmail);
					GUILayout.Space(5);

					GUILayout.BeginHorizontal();
					GUILayout.FlexibleSpace();

					GUI.enabled = CanPerformCurrentAction() && wasGUIEnabled;

					if (GUILayout.Button("Change", constants.smallButton))
					{
						PerformCurrentAction();
					}

					GUI.enabled = wasGUIEnabled;

					if (GUILayout.Button("Cancel", constants.smallButton))
						currAction = Action.Main;
					GUILayout.EndHorizontal();
					break;
			}
		}

		void DoRefreshDatabases()
		{
			MaintDatabaseRecord[] rec = AssetServer.AdminRefreshDatabases();

			if (rec != null)
			{
				databases = rec;
				isConnected = true;
			}
			else
			{
				databases = new MaintDatabaseRecord[] { };
				lv2.totalRows = 0;
			}

			lv.row = -1;
			lv.totalRows = databases.Length;
			lv2.totalRows = 0;
			users = new MaintUserRecord[] { };
		}

		void DoConnect()
		{
			EditorPrefs.SetString("ASAdminServer", server);

			userSelected = false;
			isConnected = false;
			projectSelected = false;

			lv.row = -1;
			lv2.row = -1;

			lv.totalRows = 0;
			lv2.totalRows = 0;

			int port = ASEditorBackend.kDefaultServerPort;
			string lserver;

			if (server.IndexOf(":") > 0)
			{
				int.TryParse(server.Substring(server.IndexOf(":") + 1), out port);
				lserver = server.Substring(0, server.IndexOf(":"));
			}
			else
				lserver = server;


			AssetServer.AdminSetCredentials(lserver, port, user, password);
			DoRefreshDatabases();
		}

		void DoGetUsers()
		{
			MaintUserRecord[] rec = AssetServer.AdminGetUsers(databases[lv.row].dbName);

			if (rec != null)
				users = rec;
			else
				users = new MaintUserRecord[] { };

			lv2.totalRows = users.Length;
			lv2.row = -1;
		}

		public bool DoGUI()
		{
			bool wasGUIEnabled = GUI.enabled;

			if (constants == null)
			{
				constants = new ASMainWindow.Constants();
				constants.toggleSize = constants.toggle.CalcSize(new GUIContent("X"));
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

			Event evt = Event.current;

			if (evt.type == EventType.KeyDown && evt.keyCode == KeyCode.Return)
			{
				if (CanPerformCurrentAction())
					PerformCurrentAction();
			}

			if (evt.type == EventType.KeyDown && evt.keyCode == KeyCode.Escape && currAction != Action.Main)
			{
				currAction = Action.Main;
				evt.Use();
			}

			GUILayout.BeginHorizontal();
			server = EditorGUILayout.TextField("Server Address:", server);
			ServersPopup();
			GUILayout.EndHorizontal();

			user = EditorGUILayout.TextField("User Name:", user);
			password = EditorGUILayout.PasswordField("Password:", password);

			GUILayout.BeginHorizontal();

			GUILayout.FlexibleSpace();

			GUI.enabled = CanPerformCurrentAction() && wasGUIEnabled;

			if (GUILayout.Button("Connect", constants.smallButton))
			{
				PerformCurrentAction();
			}

			GUI.enabled = wasGUIEnabled;

			GUILayout.EndHorizontal();

			if (AssetServer.GetAssetServerError() != string.Empty)
			{
				GUILayout.Label(AssetServer.GetAssetServerError(), constants.errorLabel);
			}

			GUILayout.EndVertical();
			GUILayout.EndVertical();
			

			GUILayout.BeginVertical(constants.groupBox);
			GUILayout.Box("Admin Actions", constants.title);
			GUILayout.BeginVertical(constants.contentBox);
			ActionBox();
			GUILayout.EndVertical();
			GUILayout.EndVertical();

			GUILayout.EndHorizontal();

			GUILayout.BeginHorizontal();
			//---
			// ListView projects
			GUILayout.BeginVertical(constants.groupBox);

			GUILayout.Box("Project", constants.title);

			foreach (ListViewElement el in ListViewGUILayout.ListView(lv, constants.background))
			{
				if (el.row == lv.row && Event.current.type == EventType.Repaint)
					constants.entrySelected.Draw(el.position, false, false, false, false);

				GUILayout.Label(databases[el.row].name);
			}

			if (lv.selectionChanged)
			{
				if (lv.row > -1)
					projectSelected = true;

				currAction = Action.Main;

				DoGetUsers();
			}

			GUILayout.EndVertical();
			//---

			//---
			// ListView Users
			GUILayout.BeginVertical(constants.groupBox);

			SplitterGUILayout.BeginHorizontalSplit(lvSplit);
			GUILayout.Box("", constants.columnHeader);
			GUILayout.Box("User", constants.columnHeader);
			GUILayout.Box("Full Name", constants.columnHeader);
			GUILayout.Box("Email", constants.columnHeader);
			SplitterGUILayout.EndHorizontalSplit();

			int margins = EditorStyles.label.margin.left; // TODO: handle this properly

			foreach (ListViewElement el in ListViewGUILayout.ListView(lv2, constants.background))
			{
				if (el.row == lv2.row && Event.current.type == EventType.Repaint)
					constants.entrySelected.Draw(el.position, false, false, false, false);

				bool wasToggle = users[el.row].enabled != 0;
				bool nowToggle = GUI.Toggle(new Rect(el.position.x + 2, el.position.y - 1, constants.toggleSize.x, constants.toggleSize.y), wasToggle, "");
				GUILayout.Space(constants.toggleSize.x);

				if (wasToggle != nowToggle)
				{
					if (AssetServer.AdminSetUserEnabled(databases[lv.row].dbName, users[el.row].userName, 
						users[el.row].fullName, users[el.row].email, nowToggle ? 1 : 0))
					{
						users[el.row].enabled = nowToggle ? 1 : 0;
					}
				}

				GUILayout.Label(users[el.row].userName, GUILayout.Width(lvSplit.realSizes[1] - margins));
				GUILayout.Label(users[el.row].fullName, GUILayout.Width(lvSplit.realSizes[2] - margins));
				GUILayout.Label(users[el.row].email);
			}

			if (lv2.selectionChanged)
			{
				if (lv2.row > -1)
					userSelected = true;

				if (currAction == Action.SetPassword)
					currAction = Action.Main;
			}


			GUILayout.EndVertical();
			//---

			GUILayout.EndHorizontal();

			GUILayout.Space(10);

			if (!splittersOk && Event.current.type == EventType.Repaint)
			{
				splittersOk = true;
				parentWin.Repaint();
			}

			return true;
		}
	}
}