#include "UnityPrefix.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "GizmoManager.h"
#include "GizmoDrawers.h"
#include "Runtime/Misc/DebugUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/NavMesh/NavMeshVisualization.h"
#include "Editor/Src/OcclusionCullingVisualization.h"
#include "Editor/Src/LightProbeVisualization.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Scripting.h"
#include "GizmoRenderer.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

static void AddGizmoRenderersFromAssembly (MonoAssembly* ass);
static void ForwardMonoDrawGizmo (Object& object, int options, void* userData);
static void ForwardMonoBehaviourDrawGizmo (Object& object, int options, void* userData);
static bool CanForwardMonoBehaviourDrawGizmo (Object& object, int options, void* userData);

void RebuildGizmoRenderers ();
static void RegisterGizmoSetup ();

struct MonoGizmoBinding
{
	MonoObject* method;
	MonoObject* type;
	int         options;
};

struct CachedMonoBehaviourGizmo
{
	MonoClass* klass;
	MonoMethod* method;
};

/// We do all typechecking in C# so we don't need to do any here.
static void ForwardMonoDrawGizmo (Object& object, int options, void* userData)
{
	MonoMethod* method = (MonoMethod*)userData;
	MonoObject* mono = Scripting::ScriptingWrapperFor(const_cast<Object*>(&object));
	if (mono)
	{
		gizmos::g_GizmoColor = ColorRGBAf(1.0f,1.0f,1.0f,1.0f);
		gizmos::BeginGizmo( Vector3f::zero );
		ClearGizmoMatrix ();

		void* invokeargs[2] = { Scripting::ScriptingWrapperFor(const_cast<Object*>(&object)), &options };
		MonoException* exception = NULL;
		mono_runtime_invoke_profiled (method, NULL, invokeargs, &exception);
		if (exception)
			Scripting::LogException(exception, (&object)->GetInstanceID());
	}
}

static void ForwardMonoBehaviourDrawGizmo (Object& object, int options, void* userData)
{
	ForwardMonoDrawGizmo (object, options, ((CachedMonoBehaviourGizmo*)userData)->method);
}

/// We do all typechecking in C# so we don't need to do any here.
static bool CanForwardMonoBehaviourDrawGizmo (Object& object, int options, void* userData)
{
	CachedMonoBehaviourGizmo* cached = (CachedMonoBehaviourGizmo*)userData;
	MonoObject* monoObject = Scripting::ScriptingWrapperFor(const_cast<Object*>(&object));
	if (monoObject)
		return mono_class_is_subclass_of (mono_object_get_class(monoObject), cached->klass, false);
	else
		return false;
}

static void AddGizmoRenderersFromAssembly (MonoAssembly* ass)
{
	if (ass == NULL)
		return;
		
	// Use C# to extract all gizmo renderers
	// - C# does all the type checking so we can do a real fast call to draw gizmos at runtime
	void* params[1] = { mono_assembly_get_object (mono_domain_get(), ass) };
	MonoArray* commandArray = (MonoArray*)CallStaticMonoMethod("AttributeHelper", "ExtractGizmos", params);
	if (commandArray == NULL)
		return;
	
	for (int i=0;i<mono_array_length(commandArray);i++)
	{
		MonoGizmoBinding& binding = GetMonoArrayElement<MonoGizmoBinding> (commandArray, i);
		MonoClass* klass = GetScriptingTypeRegistry().GetType(binding.type);
		const char* klassName = mono_class_get_name(klass);
		MonoMethod* method = mono_reflection_method_get_method(binding.method);
		
		if (mono_class_is_subclass_of (klass, GetMonoManager().GetCommonClasses().monoBehaviour, false))
		{
			CachedMonoBehaviourGizmo* cached = new CachedMonoBehaviourGizmo ();
			cached->klass = klass;
			cached->method = method;
			GizmoManager::Get ().AddGizmoRenderer ("MonoBehaviour", ForwardMonoBehaviourDrawGizmo, binding.options, CanForwardMonoBehaviourDrawGizmo, cached);
		}
		else
		{
			GizmoManager::Get ().AddGizmoRenderer (klassName, ForwardMonoDrawGizmo, binding.options, NULL, method);
		}
	}
}

