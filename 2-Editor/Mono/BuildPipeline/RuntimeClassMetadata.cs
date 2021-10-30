using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEditorInternal;
using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text.RegularExpressions;
using Mono.Cecil;

namespace UnityEditor
{

	/// <summary>
	///  Registry for Unity native-managed class dependencies. Aimed to make native and managed code stripping possible.
	///  Note: only UnityEngine.dll content is covered there.
	/// </summary>
	internal class RuntimeClassRegistry
	{
		protected Dictionary<int, string> nativeClasses = new Dictionary<int, string>();
		protected HashSet<string> monoClasses = new HashSet<string>();

		public RuntimeClassRegistry()
		{
			InitRuntimeClassRegistry();
		}

		public void AddNativeClassID(int ID)
		{
			string className = BaseObjectTools.ClassIDToString(ID);
			////System.Console.WriteLine("Looking for ID {0} name {1} --> is manager? {2}", ID, className, functionalityGroups.ContainsValue(className));

			// Native class found and it is not one of the tracked managers
			if (className.Length > 0 && !functionalityGroups.ContainsValue(className))
				nativeClasses[ID] = className;
		}

		public void AddMonoClass(string className)
		{
			monoClasses.Add(className);
		}

		public void AddMonoClasses(List<string> classes)
		{
			foreach (string clazz in classes)
				AddMonoClass(clazz);
		}

		public void AddNativeClassFromName(string className)
		{
			int classID = BaseObjectTools.StringToClassID(className);
			////System.Console.WriteLine("Looking for name {1}  ID {0}", classID, className);

			if (-1 != classID && ! BaseObjectTools.IsBaseObject(classID))
				nativeClasses[classID] = className;
		}

		public void SynchronizeMonoToNativeClasses()
		{
			foreach (string monoClass in monoClasses)
				AddNativeClassFromName(monoClass);
		}

		public void SynchronizeNativeToMonoClasses()
		{
			foreach (string nativeClass in nativeClasses.Values)
				AddMonoClass(nativeClass);
		}

		public void SynchronizeClasses()
		{
			SynchronizeMonoToNativeClasses();
			SynchronizeNativeToMonoClasses();
			InjectFunctionalityGroupDependencies();
			SynchronizeMonoToNativeClasses();
		}

		public void InjectFunctionalityGroupDependencies()
		{
			HashSet<string> functionalityGroupsTouched = new HashSet<string>();


			foreach (string group in functionalityGroups.Keys)
			{
				foreach (string monoClass in monoClasses)
					if (groupManagedDependencies[group].Contains(monoClass) ||
					    groupNativeDependencies[group].Contains(monoClass))
						functionalityGroupsTouched.Add(group);
			}


			foreach (string group in functionalityGroupsTouched)
			{
				////Console.WriteLine("Group touched: {0}", group);
				foreach (string depClass in groupManagedDependencies[group])
					AddMonoClass(depClass);

				foreach (string depClass in groupNativeDependencies[group])
					AddNativeClassFromName(depClass);
			}
		}

		public List<string> GetAllNativeClassesAsString()
		{
			return new List<string>(nativeClasses.Values);
		}

		public List<string> GetAllManagedClassesAsString()
		{
			//foreach (string klass in monoClasses)
			//	Console.WriteLine("---> Managed class {0}", klass);

			return new List<string>(monoClasses);
		}

		public static RuntimeClassRegistry Produce(int[] nativeClassIDs)
		{
			RuntimeClassRegistry res = new RuntimeClassRegistry();

			foreach (int ID in nativeClassIDs)
			{
				res.AddNativeClassID(ID);
			}

			return res;
		}


		public void AddFunctionalityGroup(string groupName, string managerClassName)
		{
			functionalityGroups.Add(groupName, managerClassName);
			groupManagedDependencies[groupName] = new HashSet<string>();
			groupNativeDependencies[groupName] = new HashSet<string>();
		}

		public void AddNativeDependenciesForFunctionalityGroup(string groupName, string depClassName)
		{
			groupNativeDependencies[groupName].Add(depClassName);
		}

