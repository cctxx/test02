#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "MonoBehaviour.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "MonoScript.h"
#include "MonoTypeSignatures.h"
#include "MonoManager.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Math/Gradient.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "tabledefs.h"
#include "Runtime/Mono/MonoBehaviourSerialization.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Scripting/ScriptingObjectWithIntPtrField.h"

using namespace std;

//Exotic inclusion of .h file that contains a lot of function bodies.
//Only way I could figure out how to split up this 2500 line file into multiple parts.
//splitting it up normally doesn't work, because all invocations of a template function
//need to be in the same compilation unit, and this serializationcode is all template functions
//calling other template functions.
#include "Runtime/Mono/MonoBehaviourSerialization_Array.h"

// Shoudl not be used!!!
#include "Runtime/GameCode/CallDelayed.h"

#if UNITY_EDITOR
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/Utility/RuntimeClassHashing.h"
#endif

struct TransferScriptInstance;

template<class TransferFunction>
void TransferScriptData (TransferScriptInstance& info, TransferFunction& transfer);

#if ENABLE_MONO
const char* CalculateMonoPPtrTypeString (char* buffer, MonoClass* klass)
{
	AssertIf(buffer == NULL);
	
	// Potential buffer overflow
	char* c = buffer;
	*c++ = 'P';
	*c++ = 'P';
	*c++ = 't';
	*c++ = 'r';
	*c++ = '<';
	*c++ = '$';
	
	const char* className = mono_class_get_name(klass);
	while (*className)
	{
		*c = *className;
		c++;
		className++;
	}
	
	*c++ = '>';
	*c++ = '\0';
	return buffer;
}

#if SUPPORT_TEXT_SERIALIZATION
YAMLNode* ConvertBackupToYAML (BackupState& binary);

template<>
class YAMLSerializeTraits<MonoPPtr> : public YAMLSerializeTraits<PPtr<Object> >
{
};
#endif

template<>
class SerializeTraits<MonoPPtr> : public SerializeTraitsBase<MonoPPtr>
{
public:
	
	typedef MonoPPtr	value_type;
	
	inline static const char* GetTypeString (void* data)
	{
		MonoPPtr* ptr = reinterpret_cast<MonoPPtr*> (data);
		AssertIf(ptr == NULL);
		// Needed for arrays (Could write custom Array & Traits class but thats a lot of work)
		if (ptr->m_Buffer == NULL)
			return "PPtr<$>";
		
		return CalculateMonoPPtrTypeString(ptr->m_Buffer, ptr->m_Class);
	}
	
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return true; }
	inline static bool AllowTransferOptimization ()	{ return false; }
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		data.Transfer (transfer);
	}
};

template<>
struct RemapPPtrTraits<MonoPPtr>
{
	static bool IsPPtr () 														{ return true; }
	static int GetInstanceIDFromPPtr (const MonoPPtr& data)		{ return data.GetInstanceID (); }
	static void SetInstanceIDOfPPtr (MonoPPtr& data, SInt32 instanceID)	{ data.SetInstanceID (instanceID);  }
};

template<class TransferFunction> inline 
void TransferPPtr (MonoObject* instance, MonoClassField* field, const char* name, MonoClass* klass, int classID, TransferFunction& transfer, TransferMetaFlags metaFlags)
{
	MonoPPtr pptr;
	if (transfer.IsWritingPPtr ())
	{
		MonoObject* referencedObject = NULL;
		mono_field_get_value (instance, field, &referencedObject);
		if (referencedObject != NULL)
			pptr.SetInstanceID (Scripting::GetInstanceIDFromScriptingWrapper (referencedObject));
	}
	
	// Potential buffer overflow
	char buffer[128];
	pptr.m_Buffer = buffer;
	pptr.m_Class = klass;
	
	transfer.Transfer (pptr, name, metaFlags);
	
	if (transfer.DidReadLastPPtrProperty ())
	{
		MonoObject* newValue = NULL;
		newValue = TransferPPtrToMonoObject (pptr.GetInstanceID (), klass, classID, field, instance, transfer.GetFlags() & kThreadedSerialization);
		mono_field_set_value (instance, field, newValue);
	}
}

template<class T>
void TransferWithProxyElement (T& t, vector<MonoPPtr>& arr, MonoPPtr&proxy, const char* name, TransferMetaFlags metaflags)
{
	t.Transfer(arr, name, metaflags);
}

template<>
void TransferWithProxyElement<ProxyTransfer> (ProxyTransfer& t, vector<MonoPPtr>& arr, MonoPPtr& proxy, const char* name, TransferMetaFlags metaflags)
{
	t.BeginTransfer (name, SerializeTraits<vector<MonoPPtr> >::GetTypeString (&arr), (char*)&arr, metaflags);
	t.TransferSTLStyleArrayWithElement(proxy, kNoTransferFlags);
	t.EndTransfer ();
}

template<class T, class TransferFunction> inline 
void TransferBuiltins (MonoObject* instance, MonoClassField* field, const char* name, TransferFunction& transfer, TransferMetaFlags metaFlags)
{
	T* value = reinterpret_cast<T*> (reinterpret_cast<UInt8*> (instance) + mono_field_get_offset(field));
	transfer.Transfer (*value, name, metaFlags);
}

template<class TransferFunction>  inline 
void TransferString (MonoObject* instance, MonoClassField* field, const char* name, TransferFunction& transfer, TransferMetaFlags metaFlags)
{
	UnityStr stdString;
	
	if (transfer.IsWriting ())
	{
		MonoString *strval;
		mono_field_get_value (instance, field, &strval);
		char *p = mono_string_to_utf8 (strval);
		if (p)
			stdString = p;
		else
			stdString.clear ();
		g_free (p);
	}
	
	transfer.Transfer (stdString, name, metaFlags);

	if (transfer.DidReadLastProperty ())
	{
		ScriptingStringPtr monoString = scripting_string_new(stdString);
		mono_field_set_value (instance, field, monoString);
	}
}


static bool CalculateTransferPrivateVariables (MonoClass* klass)
{
	MonoCustomAttrInfo* attr = mono_custom_attrs_from_class(klass);
	bool hasPrivate = attr != NULL && mono_custom_attrs_has_attr (attr, MONO_COMMON.serializePrivateVariables);
	if (attr != NULL)
		mono_custom_attrs_free (attr);
	return hasPrivate;
}

#if MONO_QUALITY_ERRORS
static void ReplacePrivateWithNULLWrapper (MonoObject* instance, MonoClassField* field)
{
	// This is necessary since proxy transfer will not give us an instance for array elements
	if (instance == NULL)
		return;
	
	// This way Mono won't throw an exception when an object is NULL instead the C++ side can handle it.
	MonoType* monoType = mono_field_get_type (field);
	int type = mono_type_get_type (monoType);
	
	if (type == MONO_TYPE_CLASS)
	{
		MonoClass* referencedClass = mono_type_get_class_or_element_class (monoType);
		int classID = Scripting::GetClassIDFromScriptingClass (referencedClass);
		if (classID != ClassID (MonoBehaviour) && classID != -1)
		{
			MonoObject* referencedObject;
			mono_field_get_value (instance, field, &referencedObject);
			if (referencedObject == NULL)
			{
				referencedObject = Scripting::ScriptingObjectNULL (referencedClass);
				mono_field_set_value (instance, field, referencedObject);
			}
		}
	}
}

