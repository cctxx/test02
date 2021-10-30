using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;
using System.Collections;
using System.IO;


namespace UnityEditor
{

class ValueSmoother
{
	public ValueSmoother ()
	{
		m_TargetValue = 0.0f;
		m_CurrentValue = 0.0f;
		m_LastUpdateTime = Time.realtimeSinceStartup;
	}

	// Get animated progress
	public float GetSmoothValue ()
	{
		return m_CurrentValue;
	}

	// Can be called every frame
	public void SetTargetValue (float value)
	{
		m_TargetValue = value;
		if (value == 0.0f)
			m_CurrentValue = 0.0f;
	}

	// Call as often as possible
	public void Update ()
	{
		if (m_CurrentValue < 1.0f)
		{
			float deltaTime = Mathf.Clamp (Time.realtimeSinceStartup - m_LastUpdateTime, 0.0f, 0.1f);
			float weight = 1.0f; // we could tweek this runtime for nicer smoothing
			float frac = weight * deltaTime;
			m_CurrentValue = (m_CurrentValue * (1.0f-frac)) + (m_TargetValue * frac);

			if (m_CurrentValue > 0.995f)
			{
				m_CurrentValue = 1.0f;
			}
		}

		m_LastUpdateTime = Time.realtimeSinceStartup;
	}

	private float m_TargetValue;
	private float m_CurrentValue;
	private float m_LastUpdateTime;
}


internal class UploadingBuildsMonitor : ScriptableObject
{
	class Content
	{
		public GUIContent m_ProgressBarText = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.ProgressBarText");
		public GUIContent m_NoSessionDialogHeader = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.NoSessionDialogHeader");
		public GUIContent m_NoSessionDialogText = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.NoSessionDialogText");
		public GUIContent m_NoSessionDialogButtonOK = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.NoSessionDialogButtonOK");
		public GUIContent m_OverwriteDialogHeader = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.OverwriteDialogHeader");
		public GUIContent m_OverwriteDialogText = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.OverwriteDialogText");
		public GUIContent m_OverwriteDialogButtonOK = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.OverwriteDialogButtonOK");
		public GUIContent m_OverwriteDialogButtonCancel = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.OverwriteDialogButtonCancel");
		public GUIContent m_OverwriteDialogButtonVersion = EditorGUIUtility.TextContent ("UploadingBuildsMonitor.OverwriteDialogButtonVersion");
	}
	
	
	static Content s_Content = null;
	
	
	static UploadingBuildsMonitor s_UploadingBuildsMonitor;
	UploadingBuild[] m_UploadingBuilds = new UploadingBuild[0];
	Dictionary<string, ValueSmoother> m_ProgressSmoothers = new Dictionary<string,ValueSmoother> ();
	bool m_DidInit = false;
	
	
	UploadingBuildsMonitor ()
	{
		if (s_Content == null)
		{
			s_Content = new Content ();
		}
	}


	public static void Activate ()
	{
		if (s_UploadingBuildsMonitor == null)
		{
			CreateInstance (typeof (UploadingBuildsMonitor));
		}
	}
	
	
	public static void Deactivate ()
	{
		DestroyImmediate (s_UploadingBuildsMonitor);
	}
	
	
	public static string GetActiveSessionID ()
	{
		return string.IsNullOrEmpty (AssetStoreClient.ActiveSessionID) ? "" : AssetStoreClient.ActiveSessionID + InternalEditorUtility.GetAuthToken();
	}
		
