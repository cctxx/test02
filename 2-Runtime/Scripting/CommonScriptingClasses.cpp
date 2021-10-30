#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "CommonScriptingClasses.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

static ScriptingMethodPtr OptionalMethod(const char* namespaze, const char* klass, const char* name)
{
	return GetScriptingManager().GetScriptingMethodRegistry().GetMethod(namespaze,klass,name);
}

static ScriptingMethodPtr RequireMethod(const char* namespaze, const char* klass, const char* name)
{
	ScriptingMethodPtr method = GetScriptingManager().GetScriptingMethodRegistry().GetMethod(namespaze,klass,name);
	if (!method)
		ErrorString(Format("Unable to find method %s in %s",name,klass));
	return method;
}


static ScriptingTypePtr OptionalType(const char* namespaze, const char* name)
{
	return GetScriptingTypeRegistry().GetType(namespaze,name);
}

static ScriptingTypePtr RequireType(const char* namespaze, const char* name)
{
	ScriptingTypePtr t = OptionalType(namespaze,name);
	if (!t)
		ErrorString(Format("Unable to find type %s.%s",namespaze,name));
	return t;
}

static ScriptingTypePtr RequireUnityEngineType(const char* name)
{
	return RequireType("UnityEngine",name);
}

static ScriptingTypePtr OptionalUnityEngineType(const char* name)
{
	return OptionalType("UnityEngine",name);
}