static MonoObject* PrepareRefcountedTransfer (MonoClassField* field, MonoClass* referencedClass, MonoObject* instance)
{
	MonoObject* referencedObject = NULL;
	if (instance != NULL)
		mono_field_get_value (instance, field, &referencedObject);
	
	if (referencedObject == NULL)
	{
		referencedObject = ScriptingInstantiateObject (referencedClass);
		mono_runtime_object_init_log_exception (referencedObject);
		if (instance != NULL)
			mono_field_set_value (instance, field, referencedObject);
	}
	
	return referencedObject;
}
#endif

static MonoObject* PrepareTransfer (ScriptingClass* scriptingClass, MonoClassField* field, MonoObject* instance)
{
	MonoObject* value = NULL;
	if (instance)
	{
		mono_field_get_value (instance, field, &value);
		if (value == NULL)
		{
			value = ScriptingInstantiateObject (scriptingClass);			mono_runtime_object_init_log_exception (value);
			mono_field_set_value (instance, field, value);	
		}
	}
	return value;
}

static AnimationCurve* PrepareAnimationCurveTransfer (MonoClassField* field, MonoObject* instance)
	{
	MonoObject* monoObject = PrepareTransfer (MONO_COMMON.animationCurve, field, instance);
	if (monoObject)
		return ExtractMonoObjectData<AnimationCurve*>(monoObject);
		return NULL;
	}


static GradientNEW* PrepareGradientTransfer (MonoClassField* field, MonoObject* instance)
{
	MonoObject* monoObject = PrepareTransfer (MONO_COMMON.gradient, field, instance);
	if (monoObject)
		return ExtractMonoObjectData<GradientNEW*>(monoObject);
	return NULL;
}

static RectOffset* PrepareRectOffsetTransfer (MonoClassField* field, MonoObject* instance)
{
	MonoObject* monoObject = PrepareTransfer (MONO_COMMON.rectOffset, field, instance);
	if (monoObject)
		return ExtractMonoObjectData<RectOffset*>(monoObject);
	return NULL;
}

static GUIStyle* PrepareGUIStyleTransfer (MonoClassField* field, MonoObject* instance)
{
	MonoObject* monoObject = PrepareTransfer (MONO_COMMON.guiStyle, field, instance);
	if (monoObject)
		return ExtractMonoObjectData<GUIStyle*>(monoObject);
	return NULL;
}

/// Returns only true on serializable classes.
/// But only if we are in the same assembly.
static bool PrepareTransferEmbeddedClass (MonoClassField* field, MonoClass* referencedClass, MonoObject* instance, TransferScriptInstance& output, int currentDepth)
{
	if (!PrepareTransferEmbeddedClassCommonChecks(referencedClass))
		return false;
	
	MonoObject* referencedObject = NULL;
	if (instance)
	{
		mono_field_get_value (instance, field, &referencedObject);
		
		if (referencedObject == NULL)
		{
			referencedObject = ScriptingInstantiateObject (referencedClass);
			mono_runtime_object_init_log_exception (referencedObject);
			mono_field_set_value (instance, field, referencedObject);
		}
	}
	else
	{
		referencedObject = ScriptingInstantiateObject (referencedClass);
	}
	
	output.instance = referencedObject;
	output.klass = referencedClass;
	output.transferPrivate = CalculateTransferPrivateVariables(referencedClass);
	output.commonClasses = &GetMonoManager().GetCommonClasses();
	output.depthCounter = currentDepth + 1;
	Assert(currentDepth >= 0);
	
	return referencedObject != NULL;
}
						 
static bool HasAttribute(MonoClass* klass, MonoClassField *field, ScriptingClass* attributeClass)
{
	MonoCustomAttrInfo* attr = mono_custom_attrs_from_field (klass, field);

	if (attr == NULL)
		return false;

	bool has = mono_custom_attrs_has_attr (attr, attributeClass);
	mono_custom_attrs_free(attr);
	return has;
}

// Want to keep this logging ability around for some time, say until 2013
//#define TF_LOG(...) printf_console (__VA_ARGS__)
#define TF_LOG(...)

#if UNITY_EDITOR

// Return true if this field should be skipped and not serialized
static bool ProcessClassFields (char const* name, MonoType* monoType, cil::SerializeTracker& targetFields)
{
	char* typeName = mono_type_get_name_full (monoType, MONO_TYPE_NAME_FORMAT_IL);

	// In case target doesn't have any more fields, skip the current one
	if (!targetFields.IsFieldValid ())
	{
		g_free (typeName);
		return false;
	}
	
	// The field doesn't match the current in the target structure, skip
	if (!targetFields.IsCurrent (typeName, name))
	{
		// It can also be that target has more fields than source, but we catch that before
		// we start to serialize, so this shouldn't happen.
		if (targetFields.HasField (typeName, name))
		{
			while (targetFields.IsFieldValid () && !targetFields.IsCurrent (typeName, name))
			{
				cil::TypeDB::Field const& dfield = targetFields.CurrentField();
				
				// Should not happen!
				WarningString (Format ("Unable to properly serialize object for the player of class '%s' because of extra field '%s' of type '%s' (expecting '%s' '%s')", typeName, dfield.name.c_str(), dfield.typeName.c_str(), typeName, name
									   ));
				TF_LOG ("  - %s %s [EXTRA IN TARGET!]\n", dfield.typeName.c_str(), dfield.name.c_str());
				++targetFields;
			}
		}
		else
		{
			TF_LOG ("  - %s %s [SKIPPED!]\n", typeName, name);
			g_free (typeName);
			return false;
		}
	}
	else
		++targetFields;
	
	TF_LOG ("  - %s %s\n", typeName, name);
	
	g_free (typeName);
	return true;
}

#endif // UNITY_EDITOR

