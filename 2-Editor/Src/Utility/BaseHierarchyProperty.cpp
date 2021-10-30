#include "UnityPrefix.h"
#include "Runtime/Utilities/Word.h"
#include "BaseHierarchyProperty.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/vector_set.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/Utility/AssetReferenceFilter.h"

std::map <std::string, ScriptingClassPtr> BaseHierarchyProperty::s_FullNameToScriptingClassMap;

namespace 
{
	std::string kAssetSearchTypeNames [BaseHierarchyProperty::kNumSpecialSearchTypes] = { "prefab", "model"};

	void SplitFullNameIntoNamespaceAndClassName (const std::string &fullName, std::string &outNamespace, std::string &outClassName)
	{
		string::size_type pos = fullName.rfind('.');
		if (pos != string::npos)
		{
			// Found namespace
			outNamespace.assign(fullName.begin(), fullName.begin() + pos);
			outClassName.assign(fullName.begin() + pos + 1, fullName.end());
		}
		else
		{
			// Found no namespace
			outNamespace = "";
			outClassName = fullName;
		}
	}

	static std::string Combine (const std::string& nameSpace, const std::string& className) 
	{
		if (nameSpace.empty ())
			return className;
		return nameSpace + "." + className;
	}
}



BaseHierarchyProperty::BaseHierarchyProperty () 
{ 
	m_HasAnyFilters = false; 
	m_ShowAllHits = false;
}

BaseHierarchyProperty::~BaseHierarchyProperty ()
{}

std::string ClassNameRemapping (const std::string& className)
{
	// Assumes that className is lowercase
	if (className == "animation")
		return "animationclip";
	else if (className == "scene")
		return "sceneasset";
	else if (className == "script")
		return "monoscript";
	else if (className == "terrain")
		return "terraindata";
	return className;
}

void BaseHierarchyProperty::SetSearchFilter (	const std::vector<std::string>& nameFilters,
												const std::vector<std::string>& classNames,
												const std::vector<std::string>& assetLabels,
												const std::vector<int>& referencedInstanceIDs,
												bool showAllHits
												)
{
	m_ShowAllHits = showAllHits;

	m_NameFilters.clear();
	for (unsigned i=0; i<nameFilters.size(); ++i)
		m_NameFilters.push_back ( ToLower (nameFilters[i]).c_str());  //  Lower case because search is case insensitive 
	m_ClassIDs.clear ();
	m_ScriptClassNames.clear ();
	m_ScriptClassPtrs.clear ();
	m_SpecialSearchTypes.clear ();
	for (unsigned i=0; i<classNames.size(); ++i)
	{
		string lowerCaseClassName = ToLower (classNames[i]).c_str();
		SpecialSearchType specialSearchType = GetSpecialAssetSearchType (lowerCaseClassName);
		if (specialSearchType != kNotSpecialType)
		{
			m_SpecialSearchTypes.push_back (specialSearchType);
		}
		else 
		{
			lowerCaseClassName = ClassNameRemapping (lowerCaseClassName);
			bool added = false;
			int classID = Object::StringToClassIDCaseInsensitive(lowerCaseClassName.c_str());
			if (classID >= 0)
			{
				m_ClassIDs.push_back (classID);
				added = true;
			} 
			
			// Namespace handling
			string nameSpace;
			string className;
			SplitFullNameIntoNamespaceAndClassName (classNames[i], nameSpace, className);
			if (!added)
			{
				// Try with given namespace (can be without namespace here)
				if (ScriptingClassPtr classPtr = GetMonoManager().GetMonoClassCaseInsensitive (className.c_str(), nameSpace.c_str()))
				{
					m_ScriptClassNames.push_back (classNames[i].c_str());
					m_ScriptClassPtrs.push_back (classPtr);
					added = true;
				}
			}

			if (!added)
			{
				// If className was not found with given namespace we try any namespace (this finds e.g UnityEngine.GUISkin if just 'guiskin' was given.  Useful if no namespace or incorrect namespace)
				if (ScriptingClassPtr classPtr = GetMonoManager().GetMonoClassCaseInsensitive (className.c_str()))
				{
					m_ScriptClassNames.push_back (className.c_str());
					m_ScriptClassPtrs.push_back (classPtr);
					added = true;
				}
			}

			if (!added)
			{
				m_ClassIDs.push_back (-1); // Add invalid classID to ensure that filtering is not disabled: we want empty search result for invalid types.
			}	
		}	
	}

	m_AssetLabels.clear();
	for (unsigned i=0; i<assetLabels.size(); ++i)
		m_AssetLabels.push_back(assetLabels[i].c_str());

	m_ReferencedInstanceIDs.clear();
	for (unsigned i=0; i<referencedInstanceIDs.size(); ++i)
		m_ReferencedInstanceIDs.push_back(referencedInstanceIDs[i]);

	m_HasAnyFilters =	!m_NameFilters.empty() || 
						!m_ClassIDs.empty() || 
						!m_SpecialSearchTypes.empty() || 
						!m_ScriptClassNames.empty() || 
						!m_AssetLabels.empty() || 
						!m_ReferencedInstanceIDs.empty();
	
	// Reset visible flags, if filters are empty
	if (CanAccessObjectsWhenFiltering() && !m_HasAnyFilters)
	{
		std::vector<GameObject*> objects;
		Object::FindObjectsOfType (&objects);
		for (int i=0;i<objects.size();i++)
		{
			GameObject* o = objects[i];
			o->SetMarkedVisible(GameObject::kSelfVisible);
		}
	}
}