		public void AddManagedDependenciesForFunctionalityGroup(string groupName, Type depClass)
		{
			AddManagedDependenciesForFunctionalityGroup(groupName, ResolveTypeName(depClass));
		}

		public void AddManagedDependenciesForFunctionalityGroup(string groupName, string depClassName)
		{
			AddManagedDependenciesForFunctionalityGroup(groupName, depClassName, null);
		}

		private string ResolveTypeName(Type type)
		{
			string res = type.FullName;

			return res.Substring(res.LastIndexOf(".") + 1).Replace("+", "/");
		}

		public void AddManagedDependenciesForFunctionalityGroup(string groupName, Type depClass, string retain)
		{
			AddManagedDependenciesForFunctionalityGroup(groupName, ResolveTypeName(depClass), retain);
		}

		public void AddManagedDependenciesForFunctionalityGroup(string groupName, string depClassName, string retain)
		{
			groupManagedDependencies[groupName].Add(depClassName);

			if (retain != null)
				SetRetentionLevel(depClassName, retain);
		}

		public void SetRetentionLevel(string className, string level)
		{
			retentionLevel[className] = level;
		}

		public string GetRetentionLevel(string className)
		{
			if (retentionLevel.ContainsKey(className))
				return retentionLevel[className];

			return "fields";
		}

