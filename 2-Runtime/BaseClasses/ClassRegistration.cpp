#include "UnityPrefix.h"
#include "ClassRegistration.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Modules/ModuleRegistration.h"

// IPhone platform with stripping overwrites the "RegisterAllClasses" function.
// See GenerateRegisterClassesForStripping in MonoInternalCallGenerator.cs.
// This is why the iPhone builds overwrite the function name here.
#if UNITY_IPHONE
#define RegisterAllClasses RegisterAllClassesIPhone
#endif

using namespace std;
#if DEBUGMODE
static void VerifyThatAllClassesHaveBeenRegistered(const RegisteredClassSet& explicitlyRegistered);
#endif

#define RESERVE_CLASSID(klass,classID)            ValidateRegisteredClassID (context,classID, #klass);
#define RESERVE_DEPRECATED_CLASSID(klass,classID) ValidateRegisteredClassID (context,classID, #klass);

void ValidateRegisteredClassID (ClassRegistrationContext& context, int classID, const char* className)
{
#if DEBUGMODE
	RegisteredClassSet& explicitlyRegistered = *reinterpret_cast<RegisteredClassSet*> (context.explicitlyRegistered);
	bool didNotExist = explicitlyRegistered.insert(classID).second;

	if (!didNotExist) FatalErrorString(Format("ClassID %d conflicts with that of another class. Please resolve the conflict. (%s)", classID, className));
#endif
}


#if DEBUGMODE
void RegisterDeprecatedClassIDs (ClassRegistrationContext& context)
{
	/// DO NOT REMOVE CLASS IDS FROM THIS LIST TO MAKE ROOM FOR A NEW ONE. IT WILL RESULT IN CLASSID CONFLICTS ON OLD PROJECTS
	
	RESERVE_DEPRECATED_CLASSID (BehaviourManager, 7) // Removed in Unity 3.2 
	RESERVE_DEPRECATED_CLASSID (Filter, 16) // Removed ages ago
	RESERVE_DEPRECATED_CLASSID (PipelineManager, 31)    // Removed in Unity 3.2
	RESERVE_DEPRECATED_CLASSID (BaseBehaviourManager, 34)   // Removed in Unity 3.2
	RESERVE_DEPRECATED_CLASSID (LateBehaviourManager, 35)  // Removed in Unity 3.2 
	RESERVE_DEPRECATED_CLASSID (FixedBehaviourManager, 46)  // Removed in Unity 3.2 
	RESERVE_DEPRECATED_CLASSID (UpdateManager, 63)  // Removed in Unity 3.2 
	RESERVE_DEPRECATED_CLASSID (RenderLayer, 67) // Intermediate abstract class refactored away post-Unity 3.2
	RESERVE_DEPRECATED_CLASSID (AnimationTrack2, 112)
	RESERVE_DEPRECATED_CLASSID (ResourceManagerOLD, 113)
	RESERVE_DEPRECATED_CLASSID (GooballCollider, 77)
	RESERVE_DEPRECATED_CLASSID (VertexSnapper, 79)
	RESERVE_DEPRECATED_CLASSID (LightManager, 85) // -> changed id DO NOT REUSE
	//RESERVE_DEPRECATED_CLASSID (PreloadManager, 90) // Now used by Avatar
	//RESERVE_DEPRECATED_CLASSID (ScaleFilter, 91) // deprecated pre 1.0, now used by AnimatorController
	//RESERVE_DEPRECATED_CLASSID (TextureRect, 93) // Now used by RuntimeAnimatorController
	//RESERVE_DEPRECATED_CLASSID (MotorJoint, 95) // Now used by Animator
	RESERVE_DEPRECATED_CLASSID (Decal, 97) // Pre 1.0
	RESERVE_DEPRECATED_CLASSID (EulerRotationMotor, 139)
	RESERVE_DEPRECATED_CLASSID (ParticleCloudColor, 103) // Pre 1.0
	RESERVE_DEPRECATED_CLASSID (TextScript, 105)
	RESERVE_DEPRECATED_CLASSID (VertexProgram, 106)
	RESERVE_DEPRECATED_CLASSID (FragmentProgram, 107)
	RESERVE_DEPRECATED_CLASSID (GooStickyness, 151)
	RESERVE_DEPRECATED_CLASSID (ClothAnimator, 99) // PRE 1.0
	RESERVE_DEPRECATED_CLASSID (PatchRenderer, 100) // PRE 1.0
	RESERVE_DEPRECATED_CLASSID (Stretcher, 101) // PRE 1.0
	RESERVE_DEPRECATED_CLASSID (AudioManager, 80) // -> changed id DO NOT REUSE
	//RESERVE_DEPRECATED_CLASSID (AxisRenderer, 1008) // REMOVED in 2.0, now used by ComputeShaderImporter
	RESERVE_DEPRECATED_CLASSID (BBoxRenderer, 1009) // REMOVED in 2.0
	RESERVE_DEPRECATED_CLASSID (CopyTransform, 1010) // REMOVED in 2.1
	//RESERVE_DEPRECATED_CLASSID (DotRenderer, 1011) // REMOVED in 2.0, now used by AvatarMask
	RESERVE_DEPRECATED_CLASSID (SphereRenderer, 1012) // Not here anymore
	RESERVE_DEPRECATED_CLASSID (WireRenderer, 1024) // Removed in 2.0
	RESERVE_DEPRECATED_CLASSID (AnimationManager, 71) // Removed in 4.3
}

