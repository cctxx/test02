#pragma once

class Texture;

#include "Runtime/Math/Color.h"
#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetPipeline/AssetLabels.h"
#include "Runtime/Scripting/ScriptingObjectOfType.h"



class BaseHierarchyProperty
{
	virtual bool DoNext (bool enterChildren) = 0;
	
public:
	typedef UNITY_STRING(kMemEditorUtility) FilterString;

	enum HierarchyType { kHierarchyAssets=1, kHierarchyGameObjects=2 };
	
	BaseHierarchyProperty ();
	virtual ~BaseHierarchyProperty ();
	
	virtual void Reset () = 0;

	virtual bool HasChildren () = 0;

	virtual bool IsExpanded (int* expanded, int expandedSize) = 0;
	virtual bool IsMainAssetRepresentation () const { return true; }

	virtual bool HasFullPreviewImage () const { return false; }

	// Enum used for asset types that do not have a unique classID
	enum IconDrawStyle
	{
		kNonTexture         = 0,
		kTexture            = 1
	};
	virtual IconDrawStyle GetIconDrawStyle () const { return kNonTexture; }
	
	virtual UnityGUID GetGUID () = 0;
	
	virtual bool Previous (int* expanded, int expandedSize) = 0;
	virtual bool Parent () = 0;
	virtual std::vector<int> GetAncestors () = 0;
	virtual int GetColorCode () = 0;
	
	virtual int GetInstanceID () const = 0;
	virtual int GetDepth () = 0;
	virtual bool IsValid () = 0;
	virtual int GetClassID () const = 0;
	virtual const AssetLabels* GetLabels() = 0;
	virtual std::string GetScriptFullClassName() = 0;
	
	virtual Texture* GetCachedIcon () = 0;

	virtual bool Skip (int rows, int* expanded, int expandedSize);
	virtual int CountRemaining (int* expanded, int expandedSize);
	bool Find (int instanceID, int* expanded, int expandedSize) ;

	std::vector<int> FindAllAncestors (const int* instanceIDs, int size);
	
	bool FilterSingle() ;
	bool Next (int* expanded, int expandedSize);									// Iterate tree either from start (using Reset()) or from a certain property (using Find())
	bool NextWithDepthCheck (int* expanded, int expandedSize, int minDepth);		// Iterate subtree: Use minDepth to ensure iteration is stopped when leaving minDepth 
	int GetRow () { return m_Row; }
	
	virtual const char* GetName () = 0;

	virtual bool CanAccessObjectsWhenFiltering () { return false; }
	virtual HierarchyType GetHierarchyType () = 0;
	virtual bool IsFolder () { return false;}

	// Enum used for asset types that do not have a unique classID
	enum SpecialSearchType
	{
		kNotSpecialType = -1,
		kPrefab,
		kModel,
		kNumSpecialSearchTypes // must be last
	};
	virtual bool IsSpecialSearchType (SpecialSearchType specialSearchType) const { return false;}
	SpecialSearchType GetSpecialAssetSearchType (const std::string& text) const;

	void SetSearchFilter (	const std::vector<std::string>& nameFilters,	
							const std::vector<std::string>& classNames,   // Can be builtin class names and/or script class names
							const std::vector<std::string>& assetLabels,
							const std::vector<int>& referencedInstanceIDs,
							bool showAllHits
							);

	static void ClearNameToScriptingClassMap() {s_FullNameToScriptingClassMap.clear();}
	static ScriptingClassPtr GetScriptingClassFromFullName (const std::string& fullName);

protected:
	static bool IsInExpandedArray(int instanceID, int* expanded, int expandedSize);

	// Map used for caching monoclasses so we do not have to look them up over and over again... Cleared and rebuild after assembly reloading
	static std::map<std::string, MonoClass*> s_FullNameToScriptingClassMap;

	// All filter functions below returns true if the filter is inactive or 
	// if the current property should be kept during a search
	bool FilterByName ();
	bool FilterByType ();
	bool FilterByAssetLabel ();
	bool FilterByReferences ();
	
	int m_Row;
	
	// Search data
	UNITY_VECTOR(kMemEditorUtility, FilterString)	 m_NameFilters;
	UNITY_VECTOR(kMemEditorUtility, int)			 m_ClassIDs;
	UNITY_VECTOR(kMemEditorUtility, SpecialSearchType) m_SpecialSearchTypes;
	UNITY_VECTOR(kMemEditorUtility, FilterString)	 m_ScriptClassNames;		// 1:1 with m_ScriptClassPtrs
	UNITY_VECTOR(kMemEditorUtility, ScriptingClassPtr) m_ScriptClassPtrs;		// 1:1 with m_ScriptClassNames, cached so we dont need too find it over and over again when iterating assets
	UNITY_VECTOR(kMemEditorUtility, FilterString)	 m_AssetLabels;
	UNITY_VECTOR(kMemEditorUtility, int)			 m_ReferencedInstanceIDs;
	bool											 m_HasAnyFilters;
	bool											 m_ShowAllHits;
};
