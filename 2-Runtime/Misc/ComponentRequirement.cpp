#include "UnityPrefix.h"
#include "ComponentRequirement.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include <algorithm>

#if UNITY_EDITOR
#include "Editor/Src/LicenseInfo.h"
#endif

#if !UNITY_WIN && !UNITY_XENON && !UNITY_PS3 && !UNITY_ANDROID
#pragma optimization_level 0
#endif

typedef std::map<int, vector_set<int> >  ClassIDToClassIDsMap;
static ClassIDToClassIDsMap* gRequiredClasses;
static ClassIDToClassIDsMap* gConflictingClasses;
typedef std::set<int> ClassIDSet;
static ClassIDSet* gAllowsMultipleInclusion;
static ClassIDSet* gDoesComponentAllowReplacement;
#if UNITY_EDITOR
static ComponentsHierarchy*          gComponentHierarchy;
#endif

namespace ComponentRequirements
{
	void StaticInitialize()
	{
		gRequiredClasses = UNITY_NEW(ClassIDToClassIDsMap,kMemResource);
		gConflictingClasses = UNITY_NEW(ClassIDToClassIDsMap,kMemResource);
		gAllowsMultipleInclusion = UNITY_NEW(ClassIDSet,kMemResource);
		gDoesComponentAllowReplacement = UNITY_NEW(ClassIDSet,kMemResource);
#if UNITY_EDITOR
		gComponentHierarchy = UNITY_NEW(ComponentsHierarchy,kMemResource);
#endif
	}
	void StaticDestroy()
	{
		UNITY_DELETE(gRequiredClasses,kMemResource);
		UNITY_DELETE(gConflictingClasses,kMemResource);
		UNITY_DELETE(gAllowsMultipleInclusion,kMemResource);
		UNITY_DELETE(gDoesComponentAllowReplacement,kMemResource);
#if UNITY_EDITOR
		UNITY_DELETE(gComponentHierarchy,kMemResource);
#endif
	}
}
static RegisterRuntimeInitializeAndCleanup s_ComponentRequirementsCallbacks(ComponentRequirements::StaticInitialize, ComponentRequirements::StaticDestroy);

const vector_set<int>& FindRequiredComponentsForComponent (int componentClassID)
{
	InitComponentRequirements ();
	return (*gRequiredClasses)[componentClassID];
}

void FindAllRequiredComponentsRecursive (int componentClassID, vector_set<int>& results)
{
	// Already added
	if (!results.insert(componentClassID).second)
		return;
	
	const vector_set<int>& thisClassResult = FindRequiredComponentsForComponent (componentClassID);
	for (int i=0;i<thisClassResult.size();i++)
		FindAllRequiredComponentsRecursive(thisClassResult[i], results);
}

const vector_set<int>& FindConflictingComponents (int classID)
{
	InitComponentRequirements ();
	return (*gConflictingClasses)[classID];
}


int GetAllowComponentReplacementClass (int classID)
{
	while (classID != ClassID (Object))
	{
		if (gDoesComponentAllowReplacement->count (classID))
			return classID;
			
		classID = Object::GetSuperClassID (classID);
	}
	return -1;
}

bool DoesComponentAllowMultipleInclusion (int componentClassID)
{
	InitComponentRequirements ();
	return gAllowsMultipleInclusion->find(componentClassID) != gAllowsMultipleInclusion->end();
}
#if UNITY_EDITOR
const ComponentsHierarchy& GetComponentsHierarchy ()
{
	InitComponentRequirements ();
	return (*gComponentHierarchy);
}
#endif

static void AddRequiredClassIMPL (const string& aClass, const string& requiredClass)
{
	int classID = Object::StringToClassID (aClass);
	int requiredClassID = Object::StringToClassID (requiredClass);
#if !SUPPORTS_NATIVE_CODE_STRIPPING
	Assert (classID != -1 && requiredClassID != -1);
#endif

	std::vector<SInt32> derivedClasses;
	Object::FindAllDerivedClasses (classID, &derivedClasses, false);	
	
	for (std::vector<SInt32>::const_iterator i=derivedClasses.begin ();i!=derivedClasses.end ();++i)
		(*gRequiredClasses)[*i].insert (requiredClassID);
	
	(*gRequiredClasses)[classID].insert (requiredClassID);
}

