using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.IO;
using System.Reflection;

namespace UnityEditor
{
	internal class ASEditorBackend
	{
		public const string kServerSettingsFile = "Library/ServerPreferences.plist";
		public const string kUserName = "Maint UserName";
		public const string kPassword = "Maint Password";
		public const string kTimeout = "Maint Timeout";
		public const string kSettingsType = "Maint settings type";
		public const string kConnectionSettings = "Maint Connection Settings";
		public const string kPortNumber = "Maint port number";
		public const string kServer = "Maint Server";
		public const string kDatabaseName = "Maint database name";
		public const string kProjectName = "Maint project name";

		public const int kDefaultServerPort = 10733;

        public static ASMainWindow asMainWin = null;

        public static ASMainWindow ASWin { get { return asMainWin != null ? asMainWin : EditorWindow.GetWindowDontShow<ASMainWindow>(); } }

		public static void DoAS ()
		{
            if (!ASWin.Error)
            {
                ASWin.Show();
                ASWin.Focus();
            }
		}

        public static void ShowASConflictResolutionsWindow(string[] conflicting)
        {
            ASWin.ShowConflictResolutions(conflicting);
        }

        public static void CommitItemsChanged()
        {
            // this gets called from cpp side without knowing if window is active
            if (asMainWin != null || (asMainWin == null && Resources.FindObjectsOfTypeAll(typeof(ASMainWindow)).Length != 0))
                ASWin.CommitItemsChanged();
        }

        //------------------------------------------
        // Callbacks for after mono-to-cpp asset server action is finished
        //---

        public static void CBReinitCommitWindow(int actionResult)
        {
            if (ASWin.asCommitWin != null)
                ASWin.asCommitWin.Reinit(actionResult != 0);
        }

        public static void CBCommitFinished(int actionResult)
        {
            if (ASWin.asCommitWin != null)
                ASWin.asCommitWin.CommitFinished(actionResult != 0);
        }

        public static void CBOverviewsCommitFinished(int actionResult)
        {
            if (ASWin != null)
                ASWin.CommitFinished(actionResult != 0);
        }

        public static void CBReinitOnSuccess(int actionResult)
        {
            if (actionResult != 0)
                ASWin.Reinit();
            else
                ASWin.Repaint();
        }

        public static void CBReinitASMainWindow()
        {
            ASWin.Reinit();
        }

        public static void CBDoDiscardChanges(int actionResult) { ASWin.DoDiscardChanges(actionResult != 0); }
		public static void CBInitUpdatePage(int actionResult) { ASWin.InitUpdatePage(actionResult != 0); }
		public static void CBInitHistoryPage(int actionResult) { ASWin.InitHistoryPage(actionResult != 0); }
		public static void CBInitOverviewPage(int actionResult) { ASWin.InitOverviewPage(actionResult != 0); }

        //-----------


		public static bool SettingsIfNeeded()
		{
			return InitializeMaintBinding ();
		}

		public static bool SettingsAreValid()
		{
			PListConfig plc = new PListConfig(kServerSettingsFile);

			string userName = plc[kUserName];
			string server = plc[kServer];
			string databaseName = plc[kDatabaseName];
			string timeout = plc[kTimeout];
			string port = plc[kPortNumber];

			return !(userName.Length == 0 || server.Length == 0 || databaseName.Length == 0 || timeout.Length == 0 || port.Length == 0);
		}

        internal static string GetPassword(string server, string user)
        {
            string key = "ASPassword::" + server + "::" + user;
            return EditorPrefs.GetString(key, string.Empty);
        }

		internal static void SetPassword(string server, string user, string password)
		{
			string key = "ASPassword::" + server + "::" + user;
			EditorPrefs.SetString(key, password);
		}

		internal static void AddUser(string server, string user)
		{
			string key = "ASUser::" + server;
			EditorPrefs.SetString(key, user);
		}

		internal static string GetUser(string server)
		{
			string key = "ASUser::" + server;
			return EditorPrefs.GetString(key, string.Empty);
		}

		public static bool InitializeMaintBinding ()
		{
			PListConfig plc = new PListConfig(kServerSettingsFile);

			string userName = plc[kUserName];
			string server = plc[kServer];
			string project = plc[kProjectName];
			string databaseName = plc[kDatabaseName];
			string port = plc[kPortNumber];

			int timeout;

			if (!int.TryParse(plc[kTimeout], out timeout))
				timeout = 5;

			if ( server.Length == 0 || project.Length == 0 || databaseName.Length == 0 || userName.Length == 0)
			{
				AssetServer.SetProjectName("");
				return false;
			}
		
			AssetServer.SetProjectName(string.Format("{0} @ {1}", project, server));

			string connectionString =
				"host='" + server +
				"' user='" + userName +
				"' password='" + GetPassword(server, userName) +
				"' dbname='" + databaseName + 
				"' port='" + port + 
				"' sslmode=disable " + plc[kConnectionSettings];
		
			AssetServer.Initialize(userName, connectionString, timeout);

			return true;
		}

		/*-----------------------------------------------------------------------------*/
		/* Functions for asset server testing */
		/*-----------------------------------------------------------------------------*/
		static string s_TestingConflictResClass;
		static string s_TestingConflictResFunction;

		public static void Testing_SetActionFinishedCallback(string klass, string name)
		{
			// these are reset when recompiling, so saving in cpp side
			AssetServer.SaveString("s_TestingClass", klass);
			AssetServer.SaveString("s_TestingFunction", name);
			AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "Testing_DummyCallback");
		}

