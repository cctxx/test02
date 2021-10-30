using UnityEditor;

namespace UnityEditorInternal
{
	/// <summary>
	/// Helper factory for <see cref="UnityEditor.MonoScript"/> instances.
	/// </summary>
	public static class MonoScripts
	{
		public static MonoScript CreateMonoScript(string scriptContents, string className, string nameSpace, string assemblyName, bool isEditorScript)
		{
			var script = new MonoScript();
			script.Init(scriptContents, className, nameSpace, assemblyName, isEditorScript);
			return script;
		}
	}
}