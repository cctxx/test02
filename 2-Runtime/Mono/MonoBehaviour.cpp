#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "MonoBehaviour.h"
#include "MonoBehaviourAnimationBinding.h"
#include "Runtime/Mono/Coroutine.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Scripting/ScriptingObjectWithIntPtrField.h"
#include "MonoScript.h"
#include "MonoScriptCache.h"
#include "MonoTypeSignatures.h"
#include "MonoManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/BaseClasses/MessageIdentifiers.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "tabledefs.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Misc/MessageParameters.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Renderable.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Misc/AsyncOperation.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Threads/ThreadSpecificValue.h"
#include "Runtime/Audio/AudioCustomFilter.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Scripting/Backend/ScriptingArguments.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Physics2D/CollisionListener2D.h"
#include "Runtime/Interfaces/IPhysics.h"

#if ENABLE_WWW
#include "Runtime/Export/WWW.h"
#endif
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/BaseClasses/RefCounted.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#if UNITY_EDITOR
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/AssetPipeline/LogicGraphCompilationPipeline.h"
#endif

#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Scripting.h"

#include "Runtime/Interfaces/IAudio.h"

IMPLEMENT_CLASS_HAS_INIT (MonoBehaviour)

using namespace std;
#define DEBUG_COROUTINE 0
#define DEBUG_COROUTINE_LEAK 0
#if DEBUG_COROUTINE_LEAK
int gCoroutineCounter = 0;
#endif


PROFILER_INFORMATION(gMonoImageFxProfile, "Camera.ImageEffect", kProfilerRender);


bool IsInstanceValid (ScriptingObjectPtr target);

#if UNITY_EDITOR
BackupState::BackupState ()
:	yamlState(NULL)
,	inYamlFormat (false)
,	loadedFromDisk (false)
{}

BackupState::~BackupState ()
{
	delete yamlState;
}

#endif

MonoBehaviour::MonoBehaviour (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_UpdateNode(this)
,	m_FixedUpdateNode(this)
,	m_LateUpdateNode(this)
,	m_GUINode(this)
,	m_OnRenderObjectNode(this)
{
	m_Methods = NULL;
	m_ScriptCache = NULL;
	m_DidAwake = m_DidStart = m_IsDestroying = false;
	m_UseGUILayout = true;
	m_GUIState = NULL;
	#if UNITY_EDITOR
	m_EditorHideFlags = 0;
	m_Backup = NULL;
	#endif
	#if ENABLE_AUDIO_FMOD 
	m_AudioCustomFilter = NULL;
	#endif
}

MonoBehaviour::~MonoBehaviour()
{
	Assert(m_ActiveCoroutines.empty());

	LockObjectCreation();
	ReleaseMonoInstance ();
	StopAllCoroutines ();
	UnlockObjectCreation();
	
#if UNITY_EDITOR
	delete m_Backup;
#endif
#if ENABLE_UNITYGUI
	delete m_GUIState;
#endif

#if ENABLE_AUDIO_FMOD
	delete m_AudioCustomFilter;
#endif
}

bool MonoBehaviour::IsScriptableObject()
{
	if (m_ScriptCache != NULL)
		return m_ScriptCache->scriptType == kScriptTypeScriptableObjectDerived || m_ScriptCache->scriptType == kScriptTypeEditorScriptableObjectDerived;
	else
		return false;
}

#if ENABLE_UNITYGUI
ObjectGUIState& MonoBehaviour::GetObjectGUIState()
{
	if (m_GUIState)
		return *m_GUIState;
	else
	{
		m_GUIState = new ObjectGUIState ();
		return *m_GUIState;
	}
}
#endif

bool MonoBehaviour::IsPlayingOrAllowExecuteInEditMode() const
{
	#if UNITY_EDITOR
	return IsWorldPlaying () || GetRunInEditMode();
	#else
	return true;
	#endif
}

bool MonoBehaviour::WillUnloadScriptableObject ()
{
	MonoScript* script = m_Script;
	if (GetInstance() == SCRIPTING_NULL || script == NULL)
		return true;
	
	///@TODO: SHould this check for m_DidAwake? Can this happen in any way?
	
	ScriptingObjectPtr instance = GetInstance();
	if (IsScriptableObject())
	{
		ScriptingMethodPtr method = m_Methods[MonoScriptCache::kRemoveFromManager];
		if (method != SCRIPTING_NULL)
			CallMethodInactive(method);
		
		if (IsInstanceValid(instance))
		{
			method = m_Methods[MonoScriptCache::kRemoveFromManagerInternal];
      		if (method != SCRIPTING_NULL)
				CallMethodInactive(method);
		}
	}
	return IsInstanceValid(instance);
}


// IsInstanceValid is used to determine if the C# side destroyed the object during the script callback using DestroyImmediate.
// For example Awake and OnEnable are called from the same C++ code. If the object is destroyed by Awake, we must not call OnEnable and not touch MonoBehaviour member variables.
bool IsInstanceValid (ScriptingObjectPtr target)
{
	if (target == SCRIPTING_NULL)
		return false;
	ScriptingObjectOfType<Object> wrapper(target);
	return wrapper.GetCachedPtr() != NULL;
}

void MonoBehaviour::DeprecatedAddToManager ()
{
	ScriptingObjectPtr instance = GetInstance();
	if (instance && IsPlayingOrAllowExecuteInEditMode ())
	{
		MonoScript* script = m_Script;
		int executionOrder = script ? script->GetExecutionOrder() : 0;
		///Try removing this
		if ((IsInstanceValid (instance) && m_Methods[MonoScriptCache::kCoroutineStart]) || m_Methods[MonoScriptCache::kCoroutineMain])
			CallDelayed (DelayedStartCall, this, -10, NULL, 0.0F, NULL, DelayedCallManager::kRunDynamicFrameRate | DelayedCallManager::kRunFixedFrameRate | DelayedCallManager::kRunStartupFrame);
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kUpdate])
			GetBehaviourManager().AddBehaviour (m_UpdateNode, executionOrder);
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kFixedUpdate])
			GetFixedBehaviourManager().AddBehaviour (m_FixedUpdateNode, executionOrder);
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kLateUpdate])
			GetLateBehaviourManager().AddBehaviour (m_LateUpdateNode, executionOrder);
		
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kRenderObject])
			GetRenderManager().AddOnRenderObject (m_OnRenderObjectNode);
		
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kAddToManager])
		{
			// @TODO: This is only for backwards compatiblity
			SetupAwake ();
			
			// If we got disabled or destroyed from a call to Awake, do not proceed calling OnEnable and so on.
			// Just quit, we are removed from all managers by a call to RemoveFromManager already.
			if( !IsInstanceValid (instance) ||  !GetEnabled() )
				return;
			CallMethodIfAvailable (MonoScriptCache::kAddToManager);
		}
		
#if ENABLE_IMAGEEFFECTS
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kRenderImageFilter])
		{
			Camera *camera = QueryComponent (Camera);
			if (camera)
			{
				bool afterOpaque = scripting_method_has_attribute(m_Methods[MonoScriptCache::kRenderImageFilter], MONO_COMMON.imageEffectOpaque);
				bool transformsToLDR = scripting_method_has_attribute(m_Methods[MonoScriptCache::kRenderImageFilter], MONO_COMMON.imageEffectTransformsToLDR);
				
				ImageFilter filter (this, &RenderImageFilter, transformsToLDR, afterOpaque);
				camera->AddImageFilter (filter);
			}
		}
#endif
		
#if ENABLE_UNITYGUI
		if (IsInstanceValid (instance) && m_Methods[MonoScriptCache::kGUI])
			GetGUIManager().AddGUIScript (m_GUINode);
#endif
		
		if ( IsInstanceValid (instance) )
			SetByPassOnDSP(false);
	}
}

void MonoBehaviour::WillDestroyComponent ()
{
	Super::WillDestroyComponent();

	if (m_IsDestroying)
	{
		ErrorString("DestroyImmediate should not be called on the same game object when destroying a MonoBehaviour");
		return;
	}
	
	m_IsDestroying = true;
	ScriptingMethodPtr method;
	
	ScriptingObjectPtr instance = GetInstance();
	if (instance && m_DidAwake)
	{
		// OnDisable must be called for scriptable objects since we never get a Deactivate call there.
		if (IsScriptableObject())
		{
			method = m_Methods[MonoScriptCache::kRemoveFromManager];
			if (method)
				CallMethodInactive (method);
			
			if (IsInstanceValid(instance))
			{
				method = m_Methods[MonoScriptCache::kRemoveFromManagerInternal];
				if (method)
					CallMethodInactive (method);
			}
		}

		// OnDestroy
		if (IsInstanceValid(instance))
		{
			method = m_Methods[MonoScriptCache::kOnDestroy];
			if (method != SCRIPTING_NULL)
				CallMethodInactive(method);
		}
	}
}