void FillCommonScriptingClasses(CommonScriptingClasses& commonScriptingClasses)
{
	ScriptingTypeRegistry& typeRegistry = GetScriptingManager().GetScriptingTypeRegistry();

#if ENABLE_SCRIPTING
	commonScriptingClasses.monoBehaviour = RequireUnityEngineType ("MonoBehaviour");
	commonScriptingClasses.component = RequireUnityEngineType ("Component");
	commonScriptingClasses.scriptableObject = RequireUnityEngineType ("ScriptableObject");
	commonScriptingClasses.vector2 = RequireUnityEngineType ("Vector2");
	commonScriptingClasses.vector3 = RequireUnityEngineType ("Vector3");
	commonScriptingClasses.vector4 = RequireUnityEngineType ("Vector4");
	commonScriptingClasses.rect = RequireUnityEngineType ("Rect");
	commonScriptingClasses.rectOffset = RequireUnityEngineType ("RectOffset");
	commonScriptingClasses.quaternion = RequireUnityEngineType ("Quaternion");
	commonScriptingClasses.matrix4x4 = RequireUnityEngineType ("Matrix4x4");
	commonScriptingClasses.bounds = RequireUnityEngineType ("Bounds");
	commonScriptingClasses.resolution = RequireUnityEngineType ("Resolution");
	commonScriptingClasses.particle = RequireUnityEngineType ("Particle");
	commonScriptingClasses.color = RequireUnityEngineType ("Color");
	commonScriptingClasses.color32 = RequireUnityEngineType ("Color32");
	commonScriptingClasses.raycastHit = OptionalUnityEngineType ("RaycastHit");
	commonScriptingClasses.raycastHit2D = OptionalUnityEngineType ("RaycastHit2D");
	commonScriptingClasses.animationState = OptionalUnityEngineType ("AnimationState");
	commonScriptingClasses.collider = OptionalUnityEngineType ("Collider");
	commonScriptingClasses.camera = RequireUnityEngineType ("Camera");
	commonScriptingClasses.renderTexture = RequireUnityEngineType ("RenderTexture");
	commonScriptingClasses.layerMask = RequireUnityEngineType ("LayerMask");
	commonScriptingClasses.waitForSeconds = RequireUnityEngineType ("WaitForSeconds");
	commonScriptingClasses.waitForFixedUpdate = RequireUnityEngineType ("WaitForFixedUpdate");
	commonScriptingClasses.waitForEndOfFrame = RequireUnityEngineType ("WaitForEndOfFrame");
	commonScriptingClasses.characterInfo = RequireUnityEngineType ("CharacterInfo");
	commonScriptingClasses.font_InvokeFontTextureRebuildCallback_Internal = RequireMethod ("UnityEngine","Font","InvokeFontTextureRebuildCallback_Internal");
#if ENABLE_WWW
	commonScriptingClasses.www = OptionalUnityEngineType ("WWW");
#endif
#if UNITY_WII
	commonScriptingClasses.waitAsyncOperationFinish = GetBuiltinScriptingClass ("WaitAsyncOperationFinish");
#endif
	commonScriptingClasses.lodMesh = OptionalUnityEngineType ("Mesh");
	commonScriptingClasses.coroutine = RequireUnityEngineType ("Coroutine");
	commonScriptingClasses.collision = OptionalUnityEngineType ("Collision");
	commonScriptingClasses.contactPoint = OptionalUnityEngineType ("ContactPoint");
	commonScriptingClasses.controllerColliderHit = OptionalUnityEngineType ("ControllerColliderHit");
	commonScriptingClasses.collision2D = OptionalUnityEngineType ("Collision2D");
	commonScriptingClasses.contactPoint2D = OptionalUnityEngineType ("ContactPoint2D");
	commonScriptingClasses.unityEngineObject = RequireUnityEngineType ("Object");
	commonScriptingClasses.event = RequireUnityEngineType ("Event");
#if ENABLE_MONO
	commonScriptingClasses.serializeField = RequireUnityEngineType ("SerializeField");
	commonScriptingClasses.serializePrivateVariables = GetBuiltinScriptingClass ("SerializePrivateVariables");
	commonScriptingClasses.hideInInspector = GetBuiltinScriptingClass ("HideInInspector");
#endif
#if ENABLE_NETWORK
	commonScriptingClasses.RPC = OptionalUnityEngineType ("RPC");
	commonScriptingClasses.hostData = OptionalUnityEngineType ("HostData");
	commonScriptingClasses.bitStream = OptionalUnityEngineType ("BitStream");
	commonScriptingClasses.networkPlayer = OptionalUnityEngineType ("NetworkPlayer");
	commonScriptingClasses.networkViewID = OptionalUnityEngineType ("NetworkViewID");
	commonScriptingClasses.networkMessageInfo = OptionalUnityEngineType ("NetworkMessageInfo");
#endif // ENABLE_NETWORK
	commonScriptingClasses.guiStyle = RequireUnityEngineType ("GUIStyle");
	commonScriptingClasses.animationCurve = RequireUnityEngineType ("AnimationCurve");
	commonScriptingClasses.boneWeight = RequireUnityEngineType ("BoneWeight");
#if ENABLE_TERRAIN
	commonScriptingClasses.terrain = OptionalUnityEngineType("Terrain");
	commonScriptingClasses.detailPrototype = OptionalUnityEngineType ("DetailPrototype");
	commonScriptingClasses.treePrototype = OptionalUnityEngineType ("TreePrototype");
	commonScriptingClasses.treeInstance = OptionalUnityEngineType ("TreeInstance");
	commonScriptingClasses.splatPrototype = OptionalUnityEngineType ("SplatPrototype");
#endif
	commonScriptingClasses.animationEvent = RequireUnityEngineType ("AnimationEvent");
	commonScriptingClasses.assetBundleRequest = RequireUnityEngineType ("AssetBundleRequest");
	commonScriptingClasses.asyncOperation = RequireUnityEngineType ("AsyncOperation");
#if ENABLE_WWW
	commonScriptingClasses.assetBundleCreateRequest = RequireUnityEngineType ("AssetBundleCreateRequest");
    // Stop scaring the customers and only load this when it's needed and available
	commonScriptingClasses.cacheIndex = RequireUnityEngineType ("CacheIndex");
#else
    commonScriptingClasses.cacheIndex = SCRIPTING_NULL;
#endif
	commonScriptingClasses.cachedFile = OptionalUnityEngineType ("CachedFile");

	commonScriptingClasses.keyframe = RequireUnityEngineType ("Keyframe");
	commonScriptingClasses.inputEvent = RequireUnityEngineType ("Event");
	commonScriptingClasses.imageEffectOpaque = OptionalUnityEngineType ("ImageEffectOpaque");
	commonScriptingClasses.imageEffectTransformsToLDR = OptionalUnityEngineType ("ImageEffectTransformsToLDR");
	commonScriptingClasses.iEnumerator = typeRegistry.GetType("System.Collections", "IEnumerator");
	commonScriptingClasses.systemObject = typeRegistry.GetType("System","Object");


	commonScriptingClasses.string = typeRegistry.GetType("System", "String");
	commonScriptingClasses.int_32 = typeRegistry.GetType("System", "Int32");
	commonScriptingClasses.floatSingle = typeRegistry.GetType("System", "Single");
	commonScriptingClasses.floatDouble = typeRegistry.GetType("System", "Double");
	commonScriptingClasses.byte = typeRegistry.GetType("System", "Byte");

#if ENABLE_MONO || UNITY_WINRT
	commonScriptingClasses.intptr = typeRegistry.GetType("System", "IntPtr");
	commonScriptingClasses.uInt_16 = typeRegistry.GetType("System", "UInt16");
	commonScriptingClasses.uInt_32 = typeRegistry.GetType("System", "UInt32");
	commonScriptingClasses.int_16 = typeRegistry.GetType("System", "Int16");
	commonScriptingClasses.multicastDelegate = typeRegistry.GetType("System", "MulticastDelegate");
	commonScriptingClasses.extractRequiredComponents = RequireMethod ("UnityEngine","AttributeHelperEngine", "GetRequiredComponents");
#endif

#if ENABLE_MONO || UNITY_WP8
	commonScriptingClasses.invokeMember = RequireMethod ("UnityEngine","SetupCoroutine", "InvokeMember");
	commonScriptingClasses.invokeStatic = RequireMethod ("UnityEngine","SetupCoroutine", "InvokeStatic");
#endif
#if ENABLE_MONO
	commonScriptingClasses.checkIsEditMode = RequireMethod ("UnityEngine","AttributeHelperEngine", "CheckIsEditorScript");
	commonScriptingClasses.extractStacktrace = RequireMethod ("UnityEngine","StackTraceUtility", "ExtractStackTrace");
	commonScriptingClasses.extractStringFromException = RequireMethod ("UnityEngine","StackTraceUtility", "ExtractStringFromExceptionInternal");
	commonScriptingClasses.postprocessStacktrace = RequireMethod ("UnityEngine","StackTraceUtility", "PostprocessStacktrace");
#endif

	commonScriptingClasses.IEnumerator_MoveNext = RequireMethod("System.Collections", "IEnumerator","MoveNext");
#if ENABLE_MONO || UNITY_WINRT
	commonScriptingClasses.callLogCallback = RequireMethod ("UnityEngine", "Application", "CallLogCallback");
	commonScriptingClasses.IEnumerator_Current = GetScriptingMethodRegistry().GetMethod("System.Collections", "IEnumerator","get_Current");
	commonScriptingClasses.IDisposable_Dispose = GetScriptingMethodRegistry().GetMethod("System", "IDisposable","Dispose");
#endif

	commonScriptingClasses.beginGUI = RequireMethod ("UnityEngine", "GUIUtility", "BeginGUI");
	commonScriptingClasses.endGUI = RequireMethod ("UnityEngine", "GUIUtility", "EndGUI");
	commonScriptingClasses.callGUIWindowDelegate = RequireMethod ("UnityEngine", "GUI", "CallWindowDelegate");
	commonScriptingClasses.makeMasterEventCurrent = RequireMethod ("UnityEngine", "Event", "Internal_MakeMasterEventCurrent");
	commonScriptingClasses.doSendMouseEvents = RequireMethod ("UnityEngine", "SendMouseEvents", "DoSendMouseEvents");

#if ENABLE_MONO
	commonScriptingClasses.stackTraceUtilitySetProjectFolder = RequireMethod ("UnityEngine", "StackTraceUtility", "SetProjectFolder");
#endif

#if ENABLE_MONO
	commonScriptingClasses.enumClass = mono_get_enum_class ();
	commonScriptingClasses.floatSingleArray = mono_array_class_get(commonScriptingClasses.floatSingle, 1);
#endif // ENABLE_MONO


#if UNITY_EDITOR
	commonScriptingClasses.monoReloadableIntPtr = GetMonoManager().GetBuiltinEditorMonoClass ("MonoReloadableIntPtr");
	commonScriptingClasses.monoReloadableIntPtrClear = GetMonoManager().GetBuiltinEditorMonoClass ("MonoReloadableIntPtrClear");
	commonScriptingClasses.gameViewStatsGUI = RequireMethod ("UnityEditor", "GameViewGUI", "GameViewStatsGUI");
	commonScriptingClasses.beginHandles = RequireMethod ("UnityEditor", "HandleUtility", "BeginHandles");
	commonScriptingClasses.endHandles = RequireMethod ("UnityEditor", "HandleUtility", "EndHandles");
	commonScriptingClasses.setViewInfo = RequireMethod ("UnityEditor", "HandleUtility", "SetViewInfo");
	commonScriptingClasses.handleControlID = RequireMethod ("UnityEditor","EditorGUIUtility","HandleControlID");
	commonScriptingClasses.callGlobalEventHandler = RequireMethod ("UnityEditor", "EditorApplication", "Internal_CallGlobalEventHandler");
	commonScriptingClasses.callAnimationClipAwake = RequireMethod ("UnityEditor", "AnimationUtility", "Internal_CallAnimationClipAwake");
	commonScriptingClasses.statusBarChanged = RequireMethod ("UnityEditor", "AppStatusBar", "StatusChanged");
	commonScriptingClasses.lightmappingDone = RequireMethod ("UnityEditor", "LightmappingWindow", "LightmappingDone");
	commonScriptingClasses.consoleLogChanged = RequireMethod ("UnityEditor", "ConsoleWindow", "LogChanged");
	commonScriptingClasses.clearUndoSnapshotTarget = RequireMethod ("UnityEditor", "Undo", "ClearSnapshotTarget");
	commonScriptingClasses.repaintAllProfilerWindows = RequireMethod ("UnityEditor", "ProfilerWindow", "RepaintAllProfilerWindows");
	commonScriptingClasses.getGameViewAspectRatio = RequireMethod ("UnityEditor", "CameraEditor", "GetGameViewAspectRatio");
	commonScriptingClasses.substanceMaterialInformation = RequireType("UnityEditor", "ProceduralMaterialInformation" );
	commonScriptingClasses.propertyModification = RequireType("UnityEditor", "PropertyModification" );
	commonScriptingClasses.undoPropertyModification = RequireType("UnityEditor", "UndoPropertyModification" );
	commonScriptingClasses.callbackOrderAttribute = RequireType("UnityEditor", "CallbackOrderAttribute" );
	commonScriptingClasses.postProcessBuildAttribute = RequireType ("UnityEditor.Callbacks", "PostProcessBuildAttribute");
	commonScriptingClasses.postProcessSceneAttribute = RequireType ("UnityEditor.Callbacks", "PostProcessSceneAttribute");
	commonScriptingClasses.didReloadScripts = RequireType ("UnityEditor.Callbacks", "DidReloadScripts");
	commonScriptingClasses.onOpenAssetAttribute = RequireType ("UnityEditor.Callbacks", "OnOpenAssetAttribute");

	commonScriptingClasses.animationClipSettings = RequireType ("UnityEditor","AnimationClipSettings");
	commonScriptingClasses.muscleClipQualityInfo = RequireType ("UnityEditor","MuscleClipQualityInfo");
#endif // UNITY_EDITOR

	commonScriptingClasses.gradient = RequireUnityEngineType ("Gradient");
	commonScriptingClasses.gradientColorKey = RequireUnityEngineType ("GradientColorKey");
	commonScriptingClasses.gradientAlphaKey = RequireUnityEngineType ("GradientAlphaKey");

#if ENABLE_SUBSTANCE
	commonScriptingClasses.substancePropertyDescription = OptionalUnityEngineType ("ProceduralPropertyDescription");
#endif

	commonScriptingClasses.animatorStateInfo= RequireUnityEngineType ("AnimatorStateInfo");
	commonScriptingClasses.animatorTransitionInfo= RequireUnityEngineType ("AnimatorTransitionInfo");
	commonScriptingClasses.animationInfo= RequireUnityEngineType ("AnimationInfo");
	commonScriptingClasses.skeletonBone = RequireUnityEngineType ("SkeletonBone");
	commonScriptingClasses.humanBone = RequireUnityEngineType ("HumanBone");

#if ENABLE_GAMECENTER
	commonScriptingClasses.gameCenter = GetMonoManager().GetMonoClass("GameCenterPlatform", "UnityEngine.SocialPlatforms.GameCenter");
	commonScriptingClasses.gcAchievement = GetMonoManager().GetMonoClass("GcAchievementData", "UnityEngine.SocialPlatforms.GameCenter");
	commonScriptingClasses.gcAchievementDescription = GetMonoManager().GetMonoClass("GcAchievementDescriptionData", "UnityEngine.SocialPlatforms.GameCenter");
	commonScriptingClasses.gcScore = GetMonoManager().GetMonoClass("GcScoreData", "UnityEngine.SocialPlatforms.GameCenter");
	commonScriptingClasses.gcUserProfile = GetMonoManager().GetMonoClass("GcUserProfileData", "UnityEngine.SocialPlatforms.GameCenter");
#endif

#if ENABLE_WEBCAM
	// We support stripping there
	#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
		commonScriptingClasses.webCamDevice = OptionalUnityEngineType("WebCamDevice");
	#else
		commonScriptingClasses.webCamDevice = RequireUnityEngineType("WebCamDevice");
	#endif
#endif

	commonScriptingClasses.display 						= RequireUnityEngineType("Display");
	commonScriptingClasses.displayRecreateDisplayList	= RequireMethod("UnityEngine","Display","RecreateDisplayList");
	commonScriptingClasses.displayFireDisplaysUpdated	= RequireMethod("UnityEngine","Display","FireDisplaysUpdated");

#if UNITY_IPHONE
	commonScriptingClasses.adBannerView					= OptionalUnityEngineType("ADBannerView");
	commonScriptingClasses.adInterstitialAd				= OptionalUnityEngineType("ADInterstitialAd");
	commonScriptingClasses.adFireBannerWasClicked		= OptionalMethod ("UnityEngine", "ADBannerView", "FireBannerWasClicked");
	commonScriptingClasses.adFireBannerWasLoaded		= OptionalMethod ("UnityEngine", "ADBannerView", "FireBannerWasLoaded");
	commonScriptingClasses.adFireInterstitialWasLoaded	= OptionalMethod ("UnityEngine", "ADInterstitialAd", "FireInterstitialWasLoaded");
#endif


#endif
}

void ClearCommonScriptingClasses(CommonScriptingClasses& commonScriptingClasses)
{
	memset(&commonScriptingClasses, 0, sizeof(commonScriptingClasses));
}

#endif
