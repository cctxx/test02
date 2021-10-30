#ifndef MONOBEHAVIOUR_H
#define MONOBEHAVIOUR_H

#include "Runtime/GameCode/Behaviour.h"
#include "MonoIncludes.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"


class MonoScript;
class MonoBehaviour;
typedef ListNode<MonoBehaviour> MonoBehaviourListNode;
class MonoManager;
struct ObjectGUIState;
struct Coroutine;
class RenderTexture;
struct MonoScriptCache;

#if ENABLE_AUDIO_FMOD
class AudioCustomFilter;
namespace FMOD {
	class DSP;
}
#endif

class TerrainInstance;


#if UNITY_EDITOR
#include "Editor/Src/Utility/YAMLNode.h"

struct BackupState
{
	BackupState ();
	~BackupState ();
	
	TypeTree             typeTree;
	dynamic_array<UInt8> state;
	bool                 inYamlFormat;
	bool                 loadedFromDisk;
	YAMLNode*            yamlState;


	void SetYamlBackup (YAMLNode* root) { delete yamlState; inYamlFormat = true; yamlState = root; }
	bool IsYaml () const { return inYamlFormat; }
	bool HasYamlData () const { return yamlState != NULL; }

	private:

	// Preventy copy constructor
	BackupState (const BackupState& preventCopy);
	void operator = (const BackupState& preventCopy);
};
#endif


class MonoBehaviour : public Behaviour
{
public:

	REGISTER_DERIVED_CLASS (MonoBehaviour, Behaviour)
	DECLARE_OBJECT_SERIALIZE (MonoBehaviour)
	
	MonoBehaviour (MemLabelId label, ObjectCreationMode mode);
	// virtual ~MonoBehaviour (); declared-by-macro
	
	// Tag class as sealed, this makes QueryComponent faster.
	static bool IsSealedClass ()				{ return true; }
	
	/// Returns the MonoObject representing this MonoBehaviour		
	ScriptingObjectPtr GetInstance () 
	{ 
		#if ENABLE_SCRIPTING
			return GetCachedScriptingObject(); 
		#else
			return SCRIPTING_NULL;
		#endif
	}
	ScriptingClassPtr GetClass ();

	void RebuildMonoInstanceFromScriptChange (ScriptingObjectPtr instance);
	/// Changes the used script to newScript
	/// It is critical you call Awake
	void SetScript (const PPtr<MonoScript>& newScript, ScriptingObjectPtr instance = SCRIPTING_NULL);
	PPtr<MonoScript> GetScript () const { return m_Script; }

	Coroutine* InvokeMethodOrCoroutineChecked (ScriptingMethodPtr method, ScriptingObjectPtr value = SCRIPTING_NULL);

	/// Starts a coroutine
	/// If value is not null the value will be passed into the object.
	/// If the method is not a coroutine, it will just be invoked
	Coroutine* CreateCoroutine(ScriptingObjectPtr enumerator, ScriptingMethodPtr method);
	Coroutine* StartCoroutine (const char* name, ScriptingObjectPtr value = SCRIPTING_NULL);
	ScriptingObjectPtr StartCoroutineManaged (const char* name, ScriptingObjectPtr value = SCRIPTING_NULL);
	ScriptingObjectPtr StartCoroutineManaged2 (ScriptingObjectPtr enumerator);
	
	std::string GetScriptClassName ();
	std::string GetScriptFullClassName ();

	/// Calls a method with methodName if the method is implemented by the class
	bool CallMethodInactive (const char* methodName);

	/// Calls a method
	bool CallMethodInactive (ScriptingMethodPtr method);

#if UNITY_EDITOR
	// Check the consistency of the mono behavior... (validate fields ect in mono land)
	virtual void CheckConsistency ();
#endif

	/// Forwards Reset to the MonoClass in edit mode only.
	virtual void SmartReset ();

	enum GUILayoutType {kNoLayout = 0, kGameLayout = 1, kEditorWindowLayout = 2 };
#if ENABLE_UNITYGUI
	/// Caller for the GUI functions.
	virtual bool DoGUI (GUILayoutType layoutType, int skin);
#endif

	// ImageFilter
	#if ENABLE_IMAGEEFFECTS
	static void RenderImageFilter (Unity::Component* component, RenderTexture *source, RenderTexture *destination);
	#endif
	
	virtual void Deactivate (DeactivateOperation operation);
	virtual void AwakeFromLoad (AwakeFromLoadMode mode);

	virtual UInt32 CalculateSupportedMessages ();

	ScriptingMethodPtr FindMethod (const char* name);

	void InvokeOnRenderObject ();
	