void MonoBehaviour::RemoveFromManager ()
{
	m_UpdateNode.RemoveFromList();
	m_FixedUpdateNode.RemoveFromList();
	m_LateUpdateNode.RemoveFromList();
	m_GUINode.RemoveFromList();
	m_OnRenderObjectNode.RemoveFromList();
	
#if ENABLE_IMAGEEFFECTS
	if (GetInstance() && m_Methods[MonoScriptCache::kRenderImageFilter])
	{
		Camera *camera = QueryComponent (Camera);
		if (camera)
		{
			ImageFilter filter (this, &RenderImageFilter, false, false);
			camera->RemoveImageFilter (filter);
		}
	}
#endif
	
	if (IsPlayingOrAllowExecuteInEditMode() && GetCachedScriptingObject()) 
	{
		ScriptingObjectPtr instance = GetInstance();
		
		if (IsInstanceValid(instance) && m_Methods[MonoScriptCache::kRemoveFromManager] && m_DidAwake)
			CallMethodInactive(m_Methods[MonoScriptCache::kRemoveFromManager]);
		if (IsInstanceValid(instance) && m_Methods[MonoScriptCache::kRemoveFromManagerInternal] && m_DidAwake)
			CallMethodInactive(m_Methods[MonoScriptCache::kRemoveFromManagerInternal]);
		//@TODO: Try removing this
		if (IsInstanceValid(instance) && (m_Methods[MonoScriptCache::kCoroutineStart] || m_Methods[MonoScriptCache::kCoroutineMain]))
			GetDelayedCallManager().CancelCallDelayed (this, DelayedStartCall, NULL, NULL);
		
		if ( IsInstanceValid(instance) )
			SetByPassOnDSP(true);
	}	
}	

bool MonoBehaviour::CallMethodInactive (ScriptingMethodPtr method)
{
	AssertIf (GetInstance() == SCRIPTING_NULL);
	AssertIf (method == SCRIPTING_NULL);

	ScriptingObjectPtr instance = GetInstance();
	ScriptingInvocation invocation(method);
	invocation.object = instance;
	invocation.logException = true;
	invocation.objectInstanceIDContextForException = Scripting::GetInstanceIDFromScriptingWrapper(instance);
	invocation.AdjustArgumentsToMatchMethod();
	invocation.InvokeChecked();
	return (NULL == invocation.exception);
}

bool MonoBehaviour::CallMethodInactive (const char* methodName)
{
	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetClass(),methodName,ScriptingMethodRegistry::kWithoutArguments);

	if (!method)
		return false;

	CallMethodInactive (method);
	
	return true;
}

bool MonoBehaviour::DoGUI (MonoBehaviour::GUILayoutType layoutType, int skin)
{
	if (GetInstance () == SCRIPTING_NULL)
		return false;
	ScriptingMethodPtr method = GetMethod (MonoScriptCache::kGUI);
	if (method == SCRIPTING_NULL)
		return false;
	Start ();

	PPtr<MonoBehaviour> behaviourPPtr = this;
	ScriptingObjectPtr monoObject = GetInstance ();
	int instanceID = GetInstanceID();
	GUIState &guiState = GetGUIState ();
	guiState.m_CanvasGUIState.m_GUIClipState.BeginOnGUI (*guiState.m_CurrentEvent);

	guiState.BeginOnGUI(GetObjectGUIState());

	int allowGUILayoutParam = layoutType;
	
	ScriptingInvocation invocation;
	invocation.AddInt(skin);
	invocation.AddInt(instanceID);
	invocation.AddInt(allowGUILayoutParam);
	invocation.method = MONO_COMMON.beginGUI;
	invocation.Invoke();

#if UNITY_EDITOR
	ScriptingInvocation invocation2;
	invocation2.method = MONO_COMMON.clearUndoSnapshotTarget;
	invocation2.Invoke();
#endif
	
	ScriptingExceptionPtr exception = NULL;
	ScriptingInvocationNoArgs userOnGUIInvocation;
	userOnGUIInvocation.method = method;
	userOnGUIInvocation.object = monoObject;
	userOnGUIInvocation.logException = false;
	userOnGUIInvocation.Invoke(&exception);

	// Display exception (except for ExitGUIException which exists only to successfully exit out of a GUI loop)
	if (exception)
	{
#if ENABLE_MONO
		void* excparams[] = {exception}; 
		MonoObject* res = CallStaticMonoMethod("GUIUtility", "EndGUIFromException", excparams);
		guiState.m_CanvasGUIState.m_GUIClipState.EndThroughException();
		if (MonoObjectToBool(res))
		{
			guiState.EndOnGUI ();
			return guiState.m_CurrentEvent->type == InputEvent::kUsed;
		}
		else
		{
			guiState.EndOnGUI ();
			::Scripting::LogException(exception, behaviourPPtr.GetInstanceID());
			return false;
		}
#endif
	}
	else
	{	
		// Mono is not happy about receiving bools from internal calls,
		// especially since bools are not exactly specified on how many bytes they contain on the C++ side
		
		ScriptingInvocation endGUIInvocation;
		endGUIInvocation.method = MONO_COMMON.endGUI;
		endGUIInvocation.AddInt((int)layoutType);
		endGUIInvocation.Invoke();

		guiState.EndOnGUI ();
		guiState.m_CanvasGUIState.m_GUIClipState.EndOnGUI (*guiState.m_CurrentEvent);
	}
	return guiState.m_CurrentEvent->type == InputEvent::kUsed;
}

#if 0
// IM GUI in retained GUI callbacks. not used right now.
bool MonoBehaviour::DoCustomGUI (Rectf &position, const ColorRGBAf &guiColor)
{
	MonoObject *instance = GetInstance ();
	MonoMethod* method = GetMethod(MonoScriptCache::kCustomGUI);
	if (instance == NULL || method == NULL)
		return false;
	Start ();
	
	GUIState &guiState = GetGUIState ();
	guiState.SetObjectGUIState (GetObjectGUIState());
	guiState.BeginOnGUI();
	guiState.m_OnGUIState.m_Color = guiColor;
	
	MonoException* exception;
	
	void* params[] = { &position };	
	bool retval = mono_runtime_invoke_profiled(method, instance, params, &exception);

	// TODO: Handle GUI exceptions
	AssertIf (exception);
	
	guiState.EndOnGUI ();
	return retval;
}
#endif

void MonoBehaviour::SetUseGUILayout (bool use)
{
	m_UseGUILayout = use;
}

bool MonoBehaviour::GetUseGUILayout ()
{
	return m_UseGUILayout;
}

#if ENABLE_AUDIO_FMOD

FMOD::DSP* MonoBehaviour::GetOrCreateDSP() 
{	
	IAudio* audio = GetIAudio();
	if (!audio)
		return NULL;

	if (!m_AudioCustomFilter)
	{
	if (m_Methods && m_Methods[MonoScriptCache::kAudioFilterRead] && IsActive())
			m_AudioCustomFilter = audio->CreateAudioCustomFilter(this);
	else
		return NULL;
}

	return audio->GetOrCreateDSPFromCustomFilter(m_AudioCustomFilter);
}

FMOD::DSP* MonoBehaviour::GetDSP() const
{	
	IAudio* audio = GetIAudio();
	if (!audio)
		return NULL;

	if (m_AudioCustomFilter)
		return audio->GetDSPFromAudioCustomFilter(m_AudioCustomFilter);
	else
		return NULL;
}
#endif

bool MonoBehaviour::HaveAudioCallback() const
{ 
	return m_Methods ? m_Methods[MonoScriptCache::kAudioFilterRead] != SCRIPTING_NULL : false; 
}