		static void Testing_DummyCallback(bool success)
		{
			Testing_Invoke(AssetServer.GetAndRemoveString("s_TestingClass"), AssetServer.GetAndRemoveString("s_TestingFunction"), success);
		}

		static void Testing_SetExceptionHandler(string exceptionHandlerClass, string exceptionHandlerFunction)
		{
			AssetServer.SaveString("s_ExceptionHandlerClass", exceptionHandlerClass);
			AssetServer.SaveString("s_ExceptionHandlerFunction", exceptionHandlerFunction);
		}

		static void Testing_Invoke(string klass, string method, params object[] prms)
		{
			try
			{
				System.AppDomain app = System.AppDomain.CurrentDomain;
				foreach (System.Reflection.Assembly asm in app.GetAssemblies())
				{
					if (asm.GetName().Name != "UnityEditor" && asm.GetName().Name != "UnityEngine")
					{
						foreach (System.Type t in AssemblyHelper.GetTypesFromAssembly(asm))
						{
							if (t.Name == klass)
							{
								t.InvokeMember(method, BindingFlags.InvokeMethod | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static, null, null, prms);
							}
						}
					}
				}
			}
			catch (System.Exception e)
			{
				if (e.InnerException != null && e.InnerException.GetType() == typeof(ExitGUIException))
					throw e;

				Testing_Invoke(AssetServer.GetString("s_ExceptionHandlerClass"), AssetServer.GetString("s_ExceptionHandlerFunction"), new object[] { e });
			}
		}

		public static void Testing_SetActiveDatabase(string host, int port, string projectName, string dbName, string user, string pwd)
		{
			PListConfig plc = new PListConfig(kServerSettingsFile);
			
			plc[kServer] = host;
			plc[kUserName] = user;
			plc[kDatabaseName] = dbName;
			plc[kPortNumber] = port.ToString();
			plc[kProjectName] = projectName;
			//!!!
			plc[kPassword] = "";
			plc[kSettingsType] = "manual";
			plc[kTimeout] = "5";
			plc[kConnectionSettings] = "";

			plc.Save();
		}

		public static bool Testing_SetupDatabase(string host, int port, string adminUser, string adminPwd, string user, string pwd, string projectName)
		{
			AssetServer.AdminSetCredentials(host, port, adminUser, adminPwd);

			// check if database already exists
			MaintDatabaseRecord[] recs = AssetServer.AdminRefreshDatabases();

			if (recs == null)
				return false;

			foreach (MaintDatabaseRecord dbrec in recs)
			{
				if (dbrec.name == projectName)
					AssetServer.AdminDeleteDB(projectName);
			}
			
			if (AssetServer.AdminCreateDB(projectName) == 0)
			{
				return false;
			}

			string dbName = AssetServer.GetDatabaseName(host, adminUser, adminPwd, port.ToString(), projectName);

            if (!AssetServer.AdminSetUserEnabled(dbName, user, user, string.Empty, 1))
			{
				return false;
			}

			Testing_SetActiveDatabase(host, port, projectName, dbName, user, pwd);

			return true;
		}

		public static string[] Testing_GetAllDatabaseNames()
		{
			MaintDatabaseRecord[] recs = AssetServer.AdminRefreshDatabases();
			string[] result = new string[recs.Length];

			for (int i = 0; i < recs.Length; i++)
				result[i] = recs[i].name;

			return result;
		}

		public static void Testing_SetConflictResolutionFunction(string klass, string fn)
		{
			s_TestingConflictResClass = klass;
			s_TestingConflictResFunction = fn;
		}

		public static void Testing_DummyConflictResolutionFunction(string[] conflicting)
		{
			Testing_Invoke(s_TestingConflictResClass, s_TestingConflictResFunction, new object[] {conflicting});
		}

		/*-----------------------------------------------------------------------------*/
	}

	internal class PListConfig
	{
		string fileName;
		string xml;

		// XML parsers must die
		public PListConfig(string fileName)
		{
			if (System.IO.File.Exists(fileName))
			{
				StreamReader sr = new StreamReader(fileName);
				xml = sr.ReadToEnd();
				sr.Close();
			}else
				Clear();

			this.fileName = fileName;
		}

		private static Regex GetRegex(string paramName)
		{
			return new Regex(@"(?<Part1><key>" + paramName + @"</key>\s*<string>)(?<Value>.*)</string>");
		}

		public string this[string paramName]
		{
			get 
			{
				Match m = GetRegex(paramName).Match(xml);
				return m.Success ? m.Groups["Value"].Value : string.Empty;
			}
			set 
			{
				Match m = GetRegex(paramName).Match(xml);
				if (m.Success)
					xml = GetRegex(paramName).Replace(xml, "${Part1}" + value + "</string>");
				else
					WriteNewValue(paramName, value);
			}
		}

		public void Save()
		{
			StreamWriter sw = new StreamWriter(fileName);
			sw.Write(xml);
			sw.Close();
		}

		private void WriteNewValue(string key, string val)
		{
			Regex rx = new Regex("</dict>");
			xml = rx.Replace(xml, "\t<key>" + key + "</key>\n" + "\t<string>" + val + "</string>\n" + "</dict>");
		}

		public void Clear()
		{
			xml =	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
					"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" +
					"<plist version=\"1.0\">\n" +
					"<dict>\n" +
					"</dict>\n" +
					"</plist>\n";
		}
	}
}