void RegisterReservedClassIDs (ClassRegistrationContext& context)
{
	RESERVE_CLASSID (PreviewAssetType, 1108);
	RESERVE_CLASSID (GUITransform, 187)
	RESERVE_CLASSID (GUIButton, 188)
	RESERVE_CLASSID (GUIGroup, 189)
	RESERVE_CLASSID (GUIComponent, 190)
	RESERVE_CLASSID (GUICanvas, 219)
	RESERVE_CLASSID (GUIToggle, 200)
	RESERVE_CLASSID (GUIImage, 201)
	RESERVE_CLASSID (GUILabel, 202)
	RESERVE_CLASSID (GUISlider, 211)
	RESERVE_CLASSID (GUITextField, 204)
	RESERVE_CLASSID (GUIKeyboardControl, 210)
	RESERVE_CLASSID (InWorldGUI, 209)
	RESERVE_CLASSID (GUICamera, 217)
	RESERVE_CLASSID (TextureAtlas, 203)
}

static void VerifyThatAllClassesHaveBeenRegistered(const RegisteredClassSet& explicitlyRegistered)
{
	const RegisteredClassSet& classes = GetVerifyClassRegistration ();

	for (RegisteredClassSet::const_iterator i=classes.begin();i != classes.end();++i)
	{
		if (explicitlyRegistered.count (*i) == 0)
		{
			FatalErrorString(Format("ClassID %d has not been registered but is included in the build. You must add the class to RegisterAllClasses.", *i));
		}
	}

	Assert(classes == explicitlyRegistered);
}
#endif