void RebuildGizmoRenderers () {

	GizmoManager& gizmos = GizmoManager::Get ();

	// Cleanup cached gizmo setups
	for (GizmoManager::GizmoSetups::iterator i=gizmos.m_GizmoSetups.begin();i!=gizmos.m_GizmoSetups.end();i++)
	{
		if (i->second.canDrawFunc == CanForwardMonoBehaviourDrawGizmo)	
		{
			CachedMonoBehaviourGizmo* cached = (CachedMonoBehaviourGizmo*)i->second.userData;
			delete cached;
		}
	}
	gizmos.ClearGizmoRenderers();

	gizmos.AddGizmoRenderer ("Light", DrawLightGizmo, GizmoManager::kSelectedOrChild | GizmoManager::kPickable);
	gizmos.AddGizmoRenderer ("AudioReverbZone", DrawAudioReverbZoneGizmo, GizmoManager::kSelectedOrChild | GizmoManager::kPickable);
	gizmos.AddGizmoRenderer ("AudioSource", DrawAudioSourceGizmo, GizmoManager::kSelectedOrChild | GizmoManager::kPickable);
	gizmos.AddGizmoRenderer ("Camera", DrawCameraGizmo, GizmoManager::kSelectedOrChild | GizmoManager::kPickable);
	gizmos.AddGizmoRenderer ("Projector", DrawProjectorGizmo, GizmoManager::kSelectedOrChild | GizmoManager::kPickable);
	gizmos.AddGizmoRenderer ("MonoBehaviour", DrawMonoGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, CanDrawMonoGizmo);
	gizmos.AddGizmoRenderer ("MonoBehaviour", DrawMonoGizmoSelected, GizmoManager::kSelectedOrChild, CanDrawMonoGizmoSelected);
	gizmos.AddGizmoRenderer ("MonoBehaviour", DrawMonoScriptIcon, GizmoManager::kPickable | GizmoManager::kNotSelected, CanDrawMonoScriptIcon, NULL, true);
	gizmos.AddGizmoRenderer ("GameObject", DrawGameObjectIcon, GizmoManager::kPickable | GizmoManager::kNotSelected, CanDrawGameObjectIcon, NULL, true);
	gizmos.AddGizmoRenderer ("WindZone", DrawWindGizmo, GizmoManager::kSelectedOrChild | GizmoManager::kPickable);
	
	
	//@TODO: make wizard gizmos work again!
	//gizmos.AddStaticGizmoRenderer(DrawWizardGizmos);
	gizmos.AddStaticGizmoRenderer(DrawDebugLinesGizmo);
	gizmos.AddStaticGizmoRenderer(DrawNavMeshGizmoImmediate);
    gizmos.AddStaticGizmoRenderer(DrawUmbraLinesGizmoImmediate);
	gizmos.AddStaticGizmoRenderer(DrawLightProbeGizmoImmediate);
	
	gizmos.AddGizmoRenderer ("OcclusionArea", DrawOcclusionAreaGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected );
	gizmos.AddGizmoRenderer ("OcclusionArea", DrawOcclusionAreaGizmoSelected, GizmoManager::kSelectedOrChild );
	gizmos.AddGizmoRenderer ("OcclusionPortal", DrawOcclusionPortal, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild);

	gizmos.AddGizmoRenderer ("Animation", DrawAnimationGizmo, GizmoManager::kSelected);
	gizmos.AddGizmoRenderer ("NavMeshAgent", DrawNavMeshAgentGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("NavMeshObstacle", DrawNavMeshObstacleGizmo, GizmoManager::kSelectedOrChild);
//	gizmos.AddGizmoRenderer ("Animation", DrawAnimationGizmo, GizmoManager::kActive);

	// 2D physics
	#if ENABLE_2D_PHYSICS
	gizmos.AddGizmoRenderer ("BoxCollider2D", Draw2DColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("PolygonCollider2D", Draw2DColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("CircleCollider2D", Draw2DColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("EdgeCollider2D", Draw2DColliderGizmo, GizmoManager::kSelectedOrChild);
		#if ENABLE_SPRITECOLLIDER
		gizmos.AddGizmoRenderer ("SpriteCollider2D", Draw2DColliderGizmo, GizmoManager::kSelectedOrChild);
		#endif
	#endif // #if ENABLE_2D_PHYSICS

	
	/// Icon gizmos
	gizmos.AddGizmoRenderer ("Light", DrawPickGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, HasPickGizmo, NULL, true);
	gizmos.AddGizmoRenderer ("Camera", DrawPickGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, HasPickGizmo, NULL, true);
	gizmos.AddGizmoRenderer ("AudioSource", DrawPickGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, HasPickGizmo, NULL, true);
	gizmos.AddGizmoRenderer ("LensFlare", DrawPickGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, HasPickGizmo, NULL, true);
	gizmos.AddGizmoRenderer ("Projector", DrawPickGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, HasPickGizmo, NULL, true);	
	gizmos.AddGizmoRenderer ("WindZone", DrawPickGizmo, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, HasPickGizmo, NULL, true);	
	gizmos.AddGizmoRenderer ("ParticleSystem", DrawParticleSystemIcon, GizmoManager::kPickable | GizmoManager::kNotSelected | GizmoManager::kSelectedOrChild, CanDrawParticleSystemIcon, NULL, true);	

	
	#if 0
	// Temporary visualizition of bounding volumes for debugging purposes.
	gizmos.AddGizmoRenderer ("Renderer", DrawDebugRendererBoundsGizmos, GizmoManager::kNotSelected | GizmoManager::kAddDerivedClasses);
	gizmos.AddGizmoRenderer ("SkinnedMeshRenderer", DrawDebugRendererBonesBoundsGizmos, GizmoManager::kNotSelected | GizmoManager::kAddDerivedClasses);
	#endif

	GlobalCallbacks::Get().registerGizmos.Invoke();
	
	// Setup mono gizmos
	for (int i=MonoManager::kEditorAssembly;i<GetMonoManager().GetAssemblyCount();i++)
	{
		if (GetMonoManager().GetAssembly(i))
			AddGizmoRenderersFromAssembly (GetMonoManager().GetAssembly(i));
	}
}

static void RegisterGizmoSetup ()
{
	GlobalCallbacks::Get().didReloadMonoDomain.Register(RebuildGizmoRenderers);
	RebuildGizmoRenderers ();
}

STARTUP (RegisterGizmoSetup)