inline void MonoBehaviour::CallMethodIfAvailable (int methodIndex)
{
	AssertIf (methodIndex < 0 || methodIndex >= MonoScriptCache::kMethodCount);
	ScriptingMethodPtr method = m_Methods[methodIndex];
	if (method == SCRIPTING_NULL)
	{
		return;
	}

	AssertIf (GetInstance() == SCRIPTING_NULL);
	AssertIf (!m_DidAwake);

	if (!IsActive ())
		return;
	
	ScriptingInvocationNoArgs invocation(method);
	invocation.objectInstanceIDContextForException = GetInstanceID();
	invocation.object = GetInstance();
	invocation.Invoke();
}

inline void MonoBehaviour::Start ()
{
	DebugAssertIf (!m_DidAwake);

	
	#if UNITY_EDITOR
	AssertIf (!IsActive () && !GetRunInEditMode());
	#else
	AssertIf (!IsActive ());
	#endif
	
	if (m_DidStart)
		return;

	AssertIf (GetInstance() == SCRIPTING_NULL);
	
	m_DidStart = true;
	
	ScriptingMethodPtr method;
	method = m_Methods[MonoScriptCache::kCoroutineMain];
	
	if (method)
		InvokeMethodOrCoroutineChecked (method, SCRIPTING_NULL);
	
	method = m_Methods[MonoScriptCache::kCoroutineStart];
	if (method)
		InvokeMethodOrCoroutineChecked (method, SCRIPTING_NULL);
}


#if UNITY_EDITOR
void MonoBehaviour::RestartExecuteInEditModeScripts ()
{
	std::vector<SInt32> behaviours;
	Object::FindAllDerivedObjects (ClassID(MonoBehaviour), &behaviours);
	
	for (std::vector<SInt32>::iterator it = behaviours.begin(), itEnd = behaviours.end(); it != itEnd; ++it)
	{
		MonoBehaviour* beh = dynamic_pptr_cast<MonoBehaviour*> (Object::IDToPointer(*it));
		if (beh == NULL || !beh->GetRunInEditMode() || !beh->IsActive() || !beh->GetEnabled())
			continue;
		
		// disable and enable the script (careful to not call SetDirty)
		beh->SetEnabledNoDirty (false);
		beh->SetEnabledNoDirty (true);
		// and Start it again
		beh->m_DidStart = false;
		beh->Start ();
	}
}

bool MonoBehaviour::GetRunInEditMode () const
{
	if (m_ScriptCache)
		return m_ScriptCache->runInEditMode;
	else
		return false;
}

#endif


void MonoBehaviour::CallUpdateMethod(int methodIndex)
{
	AssertIf (!IsPlayingOrAllowExecuteInEditMode ());
	AssertIf (!IsActive ());
	AssertIf (!GetEnabled ());
	ScriptingObjectPtr instance = GetInstance();
	
	if (instance == SCRIPTING_NULL)
		return;
	// Ensure Start has been called
	Start ();
	
	// We might be getting destroyed in Start
	if (!IsInstanceValid(instance))
		return;

	// Call Update
	CallMethodIfAvailable (methodIndex);
}

void MonoBehaviour::Update ()
{
	CallUpdateMethod (MonoScriptCache::kUpdate);
}

void MonoBehaviour::LateUpdate ()
{
	CallUpdateMethod (MonoScriptCache::kLateUpdate);
}

void MonoBehaviour::FixedUpdate ()
{
	CallUpdateMethod (MonoScriptCache::kFixedUpdate);
}

ScriptingMethodPtr MonoBehaviour::FindMethod (const char* name)
{
	if (GetInstance() == SCRIPTING_NULL)
		return SCRIPTING_NULL;
	
	const MethodCache* cache = GetMethodCache();
	
	MethodCache::const_iterator i = cache->find (name);
	if (i != cache->end ())
		return i->second;
	else
		return SCRIPTING_NULL;
}


#if ENABLE_MONO || UNITY_WINRT
void MonoBehaviour::InvokeOnRenderObject ()
{
	DebugAssertIf (!IsPlayingOrAllowExecuteInEditMode());
	AssertIf (!GetEnabled ());
	AssertIf (!IsActive ());
	if (GetInstance())
	{
		Start ();
		CallMethodIfAvailable (MonoScriptCache::kRenderObject);
	}
}
#endif //ENABLE_MONO || UNITY_WINRT

#if ENABLE_IMAGEEFFECTS
void MonoBehaviour::RenderImageFilter (Unity::Component* component, RenderTexture *source, RenderTexture *destination)
{
	MonoBehaviour* self = static_cast<MonoBehaviour*>(component);

	AssertIf (!self->GetEnabled ());
	AssertIf (!self->IsActive ());
	if (self->IsPlayingOrAllowExecuteInEditMode () && self->GetInstance())
	{
		self->Start ();

		ScriptingMethodPtr method = self->m_Methods[MonoScriptCache::kRenderImageFilter];
		// Early out message not supported
		if (method == SCRIPTING_NULL)
			return;
		
		PROFILER_AUTO_GFX(gMonoImageFxProfile, self)

		ScriptingObjectPtr instance = self->GetInstance();
		ScriptingInvocation invocation(method);
		invocation.object = instance;
		invocation.AddObject(Scripting::ScriptingWrapperFor (source));
		invocation.AddObject(Scripting::ScriptingWrapperFor (destination));
		invocation.objectInstanceIDContextForException = self->GetInstanceID();
		invocation.Invoke();
	}
}
#endif // ENABLE_IMAGEEFFECTS



#if ENABLE_MONO
static MonoObject* ConvertBuiltinMonoValue( MonoObject* value, int wantedType )
{
	#define SUPPORT_CONVERSION(srctype,dsttype,srccode,dstclass) \
		case srccode: \
			res = mono_object_new( domain, classes.dstclass ); \
			ExtractMonoObjectData<dsttype>(res) = static_cast<dsttype>( ExtractMonoObjectData<srctype>(value) ); \
			return res
	
	MonoClass* valueClass = mono_object_get_class (value);
	int type = mono_type_get_type( mono_class_get_type (valueClass) );
	// types match exactly - return input object
	if( type == wantedType )
		return value;
	
	MonoDomain* domain = mono_domain_get();
	const CommonScriptingClasses& classes = GetMonoManager().GetCommonClasses();
	MonoObject* res;
	
	if( wantedType == MONO_TYPE_I4 )
	{
		switch( type ) {
			SUPPORT_CONVERSION(float, int,MONO_TYPE_R4,int_32);
			SUPPORT_CONVERSION(double,int,MONO_TYPE_R8,int_32);
		}
	}
	else if( wantedType == MONO_TYPE_R4 )
	{
		switch( type ) {
			SUPPORT_CONVERSION(int,   float,MONO_TYPE_I4,floatSingle);
			SUPPORT_CONVERSION(double,float,MONO_TYPE_R8,floatSingle);
		}
	}
	else if( wantedType == MONO_TYPE_R8 )
	{
		switch( type ) {
			SUPPORT_CONVERSION(int,  double,MONO_TYPE_I4,floatDouble);
			SUPPORT_CONVERSION(float,double,MONO_TYPE_R4,floatDouble);
		}
	}
		
	#undef SUPPORT_CONVERSION
	
	// can't convert
	return NULL;
}
#endif //ENABLE_MONO


static bool CompareCoroutine (void* callBackUserData, void* cancelUserdata)
{
	return callBackUserData == cancelUserdata;
}