	public static void HandleNoSession (string displayName)
	{
		Activate ();
		
		AssetStoreLoginWindow.Login (
			s_Content.m_NoSessionDialogText.text,
			(string errorMessage) => {
				if (string.IsNullOrEmpty (errorMessage))
				{
					UploadingBuildsUtility.ResumeBuildUpload (displayName);
				}
				else
				{
					UploadingBuildsUtility.RemoveUploadingBuild (displayName);
				}
			}
		);
	}
	
	
	public static void OverwritePrompt (string displayName)
	{
		int choice = EditorUtility.DisplayDialogComplex (
			s_Content.m_OverwriteDialogHeader.text,
			s_Content.m_OverwriteDialogText.text,
			s_Content.m_OverwriteDialogButtonOK.text,
			s_Content.m_OverwriteDialogButtonCancel.text,
			s_Content.m_OverwriteDialogButtonVersion.text
		);
		
		if (choice == 1)
		// "Cancel"
		{
			UploadingBuildsUtility.RemoveUploadingBuild (displayName);
			AsyncProgressBar.Clear ();
			
			s_UploadingBuildsMonitor.SyncToState ();
		}
		else
		// 0: "OK" / 2: "Version"
		{
			UploadingBuildsUtility.ResumeBuildUpload (displayName, choice == 0);
		}
	}


	void OnEnable()
	{
		s_UploadingBuildsMonitor = this;
		EditorApplication.update += Update;
		SyncToState ();
	}


	void OnDisable()
	{
		s_UploadingBuildsMonitor = null;
		EditorApplication.update -= Update;
	}


	public static void InternalStateChanged ()
	{
		if (s_UploadingBuildsMonitor != null)
		{
			s_UploadingBuildsMonitor.SyncToState ();
		}
	}


	void SyncToState ()
	{
		m_UploadingBuilds = UploadingBuildsUtility.GetUploadingBuilds ();
	}


	ValueSmoother GetProgressSmoother (string url)
	{
		ValueSmoother vs;
		m_ProgressSmoothers.TryGetValue (url, out vs);
		if (vs == null)
		{
			vs = new ValueSmoother ();
			m_ProgressSmoothers[url] = vs;
		}
		return vs;
	}


	void UploadSmoothing ()
	{
		if (m_ProgressSmoothers.Count > 0)
		{
			bool needReSync = false;
			List<string> keysToDelete = null;
			foreach (var vs in m_ProgressSmoothers)
			{
				vs.Value.Update();
				if (vs.Value.GetSmoothValue() < 1.0f)
				{
					needReSync = true;
				}
				else
				{
					if (keysToDelete == null)
					{
						keysToDelete = new List<string> ();
					}
					keysToDelete.Add (vs.Key);
				}
			}

			if (keysToDelete != null)
			// Cleanup of unused smoothers
			{
				foreach (string key in keysToDelete)
				{
					m_ProgressSmoothers.Remove (key);
				}	
			}	

			if (needReSync)
			{
				SyncToState ();
			}
		}

	}


	void Update ()
	{
		if (!m_DidInit)
		{
			SyncToState ();
			m_DidInit = true;
		}

		UploadSmoothing ();
		
		if (m_UploadingBuilds.Length > 0)
		// Just handling the first build in the queue at the moment
		{
			UpdateBuild (ref m_UploadingBuilds[0]);
		}
		else
		// No need for a monitor with no subject
		{
			Deactivate ();
		}
	}
	
	
	void UpdateBuild (ref UploadingBuild build)
	{
		UploadingBuildStatus buildsStatus = build.status;

		switch (buildsStatus)
		{
			case UploadingBuildStatus.Uploaded:
			case UploadingBuildStatus.UploadFailed:
				UploadingBuildsUtility.RemoveUploadingBuild (build.displayName);
				AsyncProgressBar.Clear ();
				
				SyncToState ();
			break;
			case UploadingBuildStatus.Uploading:
				ValueSmoother vs = GetProgressSmoother (build.url);
				vs.SetTargetValue (build.progress);
				
				AsyncProgressBar.Display (s_Content.m_ProgressBarText.text, vs.GetSmoothValue ());
			break;
			case UploadingBuildStatus.Authorizing:
			case UploadingBuildStatus.Authorized:
				AsyncProgressBar.Display (s_Content.m_ProgressBarText.text, 0.0f);
			break;
			default:
				Debug.LogError ("Unhandled enum");
			break;
		}
	}
}
}
