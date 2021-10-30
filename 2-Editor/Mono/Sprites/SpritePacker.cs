#if ENABLE_SPRITES
using System;
using System.Linq;
using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor.Sprites
{
	public sealed partial class Packer
	{
		public enum Execution
		{
			Normal = 0,
			ForceRegroup
		}

		public static string kDefaultPolicy = typeof(DefaultPackerPolicy).Name;

		private static string[] m_policies = null;
		public static string[] Policies
		{
			get
			{
				RegenerateList();
				return m_policies;
			}
		}

		private static string m_selectedPolicy = null;
		private static void SetSelectedPolicy(string value)
		{
			m_selectedPolicy = value;
			PlayerSettings.spritePackerPolicy = m_selectedPolicy;
		}
		public static string SelectedPolicy
		{
			get
			{
				RegenerateList();
				return m_selectedPolicy;
			}
			set
			{
				RegenerateList();
				if (value == null)
					throw new ArgumentNullException();
				if (!m_policies.Contains(value))
					throw new ArgumentException("Specified policy {0} is not in the policy list.", value);
				SetSelectedPolicy(value);
			}
		}

		private static Dictionary<string, Type> m_policyTypeCache = null;
		private static void RegenerateList()
		{
			if (m_policies != null)
				return;

			IEnumerable<Type> types =
				from a in AppDomain.CurrentDomain.GetAssemblies()
				from t in a.GetTypes()
				where typeof(IPackerPolicy).IsAssignableFrom(t) && (t != typeof(IPackerPolicy))
				select t;

			m_policies = types.Select(t => t.Name).ToArray();
			m_policyTypeCache = types.Select(t => new KeyValuePair<string, Type>(t.Name, t)).ToDictionary(t => t.Key, t => t.Value);
			m_selectedPolicy = String.IsNullOrEmpty(PlayerSettings.spritePackerPolicy) ? kDefaultPolicy : PlayerSettings.spritePackerPolicy;

			// Did policies change?
			if (!m_policies.Contains(m_selectedPolicy))
				SetSelectedPolicy(kDefaultPolicy);
		}

		internal static string GetSelectedPolicyId()
		{
			RegenerateList();

			Type t = m_policyTypeCache[m_selectedPolicy];
			IPackerPolicy policy = Activator.CreateInstance(t) as IPackerPolicy;
			string versionString = string.Format("{0}::{1}", t.AssemblyQualifiedName, policy.GetVersion());

			return versionString;
		}

		internal static void ExecuteSelectedPolicy(BuildTarget target, TextureImporter[] textureImporters)
		{
			RegenerateList();

			Type t = m_policyTypeCache[m_selectedPolicy];
			IPackerPolicy policy = Activator.CreateInstance(t) as IPackerPolicy;
			policy.OnGroupAtlases(target, new PackerJob(), textureImporters);
		}

		internal static void SaveUnappliedTextureImporterSettings()
		{
			foreach (InspectorWindow i in InspectorWindow.GetAllInspectorWindows())
			{
				ActiveEditorTracker activeEditor = i.GetTracker();
				foreach (Editor e in activeEditor.activeEditors)
				{
					TextureImporterInspector inspector = e as TextureImporterInspector;
					if (inspector == null)
						continue;
					if (!inspector.HasModified())
						continue;
					TextureImporter importer = inspector.target as TextureImporter;
					if (EditorUtility.DisplayDialog("Unapplied import settings", "Unapplied import settings for \'" + importer.assetPath + "\'", "Apply", "Revert"))
					{
						inspector.ApplyAndImport(); // No way to apply/revert only some assets. Bug: 564192.
					}
				}
			}
		}
	}

	public interface IPackerPolicy
	{
		void OnGroupAtlases(BuildTarget target, PackerJob job, TextureImporter[] textureImporters);
		int GetVersion();
	}
}

#endif