void RegisterAllClasses()
{
	ClassRegistrationContext context;
#if DEBUGMODE
	RegisteredClassSet explicitlyRegistered;
	context.explicitlyRegistered = &explicitlyRegistered;
#endif
	
	REGISTER_CLASS (GameObject)
	REGISTER_CLASS (Component)
	REGISTER_CLASS (LevelGameManager)
	REGISTER_CLASS (Transform)
	REGISTER_CLASS (TimeManager)
	REGISTER_CLASS (GlobalGameManager)
	REGISTER_CLASS (Behaviour)
	REGISTER_CLASS (GameManager)
	REGISTER_CLASS (ParticleAnimator)
	REGISTER_CLASS (InputManager)
	REGISTER_CLASS (EllipsoidParticleEmitter)
	REGISTER_CLASS (Pipeline)
	REGISTER_CLASS (EditorExtension)
	REGISTER_CLASS (Camera)
	REGISTER_CLASS (Material)
	REGISTER_CLASS (Mesh)
	REGISTER_CLASS (MeshRenderer)
	REGISTER_CLASS (MeshFilter)
	REGISTER_CLASS (Renderer)
	REGISTER_CLASS (ParticleRenderer)
	REGISTER_CLASS (Texture)
	REGISTER_CLASS (Texture2D)
	REGISTER_CLASS (SceneSettings)
	REGISTER_CLASS (OcclusionPortal)
	REGISTER_CLASS (Skybox)
	REGISTER_CLASS (QualitySettings)
	REGISTER_CLASS (Shader)
	REGISTER_CLASS (TextAsset)
	REGISTER_CLASS (ComputeShader)
	REGISTER_CLASS (WorldParticleCollider)
	REGISTER_CLASS (TagManager)
	REGISTER_CLASS (RenderTexture)
	REGISTER_CLASS (MeshParticleEmitter)
	REGISTER_CLASS (ParticleEmitter)
	REGISTER_CLASS (Cubemap)
	REGISTER_CLASS (GUILayer)
	REGISTER_CLASS (ScriptMapper)
	REGISTER_CLASS (TrailRenderer)
	REGISTER_CLASS (DelayedCallManager)
	REGISTER_CLASS (TextMesh)
	REGISTER_CLASS (Light)
	REGISTER_CLASS (CGProgram)
	REGISTER_CLASS (LightProbes)
	REGISTER_CLASS (ResourceManager)
	REGISTER_CLASS (Texture3D)
	REGISTER_CLASS (Projector)
	REGISTER_CLASS (LineRenderer)
	REGISTER_CLASS (Flare)
	REGISTER_CLASS (Halo)
	REGISTER_CLASS (LensFlare)
	REGISTER_CLASS (FlareLayer)
	REGISTER_CLASS (HaloLayer)
	REGISTER_CLASS (HaloManager)
	REGISTER_CLASS (PreloadData)
	REGISTER_CLASS (LightmapSettings)
	REGISTER_CLASS (RenderSettings)
	REGISTER_CLASS (NamedObject)
	REGISTER_CLASS (GUIText)
	REGISTER_CLASS (GUITexture)
	REGISTER_CLASS (Font)
	REGISTER_CLASS (GUIElement)
	REGISTER_CLASS (SkinnedMeshRenderer)
	REGISTER_CLASS (BuildSettings)
	REGISTER_CLASS (AssetBundle)
	REGISTER_CLASS (OcclusionArea)
	REGISTER_CLASS (ParticleSystem)
	REGISTER_CLASS (ParticleSystemRenderer)
	REGISTER_CLASS (GraphicsSettings)
	REGISTER_CLASS (PlayerSettings)
	REGISTER_CLASS (SubstanceArchive)
	REGISTER_CLASS (ProceduralMaterial)
	REGISTER_CLASS (ProceduralTexture)
	REGISTER_CLASS (LODGroup)
	REGISTER_CLASS (LightProbeGroup)
	REGISTER_CLASS (WindZone)
	
#if ENABLE_SCRIPTING
	REGISTER_CLASS (MonoScript)
	REGISTER_CLASS (MonoManager)
	REGISTER_CLASS (MonoBehaviour)
#endif
		
#if ENABLE_NETWORK
	REGISTER_CLASS (NetworkView)
	REGISTER_CLASS (NetworkManager)
	REGISTER_CLASS (MasterServerInterface)
#endif
	
#if ENABLE_SPRITES
	REGISTER_CLASS (SpriteRenderer)
	REGISTER_CLASS (Sprite)	
	#if UNITY_EDITOR
		REGISTER_CLASS (CachedSpriteAtlas)
	#endif
#endif

	RegisterAllAvailableModuleClasses (context);
	
	// Editor Only classes following:	
#if UNITY_EDITOR
	REGISTER_CLASS (EditorSettings)
	REGISTER_CLASS (EditorUserSettings)
	REGISTER_CLASS (Prefab)
	REGISTER_CLASS (EditorExtensionImpl)
	REGISTER_CLASS (AssetImporter)
	REGISTER_CLASS (AssetDatabase)
	REGISTER_CLASS (Mesh3DSImporter)
	REGISTER_CLASS (TextureImporter)
	REGISTER_CLASS (ShaderImporter)
	REGISTER_CLASS (ComputeShaderImporter)
	REGISTER_CLASS (AudioImporter)
	REGISTER_CLASS (GUIDSerializer)
	REGISTER_CLASS (AssetMetaData)
	REGISTER_CLASS (DefaultAsset)
	REGISTER_CLASS (DefaultImporter)
	REGISTER_CLASS (TextScriptImporter)
	REGISTER_CLASS (SceneAsset)
	REGISTER_CLASS (NativeFormatImporter)
	REGISTER_CLASS (MonoImporter)
	REGISTER_CLASS (MonoAssemblyImporter)
	REGISTER_CLASS (AssetServerCache)
	REGISTER_CLASS (LibraryAssetImporter)
	REGISTER_CLASS (ModelImporter)
	REGISTER_CLASS (FBXImporter)
	REGISTER_CLASS (TrueTypeFontImporter)
	REGISTER_CLASS (EditorBuildSettings)
	REGISTER_CLASS (DDSImporter)
	REGISTER_CLASS (InspectorExpandedState)
	REGISTER_CLASS (AnnotationManager)
	REGISTER_CLASS (EditorUserBuildSettings)
	REGISTER_CLASS (PVRImporter)
	REGISTER_CLASS (HierarchyState)
	REGISTER_CLASS (Transition)	
	REGISTER_CLASS (State)		
	REGISTER_CLASS (HumanTemplate)
	REGISTER_CLASS (StateMachine)
	REGISTER_CLASS (AvatarMask)	
	REGISTER_CLASS (BlendTree)
	REGISTER_CLASS (SubstanceImporter)

#if !UNITY_LINUX
	REGISTER_CLASS (MovieImporter)
#endif
#endif // UNITY_EDITOR
	
#if DEBUGMODE
	VerifyThatAllClassesHaveBeenRegistered(explicitlyRegistered);
	RegisterDeprecatedClassIDs(context);
	RegisterReservedClassIDs(context);
#endif
}
