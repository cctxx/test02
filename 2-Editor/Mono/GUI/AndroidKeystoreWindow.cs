using UnityEngine;
using System.Collections;
using UnityEditor;
using UnityEditorInternal;
using System.IO;

namespace UnityEditor.Android {

internal class AndroidKeystoreWindow : EditorWindow {

	public static void ShowAndroidKeystoreWindow (string company, string keystore, string storepass)
	{
		// Verify keystore password
		if (File.Exists(keystore))
		{
			try
			{
				AndroidSDKTools.GetInstanceOrThrowException().
					ReadAvailableKeys(keystore, storepass);
			}
			catch (System.Exception ex)
			{
				Debug.LogError(ex.ToString());
				return;
			}
		}

		Rect r = new Rect(100, 100, 500, 330);
		AndroidKeystoreWindow w = (AndroidKeystoreWindow)EditorWindow.GetWindowWithRect(typeof(AndroidKeystoreWindow), r, true,
												EditorGUIUtility.TextContent("AndroidKeystore.CreateNewKey").text);
		w.position = r;
		w.m_Parent.window.m_DontSaveToLayout = true;
		w.m_Organization = company;
		w.m_Keystore = keystore;
		w.m_StorePass = storepass;
	}
	private string m_Keystore;
	private string m_StorePass;

	public void OnGUI()
	{
		GUILayout.Label(EditorGUIUtility.TextContent("AndroidKeystore.KeyCreation"), EditorStyles.boldLabel);

		bool enableButton = false;

		string[] keys = null;
		if (File.Exists(m_Keystore))
			keys = GetAvailableKeys(m_Keystore, m_StorePass);

		float labelWidthOld = EditorGUIUtility.labelWidth;
		EditorGUIUtility.labelWidth = 150;

		EditorGUILayout.Space();

		{
			TextInput("AndroidKeystore.Alias", ref m_Alias);
			PasswordInput("AndroidKeystore.Password", ref m_Password);
			PasswordInput("AndroidKeystore.PassConfirm", ref m_PassConfirm);
			IntInput("AndroidKeystore.Validity", ref m_Validity);
		}

		m_Alias = m_Alias.ToLower();

		EditorGUILayout.Space();

		{
			TextInput("AndroidKeystore.Name", ref m_Name);
			TextInput("AndroidKeystore.OrgUnit", ref m_OrgUnit);
			TextInput("AndroidKeystore.Organization", ref m_Organization);
			TextInput("AndroidKeystore.City", ref m_City);
			TextInput("AndroidKeystore.State", ref m_State);
			TextInput("AndroidKeystore.Country", ref m_Country);
		}

		GUILayout.FlexibleSpace();
		GUILayout.BeginHorizontal();
		{
			GUILayout.FlexibleSpace();

			{
				bool noStringsPresent = m_Alias == null;
				bool keyExists = false;
				if (keys != null)
				{
					foreach (string str in keys)
						if (str.Equals(m_Alias))
							keyExists = true;
				}
				bool noCertData = !noStringsPresent &&
									m_Name.Length == 0 && m_OrgUnit.Length == 0 &&
									m_Organization.Length == 0 && m_City.Length == 0 &&
									m_State.Length == 0 && m_Country.Length == 0;

				GUIContent gc = null;
				if (noStringsPresent || m_Alias.Length == 0)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.EnterKeyalias");
				else if (keyExists)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.KeyliasExists");
				else if (m_Password.Length == 0)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.EnterPassword");
				else if (m_Password.Length < 6)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.PasswordTooShort");
				else if (!m_Password.Equals(m_PassConfirm))
					gc = EditorGUIUtility.TextContent("AndroidKeystore.PasswordsDontMatch");
				else if (m_Validity == 0)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.EnterValidityTime");
				else if (m_Validity > 1000)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.MaxValidityTime");
				else if (noCertData)
					gc = EditorGUIUtility.TextContent("AndroidKeystore.NoCertData");
				else
				{
					enableButton = true;
					if (m_Validity < 25)
						gc = EditorGUIUtility.TextContent("AndroidKeystore.RecommendedValidityTime");
					else
						gc = EditorGUIUtility.TempContent(" ");
				}

				GUILayout.Label(gc, EditorStyles.miniLabel);
			}

			{
				GUILayout.Space(5);

				const float kButtonWidth = 140;
				GUI.enabled = enableButton;
				if (GUILayout.Button(EditorGUIUtility.TextContent("AndroidKeystore.CreateKey"), GUILayout.Width(kButtonWidth)))
				{
					if (CreateKey(m_Keystore, m_StorePass))
					{
						Close();
					}
				}
				GUI.enabled = true;
			}

			GUILayout.EndHorizontal();
		}

		EditorGUILayout.Space();

		EditorGUIUtility.labelWidth = labelWidthOld;
	}
	string m_Alias;
	string m_Password;
	string m_PassConfirm;
	int m_Validity = 50;			// "A validity period of 25 years or more is recommended."
	string m_Name;
	string m_OrgUnit;
	string m_Organization;
	string m_City;
	string m_State;
	string m_Country;