Coroutine* MonoBehaviour::CreateCoroutine(ScriptingObjectPtr userCoroutine, ScriptingMethodPtr method)
{
	ScriptingMethodPtr moveNext = scripting_object_get_virtual_method(userCoroutine, MONO_COMMON.IEnumerator_MoveNext, GetScriptingMethodRegistry());

#if !UNITY_FLASH
	ScriptingMethodPtr current = scripting_object_get_virtual_method(userCoroutine, MONO_COMMON.IEnumerator_Current, GetScriptingMethodRegistry());
#else
	//todo: make flash use generic path. set a bogus value for flash here right now so it passes current != NULL check,  flash path will never use this value for now.
	ScriptingMethodPtr current = (ScriptingMethodPtr)1;
#endif

	if (current == SCRIPTING_NULL || moveNext == SCRIPTING_NULL)
	{
		std::string message = (method != SCRIPTING_NULL) ? Format ("Coroutine '%s' couldn't be started!", scripting_method_get_name(method)) : "Coroutine couldn't be started!";
		LogStringObject (message, this);
		return NULL;
	}
	
	Coroutine* coroutine = new Coroutine ();
	
	coroutine->m_CoroutineEnumeratorGCHandle = scripting_gchandle_new (userCoroutine);
	coroutine->m_CoroutineEnumerator = userCoroutine;
	coroutine->m_CoroutineMethod = method;
	coroutine->SetMoveNextMethod(moveNext);
	coroutine->SetCurrentMethod(current);
	coroutine->m_Behaviour = this;
	coroutine->m_ContinueWhenFinished = NULL;
	coroutine->m_WaitingFor = NULL;
	coroutine->m_AsyncOperation = NULL;
	coroutine->m_RefCount = 1;
	coroutine->m_IsReferencedByMono = 0;
	#if DEBUG_COROUTINE
	printf_console ("Allocate coroutine %d\n", coroutine);
	AssertIf(GetDelayedCallManager().HasDelayedCall(coroutine->m_Behaviour, Coroutine::ContinueCoroutine, CompareCoroutine, coroutine));
	#endif

	#if DEBUG_COROUTINE_LEAK			
	printf_console ("Active coroutines %d\n", gCoroutineCounter);
	gCoroutineCounter++;
	#endif

	m_ActiveCoroutines.push_back (*coroutine);
	AssertIf(&m_ActiveCoroutines.back() != coroutine);
	m_ActiveCoroutines.back ().Run ();

	AssertIf(coroutine->m_RefCount == 0);
	if (coroutine->m_RefCount <= 1)
	{
		Coroutine::CleanupCoroutine(coroutine);
		return NULL;
	}
	
	Coroutine::CleanupCoroutine(coroutine);
	return coroutine;				
}


static ScriptingObjectPtr CreateManagedWrapperForCoroutine(Coroutine* coroutine)
{
	if (coroutine == NULL) return SCRIPTING_NULL;
	Assert(!coroutine->m_IsReferencedByMono);
	coroutine->m_IsReferencedByMono = true;
	ScriptingObjectWithIntPtrField<Coroutine> wrapper = scripting_object_new(GetMonoManager ().GetCommonClasses ().coroutine);
	wrapper.SetPtr(coroutine, Coroutine::CleanupCoroutineGC);
	return wrapper.object;
}

ScriptingObjectPtr MonoBehaviour::StartCoroutineManaged (const char* name, ScriptingObjectPtr value)
{
	Coroutine* coroutine = StartCoroutine (name, value);
	return CreateManagedWrapperForCoroutine(coroutine);
}

ScriptingObjectPtr MonoBehaviour::StartCoroutineManaged2 (ScriptingObjectPtr enumerator)
{
	if (!IsActive ())
	{
		ErrorStringObject (Format ("Coroutine couldn't be started because the the game object '%s' is inactive!", GetName()), this);
		return SCRIPTING_NULL;
	}
	
	Coroutine* coroutine = CreateCoroutine(enumerator, SCRIPTING_NULL);
	return CreateManagedWrapperForCoroutine(coroutine);
}

static bool DoesMethodHaveIEnumeratorReturnType(ScriptingMethodPtr method)
{
	ScriptingClassPtr iEnumerator = GetMonoManager ().GetCommonClasses ().iEnumerator;
	ScriptingClassPtr returnType = scripting_method_get_returntype(method, GetScriptingTypeRegistry());
	return returnType == iEnumerator;
}

Coroutine* MonoBehaviour::HandleCoroutineReturnValue (ScriptingMethodPtr method, ScriptingObjectPtr returnValue)
{
	if (!DoesMethodHaveIEnumeratorReturnType(method))
		return NULL;

	return CreateCoroutine(returnValue, method);
}

ScriptingObjectPtr MonoBehaviour::InvokeMethodOrCoroutineChecked(ScriptingMethodPtr scriptingMethod, ScriptingObjectPtr value, ScriptingExceptionPtr* exception)
{
	#define PARAMETER_ERROR(x) \
	{ \
		string error = Format ("Failed to call function %s of class %s\n", scripting_method_get_name (scriptingMethod), GetScript ()->GetScriptClassName ().c_str ()); \
		error += x; \
		ErrorStringObject (error, this); \
		return SCRIPTING_NULL; \
	}
#if ENABLE_MONO


	MonoObject* instance = GetInstance();
	int argCount = scripting_method_get_argument_count (scriptingMethod, GetScriptingTypeRegistry());

	// Fast path - method takes no arguments
	if (argCount == 0)
		return mono_runtime_invoke_profiled_fast(scriptingMethod, instance, exception, NULL);

	// One argument, one value
	if (argCount != 1 || value == SCRIPTING_NULL)
	{
		if (value == NULL)
		{
			PARAMETER_ERROR (Format ("Calling function %s with no parameters but the function requires %d.", scripting_method_get_name (scriptingMethod), argCount));
		}
		else
		{
			PARAMETER_ERROR (Format ("Calling function %s with 1 parameter but the function requires %d.", scripting_method_get_name (scriptingMethod), argCount));
		}

		return NULL;
	}

	MonoClass* inParamClass = mono_object_get_class (value);

	void* iterator = NULL;

	MonoMethodSignature* sig = mono_method_signature (scriptingMethod->monoMethod);
	MonoType* methodType = mono_signature_get_params (sig, &iterator);
	MonoClass* methodClass = mono_class_from_mono_type (methodType);
	int methodTypeType = mono_type_get_type (methodType);

	void* argumentList[1] = { NULL };
		
	// Pass builtin type
	if (IsMonoBuiltinType (methodTypeType))
	{
		MonoObject* converted = ConvertBuiltinMonoValue(value, methodTypeType);
		if( converted )
		{
			argumentList[0] = ExtractMonoObjectDataPtr<void> (converted);
		}
	} 
	// Pass by value
	else if (methodTypeType == MONO_TYPE_VALUETYPE)
	{
		if (inParamClass == methodClass)
		{
			argumentList[0] = ExtractMonoObjectDataPtr<void> (value);
		}
	}
	// Pass by class
	else if (methodTypeType == MONO_TYPE_CLASS)
	{
		if (mono_class_is_subclass_of (inParamClass, methodClass, false))
		{
			argumentList[0] = value;
		}
	}
	// Passing string
	else if (methodTypeType == MONO_TYPE_STRING && methodTypeType == mono_type_get_type (mono_class_get_type (inParamClass)))
	{
		argumentList[0] = value;
	}
	else if (methodTypeType == MONO_TYPE_OBJECT)
	{
		argumentList[0] = value;
	}

	if (argumentList[0])
		return mono_runtime_invoke_profiled (scriptingMethod->monoMethod, instance, argumentList, exception);

	void* invokeargs[3] = { instance, mono_string_new_wrapper (scripting_method_get_name (scriptingMethod)), value };
	return mono_runtime_invoke_profiled (GetMonoManager().GetCommonClasses().invokeMember->monoMethod, NULL, invokeargs, exception);
	
#else
	// ToDo: make full params type check, like we do it with mono
	int argCount = scripting_method_get_argument_count(scriptingMethod, GetScriptingTypeRegistry());
	ScriptingInvocation invocation(scriptingMethod);
	invocation.object = GetInstance();

	if (argCount == 0)
		return invocation.Invoke(exception);

	if (argCount != 1 || value == SCRIPTING_NULL)
	{
		if (value == SCRIPTING_NULL)
		{
			PARAMETER_ERROR (Format ("Calling function %s with no parameters but the function requires %d.", scripting_method_get_name (scriptingMethod), argCount));
		}
		else
		{
			PARAMETER_ERROR (Format ("Calling function %s with 1 parameter but the function requires %d.", scripting_method_get_name (scriptingMethod), argCount));
		}
		return SCRIPTING_NULL;
	}

#if UNITY_WP8
	ScriptingInvocation invokeMember(MONO_COMMON.invokeMember);
	invokeMember.AddObject(GetInstance());
	invokeMember.AddString(scripting_method_get_name(scriptingMethod));
	invokeMember.AddObject(value);
	return invokeMember.Invoke(exception);
#else
	invocation.AddObject(value);
	return invocation.Invoke(exception, true);
#endif

#endif
	#undef PARAMETER_ERROR
}