// Keep the traversal behaviour in sync with HashValueTypes (ScriptingTypePtr klass)
template<class TransferFunction>
void TransferScriptData (TransferScriptInstance& info, TransferFunction& transfer)
{
	Assert(info.depthCounter >= 0);

	MonoObject* instance = info.instance;
	MonoClass* klass = info.klass;
	const CommonScriptingClasses& commonClasses = *info.commonClasses;
	
	if (transfer.IsSerializingForGameRelease ())
	{
		TF_LOG ("Serializing class: %s\n", klass ? mono_class_get_name(klass) : "NULL");
	}
	
	// Recurse into parent classes stop when we reach the monobehaviour class
	MonoClass* parentClass = mono_class_get_parent (klass);
	if (parentClass && parentClass != commonClasses.monoBehaviour && parentClass != commonClasses.scriptableObject)
	{
		TransferScriptInstance parent = info;
		parent.klass = parentClass;
		parent.transferPrivate = CalculateTransferPrivateVariables(parentClass);
		TransferScriptData (parent, transfer);
	}
	
#if UNITY_EDITOR
	cil::SerializeTracker targetFields (cil::g_CurrentTargetTypeDB, klass);
#endif
	
	MonoClassField *field;
	void* iter = NULL;
	while ((field = mono_class_get_fields (klass, &iter)))
	{
		TransferMetaFlags metaFlags = kSimpleEditorMask;
		// Exclude const attributes
		int flags = mono_field_get_flags (field);
		if (flags & (FIELD_ATTRIBUTE_STATIC | FIELD_ATTRIBUTE_INIT_ONLY | FIELD_ATTRIBUTE_NOT_SERIALIZED))
			continue;
		
		if  ( ((flags & (FIELD_ATTRIBUTE_PRIVATE)) || (flags & 0xF) == FIELD_ATTRIBUTE_FAMILY) && !info.transferPrivate &&
			!HasAttribute(klass, field, commonClasses.serializeField))
		{
#if UNITY_EDITOR
				if (transfer.GetFlags () & kSerializeDebugProperties)
					metaFlags = kDebugPropertyMask | kNotEditableMask;
				else
				{
					// @todo: This code should be removed once mono has better null ptr exception handling.
					// We set all private ptr's that are NULL to be Object wrappers with instanceID 0.
					// This way Mono won't throw an exception when an object is NULL instead the C++ side can handle it.
					ReplacePrivateWithNULLWrapper (instance, field);
					continue;
				}
#else
				continue;
#endif
		}
		
		/// FINALLY MAKE THE SERIALIZATION CODE UNDERSTANDABLE!!!!!!!!!
		/// I would like to be able to check if we actually need the name!!!!!!!!!!!
		MonoType* monoType = mono_field_get_type (field);
		int type = mono_type_get_type (monoType);
		const char* name = mono_field_get_name (field);
				
#if UNITY_EDITOR
		// Skip fields that are not available in the 'player' script
		if (transfer.IsSerializingForGameRelease () && targetFields.IsClassValid ())
			if (!ProcessClassFields (name, monoType, targetFields))
				continue;
#endif

#if UNITY_EDITOR
		if (transfer.NeedNonCriticalMetaFlags () && HasAttribute(klass, field, commonClasses.hideInInspector))
			metaFlags |= kHideInEditorMask;
		// Make sure serialized property name does not contain any '.' characters.
		// This can happen when serializing internal backing values of C# implicit
		// properties, to which mono will add the namespace path (only relevant for
		// the debug inspector).
		std::string strippedName;
		if (strchr(name, '.'))
		{
			const char *ch = name;
			while (*ch != '\0')
			{
				strippedName += (*ch != '.')?*ch:'_';
				ch++;
			}
			name = strippedName.c_str();
		}
		
		if (transfer.NeedNonCriticalMetaFlags () && HasAttribute(klass, field, commonClasses.hideInInspector))
			metaFlags |= kHideInEditorMask;
#endif
		
		switch (type)
		{
			case MONO_TYPE_STRING:
				TransferString (instance, field, name, transfer, metaFlags);
				break;
			case MONO_TYPE_I4:
				TransferBuiltins<SInt32> (instance, field, name, transfer, metaFlags);
				break;
			case MONO_TYPE_R4:
				TransferBuiltins<float> (instance, field, name, transfer, metaFlags);
				break;
			case MONO_TYPE_BOOLEAN:
				TransferBuiltins<UInt8> (instance, field, name, transfer, metaFlags | kEditorDisplaysCheckBoxMask);
				transfer.Align();
				break;
			case MONO_TYPE_U1:
				TransferBuiltins<UInt8> (instance, field, name, transfer, metaFlags);
				transfer.Align();
				break;
			case MONO_TYPE_R8:
				TransferBuiltins<double> (instance, field, name, transfer, metaFlags);
				break;
			
				
			case MONO_TYPE_VALUETYPE:
			{
				MonoClass* structClass = mono_type_get_class_or_element_class (monoType);
				if (structClass == commonClasses.vector3)
					TransferBuiltins<Vector3f> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.vector2)
					TransferBuiltins<Vector2f> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.vector4)
					TransferBuiltins<Vector4f> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.rect)
					TransferBuiltins<Rectf> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.quaternion)
					TransferBuiltins<Quaternionf> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.matrix4x4)
					TransferBuiltins<Matrix4x4f> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.bounds)
					TransferBuiltins<AABB> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.color)
					TransferBuiltins<ColorRGBAf> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.color32)
					TransferBuiltins<ColorRGBA32> (instance, field, name, transfer, metaFlags);
				else if (structClass == commonClasses.layerMask)
					TransferBuiltins<BitField> (instance, field, name, transfer, metaFlags);
				else if (mono_class_is_enum (structClass))
				{
					MonoType* enumMonoType = mono_class_enum_basetype (structClass);
					int enumType = mono_type_get_type (enumMonoType);
					switch (enumType)
					{
						case MONO_TYPE_I4:
							TransferBuiltins<SInt32> (instance, field, name, transfer, metaFlags);
							break;
						case MONO_TYPE_U1:
							TransferBuiltins<UInt8> (instance, field, name, transfer, metaFlags);
							transfer.Align();
							break;
						default:
							ErrorString (ErrorMessageForUnsupportedEnumField(monoType, mono_class_get_type(klass), name));
							break;
					}
				}
#if UNITY_EDITOR
				else if ((transfer.GetFlags () & kSerializeMonoReload) && structClass == commonClasses.monoReloadableIntPtr)
				{
					TransferBuiltins<UIntPtr> (instance, field, name, transfer, metaFlags);
				}
				else if ((transfer.GetFlags () & kSerializeMonoReload) && structClass == commonClasses.monoReloadableIntPtrClear)
				{
					TransferBuiltins<UIntPtr> (instance, field, name, transfer, metaFlags);
					// Clear value after writing, so that objects referencing it won't try to delete the C++ object
					if (transfer.IsWriting())
					{
						void** reloadValue = reinterpret_cast<void**> (reinterpret_cast<UInt8*> (instance) + mono_field_get_offset(field));
						*reloadValue = NULL;
					}
				}
#endif
			}
				break;
				
			case MONO_TYPE_CLASS:
			{
				// Serialize pptr
				MonoClass* referencedClass = mono_type_get_class_or_element_class (monoType);

				// Do not serialize delegates
				// Most delegates are not serializable, but Boo generates delegate classes 
				// that are marked as serializable
				if (mono_class_is_subclass_of (referencedClass, MONO_COMMON.multicastDelegate, false))
					continue;

				int classID = Scripting::GetClassIDFromScriptingClass (referencedClass);
				if (classID != -1)
				{
					TransferPPtr (instance, field, name, referencedClass, classID, transfer, metaFlags);
				}
				else if (referencedClass == commonClasses.animationCurve)
				{
					AnimationCurve* curve = PrepareAnimationCurveTransfer(field, instance);
					transfer.Transfer (*curve, name, metaFlags);
				}
				else if (referencedClass == commonClasses.gradient)
				{
					GradientNEW* gradient = PrepareGradientTransfer (field, instance);
					transfer.Transfer (*gradient, name, metaFlags);
				}
				else if (referencedClass == commonClasses.rectOffset)
				{
					RectOffset* offset = PrepareRectOffsetTransfer(field, instance);
					transfer.Transfer (*offset, name, metaFlags);
				}
				else if (referencedClass == commonClasses.guiStyle)
				{
					GUIStyle* style = PrepareGUIStyleTransfer(field, instance);
					transfer.Transfer (*style, name, metaFlags);
				}
				// Serialize embedded class, but only if it has attribute serializable
				else
				{
					// Unfortunately we don't support cycles.
					if (referencedClass == klass)
						break;
					
					// Make sure we don't endless loop. Put a hard cap limit on the level of nested types until we have a better general solution.
					if (info.depthCounter > kClassSerializationDepthLimit)
						return;
					
					TransferScriptInstance transferScriptInstance;
					if (PrepareTransferEmbeddedClass (field, referencedClass, instance, transferScriptInstance, info.depthCounter))
					{
						transfer.TransferWithTypeString(transferScriptInstance, name, mono_class_get_name(referencedClass), metaFlags);
					}
				}
			}
				break;
				
			case MONO_TYPE_GENERICINST:	
			case MONO_TYPE_SZARRAY:
			{
				TransferFieldOfTypeArray(instance, klass, field, name, info, transfer, monoType, type, metaFlags);
			}
			break;
				
			default:
				
				//			printf_console ("unsupported type");
				
				break;
			
		};
	}
	
	if (transfer.IsReading() || transfer.IsReadingPPtr())
		ApplyScriptDataModified(info);
}
#endif	//ENABLE_MONO
/// Mono Backup
/// Problem: Sometimes the dll is not loadable or a script class can't be loaded
///          We don't want to lose the data when loading from disk so we store it in a temporary backup until the class appears again.

