#include "UnityPrefix.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptPopupMenus.h"
#include "Runtime/Mono/tabledefs.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Scripting/ScriptingUtility.h"

using namespace std;

#if UNITY_EDITOR

static void ExtractPopupFromEnum (MonoClass* klass, map<string, int>& popup)
{
	MonoVTable * vtable = mono_class_vtable (mono_domain_get (), klass);
	MonoClassField *field;
	void* iter = NULL;
	
	MonoType* enumMonoType = mono_class_enum_basetype (klass);
	int enumType = mono_type_get_type (enumMonoType);
	
	while ((field = mono_class_get_fields (klass, &iter)))
	{
		int value;
		const char* name = mono_field_get_name (field);
		int flags = mono_field_get_flags (field);
		if (flags & FIELD_ATTRIBUTE_STATIC)
		{
			switch (enumType)
			{
				case MONO_TYPE_I4:
					mono_field_static_get_value (vtable, field, &value);
					break;
				case MONO_TYPE_U1:
					UInt8 byteValue;
					mono_field_static_get_value (vtable, field, &byteValue);
					value = byteValue;
					break;
				default:
					ErrorString (Format("Unsupported enum type: %s", name));
					break;
			}
			popup[name] = value;
		}
	}
}

const string GetFieldIdentifierForEnum(const TypeTree* typeTree)
{
	string identifier;
	while (typeTree != NULL && typeTree->m_Father != NULL)
	{
		// Skip the extra arary type injected by the serialization system
		if (typeTree->m_Father->m_Father && IsTypeTreeArray(*typeTree->m_Father))
			typeTree = typeTree->m_Father->m_Father;
		
		if (identifier.empty())
			identifier = typeTree->m_Name;
		else
			identifier = Format("%s.%s", typeTree->m_Name.c_str(), identifier.c_str());
		
		typeTree = typeTree->m_Father;
 	}
	
	return identifier;
}

static const string AppendName(const std::string& previous, MonoClassField* field)
{
	const char* name = mono_field_get_name(field);
	
	if (previous.empty())
		return name;
	else
		return Format("%s.%s", previous.c_str(), name);
}

static void BuildScriptPopupMenus (MonoClass* klass, const std::string& parentName, std::map<std::string, std::map<std::string, int> >& popups, std::set<MonoClass*> collected)
{
	const CommonScriptingClasses& commonClasses = GetMonoManager ().GetCommonClasses ();
	MonoClass* unityEngineObject = GetMonoManager().ClassIDToScriptingClass(ClassID(Object));

	while (klass && klass != commonClasses.monoBehaviour && klass != commonClasses.scriptableObject)
	{
		// Only collect classes once
		if (!collected.insert (klass).second)
			return;
		
		MonoClassField *field;
		void* iter = NULL;
		while ((field = mono_class_get_fields (klass, &iter)))
		{
			MonoType* monoType = mono_field_get_type (field);
			int typeType = mono_type_get_type (monoType);
			MonoClass* elementClass;
			
			int flags = mono_field_get_flags (field);
			if (flags & (FIELD_ATTRIBUTE_STATIC | FIELD_ATTRIBUTE_INIT_ONLY | FIELD_ATTRIBUTE_NOT_SERIALIZED))
				continue;
			
			// Extract the array field from the generic list
			MonoClassField* arrayField = GetMonoArrayFieldFromList(typeType, monoType, field);
			if (arrayField)
			{
				monoType = mono_field_get_type(arrayField);
				typeType = mono_type_get_type (monoType);
			}
			
			if (typeType == MONO_TYPE_VALUETYPE || typeType == MONO_TYPE_SZARRAY)
			{
				elementClass = mono_type_get_class_or_element_class (monoType);
				if (mono_class_is_enum (elementClass))
				{
					string name = AppendName(parentName, field);
					map<string, int>& items = popups[name];
					items.clear ();
					ExtractPopupFromEnum (elementClass, items);
					continue;
				}
			}
			if (typeType == MONO_TYPE_CLASS || typeType == MONO_TYPE_SZARRAY)
			{
				elementClass = mono_type_get_class_or_element_class (monoType);
				MonoImage* image = mono_class_get_image(elementClass);
				int classflags = mono_class_get_flags(elementClass);
				
				if ((classflags & TYPE_ATTRIBUTE_SERIALIZABLE) && image != mono_get_corlib() && GetMonoManager().GetAssemblyIndexFromImage(image) != -1 && !mono_class_is_subclass_of(elementClass, unityEngineObject, false))
					BuildScriptPopupMenus(elementClass, AppendName(parentName, field), popups, collected);
			}
		}
		
		klass = mono_class_get_parent (klass);
	}
}

void BuildScriptPopupMenus (MonoBehaviour& behaviour, std::map<string, std::map<std::string, int> >& popups)
{
	std::set<MonoClass*> collected;
	BuildScriptPopupMenus(behaviour.GetClass (), "", popups, collected);
}
#endif