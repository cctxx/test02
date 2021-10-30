using UnityEngine;
using UnityEditor;
using UnityEditor.Callbacks;
using System;
using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace UnityEditor
{
class PostprocessScene {
	internal class UnityBuildPostprocessor
	{
        [PostProcessScene(0)]
        public static void OnPostprocessScene()
		{
			int staticBatching, dynamicBatching;
			PlayerSettings.GetBatchingForPlatform (EditorUserBuildSettings.activeBuildTarget, out staticBatching, out dynamicBatching);

			if (staticBatching != 0 && PlayerSettings.advancedLicense)
				InternalStaticBatchingUtility.Combine(null, true);
		}
	}
}
}