/// We now directly get the backup from the serialized information if we can't store it directly into the mono representation
/// This makes the code hard because we need to individually support every single transfer function.
/// Also we need to manually remap pptrs into instance id's
/// 

template<class TransferFunction>
PPtr<MonoScript> MonoBehaviour::TransferEngineData (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	PPtr<MonoScript> newScript = m_Script;
#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease())
		transfer.Transfer(m_EditorHideFlags, "m_EditorHideFlags", kHideInEditorMask);
	
	if (SerializePrefabIgnoreProperties(transfer))
	{
		TransferMetaFlags mask = kNoTransferFlags;
		if (m_EditorHideFlags & kHideScriptPPtr)
			mask |= kHideInEditorMask;
		transfer.Transfer (newScript, "m_Script", mask);
	}
#else
	transfer.Transfer (newScript, "m_Script");
#endif
	
	transfer.Transfer(m_Name, "m_Name", kHideInEditorMask);
	TRANSFER_EDITOR_ONLY_HIDDEN(m_EditorClassIdentifier);
	return newScript;
}


std::string MonoBehaviour::GetDebugDescription()
{
	return Format("id:%d, script:%s",GetInstanceID(), GetScriptClassName().c_str());
}

#if ENABLE_MONO && !ENABLE_SERIALIZATION_BY_CODEGENERATION
template<class TransferFunction>
inline void MonoBehaviour::TransferWithInstance (TransferFunction& transfer)
{
	TransferWithInstance (transfer, GetInstance (), GetClass());
}

template<class TransferFunction>
void MonoBehaviour::TransferWithInstance (TransferFunction& transfer, ScriptingObjectPtr instance, ScriptingClassPtr klass)
{
	SET_ALLOC_OWNER(s_MonoDomainContainer);
	
	AssertIf (instance == NULL);
	AssertIf (klass == NULL);

	TransferScriptInstance referencedInstance;
	referencedInstance.instance = instance;
	referencedInstance.klass = klass;
	referencedInstance.commonClasses = &GetMonoManager ().GetCommonClasses ();
	referencedInstance.transferPrivate = CalculateTransferPrivateVariables(klass);
	referencedInstance.depthCounter = 0;

	TransferScriptData (referencedInstance, transfer);
}
#endif

template<class TransferFunction>
void MonoBehaviour::TransferEngineAndInstance (TransferFunction& transfer)
{
	PPtr<MonoScript> newScript = TransferEngineData (transfer);
	if (transfer.IsReadingPPtr ())
		SetScript (newScript);
	if (GetInstance())
	{
		TransferWithInstance (transfer);
	}
}


void MonoBehaviour::SetInstanceNULLAndCreateBackup ()
{
	if (GetInstance())
	{
		GetDelayedCallManager().CancelAllCallDelayed( this );
		
#if UNITY_EDITOR
		SetBackup (new BackupState ());
		ExtractBackupFromInstance (GetInstance(), GetClass(), *m_Backup, 0);
#endif
		
		ReleaseMonoInstance ();
	}
}

/// Simple Player specific serialization
#if !UNITY_EDITOR

#if SUPPORT_SERIALIZED_TYPETREES
void MonoBehaviour::VirtualRedirectTransfer (SafeBinaryRead& transfer)
{
	SET_ALLOC_OWNER(this);
	transfer.BeginTransfer ("Base", MonoBehaviour::GetTypeString(), NULL);
	
	PPtr<MonoScript> newScript = TransferEngineData (transfer);
	if (transfer.IsReadingPPtr ())
		SetScript (newScript);
	
	if (GetInstance())
	{
		transfer.OverrideRootTypeName(scripting_class_get_name(GetClass()));
		TransferWithInstance (transfer);
	}
	
	transfer.EndTransfer ();
}

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryRead<true>& transfer)
{
	SET_ALLOC_OWNER(this);
	TransferEngineAndInstance(transfer);
}

#endif

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer)
{
	TransferEngineAndInstance(transfer);
}

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer)
{
	SET_ALLOC_OWNER(this);
	TransferEngineAndInstance(transfer);
}


void MonoBehaviour::VirtualRedirectTransfer (ProxyTransfer& transfer)
{
#if !UNITY_FLASH && !UNITY_WINRT && !ENABLE_SERIALIZATION_BY_CODEGENERATION
	transfer.BeginTransfer ("Base", MonoBehaviour::GetTypeString(), NULL, kNoTransferFlags);
	
	TransferEngineAndInstance(transfer);
	
	transfer.EndTransfer ();
#endif
}


void MonoBehaviour::VirtualRedirectTransfer (RemapPPtrTransfer& transfer)
{
	TransferEngineAndInstance (transfer);
}


/// Unity Editor specific serialization
#else // #if UNITY_EDITOR

void ApplyDefaultReferences (MonoBehaviour& behaviour, const map<UnityStr, PPtr<Object> >& data)
{
	MonoObject* instance = behaviour.GetInstance();
	if (instance == NULL)
		return;
	
	const CommonScriptingClasses& commonClasses = MONO_COMMON;
	MonoClass* klass = mono_object_get_class(behaviour.GetInstance());
	while (klass && klass != commonClasses.monoBehaviour && klass != commonClasses.scriptableObject)
	{
		MonoClassField *field;
		void* iter = NULL;
		while ((field = mono_class_get_fields (klass, &iter)))
		{
			int flags = mono_field_get_flags (field);
			
			// Ignore static / nonserialized
			if (flags & (FIELD_ATTRIBUTE_STATIC | FIELD_ATTRIBUTE_INIT_ONLY | FIELD_ATTRIBUTE_NOT_SERIALIZED))
				continue;
			
			// only public fields, or marked as serialize field
			if  ( ((flags & (FIELD_ATTRIBUTE_PRIVATE)) || (flags & 0xF) == FIELD_ATTRIBUTE_FAMILY) && !HasAttribute(klass, field, commonClasses.serializeField))
				continue;
			
			// Only pptrs on the root level
			MonoType* monoType = mono_field_get_type (field);
			int type = mono_type_get_type (monoType);
			if (type == MONO_TYPE_CLASS)
			{
				MonoClass* referencedClass = mono_type_get_class_or_element_class (monoType);
				const char* name = mono_field_get_name (field);
				map<UnityStr, PPtr<Object> >::const_iterator found = data.find(name);
				if (found != data.end())
				{
					// Check if the target variable inherits from the class
					MonoObject* target = Scripting::ScriptingWrapperFor(found->second);
					if (target && mono_class_is_subclass_of(mono_object_get_class(target), referencedClass, false))
					{
						// Never override pptr values
						MonoObject* oldTarget;
						mono_field_get_value (instance, field, &oldTarget);
						if (Scripting::GetInstanceIDFromScriptingWrapper(oldTarget) == 0)
							mono_field_set_value (instance, field, target);
					}
				}
			}
		}
		
		klass = mono_class_get_parent (klass);
	}	
}