		protected void InitRuntimeClassRegistry()
		{
// We need to reference obsolete classes to catch the moment when they are removed from code base
#pragma warning disable 618
			// Runtime dependencies
			AddFunctionalityGroup ("Runtime", "[no manager]");

			AddNativeDependenciesForFunctionalityGroup ("Runtime", "GameObject");
			AddNativeDependenciesForFunctionalityGroup ("Runtime", "Material");
			AddNativeDependenciesForFunctionalityGroup ("Runtime", "PreloadData");
			// A default cubemap is created on startup, so the code won't be stripped out anyway
			AddNativeDependenciesForFunctionalityGroup ("Runtime", "Cubemap");
			// LODGroup won't be stripped out anyway
			AddNativeDependenciesForFunctionalityGroup ("Runtime", "LODGroup");

			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GameObject), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Transform), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Mesh), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SkinnedMeshRenderer), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(MeshRenderer), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(UnityException), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Resolution));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(LayerMask));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SerializeField));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(WaitForSeconds));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(WaitForFixedUpdate));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(WaitForEndOfFrame));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AssetBundleRequest));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "TouchScreenKeyboard");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "iPhoneKeyboard");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Event), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(HideInInspector));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SerializePrivateVariables));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SerializeField));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Font), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUIStyle));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUISkin), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUI), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SendMouseEvents), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SetupCoroutine), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Coroutine));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AttributeHelperEngine), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(StackTraceUtility), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUIUtility), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUI), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Application), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Animation), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AnimationClip), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AnimationEvent));            
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AsyncOperation));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(CacheIndex));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Keyframe));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(RenderTexture));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AnimationCurve), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(BoneWeight));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Particle));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SliderState), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUI.ScrollViewState), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GUIScrollGroup), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(TextEditor), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(ClassLibraryInitializer), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AssetBundleCreateRequest), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "ImageEffectTransformsToLDR");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "ImageEffectOpaque");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(Gradient), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GradientColorKey));
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(GradientAlphaKey));

			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AnimatorStateInfo), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AnimatorTransitionInfo), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AnimationInfo), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(SkeletonBone), "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(HumanBone), "all");

			AddManagedDependenciesForFunctionalityGroup ("Runtime", typeof(AudioClip), "all");

			AddManagedDependenciesForFunctionalityGroup ("Runtime", "AndroidJNI", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "AndroidJNIHelper", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "_AndroidJNIHelper", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "AndroidJavaObject", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "AndroidJavaClass", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "AndroidJavaRunnableProxy", "all");

			// Social API / GameCenter, include this always for now (this is actually only forcing inclusion of callbacks, since the rest isn't stripped)
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "SocialPlatforms.GameCenter.GameCenterPlatform", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "SocialPlatforms.GameCenter.GcLeaderboard", "all");

			AddManagedDependenciesForFunctionalityGroup ("Runtime", "UnhandledExceptionHandler", "all");
			AddManagedDependenciesForFunctionalityGroup ("Runtime", "Display", "all");

			// Networking dependencies
			AddFunctionalityGroup ("Networking", "NetworkManager");

			AddNativeDependenciesForFunctionalityGroup ("Networking", "NetworkManager");
			AddNativeDependenciesForFunctionalityGroup ("Networking", "NetworkView");

			//AddManagedDependenciesForFunctionalityGroup ("Networking", "NetworkView");
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(Network));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(NetworkMessageInfo));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(RPC));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(HostData));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(BitStream));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(NetworkPlayer));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(NetworkViewID));
			AddManagedDependenciesForFunctionalityGroup ("Networking", typeof(Ping), "all");

			// Physics dependencies
			AddFunctionalityGroup ("Physics", "PhysicsManager");

			AddNativeDependenciesForFunctionalityGroup ("Physics", "PhysicsManager");
			AddNativeDependenciesForFunctionalityGroup ("Physics", "Rigidbody");
			AddNativeDependenciesForFunctionalityGroup ("Physics", "Collider");

			AddManagedDependenciesForFunctionalityGroup ("Physics", typeof(ControllerColliderHit));
			AddManagedDependenciesForFunctionalityGroup ("Physics", typeof(RaycastHit));
			AddManagedDependenciesForFunctionalityGroup ("Physics", typeof(Collision));
			AddManagedDependenciesForFunctionalityGroup ("Physics", typeof(MeshCollider));

			// Physics 2D dependencies
			AddFunctionalityGroup ("Physics2D", "Physics2DSettings");

			AddNativeDependenciesForFunctionalityGroup ("Physics2D", "Physics2DSettings");
			AddNativeDependenciesForFunctionalityGroup ("Physics2D", "Rigidbody2D");
			AddNativeDependenciesForFunctionalityGroup ("Physics2D", "Collider2D");
			AddNativeDependenciesForFunctionalityGroup ("Physics2D", "Joint2D");
			AddNativeDependenciesForFunctionalityGroup ("Physics2D", "PhysicsMaterial2D");

			AddManagedDependenciesForFunctionalityGroup ("Physics2D", typeof (RaycastHit2D));
			AddManagedDependenciesForFunctionalityGroup ("Physics2D", typeof (Collision2D));
			AddManagedDependenciesForFunctionalityGroup ("Physics2D", typeof (JointMotor2D));
			AddManagedDependenciesForFunctionalityGroup ("Physics2D", typeof (JointAngleLimits2D));
			AddManagedDependenciesForFunctionalityGroup ("Physics2D", typeof (JointTranslationLimits2D));

			// Terrain
			// TODO: all done almost blindly - most likely we can do it more effectively and less verbose ;-)

			AddFunctionalityGroup ("Terrain", "Terrain");

			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(Terrain), "all");
			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(TerrainData), "all");
			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(TerrainCollider), "all");
			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(DetailPrototype), "all");
			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(TreePrototype), "all");
			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(TreeInstance), "all");
			AddManagedDependenciesForFunctionalityGroup("Terrain", typeof(SplatPrototype), "all");

			AddFunctionalityGroup ("Shuriken", "ParticleSystem");
			AddManagedDependenciesForFunctionalityGroup ("Shuriken", typeof(ParticleSystem));
			AddManagedDependenciesForFunctionalityGroup ("Shuriken", typeof(ParticleSystemRenderer));


#pragma warning restore 618
		}

		protected Dictionary<string, string> retentionLevel = new Dictionary<string, string>();

		/// <summary>
		/// Key - functionality group name, Value - functionality manager class
		/// </summary>
		protected Dictionary<string, string> functionalityGroups = new Dictionary<string, string>();

		/// <summary>
		/// Key - functionality group name, Value - set of functionality dependent native class names
		/// </summary>
		protected Dictionary<string, HashSet<string>> groupNativeDependencies = new Dictionary<string, HashSet<string>>();

		/// <summary>
		/// Key - functionality group name, Value - set of functionality dependent managed class names
		/// </summary>
		protected Dictionary<string, HashSet<string>> groupManagedDependencies = new Dictionary<string, HashSet<string>>();
	}
}