BaseHierarchyProperty::SpecialSearchType BaseHierarchyProperty::GetSpecialAssetSearchType (const std::string& text) const
{
	// Assumes that text is lowercase
	for (unsigned i=0; i<kNumSpecialSearchTypes; ++i)
		if (text == kAssetSearchTypeNames[i])
			return (SpecialSearchType)i;
	return kNotSpecialType;
}

bool IsMonoClassDerivedFromClass (MonoClass* klass, MonoClass* searchForThisMonoClass)
{
	while (klass != NULL)
	{
		if (klass == searchForThisMonoClass)
			return true;
		
		klass = mono_class_get_parent(klass);
	}
	return false;
}

bool IsDerivedFromClassIDSafe (int classID, int compareClassID)
{
	if (compareClassID == -1 || classID == -1)
		return false;
	return Object::IsDerivedFromClassID(classID, compareClassID);
}

MonoClass* BaseHierarchyProperty::GetScriptingClassFromFullName (const std::string& fullName)
{
	std::map<std::string, MonoClass*>::iterator it = s_FullNameToScriptingClassMap.find(fullName);
	if (it != s_FullNameToScriptingClassMap.end())
		return it->second;

	// Cache monoclass for later searches
	string nameSpace, className;
	SplitFullNameIntoNamespaceAndClassName (fullName, nameSpace, className);
	MonoClass* monoClass = GetMonoManager().GetMonoClass(className.c_str(), nameSpace.c_str());
	s_FullNameToScriptingClassMap[fullName] = monoClass;
	return monoClass;
}


bool BaseHierarchyProperty::FilterByType ()
{
	int cid = GetClassID();

	if (GetHierarchyType() == kHierarchyAssets )
	{
		// When searching in the project window we don't want to show asset child gameobjects in prefabs,
		// We only want to show the mainAsset game object but we dont want to show generated textures like for fonts
		if ((cid == ClassID(GameObject) || cid == ClassID(Texture2D)) && !IsMainAssetRepresentation())
			return false;
	}
	
	// Early out if no type filters
	if (m_ClassIDs.empty() && m_ScriptClassNames.empty() && m_SpecialSearchTypes.empty ())
		return true;

	if (cid == ClassID(MonoBehaviour))
	{
		// Check MonoBehaviour assets like guiskin
		if (GetHierarchyType() == kHierarchyAssets)
			for (unsigned i=0; i<m_ScriptClassPtrs.size(); ++i)
			{
				ScriptingClassPtr classPtr = GetScriptingClassFromFullName (GetScriptFullClassName());
				if (classPtr != NULL && IsMonoClassDerivedFromClass(classPtr, m_ScriptClassPtrs[i]))
					return true;
			}
	}
	else
	{
		// Check classIDs
		for (unsigned i=0; i<m_ClassIDs.size(); ++i)
			if (IsDerivedFromClassIDSafe (cid, m_ClassIDs[i]))
				return true;
	}

	// Check special asset search types
	for (unsigned i=0; i<m_SpecialSearchTypes.size(); ++i)
		if (IsSpecialSearchType (m_SpecialSearchTypes[i]))
			return true;


	// In game object hierarchy also check all components attached to the game object
	if (CanAccessObjectsWhenFiltering())
	{
		GameObject* go = dynamic_pptr_cast<GameObject*>(Object::IDToPointer(GetInstanceID()));
		if (go)
		{
			for (int cmpIndex=0; cmpIndex<go->GetComponentCount(); cmpIndex++)
			{
				Unity::Component &cmp = go->GetComponentAtIndex(cmpIndex);
				MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&cmp);
				if (behaviour)
				{
					for (unsigned i=0; i<m_ScriptClassPtrs.size(); ++i)
						if (IsMonoClassDerivedFromClass (behaviour->GetClass(), m_ScriptClassPtrs[i]))
							return true;
				}
				else
				{
					for (unsigned i=0; i<m_ClassIDs.size(); ++i)
						if (IsDerivedFromClassIDSafe(cmp.GetClassID(), m_ClassIDs[i]))
							return true;
				}
			}
		}
	}
	return false;	
}