Coroutine* MonoBehaviour::InvokeMethodOrCoroutineChecked (ScriptingMethodPtr method, ScriptingObjectPtr value)
{
	
	AssertIf (!IsPlayingOrAllowExecuteInEditMode ());
	
	ScriptingObjectPtr instance = GetInstance();
	AssertIf (instance == SCRIPTING_NULL);
	
	ScriptingExceptionPtr exception = NULL;
	ScriptingObjectPtr returnValue = InvokeMethodOrCoroutineChecked(method,value,&exception);
	
	if (returnValue != SCRIPTING_NULL && exception == NULL)
		return HandleCoroutineReturnValue (method, returnValue);
	
	if (exception != NULL)
		Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(instance));
	
	return NULL;
}

Coroutine* MonoBehaviour::StartCoroutine (const char* name, ScriptingObjectPtr value)
{
	AssertIf (!IsPlayingOrAllowExecuteInEditMode ());
	AssertIf (GetInstance() == SCRIPTING_NULL);

	if (!IsActive ())
	{
		ErrorStringObject (Format ("Coroutine '%s' couldn't be started because the the game object '%s' is inactive!", name, GetName()), this);
		return NULL;
	}
	
	ScriptingMethodPtr method = FindMethod (name);
	if (method == NULL)
	{
		ErrorStringObject (Format ("Coroutine '%s' couldn't be started!", name), this);
		return NULL;
	}

	return InvokeMethodOrCoroutineChecked (method, value);
}

static void DoStopCoroutine (Coroutine *coroutine)
{
	coroutine->RemoveFromList ();

	// We clear the behaviour reference prior to cleaning up the coroutine because otherwise CleanupCoroutine might remove
	// it from underneath m_ActiveCoroutines
	coroutine->m_Behaviour = NULL;

	if (coroutine->m_WaitingFor)
	{
		AssertIf (coroutine->m_WaitingFor->m_ContinueWhenFinished != coroutine);
		coroutine->m_WaitingFor->m_ContinueWhenFinished = NULL;
		coroutine->m_WaitingFor = NULL;
		Coroutine::CleanupCoroutine (coroutine);
	}
	else if (coroutine->m_AsyncOperation)
		Coroutine::CleanupCoroutine (coroutine);
}

void MonoBehaviour::StopCoroutine (const char* name)
{
	#if DEBUG_COROUTINE
	printf_console("StopCoroutine %s. %d - %s", name, this, GetName());
	#endif
	GetDelayedCallManager ().CancelCallDelayed (this, Coroutine::ContinueCoroutine, Coroutine::CompareCoroutineMethodName, (void*)name);
	for (List<Coroutine>::iterator i = m_ActiveCoroutines.begin ();
	     i != m_ActiveCoroutines.end (); ++i) {
		if (i->m_CoroutineMethod != NULL && !strcmp (name, scripting_method_get_name (i->m_CoroutineMethod))) {
			DoStopCoroutine (&(*i));
			break;
		}
	}
}

void MonoBehaviour::StopAllCoroutines ()
{
	if(m_ActiveCoroutines.empty ())
	{
		return;
	}

	#if DEBUG_COROUTINE
	printf_console("StopAllCoroutines! %d - %s", this, GetName());
	#endif
	
	DelayedCall* wwwCallback = NULL;
	#if ENABLE_WWW
	wwwCallback = WWWDelayCall::Callback;
	#endif
	
	// Remove all coroutines in this behaviour from delayed call/
	// This will call CleanupCoroutine and decrease the retain count.
	// In turn this might remove coroutines from the active coroutine list, if no one else is referencing them.
	GetDelayedCallManager ().CancelCallDelayed2 (this, Coroutine::ContinueCoroutine, wwwCallback);

	/// 1. We need to go through the remaining coroutines (Those that are still referenced from mono or other coroutines are yielding to it from another game object)
	
	while (!m_ActiveCoroutines.empty())
	{
		Coroutine *coroutine = &m_ActiveCoroutines.front();
		DoStopCoroutine (coroutine);
		#if DEBUG_COROUTINE
		printf_console ("Cleaning up Stop ALL %d\n", coroutine);
		#endif
	}
	
	// This assert is useful when you think we might leak coroutines.
	Assert (m_ActiveCoroutines.empty ());
}

#if UNITY_EDITOR
void MonoBehaviour::CheckConsistency ()
{
	Super::CheckConsistency();

	if (!GetInstance())
		return;

	bool canDestroy = GetDisableImmediateDestruction ();
	SetDisableImmediateDestruction (true);

	// Validate properties

	ScriptingMethodPtr method = m_Methods[MonoScriptCache::kValidateProperties];
	if (method && !(m_ScriptCache->scriptType == kScriptTypeMonoBehaviourDerived && GetGameObjectPtr() == NULL))
	{
		CallMethodInactive (method);
	}

	SetDisableImmediateDestruction (canDestroy);
}
#endif

void MonoBehaviour::SmartReset ()
{
	Super::SmartReset ();
	// Only in edit mode for more speed
	// and to be nice to scripters we call Reset only if we are active
	if (GetInstance() && !IsWorldPlaying () )
		CallMethodInactive("Reset");
}

void MonoBehaviour::InitializeClass ()
{
	GameObject::RegisterAllMessagesHandler (ClassID (MonoBehaviour), &MonoBehaviour::HandleNotifications, &MonoBehaviour::CanHandleNotifications);
	
	RegisterAllowNameConversion ("GUISkin", "customStyles", "m_CustomStyles");

	InitializeMonoBehaviourAnimationBindingInterface ();
}

void MonoBehaviour::CleanupClass ()
{
	CleanupMonoBehaviourAnimationBindingInterface ();
}

UInt32 MonoBehaviour::CalculateSupportedMessages ()
{
	if (!GetInstance())
		return 0;

	int mask = 0;
	if (m_Methods[MonoScriptCache::kMethodCount + kEnterContact.messageID])
		mask |= kHasCollisionEnterExit;
	if (m_Methods[MonoScriptCache::kMethodCount + kExitContact.messageID])
		mask |= kHasCollisionEnterExit;
	if (m_Methods[MonoScriptCache::kMethodCount + kStayContact.messageID])
		mask |= kHasCollisionStay;
	if (m_Methods[MonoScriptCache::kMethodCount + kOnWillRenderObject.messageID])
		mask |= kHasOnWillRenderObject;
	if (m_Methods[MonoScriptCache::kMethodCount + kAnimatorMove.messageID])
		mask |= kHasOnAnimatorMove;
	if (m_Methods[MonoScriptCache::kMethodCount + kAnimatorIK.messageID])
		mask |= kHasOnAnimatorIK;
	if (m_Methods[MonoScriptCache::kMethodCount + kCollisionEnter2D.messageID])
		mask |= kHasCollision2D;
	if (m_Methods[MonoScriptCache::kMethodCount + kCollisionExit2D.messageID])
		mask |= kHasCollision2D;
	if (m_Methods[MonoScriptCache::kMethodCount + kCollisionStay2D.messageID])
		mask |= kHasCollision2D;
	return mask;
}

bool MonoBehaviour::CanHandleNotifications (void* receiver, int messageIndex, MessageData& data) {
	DebugAssertIf (dynamic_pptr_cast<MonoBehaviour*> (static_cast<MonoBehaviour*> (receiver)) == NULL);
	MonoBehaviour* behaviour = static_cast<MonoBehaviour*> (receiver);
	return behaviour->GetInstance() && behaviour->m_Methods[MonoScriptCache::kMethodCount + messageIndex];
}

