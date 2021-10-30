#include "UnityPrefix.h"
#include "ObjectNames.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Animation/Animation.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/AssetPipeline/SubstanceImporter.h"
#include "Editor/Src/Prefabs/Prefab.h"

using namespace std;


/// Mangles a typical propertyname to a more readable format.
/// e.g. "m_LodMesh" becomes "Lod Mesh"
const char *MangleVariableName (const char *in) {
	const int kBufSize = 1024;
	static char buf[kBufSize];
	// Skip m_ at start
	if (in[0] == 'm' && in[1] == '_')
		in += 2;
	else if (in[0] == '_')
		in += 1;
	// Skip 'k' in kConstants at start
	if (in[0] == 'k' && (in[1] >= 'A' && in[1] <= 'Z'))
		in += 1;
		
	char *out = buf;
	// Make first character upper case
	if (*in >= 'a' && *in <= 'z')
	{
		*out = *in - ('a'-'A');
		++in; ++out;
	}

	// Add spaces between lowercase/uppercase characters and copy from out to in
	bool prevCapi = true;
	while (*in && out - buf < kBufSize - 1)
	{
		char c = *in;
		// if this is this capital or number and previous was not - insert space
		if( (c >= 'A' && *in <= 'Z') || (c >= '0' && c <= '9') )
		{
			if( !prevCapi )
				*out++ = ' ';
			prevCapi = true;
		} 
		else
		{
			if( c >= 'a' && c <= 'z' )
				prevCapi = false;
			else
				prevCapi = true; // don't ever insert space after non-letters
		}
		*out++ = c;
		++in;
	}
	
	*out = 0;
	
	return buf;
}

string GetObjectName (int instanceID)
{
	Object* o = PPtr<Object> (instanceID);
	if (o == NULL)
		return string ();
	return o->GetName ();
}

void SetObjectName (int instanceID, const std::string& name)
{
	Object* o = PPtr<Object> (instanceID);
	if (o != NULL)
		o->SetNameCpp (name);
}

string GetPropertyEditorPPtrName (int instanceID, const std::string& className)
{
	string title;
	if (instanceID == 0)
	{
		title = "None (" + string (MangleVariableName (className.c_str())) + ")";
		return title;
	}
	Object* o = PPtr<Object> (instanceID);
	if (o == NULL)
	{
		title = "Missing (" + string (MangleVariableName (className.c_str())) + ")";
		return title;
	}
	
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (o);
	title = o->GetName ();
	// gameobject: name only
	// pythonbehaviour: name (Scriptname)
	// components: name (component)
	// others: name only
	if( behaviour && behaviour->GetScript().IsValid() )
		title += " (" + string (behaviour->GetScript ()->GetScriptClassName ()) + ")";
	else if (dynamic_pptr_cast<Unity::Component*> (o))
		title += " (" + string (MangleVariableName (o->GetClassName ().c_str())) + ")";
		
	return title;
}

string GetDragAndDropTitle (Object* o) {
	if (o == NULL)
		return "Nothing Selected";
	
	return Append (o->GetName(), " (") + o->GetClassName () + ")";
}

string GetPropertyEditorTitle (Object* o)
{
	if (o == NULL)
		return "Nothing Selected";
	
	string title;
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (o);
	MeshFilter* meshfilter = dynamic_pptr_cast<MeshFilter*> (o);
	Animation* anim = dynamic_pptr_cast<Animation*> (o);
	AssetImporter* importer = dynamic_pptr_cast<AssetImporter*> (o);
	SubstanceImporter* substanceImporter = dynamic_pptr_cast<SubstanceImporter*> (o);

	// gameobject: name only
	// python: scriptname (Python)
	// mesh: meshname (Mesh)
	// components: component name
	// others: name (className)
	if( behaviour && behaviour->GetScript().IsValid() )
	{
		title = Append (behaviour->GetScript ()->GetName(), " (Script)");
	}
	// Is MonoImporter
	else if (o->GetClassID() == CLASS_MonoImporter)
	{
		AssetImporter* monoImporter = dynamic_pptr_cast<AssetImporter*> (o);

		MonoScript* script = AssetImporter::GetFirstDerivedObjectAtPath<MonoScript>(monoImporter->GetAssetPathName());
		if (script)
			return "Default References (" +  string(script->GetName()) + ")";
		else
			return "Default References ()";
	}
	else if (meshfilter)
	{
		Object* mesh = PPtr<Object> (meshfilter->GetSharedMesh ().GetInstanceID());
		if (mesh)
			title = Append (mesh->GetName(), " (MeshFilter)");
		else
			title = "MeshFilter: [none]";
	}
	else if (dynamic_pptr_cast<GameObject*> (o))
		title = o->GetName();
	else if (dynamic_pptr_cast<Unity::Component*> (o))
		title = o->GetClassName ();
	else if (anim) {
		Object* clip = dynamic_instanceID_cast<Object*> (anim->GetClip ().GetInstanceID());
		if (clip)
			title = Append ("Animation: ", clip->GetName());
		else
			title = "Animation: [none]";
	}
	else if (substanceImporter)
	{
		Object* assetObject = AssetImporter::GetFirstDerivedObjectAtPath<SubstanceArchive>(substanceImporter->GetAssetPathName());
		if (assetObject)
			title = string(assetObject->GetName()) + " (Substance Archive)";
		else
			title = o->GetClassName ();
	}
	// Importers don't have names. Just show the type without paranthesis (like we do for components).
	else if (importer)
		title = o->GetClassName ();
	// Show "Tags & Layers" instead of "TagManager"
	else if (o->GetClassID() == CLASS_TagManager)
		title = "Tags & Layers";
	else
		title = std::string(o->GetName()) + " (" + o->GetClassName () + ")";
		
	title = MangleVariableName (title.c_str());
	
	return title;
}

string GetAutosaveName (int instanceID) {
	Object* o = PPtr<Object> (instanceID);
	if (o == NULL)
		return "Nothing Selected";
	
	string classString;
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (o);
	if( behaviour && behaviour->GetScript().IsValid() )
		classString = behaviour->GetScript ()->GetName();
	else
		classString = o->GetClassName ();
		
	return classString;	
}


std::string GetNiceObjectType (Object* obj)
{
	MonoBehaviour* monobeh = dynamic_pptr_cast<MonoBehaviour*>(obj);
	if (monobeh != NULL)
		return monobeh->GetScriptFullClassName();
	else if (obj)
		return obj->GetClassName();
	else
		return "Missing";
}