static void AddConflictingClassIMPL (const string& aClass, const string& otherClass)
{
	int classID = Object::StringToClassID (aClass);
	int otherClassID = Object::StringToClassID (otherClass);
	if (classID == -1 || otherClassID == -1) // might have been stripped out
		return;

	std::vector<SInt32> derivedClasses;
	Object::FindAllDerivedClasses (classID, &derivedClasses, false);	

	for (std::vector<SInt32>::const_iterator i=derivedClasses.begin ();i!=derivedClasses.end ();++i)
		(*gConflictingClasses)[*i].insert (otherClassID);
	(*gConflictingClasses)[classID].insert (otherClassID);
}


#define AddRequiredClass(c, r) AddRequiredClassIMPL(#c, #r)
#define AddConflictingClass(c, r) AddConflictingClassIMPL(#c, #r)

#define AddToComponentHierarchy(className) \
{ \
	int classID = Object::StringToClassID (#className); \
	AssertIf (classID == 0 || classID == -1 || gComponentHierarchy->empty ()); \
	gComponentHierarchy->back ().second.push_back (GOComponentDescription (#className, classID)); \
	components.erase (classID); \
}	

#define AddGroup(s)	gComponentHierarchy->push_back (std::make_pair (s, std::vector<GOComponentDescription> ()));
#define AddSeparator() { gComponentHierarchy->back ().second.push_back (GOComponentDescription ("", 0)); }
#define AddAllowsMultipleInclusion(c) { int classID = Object::StringToClassID(#c); gAllowsMultipleInclusion->insert(classID); }
#define AddAllowComponentReplacement(c) { int classID = Object::StringToClassID(#c); AssertIf (classID == -1); gDoesComponentAllowReplacement->insert (classID); }



void InitComponentRequirements ()
{
	static bool gIsInitialized = false;
	if (gIsInitialized)
		return;
	gIsInitialized = true;
	
	gRequiredClasses->clear ();
	gConflictingClasses->clear ();
	gAllowsMultipleInclusion->clear ();
	std::vector<SInt32> componentsVec;
	Object::FindAllDerivedClasses (Object::StringToClassID ("Component"), &componentsVec, true);
	ClassIDSet components (componentsVec.begin (), componentsVec.end ());

	////////////////// Setup component requirements
	AddRequiredClass (Renderer, Transform);
	AddRequiredClass (MeshFilter, Transform);
	AddRequiredClass (ParticleAnimator, Transform);
	AddRequiredClass (EllipsoidParticleEmitter, Transform);
	AddRequiredClass (WorldParticleCollider, Transform);
	AddRequiredClass (ParticleSystem, Transform);
	AddRequiredClass (ParticleSystemRenderer, Transform);
	AddRequiredClass (Camera, Transform);
	AddRequiredClass (Light, Transform);

#if ENABLE_SPRITES
	AddRequiredClass (SpriteRenderer, Transform);
	AddConflictingClass (MeshFilter, SpriteRenderer);
	AddConflictingClass (MeshRenderer, SpriteRenderer);
	AddConflictingClass (SpriteRenderer, MeshFilter);
	AddConflictingClass (SpriteRenderer, MeshRenderer);
#endif // ENABLE_SPRITES

#if ENABLE_PHYSICS
	AddRequiredClass (Rigidbody, Transform);
	// Do not want 2D physics components on physics objects
	#if ENABLE_2D_PHYSICS
	AddConflictingClass (Rigidbody, Rigidbody2D);
	AddConflictingClass (Rigidbody, Collider2D);
	AddConflictingClass (Rigidbody, Joint2D);
	AddConflictingClass (Collider, Rigidbody2D);
	AddConflictingClass (Collider, Collider2D);
	AddConflictingClass (Collider, Joint2D);
	AddConflictingClass (Joint, Rigidbody2D);
	AddConflictingClass (Joint, Collider2D);
	AddConflictingClass (Joint, Joint2D);
	AddConflictingClass (ConstantForce, Rigidbody2D);
	AddConflictingClass (ConstantForce, Collider2D);
	AddConflictingClass (ConstantForce, Joint2D);

	#endif // #if ENABLE_2D_PHYSICS
#endif // #if ENABLE_PHYSICS

#if ENABLE_2D_PHYSICS
	AddRequiredClass (Rigidbody2D, Transform);
	AddRequiredClass (Collider2D, Transform);
	AddRequiredClass (Joint2D, Transform);
	AddAllowsMultipleInclusion (SpringJoint2D);
	AddAllowsMultipleInclusion (DistanceJoint2D);
	AddAllowsMultipleInclusion (HingeJoint2D);
	AddRequiredClass (Joint2D, Rigidbody2D);

	// Do not want physics components on 2D physics objects
	#if ENABLE_PHYSICS
	AddConflictingClass (Rigidbody2D, Rigidbody);
	AddConflictingClass (Rigidbody2D, Collider);
	AddConflictingClass (Rigidbody2D, Joint);
	AddConflictingClass (Rigidbody2D, ConstantForce);
	AddConflictingClass (Collider2D, Rigidbody);
	AddConflictingClass (Collider2D, Collider);
	AddConflictingClass (Collider2D, Joint);
	AddConflictingClass (Collider2D, ConstantForce);
	AddConflictingClass (Joint2D, Rigidbody);
	AddConflictingClass (Joint2D, Collider);
	AddConflictingClass (Joint2D, Joint);
	AddConflictingClass (Joint2D, ConstantForce);

	#endif // #if ENABLE_PHYSICS
#endif // #if ENABLE_2D_PHYSICS
	
	AddRequiredClass (GUIElement, Transform);
#if ENABLE_AUDIO	
	AddRequiredClass (AudioSource, Transform);
	AddRequiredClass (AudioListener, Transform);
#endif
#if ENABLE_AUDIO_FMOD
	AddRequiredClass (AudioReverbZone, Transform);
	AddRequiredClass (AudioLowPassFilter, AudioBehaviour);
	AddRequiredClass (AudioEchoFilter, AudioBehaviour);
	AddRequiredClass (AudioDistortionFilter, AudioBehaviour);
	AddRequiredClass (AudioReverbFilter, AudioBehaviour);
	AddRequiredClass (AudioHighPassFilter, AudioBehaviour);
	AddRequiredClass (AudioChorusFilter, AudioBehaviour);
#endif	
	AddRequiredClass (TextMesh, Transform);
	
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_2_a1))
		AddRequiredClass (TextMesh, MeshRenderer);

#if ENABLE_PHYSICS
	AddRequiredClass (Collider, Transform);
	AddRequiredClass (Joint, Rigidbody);
	AddRequiredClass (ConstantForce, Rigidbody);
#endif	
	
	AddRequiredClass (FlareLayer, Camera);
	AddRequiredClass (GUILayer, Camera);
	AddRequiredClass (Halo, Transform);
	
#if ENABLE_CLOTH
	AddRequiredClass (Cloth, Transform);
	AddRequiredClass (ClothRenderer, InteractiveCloth);
	AddRequiredClass (SkinnedCloth, SkinnedMeshRenderer);
#endif
	
	//////////////////// Setup allows multiple inclusion
	AddAllowsMultipleInclusion (HingeJoint);
	AddAllowsMultipleInclusion (FixedJoint);
	AddAllowsMultipleInclusion (CharacterJoint);
	AddAllowsMultipleInclusion (ConfigurableJoint);
	AddAllowsMultipleInclusion (SpringJoint);

	AddAllowsMultipleInclusion (AudioSource);
	AddAllowsMultipleInclusion (OffMeshLink);

	AddAllowsMultipleInclusion (Skybox)
	AddAllowsMultipleInclusion (MonoBehaviour)
	
	AddAllowsMultipleInclusion (NetworkView)
	
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
	{
		AddAllowsMultipleInclusion (BoxCollider);
		AddAllowsMultipleInclusion (SphereCollider);
		AddAllowsMultipleInclusion (CapsuleCollider);
		AddAllowsMultipleInclusion (MeshCollider);
	}

#if ENABLE_2D_PHYSICS
	AddAllowsMultipleInclusion (CircleCollider2D);
	AddAllowsMultipleInclusion (BoxCollider2D);
	AddAllowsMultipleInclusion (EdgeCollider2D);
	AddAllowsMultipleInclusion (PolygonCollider2D);
	#if ENABLE_SPRITECOLLIDER
	AddAllowsMultipleInclusion (SpriteCollider2D);
	#endif
#endif // #if ENABLE_2D_PHYSICS

	#if UNITY_EDITOR
	gComponentHierarchy->clear ();


	////////////////// Setup component hierarchy / Component menu
	AddGroup ("Mesh")
	AddToComponentHierarchy (MeshFilter);
	AddToComponentHierarchy (TextMesh);
	AddSeparator ();
	AddToComponentHierarchy (MeshRenderer);

	AddGroup ("Effects")
	AddToComponentHierarchy (ParticleSystem);
	AddToComponentHierarchy (TrailRenderer);
	AddToComponentHierarchy (LineRenderer);
	AddToComponentHierarchy (LensFlare);
	AddToComponentHierarchy (Halo);
	AddToComponentHierarchy (Projector);
	AddSeparator ();
	AddGroup ("Effects/Legacy Particles")
	AddToComponentHierarchy (EllipsoidParticleEmitter);
	AddToComponentHierarchy (MeshParticleEmitter);
	AddSeparator ();
	AddToComponentHierarchy (ParticleAnimator);
	AddToComponentHierarchy (WorldParticleCollider);
	AddSeparator ();
	AddToComponentHierarchy (ParticleRenderer);

	AddGroup ("Physics")
	AddToComponentHierarchy (Rigidbody);
	AddToComponentHierarchy (CharacterController);
	AddSeparator ();
	AddToComponentHierarchy (BoxCollider);
	AddToComponentHierarchy (SphereCollider);
	AddToComponentHierarchy (CapsuleCollider);
	AddToComponentHierarchy (MeshCollider);
	AddToComponentHierarchy (WheelCollider);
	AddToComponentHierarchy (TerrainCollider);
	AddSeparator ();
	AddToComponentHierarchy (InteractiveCloth);
	AddToComponentHierarchy (SkinnedCloth);
	AddToComponentHierarchy (ClothRenderer);
	AddSeparator ();
	AddToComponentHierarchy (HingeJoint);
	AddToComponentHierarchy (FixedJoint);
	AddToComponentHierarchy (SpringJoint);
	AddToComponentHierarchy (CharacterJoint);
	AddToComponentHierarchy (ConfigurableJoint);
	AddSeparator ();
	AddToComponentHierarchy (ConstantForce);

	#if ENABLE_2D_PHYSICS
	AddGroup ("Physics 2D");
	AddToComponentHierarchy (Rigidbody2D);
	AddSeparator ();
	AddToComponentHierarchy (CircleCollider2D);
	AddToComponentHierarchy (BoxCollider2D);
	AddToComponentHierarchy (EdgeCollider2D);
	AddToComponentHierarchy (PolygonCollider2D);
	#if ENABLE_SPRITECOLLIDER
		AddToComponentHierarchy (SpriteCollider2D);
	#endif
	AddSeparator ();
	AddToComponentHierarchy (SpringJoint2D);
	AddToComponentHierarchy (DistanceJoint2D);
	AddToComponentHierarchy (HingeJoint2D);
	AddToComponentHierarchy (SliderJoint2D);
	#endif // #if ENABLE_2D_PHYSICS

	AddGroup ("Navigation")
	AddToComponentHierarchy (NavMeshAgent);
	AddToComponentHierarchy (OffMeshLink);
	AddToComponentHierarchy (NavMeshObstacle);
	
	AddGroup ("Audio")
	AddToComponentHierarchy (AudioListener);
	AddToComponentHierarchy (AudioSource);
	AddToComponentHierarchy (AudioReverbZone);

	if (LicenseInfo::Flag (lf_pro_version)
	    || LicenseInfo::Flag (lf_iphone_pro)
	    || LicenseInfo::Flag (lf_android_pro)
        || LicenseInfo::Flag (lf_bb10_pro)
        || LicenseInfo::Flag (lf_tizen_pro))
	{
		AddSeparator ();
		AddToComponentHierarchy (AudioLowPassFilter);
		AddToComponentHierarchy (AudioHighPassFilter);
		AddToComponentHierarchy (AudioEchoFilter);
		AddToComponentHierarchy (AudioDistortionFilter);
		AddToComponentHierarchy (AudioReverbFilter);
		AddToComponentHierarchy (AudioChorusFilter);
	}

	AddGroup ("Rendering")
	// Camera and components that need camera
	AddToComponentHierarchy (Camera);
	AddToComponentHierarchy (Skybox);
	AddToComponentHierarchy (FlareLayer);
	AddToComponentHierarchy (GUILayer);
	AddSeparator ();
	// Lights related
	AddToComponentHierarchy (Light);
	AddToComponentHierarchy (LightProbeGroup);
	AddSeparator ();
	// Optimization
	AddToComponentHierarchy (OcclusionArea);
	AddToComponentHierarchy (OcclusionPortal);
	AddToComponentHierarchy (LODGroup);
	AddSeparator ();
	// 2D related
#if ENABLE_SPRITES
	AddToComponentHierarchy (SpriteRenderer);
	AddSeparator ();
#endif
	// Old old GUI
	AddToComponentHierarchy (GUITexture);
	AddToComponentHierarchy (GUIText);
	
	#define RemoveComponentFromMisc(x) { int removeComponentClassID = Object::StringToClassID (#x); AssertIf (removeComponentClassID == -1); components.erase (removeComponentClassID); }
	RemoveComponentFromMisc (Transform);
	RemoveComponentFromMisc (MonoBehaviour);
	RemoveComponentFromMisc (Component);
	RemoveComponentFromMisc (Pipeline);
	RemoveComponentFromMisc (HaloLayer);
	
	// Although added to a menu above, these would still appear under
	// misc for non-pro licenses if not explicitly removed here.
	RemoveComponentFromMisc (AudioLowPassFilter);
	RemoveComponentFromMisc (AudioHighPassFilter);
	RemoveComponentFromMisc (AudioEchoFilter);
	RemoveComponentFromMisc (AudioDistortionFilter);
	RemoveComponentFromMisc (AudioReverbFilter);
	RemoveComponentFromMisc (AudioChorusFilter);
	
	RemoveComponentFromMisc (RaycastCollider); // Deprecated since 3.0
	RemoveComponentFromMisc (SkinnedMeshRenderer);
	RemoveComponentFromMisc (Tree);
	RemoveComponentFromMisc (ParticleSystemRenderer);

	
	// Add others to Miscellaneous	
	const string misc = "Miscellaneous";
	gComponentHierarchy->push_back (std::make_pair (misc, std::vector<GOComponentDescription> ()));
	for (ClassIDSet::const_iterator i = components.begin ();i != components.end ();i++)
		gComponentHierarchy->back ().second.push_back (GOComponentDescription (Object::ClassIDToString (*i), *i));
	#endif
	
}