struct FileToMemoryID
{
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (!IsTypeTreePPtr (typeTree))
			return true;
		
		if (typeTree.m_ByteSize == 12)
		{
			LocalSerializedObjectIdentifier* pptrData = reinterpret_cast<LocalSerializedObjectIdentifier*> (&data[bytePosition]);
			SInt32 instanceID = 0;
			LocalSerializedObjectIdentifierToInstanceID (*pptrData, instanceID);
			pptrData->localSerializedFileIndex = instanceID;
			pptrData->localIdentifierInFile = 0;
		}
		else
		{
			SInt32* pptrData = reinterpret_cast<SInt32*> (&data[bytePosition]);
			SInt32 instanceID = 0;
			LocalSerializedObjectIdentifier identifier;
			identifier.localSerializedFileIndex = pptrData[0];
			identifier.localIdentifierInFile = pptrData[1];
			
			LocalSerializedObjectIdentifierToInstanceID (identifier, instanceID);
			
			pptrData[0] = instanceID;
			pptrData[1] = 0;
			
		}
		return true;
	}
};

struct MemoryIDToFileID
{
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (!IsTypeTreePPtr (typeTree))
			return true;
		
		if (typeTree.m_ByteSize == 12)
		{
			LocalSerializedObjectIdentifier* pptrData = reinterpret_cast<LocalSerializedObjectIdentifier*> (&data[bytePosition]);
			SInt32 instanceID = pptrData->localSerializedFileIndex;
			InstanceIDToLocalSerializedObjectIdentifier (instanceID, *pptrData);
		}
		else
		{
			SInt32* pptrData = reinterpret_cast<SInt32*> (&data[bytePosition]);
			
			LocalSerializedObjectIdentifier identifier;
			InstanceIDToLocalSerializedObjectIdentifier (pptrData[0], identifier);
			
			pptrData[0] = identifier.localSerializedFileIndex;
			pptrData[1] = identifier.localIdentifierInFile;
		}
		return true;
	}
};

static int SkipString(CachedReader &cache, int basePos, int byteOffset, bool swapEndian, const TypeTree& type)
{
	UInt32 nameLength = 0;
	cache.SetPosition(basePos + byteOffset);
	cache.Read(&nameLength, sizeof(nameLength));
	if (swapEndian)
		SwapEndianBytes(nameLength);

	byteOffset += sizeof(nameLength)+nameLength;
	if (type.m_MetaFlag & kAlignBytesFlag || type.m_Children.back().m_MetaFlag & kAlignBytesFlag)
		byteOffset = Align4(byteOffset);

	return byteOffset;
}


static bool IsTypeTreeProperMonoBehaviour (const TypeTree& typeTree, int baseSize, int* byteOffset, int basePos, int* numberOfEngineChildren, CachedReader& cache, bool swapEndian)
{
	*byteOffset = 0;
	*numberOfEngineChildren = 0;
	
	TypeTree::const_iterator i = typeTree.m_Children.begin ();
	
	if (i != typeTree.end() && i->m_Name == "m_ObjectHideFlags")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}
	
	if (i != typeTree.end() && i->m_Name == "m_ExtensionPtr")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}

	if (i != typeTree.end() && i->m_Name == "m_PrefabParentObject")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}
	
	if (i != typeTree.end() && i->m_Name == "m_PrefabInternal")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}
	
	if (i != typeTree.end() && i->m_Name == "m_GameObject")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}
	else
		return false;
	
	if (i != typeTree.end() && i->m_Name == "m_Enabled")
	{
		*byteOffset += i->m_ByteSize;
		if (i->m_MetaFlag & kAlignBytesFlag)
			*byteOffset = Align4(*byteOffset);
		*numberOfEngineChildren += 1;
		i++;
	}
	else
		return false;
	
	if (i != typeTree.end() && i->m_Name == "m_EditorHideFlags")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}
	
	if (i != typeTree.end() && i->m_Name == "m_Script")
	{
		*byteOffset += i->m_ByteSize;
		*numberOfEngineChildren += 1;
		i++;
	}
	else
		return false;
	
	if (i != typeTree.end() && i->m_Name == "m_Name")
	{
		*byteOffset = SkipString(cache, basePos, *byteOffset, swapEndian, *i);

		*numberOfEngineChildren += 1;
		i++;
	}

#if UNITY_EDITOR
	if (i != typeTree.end() && i->m_Name == "m_EditorClassIdentifier")
	{
		*byteOffset = SkipString(cache, basePos, *byteOffset, swapEndian, *i);

		*numberOfEngineChildren += 1;
		i++;
	}
#endif

	return baseSize >= *byteOffset;
}

void MonoBehaviour::ExtractBackup (SafeBinaryRead& transfer, BackupState& backup)
{
	backup.loadedFromDisk = true;
	
	const TypeTree& sourceType = *transfer.m_OldBaseType;
	int byteOffset;
	int numberOfEngineChildren;
	if (IsTypeTreeProperMonoBehaviour (sourceType, transfer.m_BaseByteSize, &byteOffset, transfer.m_BaseBytePosition, &numberOfEngineChildren, transfer.m_Cache, transfer.ConvertEndianess()))
	{
		dynamic_array<UInt8>& sourceData = backup.state;

		// Extract typetree, remove all engine serialized data (m_Enabled, m_Script, m_GameObject etc)
		backup.typeTree = sourceType;
		TypeTree::iterator begin = backup.typeTree.m_Children.begin ();
		TypeTree::iterator end = begin;
		advance (end, numberOfEngineChildren);
		RemoveFromTypeTree(backup.typeTree, begin, end);

		// Extract data 
		sourceData.resize_uninitialized (transfer.m_BaseByteSize - byteOffset);
		transfer.m_Cache.SetPosition (byteOffset + transfer.m_BaseBytePosition);
		transfer.m_Cache.Read (sourceData.begin (), sourceData.size ());

		// case 565490
		// Remove m_EditorClassIdentifier if it is in the backup, it should not be there
		// Remove the data from the sourceData and remove the data type from the type tree
		// If m_EditorClassIdentifier is in the backup it will always be the first type
		if (backup.typeTree.m_Children.size() > 0 && StrCmp(backup.typeTree.begin()->m_Name, "m_EditorClassIdentifier") == 0)
		{
			int skipped = 0;
			WalkTypeTree(*backup.typeTree.m_Children.begin(), sourceData.begin(), &skipped);
			sourceData.erase(sourceData.begin(), sourceData.begin() + skipped);
			RemoveFromTypeTree(backup.typeTree, backup.typeTree.begin(), ++backup.typeTree.begin());
		}

		
		// Swap bytes
		if (transfer.ConvertEndianess())
			ByteSwapGeneric(backup.typeTree, sourceData);
		
		if (transfer.NeedsInstanceIDRemapping ())
		{
			int pos = 0;
			FileToMemoryID remap;
			IterateTypeTree (backup.typeTree, sourceData, &pos, remap);
		}
	}
	else
	{
		string error = "Monobehaviour has unknown format!: \n";
		sourceType.DebugPrint (error);
		ErrorString (error);
	}
}

