#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h" // include before anything, to get Prof_ENABLED if that is defined
#if UNITY_EDITOR && ENABLE_MONO
#include "MonoManager.h"
#include "MonoAttributeHelpers.h"
#include <stdlib.h>

using namespace std;


static string RemoveStringFromString (string str, string remove)
{
	while (true)
	{
		int pos = str.find (remove);
		if (pos == string::npos)
			break;
		str.erase (pos, remove.size());
	}
	return str;
}

static std::string SignatureToString (MonoMethodSignature* signature)
{
	MonoType* methodType;
	void* iter = NULL;
	std::string parameters;
	while ((methodType = mono_signature_get_params (signature, &iter)))
	{
		if (!parameters.empty())
			parameters += ", ";
		parameters += mono_type_get_name(methodType);
	}

	string returnType = mono_type_get_name( mono_signature_get_return_type (signature) );
	string result = Format ("%s Func(%s)", returnType.c_str(), parameters.c_str());
	return RemoveStringFromString (result, "System.");
}



void GetMethodsWithAttribute (ScriptingClass* attributeClass, MonoMethod* comparedParams, std::vector<MonoMethod*>& resultList)
{
	MonoMethodSignature* pCompareSig = NULL;
	if (comparedParams)
		pCompareSig = mono_method_signature( comparedParams );

	for (int i = MonoManager::kEngineAssembly; i < GetMonoManager().GetAssemblyCount(); i++)
	{
		MonoAssembly* assembly = GetMonoManager().GetAssembly(i);
		if (!assembly)
			continue;

		MonoImage* pMonoImage = mono_assembly_get_image( assembly );
		if (!pMonoImage)
			continue;

		// stolen from #include... 
		const int MONO_TABLE_TYPEDEF = 2;				// mono/metadata/blob.h
		const int MONO_TOKEN_TYPE_DEF = 0x02000000;		// mono/metadata/tokentype.h

		int numFields = mono_image_get_table_rows( pMonoImage, MONO_TABLE_TYPEDEF );
		for (int typeIdx = 1; typeIdx < numFields; ++typeIdx)
		{
			guint32 token = MONO_TOKEN_TYPE_DEF | (typeIdx + 1);
			MonoClass* klass = mono_class_get (pMonoImage, token);
			gpointer iter = NULL;
			MonoMethod* method;

			if (klass == NULL) {
				// We need to call these two methods to clear the thread local
				// loader error status in mono. If not we'll randomly process the error
				// the next time it's checked.
				void* last_error = mono_loader_get_last_error ();
				mono_loader_error_prepare_exception (last_error);
 				continue;
			}

			while ((method = mono_class_get_methods(klass, &iter)))
			{
				MonoMethodSignature* sig = mono_method_signature (method);

				if (!sig) 
					continue;

				MonoCustomAttrInfo* attribInfo = mono_custom_attrs_from_method(method);
				if (!attribInfo)
					continue;

				if (!mono_custom_attrs_has_attr (attribInfo, attributeClass))
				{
					mono_custom_attrs_free(attribInfo);
					continue;
				}

				// Get static methods only
				bool isInstanceMethod = mono_signature_is_instance (sig);
				if (isInstanceMethod)
				{
					mono_custom_attrs_free(attribInfo);
					ErrorString (Format("UnityException: An [%s] attributed method is not static", mono_class_get_name(attributeClass)));
					continue;
				}

				if (pCompareSig)
				{
					if (!mono_metadata_signature_equal(sig, pCompareSig))
					{
						mono_custom_attrs_free(attribInfo);
						ErrorString (Format("UnityException: An [%s] attributed method has incorrect signature: %s. Correct signature: %s", mono_class_get_name(attributeClass), SignatureToString(sig).c_str(), SignatureToString(pCompareSig).c_str()));
						continue;
					}
				}
				else
				{
					if (mono_signature_get_param_count (sig) > 0)
					{
						mono_custom_attrs_free(attribInfo);
						ErrorString (Format("UnityException: An [%s] attributed method parameter count should be 0 but signature is: %s", mono_class_get_name(attributeClass), SignatureToString(sig).c_str()));
						continue;
					}
				}

				resultList.push_back(method);
				mono_custom_attrs_free(attribInfo);
			}
		}
	}	
}

bool AttributeSorter( MonoMethod* methodA, MonoMethod* methodB )
{
#if UNITY_EDITOR
	MonoCustomAttrInfo* attribAInfo = mono_custom_attrs_from_method(methodA);
	MonoCustomAttrInfo* attribBInfo = mono_custom_attrs_from_method(methodB);
	MonoObject* objA = mono_custom_attrs_get_attr( attribAInfo, MONO_COMMON.callbackOrderAttribute );
	MonoObject* objB = mono_custom_attrs_get_attr( attribBInfo, MONO_COMMON.callbackOrderAttribute );
	MonoClass* klassA = mono_object_get_class(objA);
	MonoClass* klassB = mono_object_get_class(objB);
	MonoProperty* propertyA = mono_class_get_property_from_name(klassA, "callbackOrder");
	MonoProperty* propertyB = mono_class_get_property_from_name(klassB, "callbackOrder");
	MonoMethod* getterA = mono_property_get_get_method(propertyA);
	MonoMethod* getterB = mono_property_get_get_method(propertyB);
	MonoException* monoException = NULL;
	MonoObject* resA = mono_runtime_invoke(getterA, objA, NULL, &monoException);
	MonoObject* resB = mono_runtime_invoke(getterB, objB, NULL, &monoException);
	int x = ExtractMonoObjectData<signed int>(resA);
	int y = ExtractMonoObjectData<signed int>(resB);

	return x < y;
#else
	return false;
#endif
}

void CallMethodsWithAttribute (ScriptingClass* attributeClass, ScriptingArguments& arguments, MonoMethod* comparedParams)
{
	vector<MonoMethod*> resultList;

	GetMethodsWithAttribute(attributeClass, comparedParams, resultList);
	sort (resultList.begin(), resultList.end(), AttributeSorter);

	for (vector<MonoMethod*>::iterator iter = resultList.begin(); iter != resultList.end(); ++iter) 
	{
		ScriptingInvocation invocation(*iter);
		invocation.Arguments() = arguments;
		invocation.Invoke();
	}
}

bool CallMethodsWithAttributeAndReturnTrueIfUsed (ScriptingClass* attributeClass, ScriptingArguments& arguments, MonoMethod* comparedParams)
{
	vector<MonoMethod*> resultList;

	GetMethodsWithAttribute(attributeClass, comparedParams, resultList);
	sort (resultList.begin(), resultList.end(), AttributeSorter);

	for (vector<MonoMethod*>::iterator iter = resultList.begin(); iter != resultList.end(); ++iter) 
	{
		ScriptingInvocation invocation(*iter);
		invocation.Arguments() = arguments;
		ScriptingObjectPtr returnValueObj = invocation.Invoke();
		if (ExtractMonoObjectData<bool>(returnValueObj))
			return true;
	}

	return false;
}


#endif //UNITY_EDITOR && ENABLE_MONO