	#if UNITY_EDITOR
	enum { kHideScriptPPtr = 1 << 0, kHideEnabled = 1 << 2 };
	void SetEditorHideFlags(int flags) { m_EditorHideFlags = flags; }

	void SetClassIdentifier(const std::string& id);
	std::string GetClassIdentifier() const { return m_EditorClassIdentifier; }
	#endif
	
	void StopCoroutine (const char* name);
	void StopAllCoroutines ();

	virtual char const* GetName () const;
	virtual void SetName (char const* name);
	
	typedef std::map<const char*, ScriptingMethodPtr, compare_cstring> MethodCache;
	const MethodCache* GetMethodCache ();

	// SetupAwake is called immediately by Activate.
	// It is delayed to make sure that all objects in the scene are setup
	// when Awake gets called on the MonoObject
	void SetupAwake ();

	bool GetUseGUILayout ();
	void SetUseGUILayout (bool use);
	
	bool WillUnloadScriptableObject ();

private:
	
	static void DeprecatedDelayedAwakeCall (Object* o, void* userData);
	static void DelayedStartCall (Object* o, void* userData);
	
	/// Calls Awake on the MonoObject if it implements the Awake method
	static void DelayedAwakeMonoBehaviour (Object* o, void* userData);

	// Registers the MonoBehaviour for the Update/FixedUpdate/LateUpdate method.
	// Update/FixedUpdate/LateUpdate is called if:
	// - We are in play mode
	// - Any of the Update, FixedUpdate, LateUpdate is implemented in the MonoClass.
	// Before invoking the Update method,
	// the Start method is invoked if it hasnt been invoked yet.
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void Update ();
	virtual void FixedUpdate ();
	virtual void LateUpdate ();
	inline void Start ();

	void DeprecatedAwakeFromLoadCodePath (AwakeFromLoadMode awakeMode);
	
	#if UNITY_EDITOR
	friend void DrawMonoGizmo (Object& object, int options, void*);
	friend bool CanDrawMonoGizmo (Object& object, int options, void*);
	#endif
	
	inline void CallMethodIfAvailable (int methodIndex);
	void CallUpdateMethod(int methodIndex);

	ScriptingObjectPtr InvokeMethodOrCoroutineChecked(ScriptingMethodPtr method, ScriptingObjectPtr value, ScriptingExceptionPtr* exception);

	// Depending on which script we use the serialized typetree changes
	// thus the typetree might change while the editor is running
	virtual bool GetNeedsPerObjectTypeTree () const { return true; }
	
	static void HandleNotifications (void* receiver, int messageIndex, MessageData& data);
	static bool CanHandleNotifications (void* receiver, int messageIndex, MessageData& data);
	
	#if UNITY_EDITOR
	bool CanAssignMonoVariable (const char* property, Object* object);
	void DidReloadDomain ();
	#endif
	
	virtual void WillDestroyComponent ();
	
	template<bool kSwap>
	void VirtualRedirectTransferStreamedBinaryRead(StreamedBinaryRead<kSwap>& transfer);
public:

	// Don't call this if GetInstance() == NULL
	ScriptingMethodPtr GetMethod (int index) const { return m_Methods[index]; }

	#if UNITY_EDITOR
	static bool CanAssignMonoVariableStatic (const char* property, Object* object);
	#endif

	// RebuildMonoInstance should only be called by MonoScript and MonoManager
	// Anulls the old instance
	// And creates a new Mono representation if the script class is availible
	// Resets m_Methods to the new class
	void RebuildMonoInstance (ScriptingObjectPtr instance);
	void ReleaseMonoInstance ();
	
	#if UNITY_EDITOR
	void SetBackup (BackupState* state) { delete m_Backup; m_Backup = state; }
	BackupState* GetBackup () { return m_Backup; }
	static void ExtractBackupFromInstance (MonoObject* instance, MonoClass* scriptClass, BackupState& backup, int flags);
	static void ExtractYAMLBackupFromInstance (MonoObject* instance, MonoClass* scriptClass, BackupState& backup, int flags);
	void RestoreInstanceStateFromBackup (BackupState& backup, int flags);
	template<class TransferFunctor>
	void ProcessBackupStateWhileReading (TransferFunctor& transfer);
	#endif

	void SetInstanceNULLAndCreateBackup ();
	Coroutine* HandleCoroutineReturnValue (ScriptingMethodPtr method, ScriptingObjectPtr returnValue);

	// Registers the notification receiver function
	static void InitializeClass ();
	static void CleanupClass ();

	#if UNITY_EDITOR
	virtual bool ShouldDisplayEnabled ();
	static void RestartExecuteInEditModeScripts ();
	#endif
	
	void TransferSafeBinaryInstanceOnly (dynamic_array<UInt8>& data, const TypeTree& typeTree, int options);
	