void MonoBehaviour::ExtractBackupFromInstance (MonoObject* instance, MonoClass* scriptClass, BackupState& backup, int flags)
{
	SET_ALLOC_OWNER(s_MonoDomainContainer);

	AssertIf (instance == NULL || scriptClass == NULL);
	// Generate type tree from instance
	backup.typeTree = TypeTree ();
	ProxyTransfer proxy (backup.typeTree, flags, NULL, 0);
	proxy.BeginTransfer ("Base", mono_class_get_name(scriptClass), NULL, kNoTransferFlags);
	
	TransferScriptInstance transferData;
	transferData.instance = instance;
	transferData.klass = scriptClass;
	transferData.commonClasses = &GetMonoManager ().GetCommonClasses ();
	transferData.transferPrivate = CalculateTransferPrivateVariables(scriptClass);
	transferData.depthCounter = 0;
	
	TransferScriptData (transferData, proxy);
	proxy.EndTransfer ();
	
	// Generate state vector
	backup.state.clear ();
	MemoryCacheWriter memoryCache (backup.state);
	StreamedBinaryWrite<false> writeStream;
	CachedWriter& writeCache = writeStream.Init (flags, BuildTargetSelection::NoTarget());
	writeCache.InitWrite (memoryCache);
	
	TransferScriptData (transferData, writeStream);

	writeCache.CompleteWriting ();
}

void MonoBehaviour::RestoreInstanceStateFromBackup (BackupState& backup, int flags)
{
	if (backup.IsYaml ())
	{
		if (backup.HasYamlData ())
		{
			yaml_document_t ydoc;
			yaml_document_initialize (&ydoc, NULL, NULL, NULL, 1, 1);
			backup.yamlState->PopulateDocument (&ydoc);

			YAMLRead transfer (&ydoc, flags);
			TransferWithInstance (transfer);
			yaml_document_delete (&ydoc);
		}
	}
	else
		TransferSafeBinaryInstanceOnly (backup.state, backup.typeTree, flags);
}

void MonoBehaviour::TransferSafeBinaryInstanceOnly (dynamic_array<UInt8>& data, const TypeTree& typeTree, int options)
{
	MemoryCacheReader memoryCache (data);
	SafeBinaryRead readStream;
	CachedReader& readCache = readStream.Init (typeTree, 0, data.size (), options);
	readCache.InitRead (memoryCache, 0, data.size ());
	
	AssertIf(GetInstance() == NULL);
	readStream.BeginTransfer ("Base", mono_class_get_name(GetClass()), NULL);
	TransferWithInstance (readStream);
	readStream.EndTransfer ();
	
	readCache.End ();
}


void MonoBehaviour::VirtualRedirectTransfer (SafeBinaryRead& transfer)
{
	SET_ALLOC_OWNER(this);
	transfer.BeginTransfer ("Base", MonoBehaviour::GetTypeString(), NULL);
	
	//	AssertIf (GetInstance() != NULL && m_Backup != NULL);
	
	PPtr<MonoScript> newScript = TransferEngineData (transfer);
	if (transfer.IsReadingPPtr ())
	{
		if ((transfer.GetFlags() & kAutoreplaceEditorWindow) && !newScript.IsValid())
			newScript = dynamic_instanceID_cast<MonoScript*> (GetPersistentManager().GetInstanceIDFromPathAndFileID("Library/unity default resources", 12059));

		SetScript (newScript);
	}	
	
	if (GetInstance())
	{
		// Override the root name to ensure that it is the same as we will be loading,
		// since the user might have changed it in the mean time.
		transfer.OverrideRootTypeName(mono_class_get_name(GetClass()));
		
		TransferWithInstance (transfer);
	}
	else
		ProcessBackupStateWhileReading (transfer);
	
	transfer.EndTransfer ();
}

template <bool kSwap>
void MonoBehaviour::VirtualRedirectTransferStreamedBinaryRead(StreamedBinaryRead<kSwap>& transfer)
{
	PPtr<MonoScript> newScript = TransferEngineData (transfer);
	if (transfer.IsReadingPPtr ())
		SetScript (newScript);
	
	if (GetInstance())
		TransferWithInstance (transfer);
}

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer)
{
	SET_ALLOC_OWNER(this);
	VirtualRedirectTransferStreamedBinaryRead(transfer);
}

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryRead<true>& transfer)
{
	SET_ALLOC_OWNER(this);
	VirtualRedirectTransferStreamedBinaryRead(transfer);
}

void MonoBehaviour::VirtualRedirectTransfer (ProxyTransfer& transfer)
{
	transfer.BeginTransfer ("Base", MonoBehaviour::GetTypeString(), NULL, kNoTransferFlags);
	TransferEngineData (transfer);
	
	if (GetInstance())
		TransferWithInstance (transfer);
	else if (m_Backup && !transfer.IsSerializingForGameRelease())
	{
		AppendTypeTree (transfer.m_TypeTree, m_Backup->typeTree.begin (), m_Backup->typeTree.end ());
	}
	
	transfer.EndTransfer ();
}

static void ShowScriptMissingWarning(MonoBehaviour& behaviour)
{
	if (behaviour.GetInstance () == NULL)
	{
		WarningString (Format( "Script attached to '%s' in scene '%s' is missing or no valid script is attached.", behaviour.GetName(), GetApplication().GetCurrentScene().c_str() ));
	}
}

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer)
{
	TransferEngineData (transfer);
	
	if (GetInstance())
		TransferWithInstance (transfer);
	else if (m_Backup && !transfer.IsSerializingForGameRelease() && !m_Backup->IsYaml ())
	{
		if (transfer.NeedsInstanceIDRemapping ())
		{
			dynamic_array<UInt8> sourceData = m_Backup->state;
			int pos = 0;
			MemoryIDToFileID remap;
			IterateTypeTree (m_Backup->typeTree, sourceData, &pos, remap);
			transfer.m_Cache.Write (sourceData.begin (), sourceData.size ());
		}
		else
			transfer.m_Cache.Write (m_Backup->state.begin (), m_Backup->state.size ());
	}
	
	if (transfer.IsSerializingForGameRelease())
		ShowScriptMissingWarning(*this);
}

void MonoBehaviour::VirtualRedirectTransfer (StreamedBinaryWrite<true>& transfer)
{
	TransferEngineAndInstance (transfer);

	if (transfer.IsSerializingForGameRelease())
		ShowScriptMissingWarning(*this);
}

template<class TransferFunctor>
void MonoBehaviour::ProcessBackupStateWhileReading (TransferFunctor& transfer)
{
	SetBackup (new BackupState ());
	ExtractBackup (transfer, *m_Backup);
}

char const* kRemoveNodes[] =
{
	"m_ObjectHideFlags", "m_ExtensionPtr", "m_PrefabParentObject", "m_PrefabInternal",
	"m_GameObject", "m_Enabled", "m_EditorHideFlags", "m_Script", "m_Name"
	
#if UNITY_EDITOR
	, "m_EditorClassIdentifier"
#endif
};

static const size_t kRemoveNodesCount = sizeof(kRemoveNodes) / sizeof(kRemoveNodes[0]);


bool GetInstanceIDDirectlyIfAvailable (YAMLMapping* map, SInt32& instanceId)
{
	if (YAMLScalar* scalar = dynamic_cast<YAMLScalar*> (map->Get ("instanceID")))
	{
		instanceId = *scalar;
		return true;
	}

	return false;
}