bool BaseHierarchyProperty::FilterByAssetLabel ()
{
	if (m_AssetLabels.empty())
		return true;

	if (const AssetLabels* labels = GetLabels())
	{
		for (unsigned i=0; i<m_AssetLabels.size(); ++i)
			if (labels->Match (m_AssetLabels[i].c_str()))
				return true;
	}

	return false;
}

bool BaseHierarchyProperty::FilterByReferences ()
{
	if (m_ReferencedInstanceIDs.empty())
		return true;

	int instanceID = GetInstanceID();
	for (unsigned i=0; i<m_ReferencedInstanceIDs.size(); ++i)
		if (IsReferencing (instanceID, m_ReferencedInstanceIDs[i]))
			return true;

	return false;
}

bool BaseHierarchyProperty::FilterByName ()
{
	if (m_NameFilters.empty() )
		return true;

	FilterString lowerName = ToLower (FilterString (GetName ()));
	
	for (unsigned i=0; i<m_NameFilters.size(); ++i)
		if (lowerName.find (m_NameFilters[i]) == std::string::npos)
			return false;

	return true;

}

bool BaseHierarchyProperty::FilterSingle () 
{
	bool keep = false;

	if (m_ShowAllHits)
	{
		// Keep if just one of the active filters matches
		if (!keep && !m_NameFilters.empty())
			keep = FilterByName ();

		if (!keep && (!m_ClassIDs.empty() || !m_ScriptClassNames.empty()))
			keep = FilterByType ();

		if (!keep && !m_AssetLabels.empty())
			keep = FilterByAssetLabel ();

		if (!keep && !m_ReferencedInstanceIDs.empty())
			keep = FilterByReferences ();
	}
	else
	{
		// Keep only if all active filters are matched
		if (FilterByName ())
			if (FilterByType ())
				if (FilterByAssetLabel ())
						if (FilterByReferences ())
							keep = true;
	}

	if (CanAccessObjectsWhenFiltering() && GetClassID() == ClassID(GameObject))
	{
		GameObject* go = dynamic_pptr_cast<GameObject*>(Object::IDToPointer(GetInstanceID()));
		if (go)
			go->SetMarkedVisible( keep ? GameObject::kSelfVisible : GameObject::kNotVisible);
	}	
	
	return keep;
}


bool BaseHierarchyProperty::NextWithDepthCheck (int* expanded, int expandedSize, int minDepth)
{
	m_Row++;

	// Fast path for no filtering
	if (!m_HasAnyFilters)
	{
		bool hasNext = DoNext (IsExpanded(expanded, expandedSize));
		return hasNext && GetDepth () >= minDepth;
	}

	// Filtering, keep searching until we find an element that is not filtered or depth check fails
	while (true)
	{
		bool hasNext = DoNext (HasChildren());
		if (!hasNext || GetDepth () < minDepth)
			return false;

		if (FilterSingle())
			return true;
	}
}

bool BaseHierarchyProperty::Next (int* expanded, int expandedSize)
{
	m_Row++;
	
	// Fast path for no filtering
	if (!m_HasAnyFilters)
		return DoNext (IsExpanded(expanded, expandedSize));
	
	// Filtering, keep searching until we find an element that is not filtered
	while (true)
	{
		bool hasNext = DoNext (HasChildren());
		if (!hasNext)
			return false;
		
		if ( FilterSingle() )
			return true;
	}
}

std::vector<int> BaseHierarchyProperty::FindAllAncestors (const int* instanceIDsSource, int size)
{
	vector_set<int> instanceIDs;
	instanceIDs.assign(instanceIDsSource, instanceIDsSource + size);
	
	std::vector<int> ancestors;
	std::vector<int> localAncestors;
	
	Reset ();

	while (Next (NULL, 0))
	{
		if (instanceIDs.count (GetInstanceID()))
		{
			localAncestors = GetAncestors ();
			ancestors.insert(ancestors.end(), localAncestors.begin(), localAncestors.end());
		}
	}
	
	return ancestors;
}


bool BaseHierarchyProperty::Skip (int rows, int* expanded, int expandedSize)
{
	int index = 0;
	while (index != rows && Next (expanded, expandedSize))
		index++;
	return index == rows;
}

int BaseHierarchyProperty::CountRemaining (int* expanded, int expandedSize)
{	
	int index = 0;
	while (Next (expanded, expandedSize))
		index++;
	return index;
}


bool BaseHierarchyProperty::Find (int instanceID, int* expanded, int expandedSize)
{
	Reset ();

	if (instanceID == 0)
		return false;
		
	while (instanceID != GetInstanceID() && Next (expanded, expandedSize) )
		;

	return instanceID == GetInstanceID();
}

bool BaseHierarchyProperty::IsInExpandedArray(int instanceID, int* expanded, int expandedSize)
{
	//@TODO: MAKE EXPANDED BE SORTED AND DO BINARY SEARCH
	for (int i=0;i<expandedSize;i++)
	{
		if (instanceID == expanded[i])
			return true;
	}
	return false;
}
