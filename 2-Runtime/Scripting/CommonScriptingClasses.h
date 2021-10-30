#pragma once
#if ENABLE_SCRIPTING
#include "Runtime/Mono/MonoTypes.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"

struct CommonScriptingClasses
{
	ScriptingClassPtr monoBehaviour;
	ScriptingClassPtr component;
	ScriptingClassPtr scriptableObject;
	ScriptingClassPtr vector2;
	ScriptingClassPtr vector3;
	ScriptingClassPtr vector4;
	ScriptingClassPtr rect;
	ScriptingClassPtr rectOffset;
	ScriptingClassPtr quaternion;
	ScriptingClassPtr matrix4x4;
	ScriptingClassPtr bounds;
	ScriptingClassPtr resolution;
	ScriptingClassPtr particle;
	ScriptingClassPtr color;
	ScriptingClassPtr color32;
	ScriptingClassPtr raycastHit;
	ScriptingClassPtr raycastHit2D;
	ScriptingClassPtr animationState;
	ScriptingClassPtr collider;
	ScriptingClassPtr camera;
	ScriptingClassPtr renderTexture;
	ScriptingClassPtr layerMask;
	ScriptingClassPtr serializeField;
	ScriptingClassPtr enumClass;
	ScriptingClassPtr iEnumerator;
	ScriptingClassPtr systemObject;

#if !UNITY_FLASH
	ScriptingClassPtr intptr;
	ScriptingClassPtr uInt_16;
	ScriptingClassPtr uInt_32;
	ScriptingClassPtr int_16;
	ScriptingClassPtr multicastDelegate;
#endif

	ScriptingClassPtr byte;
	ScriptingClassPtr int_32;
	ScriptingClassPtr string;
	ScriptingClassPtr floatSingle;
	ScriptingClassPtr floatDouble;
	ScriptingClassPtr waitForSeconds;
	ScriptingClassPtr waitForFixedUpdate;
	ScriptingClassPtr waitForEndOfFrame;

	ScriptingClassPtr characterInfo;
	ScriptingMethodPtr font_InvokeFontTextureRebuildCallback_Internal;
#if ENABLE_WWW
	ScriptingClassPtr assetBundleCreateRequest;
	ScriptingClassPtr www;
#endif
#if UNITY_WII || UNITY_PS3 || UNITY_XENON
	ScriptingClassPtr waitAsyncOperationFinish;
#endif
	ScriptingClassPtr meshData;
	ScriptingClassPtr lodMesh;
	ScriptingClassPtr coroutine;
	ScriptingClassPtr collision;
	ScriptingClassPtr contactPoint;
	ScriptingClassPtr controllerColliderHit;
	ScriptingClassPtr collision2D;
	ScriptingClassPtr contactPoint2D;
	ScriptingClassPtr event;
	ScriptingClassPtr unityEngineObject;
	ScriptingClassPtr hideInInspector;
	ScriptingClassPtr serializePrivateVariables;
#if ENABLE_NETWORK
	ScriptingClassPtr RPC;
	ScriptingClassPtr hostData;
	ScriptingClassPtr bitStream;
	ScriptingClassPtr networkPlayer;
	ScriptingClassPtr networkViewID;
	ScriptingClassPtr networkMessageInfo;
#endif

	ScriptingClassPtr guiStyle;
	ScriptingClassPtr animationCurve;
	ScriptingClassPtr keyframe;
	ScriptingClassPtr boneWeight;

#if ENABLE_MONO || UNITY_WP8
	ScriptingMethodPtr invokeMember;
	ScriptingMethodPtr invokeStatic;
#endif
#if ENABLE_MONO
	ScriptingMethodPtr checkIsEditMode;
	ScriptingMethodPtr extractStacktrace;
	ScriptingMethodPtr extractStringFromException;
	ScriptingMethodPtr postprocessStacktrace;
#endif
	ScriptingMethodPtr IEnumerator_MoveNext;
#if ENABLE_MONO || UNITY_WINRT
	ScriptingMethodPtr callLogCallback;
	ScriptingMethodPtr IEnumerator_Current;
	ScriptingMethodPtr IDisposable_Dispose;
	ScriptingMethodPtr extractRequiredComponents;
#endif