bool GetInstanceIDFromFileIDIfAvailable (YAMLMapping* map, SInt32& instanceId)
{
	int fileID = 0;

	if (YAMLScalar* yFileId = dynamic_cast<YAMLScalar*>(map->Get("fileID")))
		fileID = (int)*yFileId;
	else
		return false;

	if (YAMLScalar* yGuid = dynamic_cast<YAMLScalar*>(map->Get("guid")))
	{
		int type = 0;
		if (YAMLScalar* yType = dynamic_cast<YAMLScalar*>(map->Get("type")))
			type = (int)*yType;

		// GUID is present, PPtr is referencing an object from another file
		UnityGUID guid = (UnityGUID)*yGuid;
		std::string pathName = GetPathNameFromGUIDAndType (guid, type);

		if (pathName.empty ())
			return false;

		instanceId = GetGUIDPersistentManager ().GetInstanceIDFromPathAndFileID(pathName, fileID);
	}
	else
	{
		// PPtr is referencing an object from the file we're reading
		LocalSerializedObjectIdentifier localId;
		localId.localIdentifierInFile = fileID;
		GetPersistentManager ().LocalSerializedObjectIdentifierToInstanceIDInternal (localId, instanceId);
	}

	return true;
}

template<class FunctorT>
void IterateYAMLTree (YAMLNode* node, FunctorT func)
{
	if (YAMLMapping* map = dynamic_cast<YAMLMapping*> (node))
	{
		if (func (map))
		{
			for (YAMLMapping::const_iterator it = map->begin (), end = map->end (); it != end; ++it)
				IterateYAMLTree (it->second, func);
		}
	} else if (YAMLSequence* seq = dynamic_cast<YAMLSequence*> (node))
	{
		if (func (seq))
		{
			for (YAMLSequence::const_iterator it = seq->begin (), end = seq->end (); it != end; ++it)
				IterateYAMLTree (*it, func);
		}
	} else if (YAMLScalar* scalar = dynamic_cast<YAMLScalar*> (node))
	{
		func (scalar);
	}
}

struct ConvertYAMLFileIDToInstanceID
{
	bool operator() (YAMLNode*) const { return true; }

	// We're only interested in mappings
	bool operator() (YAMLMapping* map) const
	{
		SInt32 instanceId = 0;

		if (GetInstanceIDFromFileIDIfAvailable (map, instanceId))
		{
			// Modify the mapping, changing fileID into InstanceID
			map->Clear ();
			
			map->Append ("instanceID", instanceId);

			// We do not want to iterate this mapping anymore
			return false;
		}

		return true;
	}
};

struct ConvertYAMLInstanceIDToFileID
{
	bool m_AllowLocalIdentifierInFile;
	ConvertYAMLInstanceIDToFileID (bool allowLocalIdentifierInFile) : m_AllowLocalIdentifierInFile (allowLocalIdentifierInFile) {}

	bool operator() (YAMLNode*) const { return true; }

	// We're only interested in mappings
	bool operator() (YAMLMapping* map) const
	{
		SInt32 instanceID = 0;

		if (GetInstanceIDDirectlyIfAvailable (map, instanceID))
		{
			LocalSerializedObjectIdentifier localIdentifier;
			InstanceIDToLocalSerializedObjectIdentifier (instanceID, localIdentifier);
		
			// Modify the mapping, changing fileID into InstanceID
			map->Clear();

			if (m_AllowLocalIdentifierInFile && localIdentifier.localSerializedFileIndex == 0)
			{
				map->Append ("fileID", localIdentifier.localIdentifierInFile);
			}
			else
			{
				GUIDPersistentManager& pm = GetGUIDPersistentManager();
				pm.Lock();
				SerializedObjectIdentifier identifier;
				if (pm.InstanceIDToSerializedObjectIdentifier(instanceID, identifier))
				{
					FileIdentifier id = pm.PathIDToFileIdentifierInternal(identifier.serializedFileIndex);
					map->Append ("fileID", identifier.localIdentifierInFile);
					map->Append ("guid", id.guid);
					map->Append ("type", id.type);
				}
				pm.Unlock();
			}

			// We do not want to iterate this mapping anymore
			return false;
		}

		return true;
	}
};


void YAMLConvertFileIDToInstanceID (YAMLMapping* map)
{
	ConvertYAMLFileIDToInstanceID convertor;
	IterateYAMLTree (map, convertor);
}

void YAMLConvertInstanceIDToFileID (YAMLMapping* map, bool allowLocalIdentifierInFile)
{
	ConvertYAMLInstanceIDToFileID convertor (allowLocalIdentifierInFile);
	IterateYAMLTree (map, convertor);
}

void MonoBehaviour::ExtractBackup (YAMLRead& transfer, BackupState& backup)
{
	backup.loadedFromDisk = true;
	// Node graph for current object
	YAMLNode* yroot = transfer.GetCurrentNode ();

	if (YAMLMapping* monoBehaviour = dynamic_cast<YAMLMapping*> (yroot))
	{
		// Remove engine values
		for (int i=0; i<kRemoveNodesCount; ++i)
			monoBehaviour->Remove (kRemoveNodes[i]);

		// case 565490
		// Remove any duplicates of m_EditorClassIdentifier, caused by a (now fixed) bug in the backup code 
		YAMLNode* duplicateNode = NULL;
		while ((duplicateNode = monoBehaviour->Get("m_EditorClassIdentifier")) != NULL)
		{
			monoBehaviour->Remove("m_EditorClassIdentifier");
		}

		// Convert PPtr from guid/fileId to instanceID
		if (transfer.NeedsInstanceIDRemapping ())
			YAMLConvertFileIDToInstanceID (monoBehaviour);
	}

	backup.SetYamlBackup (yroot);
}

void AppendYAMLMappingToCurrentNode (YAMLWrite& transfer, YAMLMapping* map)
{
	for (YAMLMapping::const_iterator it = map->begin (), end = map->end (); it != end; ++it)
	{
		int keyId = it->first->PopulateDocument (transfer.GetDocument ());
		int valueId = it->second->PopulateDocument (transfer.GetDocument ());
		yaml_document_append_mapping_pair(transfer.GetDocument (), transfer.GetCurrentNodeIndex (), keyId, valueId);
	}
}

void MonoBehaviour::VirtualRedirectTransfer (YAMLWrite& transfer)
{
	transfer.BeginMetaGroup (MonoBehaviour::GetTypeString());
	TransferEngineData (transfer);

	if (GetInstance())
		TransferWithInstance (transfer);
	else if (m_Backup && !transfer.IsSerializingForGameRelease())
	{
		YAMLNode* backup = NULL;

		if (m_Backup->IsYaml () && m_Backup->HasYamlData ())
		{
			if (transfer.NeedsInstanceIDRemapping ())
			{
				// We need to make a temp copy of backup state for MemoryId->FileID remapping.
				// This is not very nice, but this code path is hit rarely and we convert through string
				// instead of adding code to be able to clone YAMLNode.
				std::string data = m_Backup->yamlState->EmitYAMLString ();
				backup = ParseYAMLString (data);
			}
			else
			{
				backup = m_Backup->yamlState;
			}
		}
		else if (!m_Backup->IsYaml ())
		{
			backup = ConvertBackupToYAML (*m_Backup);
		}
		
		if (YAMLMapping* monoBehaviour = dynamic_cast<YAMLMapping*> (backup))
		{
			if (transfer.NeedsInstanceIDRemapping ())
			{
				bool allowLocalIdentifier = (transfer.GetFlags () & kYamlGlobalPPtrReference) == 0;
				YAMLConvertInstanceIDToFileID (monoBehaviour, allowLocalIdentifier);
			}
			
			AppendYAMLMappingToCurrentNode (transfer, monoBehaviour);
		}
		
		if (backup != m_Backup->yamlState)
			delete backup;
	}
	
	if (transfer.IsSerializingForGameRelease() && GetInstance () == NULL)
	{
		WarningString (Format( "Script attached to '%s' in scene '%s' is missing or no valid script is attached.", GetName(), GetApplication().GetCurrentScene().c_str() ));
	}

	transfer.EndMetaGroup ();
}