static bool SetupArgumentsForMessageInvocation(ScriptingArguments& arguments, MessageData& data, ScriptingMethodPtr receivingMethod, MonoBehaviour& behaviour)
{
	switch (data.type)
	{
		case 0:
			return true;
		case ClassID(int):
			arguments.AddInt((int)data.GetGenericDataRef());
			return true;
		case ClassID(float):
			arguments.AddFloat((float)data.GetGenericDataRef());
			return true;
		case ClassID(bool):
			arguments.AddBoolean((int)data.GetGenericDataRef() != 0);
			return true;
		case ClassID(Collision):
			arguments.AddObject(GetIPhysics()->ConvertContactToMono (data.GetData<Collision*> ()));
			return true;
	#if ENABLE_2D_PHYSICS
		case ClassID(Collision2D):
			arguments.AddObject(ConvertCollision2DToScripting (data.GetData<Collision2D*> ()));
			return true;
	#endif
		case ClassID(MonoObject):
			{
				ScriptingObjectPtr parameter = data.GetScriptingObjectData ();
				arguments.AddObject(parameter);
				
				if (!parameter)
					return true;

				ScriptingClassPtr klass_of_receivingmethodargument = scripting_method_get_nth_argumenttype(receivingMethod,0,GetScriptingTypeRegistry());
				if (klass_of_receivingmethodargument == NULL)
					return true;
				ScriptingClassPtr klass_of_payload = scripting_object_get_class(parameter,GetScriptingTypeRegistry());

				if (!scripting_class_is_subclass_of(klass_of_payload, klass_of_receivingmethodargument))
				{
					std::string message = Format("%s couldn't be called because the expected parameter %s doesn't match %s.", scripting_method_get_name(receivingMethod), scripting_class_get_name(klass_of_receivingmethodargument),scripting_class_get_name(klass_of_payload));
					ErrorStringObject(message, &behaviour);
					return false;
				}

				return true;
			}
		default:
			// Otherwise its a Object derived class (The heavy typechecking is done in MonoScript)
			arguments.AddObject(Scripting::ScriptingWrapperFor (data.GetData<Object*> ()));
			return true;
	}
}

static bool ShouldMessageBeSentToDisabledGameObjects(int messageIndex)
{
	if( !IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1) && (messageIndex == kAnimatorMove.messageID || messageIndex == kAnimatorIK.messageID))
		return true;

	MessageIdentifier msg = GameObject::GetMessageHandler ().MessageIDToMessageIdentifier (messageIndex);
	
	return !(msg.options & MessageIdentifier::kDontSendToDisabled);
}

void MonoBehaviour::HandleNotifications (void* receiver, int messageIndex, MessageData& data) {
	DebugAssertIf (dynamic_pptr_cast<MonoBehaviour*> (static_cast<MonoBehaviour*> (receiver)) == NULL);
	MonoBehaviour* behaviour = static_cast<MonoBehaviour*> (receiver);

	#if UNITY_FLASH
	if(behaviour->m_Methods == NULL)
		return;
	#endif

	if (!behaviour->IsPlayingOrAllowExecuteInEditMode ())
		return;

	if (!behaviour->GetInstance())
		return;

	ScriptingMethodPtr method = behaviour->m_Methods[MonoScriptCache::kMethodCount + messageIndex];

	if (method == SCRIPTING_NULL)
		return;

	if (!behaviour->GetEnabled () && !ShouldMessageBeSentToDisabledGameObjects(messageIndex))
		return;

	ScriptingInvocation invocation(method);
	invocation.object = behaviour->GetInstance();
	invocation.objectInstanceIDContextForException = behaviour->GetInstanceID();

	if (!SetupArgumentsForMessageInvocation(invocation.Arguments(),data,method,*behaviour))
		return;	

	ScriptingExceptionPtr exception = NULL;
	ScriptingObjectPtr returnValue = invocation.Invoke(&exception);

	if (exception == NULL && returnValue)
		behaviour->HandleCoroutineReturnValue (method, returnValue);	
}

void MonoBehaviour::SetupAwake ()
{
	if (IsPlayingOrAllowExecuteInEditMode () && !m_DidAwake && GetInstance() && IsActive ())
	{
		m_DidAwake = true;
		CallMethodIfAvailable (MonoScriptCache::kAwake);
	}
}

void MonoBehaviour::DeprecatedDelayedAwakeCall (Object* o, void* userData)
{
	static_cast<MonoBehaviour*> (o)->SetupAwake ();
}

void MonoBehaviour::DelayedStartCall (Object* o, void* userData)
{
	static_cast<MonoBehaviour*> (o)->Start ();
}

void MonoBehaviour::DeprecatedAwakeFromLoadCodePath (AwakeFromLoadMode awakeMode)
{
	// We must cache the reference to other objects here
	// since Behaviour::AwakeFromLoad might destroy the object itself
	ScriptingObjectPtr instance = GetInstance();

	// Potential Call to AddToManager / RemoveFromManager with various C# callbacks
	Super::AwakeFromLoad (awakeMode);
	
	if (IsPlayingOrAllowExecuteInEditMode () && IsInstanceValid(instance) && !m_DidAwake && IsActive ())
	{
		if (awakeMode & (kInstantiateOrCreateFromCodeAwakeFromLoad | kActivateAwakeFromLoad))
			SetupAwake();
		else
			// @TODO: Do we really need this? Build this into Proper AwakeFromLoadQueue or whatever
			CallDelayed (DeprecatedDelayedAwakeCall, this, -1000, NULL, 0.0F, NULL, DelayedCallManager::kRunDynamicFrameRate | DelayedCallManager::kRunFixedFrameRate | DelayedCallManager::kRunStartupFrame);
	}
	else if (!m_DidAwake && IsInstanceValid(instance) && IsScriptableObject())
	{
		m_DidAwake = true;
		
		ScriptingMethodPtr method = m_Methods[MonoScriptCache::kAddToManager];
		if (method)
			CallMethodInactive (method);
		
		method = m_Methods[MonoScriptCache::kAwake];
		if (method)
			CallMethodInactive (method);
	}
	
	if ((awakeMode & kDidLoadFromDisk) == 0 && IsInstanceValid(instance))
	{
		ScriptingMethodPtr method = m_Methods[MonoScriptCache::kValidateProperties];
		if (method && !(m_ScriptCache->scriptType == kScriptTypeMonoBehaviourDerived && GetGameObjectPtr() == NULL))
			CallMethodInactive (method);
	}
}


#define USE_DEPRECATED_AWAKE !IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1)
//#define USE_DEPRECATED_AWAKE 1



// * Awake
// * OnEnable
// * ----- For all scripts
// * Start for all scripts
void MonoBehaviour::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
#if UNITY_EDITOR && UNITY_LOGIC_GRAPH
	LoadLogicGraphInEditor ();
#endif

	if (GetGameObjectPtr())
		GetGameObjectPtr()->SetSupportedMessagesDirty ();

	// Deprecated 3.x codepath
	if (USE_DEPRECATED_AWAKE)
	{
		DeprecatedAwakeFromLoadCodePath (awakeMode);
		return;
	}
	
	// We must cache the reference to other objects here
	// since Behaviour::AwakeFromLoad might destroy the object itself
	// and we need a way to check that this happened.
	ScriptingObjectPtr instance = GetInstance();

	// The managed MonoBehaviour could not be created. Give control to the Behaviour and move on.
	if (instance == SCRIPTING_NULL)
	{
		Super::AwakeFromLoad (awakeMode);
		return;
	}

	// CallDelayed->Start must be called before Awake otherwise objects created inside of Awake will get their Start call
	// before the Start of the Behaviour that created them.
	bool willCallAddToManager = IsPlayingOrAllowExecuteInEditMode() && GetEnabled() && IsActive();
	if (willCallAddToManager)
	{
		Super::AwakeFromLoad (awakeMode);
		return;
	}
	
	#define RETURN_IF_INSTANCE_DESTROYED 	if (!IsInstanceValid(instance)) return;
	
	bool isMonoBehaviourRequiringAwake = IsPlayingOrAllowExecuteInEditMode () && !m_DidAwake && IsActive ();
	bool isScriptableObjectRequiringAwakeAndEnable = !m_DidAwake && IsScriptableObject();

	// Awake for monobehaviours and scriptable objects
	if (isMonoBehaviourRequiringAwake || isScriptableObjectRequiringAwakeAndEnable)
	{
		CallAwake();
		RETURN_IF_INSTANCE_DESTROYED
	}

	// OnEnable for scriptable object
	if (isScriptableObjectRequiringAwakeAndEnable)
	{
		ScriptingMethodPtr enableMethod = m_Methods[MonoScriptCache::kAddToManager];
		if (enableMethod)
		{
			CallMethodInactive (enableMethod);
			RETURN_IF_INSTANCE_DESTROYED
		}
	}
	
	// Potential Call to AddToManager / RemoveFromManager with various C# callbacks
	Super::AwakeFromLoad (awakeMode);

	#undef RETURN_IF_INSTANCE_DESTROYED
}

void MonoBehaviour::CallAwake ()
{
	m_DidAwake = true;
	ScriptingMethodPtr awakeMethod = m_Methods[MonoScriptCache::kAwake];
	if (awakeMethod)
		if (!CallMethodInactive (awakeMethod))
			SetEnabled (false);
}