	ScriptingMethodPtr beginGUI;
	ScriptingMethodPtr endGUI;
	ScriptingMethodPtr callGUIWindowDelegate;

#if ENABLE_TERRAIN
	ScriptingClassPtr terrain;
	ScriptingClassPtr detailPrototype;
	ScriptingClassPtr treePrototype;
	ScriptingClassPtr treeInstance;
	ScriptingClassPtr splatPrototype;
#endif
	ScriptingClassPtr animationEvent;
	ScriptingClassPtr assetBundleRequest;
	ScriptingClassPtr asyncOperation;
	ScriptingClassPtr cacheIndex;
	ScriptingClassPtr cachedFile;
	ScriptingClassPtr inputEvent;
	ScriptingClassPtr imageEffectOpaque;
	ScriptingClassPtr muscleBoneInfo;
	ScriptingClassPtr animationClipSettings;
	ScriptingClassPtr muscleClipQualityInfo;
	ScriptingClassPtr imageEffectTransformsToLDR;

	ScriptingMethodPtr makeMasterEventCurrent;
	ScriptingMethodPtr doSendMouseEvents;
	ScriptingMethodPtr stackTraceUtilitySetProjectFolder;



#if UNITY_EDITOR
	ScriptingClassPtr  monoReloadableIntPtr;
	ScriptingClassPtr  monoReloadableIntPtrClear;
	ScriptingMethodPtr gameViewStatsGUI;
	ScriptingMethodPtr beginHandles;
	ScriptingMethodPtr endHandles;
	ScriptingMethodPtr setViewInfo;
	ScriptingMethodPtr handleControlID;
	ScriptingMethodPtr callGlobalEventHandler;
	ScriptingMethodPtr callAnimationClipAwake;
	ScriptingMethodPtr statusBarChanged;
	ScriptingMethodPtr lightmappingDone;
	ScriptingMethodPtr consoleLogChanged;
	ScriptingMethodPtr clearUndoSnapshotTarget;
	ScriptingMethodPtr repaintAllProfilerWindows;
	ScriptingMethodPtr getGameViewAspectRatio;
	ScriptingClassPtr substanceMaterialInformation;
	ScriptingClassPtr propertyModification;
	ScriptingClassPtr undoPropertyModification;
	ScriptingClassPtr exportExtensionClassAttribute;
#endif // #if UNITY_EDITOR

	ScriptingClassPtr gradient;
	ScriptingClassPtr gradientColorKey;
	ScriptingClassPtr gradientAlphaKey;
	ScriptingClassPtr callbackOrderAttribute;
	ScriptingClassPtr postProcessBuildAttribute;
	ScriptingClassPtr postProcessSceneAttribute;
	ScriptingClassPtr didReloadScripts;
	ScriptingClassPtr onOpenAssetAttribute;

#if ENABLE_SUBSTANCE
	ScriptingClassPtr substancePropertyDescription;
#endif

	ScriptingClassPtr animatorStateInfo;
	ScriptingClassPtr animatorTransitionInfo;
	ScriptingClassPtr animationInfo;
	ScriptingClassPtr floatSingleArray;
	ScriptingClassPtr skeletonBone;
	ScriptingClassPtr humanBone;

#if ENABLE_GAMECENTER
	ScriptingClassPtr gameCenter;
	ScriptingClassPtr gcScore;
	ScriptingClassPtr gcAchievement;
	ScriptingClassPtr gcAchievementDescription;
	ScriptingClassPtr gcUserProfile;
#endif

#if ENABLE_WEBCAM
	ScriptingClassPtr webCamDevice;
#endif

	ScriptingClassPtr	display;
	ScriptingMethodPtr	displayRecreateDisplayList;
	ScriptingMethodPtr	displayFireDisplaysUpdated;

#if UNITY_IPHONE
	ScriptingClassPtr	adBannerView;
	ScriptingClassPtr	adInterstitialAd;
	ScriptingMethodPtr	adFireBannerWasClicked;
	ScriptingMethodPtr	adFireBannerWasLoaded;
	ScriptingMethodPtr	adFireInterstitialWasLoaded;
#endif
};

void FillCommonScriptingClasses(CommonScriptingClasses& commonScriptingClasses);
void ClearCommonScriptingClasses(CommonScriptingClasses& commonScriptingClasses);

#define MONO_COMMON GetScriptingManager().GetCommonClasses()

#endif
