#pragma once

struct MonoPropertyModification
{
	MonoObject* target;
	MonoString* propertyPath;
	MonoString* value;
	MonoObject* objectReference;
};

inline void PropertyModificationToMono (const PropertyModification &src, MonoPropertyModification &dest)
{
	dest.target = Scripting::ScriptingWrapperFor (src.target);
	dest.propertyPath = scripting_string_new(src.propertyPath);
	dest.value = scripting_string_new(src.value);
	dest.objectReference = Scripting::ScriptingWrapperFor(src.objectReference);
}

inline void MonoToPropertyModification(const MonoPropertyModification &src, PropertyModification &dest)
{
	dest.target = ScriptingObjectToObject<Object> (src.target);
	dest.propertyPath = scripting_cpp_string_for(src.propertyPath);
	dest.value = scripting_cpp_string_for(src.value);
	dest.objectReference = ScriptingObjectToObject<Object>(src.objectReference);
}

inline void MonoToPropertyModification(MonoObject* src, PropertyModification &dest)
{
	MonoToPropertyModification(ExtractMonoObjectData<MonoPropertyModification> (src), dest);
}

inline void PropertyModificationToMono(const PropertyModification &src, MonoObject* dest)
{
	PropertyModificationToMono(src, ExtractMonoObjectData<MonoPropertyModification> (dest));
}