void MonoBehaviour::AddExternalDependencyCallbacksToManagers ()
{
#if ENABLE_IMAGEEFFECTS
	///@TODO: It doesn't look like the order of image effects is determined by the component order.
	//        Instead by the rather random from the users perspective Awake order.
	if (m_Methods[MonoScriptCache::kRenderImageFilter])
	{
		Camera *camera = QueryComponent (Camera);
		if (camera)
		{
			bool afterOpaque = scripting_method_has_attribute(m_Methods[MonoScriptCache::kRenderImageFilter], MONO_COMMON.imageEffectOpaque);
			bool transformsToLDR = scripting_method_has_attribute(m_Methods[MonoScriptCache::kRenderImageFilter], MONO_COMMON.imageEffectTransformsToLDR);
			
			ImageFilter filter (this, &RenderImageFilter, transformsToLDR, afterOpaque);
			camera->AddImageFilter (filter);
		}
	}
#endif
	
	SetByPassOnDSP(false);
}

void MonoBehaviour::SetByPassOnDSP(bool state)
{
#if ENABLE_AUDIO_FMOD
	IAudio* audio = GetIAudio();
	if (!audio) return;

	FMOD::DSP* dsp = GetOrCreateDSP();
	if (dsp)
		audio->SetBypassOnDSP(dsp,state);
#endif
}

void MonoBehaviour::AddBehaviourCallbacksToManagers ()
{
	MonoScript* script = m_Script;
	int executionOrder = script ? script->GetExecutionOrder() : 0;
	
	if (m_Methods[MonoScriptCache::kUpdate])
		GetBehaviourManager().AddBehaviour (m_UpdateNode, executionOrder);
	if (m_Methods[MonoScriptCache::kFixedUpdate])
		GetFixedBehaviourManager().AddBehaviour (m_FixedUpdateNode, executionOrder);
	if (m_Methods[MonoScriptCache::kLateUpdate])
		GetLateBehaviourManager().AddBehaviour (m_LateUpdateNode, executionOrder);
	
	if (m_Methods[MonoScriptCache::kRenderObject])
		GetRenderManager().AddOnRenderObject (m_OnRenderObjectNode);
	
#if ENABLE_UNITYGUI
	if (m_Methods[MonoScriptCache::kGUI])
		GetGUIManager().AddGUIScript (m_GUINode);
#endif
}


/* TODO: Tests
 * Destruction during OnEnable / Awake / Start (Maybe integration tests cover this already??)
 * Ensure that image effects & image filters are setup if their camera is created in OnEnable...
*/

#define RETURN_IF_DESTROYED_OR_DISABLED if (!IsInstanceValid(instance) || !GetEnabled()) return;

void MonoBehaviour::AddToManager ()
{
	if (USE_DEPRECATED_AWAKE)
	{
		DeprecatedAddToManager ();
		return;
	}

	
	ScriptingObjectPtr instance = GetInstance();
	if (instance == SCRIPTING_NULL || !IsPlayingOrAllowExecuteInEditMode ())
		return;
	
	if (m_Methods[MonoScriptCache::kCoroutineStart] || m_Methods[MonoScriptCache::kCoroutineMain])
		CallDelayed (DelayedStartCall, this, -10, NULL, 0.0F, NULL, DelayedCallManager::kRunDynamicFrameRate | DelayedCallManager::kRunFixedFrameRate | DelayedCallManager::kRunStartupFrame);
	

	// Behaviour callbacks are registered before OnEnable.
	// If an object is created in OnEnable, the order will be this script, then the created script.
	AddBehaviourCallbacksToManagers ();

	// We must call Awake here. 
	// CallDelayed->Start must be called before Awake otherwise objects created inside of Awake will get their Start call
	// before the Start of the Behaviour that created them.
	if (!m_DidAwake)
	{
		CallAwake ();
		RETURN_IF_DESTROYED_OR_DISABLED
	}

	if (m_Methods[MonoScriptCache::kAddToManager])
	{
		CallMethodIfAvailable (MonoScriptCache::kAddToManager);
		RETURN_IF_DESTROYED_OR_DISABLED
	}
	
	// External dependencies might get created by OnEnable.
	// Thus we hook them up after OnEnable
	AddExternalDependencyCallbacksToManagers ();
}

void MonoBehaviour::Deactivate (DeactivateOperation operation)
{
	// When loading a new level we don't want coroutines to stop running, so just ignore it.
	if (operation != kDeprecatedDeactivateToggleForLevelLoad)
		StopAllCoroutines ();
	
	Super::Deactivate (operation);
}


void MonoBehaviour::ReleaseMonoInstance ()
{
	Assert(m_ActiveCoroutines.empty());
	
	ScriptingObjectPtr instance = GetCachedScriptingObject();
	if (instance)
	{
		// In the player we protect against null reference exceptions by setting the instance id of the mono representation to 0.
		// This is necessary because mono classes might stick around for a bit longer until the GC cleans it up.
		// By setting the id to 0 it is impossible for another mono behaviour instance to load it from disk or other weird things.

		// In the editor we don't want to do this as it will cause references to scripts to
		// get lost when importing packages containing the referenced script.
		// ReleaseMonoInstance is called in that case because the script is getting deleted.


		#if !UNITY_EDITOR
		ScriptingObjectOfType<Object> wrapper(instance);
		wrapper.SetInstanceID(0);
		wrapper.SetCachedPtr(NULL);
		#endif

		SetCachedScriptingObject(SCRIPTING_NULL);
	}

	Assert(GetCachedScriptingObject() == SCRIPTING_NULL);
	
	m_Methods = NULL;
	if (m_ScriptCache != NULL)
	{
		const_cast<MonoScriptCache*> (m_ScriptCache)->Release();
		m_ScriptCache = NULL;
	}
}

#if UNITY_EDITOR
#define LogScriptError(x,script) DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kScriptCompileError, script, GetMonoManager().GetInstanceID ());
#endif

static UNITY_TLS_VALUE(int) s_MonoBehaviourInConstructorCounter;
int GetMonoBehaviourInConstructor()
{
	return s_MonoBehaviourInConstructorCounter;
}

static std::string SafeGetScriptFileName(MonoScript* script, ScriptingClassPtr klass)
{
	if (script)
		return script->GetName();
	
	if (klass)
		return scripting_class_get_name(klass);

	return "";
}

/////@TODO: THIS IS NOT REALLY THREAD SAFE. ALL MONO SCRIPT FUNCTIONS ARE NOT THREAD SAFE???
//// MONO SCRIPT LOADING SHOULD NOT BE DONE IN A DIFFERENT THREAD
/// ALL FUNCTIONS MODIFYING SCRIPT CONTENT MUST BE THREAD SAFE
void MonoBehaviour::RebuildMonoInstance (ScriptingObjectPtr instance)
{
	ReleaseMonoInstance ();

	MonoScript* script = 0;
	MonoScriptType type = kScriptTypeNotInitialized;

#if UNITY_EDITOR
	if (m_ScriptCache == NULL && !m_EditorClassIdentifier.empty())
	{
		std::string assembly;
		std::string ns;
		std::string klass;
		GetScriptClassIdComponents(m_EditorClassIdentifier, assembly, ns, klass);

		ScriptingClass* sc = GetMonoManager().GetMonoClassWithAssemblyName(klass, ns, assembly);
		m_ScriptCache = CreateMonoScriptCache(sc, true, this);
		if (m_ScriptCache != NULL)
		{
			m_ScriptCache->Retain();
			type = m_ScriptCache->scriptType;
		}
	}
#endif

	if (m_ScriptCache == NULL)
	{
		script = dynamic_pptr_cast<MonoScript*> (InstanceIDToObjectThreadSafe(m_Script.GetInstanceID()));
		if (script)
		{
			m_ScriptCache = script->GetScriptCache ();
			if (m_ScriptCache != NULL)
			{
				m_ScriptCache->Retain();
				type = m_ScriptCache->scriptType;
			}
		}
		else
			type = kScriptTypeScriptMissing;
	}

	// We want a warning to be printed only once, that is when entering play mode and not when returning back
	if (IsWorldPlaying() && !IsValidScriptType(type))
		WarningStringObject (FormatScriptTypeError(type, SafeGetScriptFileName(script, GetClass())), this);

	if (!IsValidScriptType(type))
		return;
	
	AssertIf(GetCachedScriptingObject() != SCRIPTING_NULL);
	Assert(m_ScriptCache != NULL && m_ScriptCache->klass != NULL);

	if (instance == SCRIPTING_NULL)
	{
		ScriptingObjectPtr newInstance;
#if ENABLE_MONO
		SET_ALLOC_OWNER(s_MonoDomainContainer);
#endif
		// Instantiate Mono class, handle exception		
		newInstance = scripting_object_new (m_ScriptCache->klass);
		if (newInstance == SCRIPTING_NULL)
		{
			if (IsWorldPlaying())
				WarningStringObject (Format("The script behaviour '%s' could not be instantiated!", script->GetScriptClassName().c_str()), this);
			return;
		}

		/// Setup Mono object pointers
		Scripting::ConnectScriptingWrapperToObject(newInstance, this);

		// Prevent certain functions inside constructor
		int constructorCount = s_MonoBehaviourInConstructorCounter;
		constructorCount++;
		s_MonoBehaviourInConstructorCounter = constructorCount;
		
		ScriptingExceptionPtr exception = NULL;

		scripting_object_invoke_default_constructor(GetInstance(), &exception);
		
		DebugAssertIf(constructorCount != s_MonoBehaviourInConstructorCounter);
		constructorCount--;
		s_MonoBehaviourInConstructorCounter = constructorCount;

		if (exception)
			Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(newInstance));
	}
	else
	{
		Scripting::ConnectScriptingWrapperToObject (instance, this);
	}
	
	// Get methods array from script
	m_Methods = m_ScriptCache->methods.begin();
	
	#if UNITY_EDITOR
	m_EditorHideFlags |= (script && script->IsBuiltinScript()) ? kHideScriptPPtr : 0;
	#endif
}