	ObjectGUIState& GetObjectGUIState();
	
	bool IsScriptableObject();
	bool IsDestroying() { return m_IsDestroying; }
	
	List<Coroutine>& GetActiveCoroutines () { return m_ActiveCoroutines; }
	
public:
	bool HaveAudioCallback() const; 
	void SetByPassOnDSP(bool state);
#if ENABLE_AUDIO_FMOD
	FMOD::DSP* GetDSP() const;
	FMOD::DSP* GetOrCreateDSP();
	AudioCustomFilter* GetAudioCustomFilter() { return m_AudioCustomFilter; }	
#endif

private:
	
	void AddBehaviourCallbacksToManagers ();
	void AddExternalDependencyCallbacksToManagers ();
	void CallAwake ();
	void DeprecatedAddToManager ();

	bool GetRunInEditMode ()const;
	bool IsPlayingOrAllowExecuteInEditMode() const;
	std::string GetDebugDescription();

	template<class TransferFunction>
	PPtr<MonoScript> TransferEngineData (TransferFunction& transfer);
	
	template<class TransferFunction>
	void TransferMonoData (TransferFunction& transfer);
	
	template<class TransferFunction>
	static void TransferWithInstance (TransferFunction& transfer, ScriptingObjectPtr instance, ScriptingClassPtr klass);
	template<class TransferFunction>
	void TransferWithInstance (TransferFunction& transfer);

#if ENABLE_SERIALIZATION_BY_CODEGENERATION
	void DoLivenessCheck(RemapPPtrTransfer& transfer);
#endif

	template<class TransferFunction>
	void TransferEngineAndInstance (TransferFunction& transfer);

#if UNITY_EDITOR
	static void ExtractBackup (class SafeBinaryRead& transfer, struct BackupState& backup);
	static void ExtractBackup (YAMLRead& transfer, BackupState& backup);
#if UNITY_LOGIC_GRAPH
	void LoadLogicGraphInEditor ();
#endif
#endif
	
	PPtr<MonoScript> m_Script;
	
	UnityStr                  m_Name;
	
	const MonoScriptCache*    m_ScriptCache;
	const ScriptingMethodPtr* m_Methods;
	
	List<Coroutine>   m_ActiveCoroutines;
	
	BehaviourListNode        m_UpdateNode;
	BehaviourListNode        m_FixedUpdateNode;
	BehaviourListNode        m_LateUpdateNode;
	MonoBehaviourListNode    m_GUINode;
	MonoBehaviourListNode    m_OnRenderObjectNode;

	// Per-monobehaviour GUI State.
	ObjectGUIState *m_GUIState;

	bool                     m_DidAwake;
	bool                     m_DidStart;
	bool                     m_UseGUILayout;
	bool                     m_IsDestroying;

private:
#if ENABLE_AUDIO_FMOD
	AudioCustomFilter*		m_AudioCustomFilter;
#endif
	
	#if UNITY_EDITOR
	BackupState*	m_Backup;
	UInt32          m_EditorHideFlags;
	UnityStr		m_EditorClassIdentifier;
	#endif
	
	friend struct Coroutine;
	friend class MonoManager;
};

#if UNITY_EDITOR
void BuildScriptPopupMenus (MonoBehaviour& behaviour, std::map<std::string, std::map<int, std::string> >& popups);
void ApplyDefaultReferences (MonoBehaviour& behaviour, const std::map<UnityStr, PPtr<Object> >& data);
#endif

void ResetAndApplyDefaultReferencesOnNewMonoBehaviour(MonoBehaviour& behaviour);

EXPORT_COREMODULE int GetMonoBehaviourInConstructor();

#if ENABLE_MONO || UNITY_WINRT
ScriptingArrayPtr RequiredComponentsOf(ScriptingClassPtr klass);
ScriptingArrayPtr RequiredComponentsOf(MonoBehaviour* script);
#endif

////@TODO: THIS SHOULD BE REMOVED AND DONE WITH THREAD_SAFE / CONSTRUCTOR_SAFE tags instead!

/// DISALLOW_IN_CONSTRUCTOR Raises an exception when executed from inside a MonoBehaviour constructor.
/// eg. GetComponent uses this to make sure no one uses it in a constructor
#if UNITY_EDITOR
#define DISALLOW_IN_CONSTRUCTOR { \
	if (GetMonoBehaviourInConstructor() == 0) ; else { \
		Scripting::RaiseMonoException("You are not allowed to call this function when declaring a variable.\nMove it to the line after without a variable declaration.\nIf you are using C# don't use this function in the constructor or field initializers, Instead move initialization to the Awake or Start function."); } \
	}
#else
#define DISALLOW_IN_CONSTRUCTOR {  }
#endif

#endif