void MonoBehaviour::VirtualRedirectTransfer (YAMLRead& transfer)
{
	SET_ALLOC_OWNER(this);
	transfer.BeginMetaGroup (MonoBehaviour::GetTypeString());

	PPtr<MonoScript> newScript = TransferEngineData (transfer);
	if (transfer.IsReadingPPtr ())
	{
		if ((transfer.GetFlags() & kAutoreplaceEditorWindow) && !newScript.IsValid())
			newScript = dynamic_instanceID_cast<MonoScript*> (GetPersistentManager().GetInstanceIDFromPathAndFileID("Library/unity default resources", 12059));

		SetScript (newScript);
	}	
	
	if (GetInstance())
		TransferWithInstance (transfer);
	else
		ProcessBackupStateWhileReading (transfer);
	
	transfer.EndMetaGroup ();
}

// NOTE: This functor does not assign the newly generated InstaceID over the old one!
struct RemapPPtrFromYAMLBackup
{
	GenerateIDFunctor* m_GenerateIDFunctor;

	RemapPPtrFromYAMLBackup (GenerateIDFunctor* functor) : m_GenerateIDFunctor (functor) {}

	bool operator() (YAMLNode*) const { return true; }

	// We're only interested in mappings
	bool operator() (YAMLMapping* map) const
	{
		SInt32 oldInstanceID = 0;

		if (GetInstanceIDDirectlyIfAvailable (map, oldInstanceID))
		{
			if (/*YAMLScalar* scalar =*/ dynamic_cast<YAMLScalar*> (map->Get( "instanceID")))
			{
				m_GenerateIDFunctor->GenerateInstanceID (oldInstanceID, kNoTransferFlags);
				return false;
			}
		}

		return true;
	}
};

struct RemapPPtrFromBackup
{
	GenerateIDFunctor* idfunctor;
	
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (!IsTypeTreePPtr (typeTree))
			return true;
		
		SInt32* pptrData = reinterpret_cast<SInt32*> (&data[bytePosition]);
		
		SInt32 oldInstanceID = *pptrData;
		SInt32 newInstanceID = idfunctor->GenerateInstanceID (oldInstanceID, typeTree.m_MetaFlag);
		*pptrData = newInstanceID;
		return true;
	}
};

void MonoBehaviour::VirtualRedirectTransfer (RemapPPtrTransfer& transfer)
{
	PPtr<MonoScript> newScript = TransferEngineData (transfer);
	if (transfer.IsReadingPPtr ())
		SetScript (newScript);
	
	if (GetInstance())
	{
		TransferWithInstance (transfer);
		return;
	}

	if (!m_Backup)
		return;

	if (transfer.NeedsInstanceIDRemapping ())
	{
		AssertString("Impossible");
		return;
	}
	
	if (!m_Backup->IsYaml ())
	{
		dynamic_array<UInt8> sourceData = m_Backup->state;
		int pos = 0;
		RemapPPtrFromBackup remap;
		remap.idfunctor = transfer.GetGenerateIDFunctor();
		IterateTypeTree (m_Backup->typeTree, sourceData, &pos, remap);
		return;
	}
	
	if (!m_Backup->HasYamlData ())
		return;
	
	RemapPPtrFromYAMLBackup remap (transfer.GetGenerateIDFunctor());
	IterateYAMLTree (m_Backup->yamlState, remap);
}

#endif // UNITY_EDITOR

ScriptingObjectPtr TransferPPtrToMonoObject(int instanceID, ScriptingClassPtr klass, int classID, ScriptingFieldPtr field, ScriptingObjectPtr parentInstance, bool threadedLoading)
{
#if ENABLE_MONO && MONO_QUALITY_ERRORS
	Object* obj = NULL;
	MonoObject* instance = NULL;
	int objClassID = 0;
	if (!threadedLoading)
	{
		obj = PPtr<Object>(instanceID);
		if (obj)
		{
			instance = Scripting::ScriptingWrapperFor(obj);
			objClassID = obj->GetClassID();
		}
	}
	else
	{
		LockObjectCreation();
		
		obj = Object::IDToPointerNoThreadCheck(instanceID);
		if (obj != NULL)
		{
			instance = Scripting::ScriptingWrapperFor(obj);
			objClassID = obj->GetClassID();
			UnlockObjectCreation();
		}
		else
		{
			UnlockObjectCreation();
			obj = GetPersistentManager ().ReadObjectThreaded (instanceID);
			if (obj != NULL)
			{
				instance = Scripting::ScriptingWrapperFor(obj);
				objClassID = obj->GetClassID();
			}
		}
	}	
	
	if (obj)
	{	
		// If we got an instance and it's the right type, just return it
		if (instance != NULL && mono_class_is_subclass_of(mono_object_get_class(instance), klass, false))
			return instance;
		
		// No cached object available
		if (objClassID == ClassID (MonoBehaviour))
		{
			// Doesn't derive from klass, so we can use the real MonoObject
			if (instance)
				return NULL;
			
			// Probably couldn't be loaded temporarily. Create fake wrapper
			// There used by some problems with duplicating objects, but i can't think of a real world reason for this.
			// We can probably remove it
			if (!mono_unity_class_is_abstract(klass))
				instance = scripting_object_new(klass);
			
			if (!instance)
			{
				//if this happens, the klass was unable to initialize, often because of an exception in the klass' static constructor.
				return NULL;
			}

			ScriptingObjectOfType<Object>(instance).SetInstanceID(instanceID);
			return instance;	
		}
	}
	
	if (classID == ClassID (MonoBehaviour))
		return NULL;
	
	if (instanceID == 0)
	{
		MonoObject* obj = MonoObjectNULL(klass, UnassignedReferenceString(parentInstance, classID, field, instanceID));
		return obj;
	}

	ScriptingObjectOfType<Object> object = mono_object_new (mono_domain_get(), klass);
	object.SetInstanceID(instanceID);
	object.SetError(UnassignedReferenceString(parentInstance, classID, field, instanceID));
	return object.GetScriptingObject();
	
#endif
	
	return TransferPPtrToMonoObjectUnChecked(instanceID, threadedLoading);
}

ScriptingObjectPtr TransferPPtrToMonoObjectUnChecked (int instanceID, bool threadedLoading)
{
	if (!threadedLoading) // Non-Threaded loading path
		return Scripting::GetScriptingWrapperForInstanceID(instanceID);

	if (instanceID == 0)
		return SCRIPTING_NULL;
	
	LockObjectCreation();
	
	Object* obj = Object::IDToPointerNoThreadCheck(instanceID);
	
	UnlockObjectCreation();

	if(!obj)
		obj = GetPersistentManager().ReadObjectThreaded(instanceID);
	
	return Scripting::ScriptingWrapperFor(obj);

}

#endif // ENABLE_SCRIPTING