void MonoBehaviour::RebuildMonoInstanceFromScriptChange (ScriptingObjectPtr instance)
{
	if (IsAddedToManager ())
		RemoveFromManager ();
	
	RebuildMonoInstance (instance);
#if UNITY_EDITOR
	if (GetInstance() != NULL)
		SetBackup(NULL);
#endif
	
	if (IsAddedToManager ())
		AddToManager ();
}

void MonoBehaviour::SetScript (const PPtr<MonoScript>& newScript, ScriptingObjectPtr instance)
{
	if (m_Script != newScript)
	{
		//LockPlayerLoop();
		
		m_Script = newScript;
		
		RebuildMonoInstanceFromScriptChange(instance);

		//UnlockPlayerLoop();
	}
	else if (IsWorldPlaying () && newScript == PPtr<MonoScript> (0))
	{
		WarningStringObject ("The referenced script on this Behaviour is missing!", this);
	}
}

#if UNITY_EDITOR

void MonoBehaviour::SetClassIdentifier (const std::string& id)
{
	if (m_EditorClassIdentifier != id)
	{
		//LockPlayerLoop();

		m_EditorClassIdentifier = id;
			
		RebuildMonoInstanceFromScriptChange(NULL);

		//UnlockPlayerLoop();
	}
	//m_EditorClassIdentifier = id;
}

bool MonoBehaviour::CanAssignMonoVariable (const char* propertyType, Object* object)
{
	return CanAssignMonoVariableStatic(propertyType, object);
}

bool MonoBehaviour::CanAssignMonoVariableStatic (const char* propertyType, Object* object)
{
	MonoObject* instance = Scripting::ScriptingWrapperFor(object);
	if (!instance)
		return false;
	
	MonoClass* assignmentClass = mono_object_get_class(instance);
	while (assignmentClass != NULL)
	{
		if (strcmp(mono_class_get_name(assignmentClass), propertyType) == 0)
			return true;
	
		assignmentClass = mono_class_get_parent(assignmentClass);
	}
	
	return false;
}

#if UNITY_LOGIC_GRAPH
void MonoBehaviour::LoadLogicGraphInEditor ()
{
	// At the moment we compile LogicGraphs just in time when loading scenes.
	// MonoBehaviour instances are currently created on the loading thread -> Before logic graphs are compiled
	// Thus we need this hack in the editor to ensure that the logic graph is active using the latest graph data
	MonoScript* script = m_Script;
	if (script != NULL && script->GetEditorGraphData() != NULL && IsWorldPlaying())
	{
		script->Rebuild(CompileAndLoadLogicGraph(*script, false));
		RebuildMonoInstance(NULL);
	}
}
#endif


void MonoBehaviour::DidReloadDomain ()
{
#if ENABLE_AUDIO_FMOD
	// release custom filter
	delete m_AudioCustomFilter;
	m_AudioCustomFilter = NULL;
#endif
	
	if (IsAddedToManager())
		AddToManager();
	else
	{
		MonoScript* script = m_Script;
		if (GetInstance() == NULL || script == NULL)
			return;
		
		if (!IsScriptableObject())
			return;
		
		ScriptingMethodPtr method = m_Methods[MonoScriptCache::kAddToManager];
		if (method != NULL)
			CallMethodInactive(method);
	}
}

bool MonoBehaviour::ShouldDisplayEnabled ()
{
	if (m_EditorHideFlags & kHideEnabled)
		return false;
	if (GetInstance())
	{
		return m_Methods[MonoScriptCache::kUpdate] || m_Methods[MonoScriptCache::kFixedUpdate] || 
		       m_Methods[MonoScriptCache::kLateUpdate] ||
		       m_Methods[MonoScriptCache::kRenderImageFilter] ||
		       m_Methods[MonoScriptCache::kCoroutineStart] || m_Methods[MonoScriptCache::kCoroutineMain] ||
			   m_Methods[MonoScriptCache::kGUI] ||
			   m_Methods[MonoScriptCache::kAddToManager] || m_Methods[MonoScriptCache::kRemoveFromManager] ||
			   m_Methods[MonoScriptCache::kAudioFilterRead];
	}
	else
		return true;
}



#endif // UNITY_EDITOR

char const* MonoBehaviour::GetName () const
{
	const GameObject* go = GetGameObjectPtr();
	if (go)
		return go->GetName();
	else
		return m_Name.c_str ();
}

void MonoBehaviour::SetName (char const* name)
{
	GameObject* go = GetGameObjectPtr();
	if (go)
		return go->SetName(name);
	else
	{
		m_Name = name;
		SetDirty();
	}
} 

ScriptingClassPtr MonoBehaviour::GetClass ()
{
	if (m_ScriptCache != NULL)
		return m_ScriptCache->klass;
	else
		return NULL;
}

const MonoBehaviour::MethodCache* MonoBehaviour::GetMethodCache ()
{
	if (m_ScriptCache != NULL)
		return &m_ScriptCache->methodCache;
	else
		return NULL;
}


std::string MonoBehaviour::GetScriptClassName ()
{
	MonoScript* script = m_Script;
	if (script)
		return script->GetScriptClassName();
	else
		return string();
}

std::string MonoBehaviour::GetScriptFullClassName ()
{
	MonoScript* script = m_Script;
	if (script)
		return script->GetScriptFullClassName();
	else
		return string();
}

#if ENABLE_MONO || UNITY_WINRT
ScriptingArrayPtr RequiredComponentsOf(ScriptingClassPtr klass)
{
	// Extract component requirements
	ScriptingObjectPtr typeObject = scripting_class_get_object(klass);
	ScriptingInvocation invocation(GetMonoManager().GetCommonClasses().extractRequiredComponents);
	invocation.AddObject(typeObject);
	return scripting_cast_object_to_array(invocation.Invoke());
}

ScriptingArrayPtr RequiredComponentsOf(MonoBehaviour* script)
{
	return RequiredComponentsOf(script->GetClass());
}
#endif

void ResetAndApplyDefaultReferencesOnNewMonoBehaviour(MonoBehaviour& behaviour)
{

#if UNITY_EDITOR
	MonoScript* script = behaviour.GetScript();
	if (script && (script->GetScriptType() == kScriptTypeEditorScriptableObjectDerived || !IsWorldPlaying()))
		ApplyDefaultReferences(behaviour, script->GetDefaultReferences());
#endif
	
	behaviour.Reset();
	behaviour.SmartReset();
	
	behaviour.AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
}

#undef LogScriptError
#endif
