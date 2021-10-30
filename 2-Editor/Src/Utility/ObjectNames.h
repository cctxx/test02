#pragma once

/// Return a nice human-readable name from a variable. 
/// Renames mixed-case to seperate words, removes m_, etc...
/// @param name the name of the variable
const char *MangleVariableName (const char *name);

/// Get the name of the object referenced by instance id
/// @param instanceID the instanceID of the object to get. Use object->GetInstanceID(), or PPtr.GetInstanceID()
/// @return the name of the object
std::string GetObjectName (int instanceID);
/// Set the name of an object.
/// @param instanceID the instanceID of the object to get. Use object->GetInstanceID(), or PPtr.GetInstanceID()
/// @param name the name to set
void SetObjectName (int instanceID, const std::string& name);

/// Get a name to use for storing autosave prefs for an object.
/// Used by inspectorviews, but included here just in case
/// @param instanceID the instanceID of the object to get. Use object->GetInstanceID(), or PPtr.GetInstanceID()
std::string GetAutosaveName (int instanceID);

/// Returns a nice looking name of the object when displaying PPtrs
std::string GetPropertyEditorPPtrName (int instanceID, const std::string& className);

/// Returns a nice looking class name or script name
std::string GetNiceObjectType (Object* obj);


/// Returns a nice looking name of the title for this inspector title bar
std::string GetPropertyEditorTitle (Object* obj);

std::string GetDragAndDropTitle (Object* o);