	private void TextInput(string label, ref string value)
	{
		if (value == null)
			value = new string(' ', 0);
		string newValue = EditorGUI.TextField(GetRect(), EditorGUIUtility.TextContent(label), value);
		if (GUI.changed)
			value = newValue;
	}
	private void PasswordInput(string label, ref string value)
	{
		if (value == null)
			value = new string(' ', 0);
		string newValue = EditorGUI.PasswordField(GetRect(), EditorGUIUtility.TextContent(label), value);
		if (GUI.changed)
			value = newValue;
	}
	private void IntInput(string label, ref int value)
	{
		int newValue = EditorGUI.IntField(GetRect(), EditorGUIUtility.TextContent(label), value);
		if (GUI.changed)
			value = newValue;
	}

	private Rect GetRect()
	{
		float h = EditorGUI.kSingleLineHeight;
		float kLabelFloatMinW = EditorGUI.kLabelW + EditorGUIUtility.fieldWidth * 0.5f + EditorGUI.kSpacing;
		float kLabelFloatMaxW = EditorGUI.kLabelW + EditorGUIUtility.fieldWidth * 0.5f + EditorGUI.kSpacing;
		Rect r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h + EditorGUI.kSpacing, h + EditorGUI.kSpacing, EditorStyles.layerMaskField, null);
		return r;
	}

	private bool CreateKey(string keystore, string storepass)
	{
		string[] dnameArray = new string[0];
		if (m_Name.Length != 0)
			ArrayUtility.Add(ref dnameArray, "CN=" + escapeDName(m_Name));
		if (m_OrgUnit.Length != 0)
			ArrayUtility.Add(ref dnameArray, "OU=" + escapeDName(m_OrgUnit));
		if (m_Organization.Length != 0)
			ArrayUtility.Add(ref dnameArray, "O=" + escapeDName(m_Organization));
		if (m_City.Length != 0)
			ArrayUtility.Add(ref dnameArray, "L=" + escapeDName(m_City));
		if (m_State.Length != 0)
			ArrayUtility.Add(ref dnameArray, "ST=" + escapeDName(m_State));
		if (m_Country.Length != 0)
			ArrayUtility.Add(ref dnameArray, "C=" + escapeDName(m_Country));
		string dname = string.Join(", ", dnameArray);
		try
		{
			AndroidSDKTools.GetInstanceOrThrowException().
				CreateKey(keystore, storepass, m_Alias, m_Password, dname, m_Validity * 365);
			s_AvailableKeyalias = null;
		}
		catch (System.Exception ex)
		{
			Debug.LogError(ex.ToString());
			return false;
		}
		return true;
	}
	private static string escapeDName(string value)
	{
		char[] c = { '"', ',' };
		for (int i = value.IndexOfAny(c, 0); i >= 0; i = value.IndexOfAny(c, i))
		{
			value = value.Insert(i, "\\"); i += 2;
		}
		return value;
	}

	static private string[] s_AvailableKeyalias = null;
	static private string s_CurrentKeystore;

	public static string[] GetAvailableKeys(string keystore, string storepass)
	{
		// validate params
		if (keystore.Length == 0 || storepass.Length == 0)
			return (s_AvailableKeyalias = null);

		if (s_AvailableKeyalias != null && keystore.Equals(s_CurrentKeystore))
			return s_AvailableKeyalias;

		s_CurrentKeystore = keystore;

		try
		{
			AndroidSDKTools tools = AndroidSDKTools.GetInstance();
			if (tools == null)
				throw new UnityException("Unable to find Android SDK!");

			return (s_AvailableKeyalias = AndroidSDKTools.GetInstanceOrThrowException().
				ReadAvailableKeys(keystore, storepass));
		}
		catch (System.Exception ex)
		{
			Debug.LogError(ex.ToString());
			return null;
		}
	}
}

} // namespace UnityEditor.Android
