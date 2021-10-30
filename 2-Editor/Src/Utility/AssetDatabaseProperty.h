#pragma once

#include "Editor/Src/AssetPipeline/AssetDatabaseStructs.h"
#include "BaseHierarchyProperty.h"
#include <stack>
class Texture;

class AssetDatabaseProperty : public BaseHierarchyProperty
{
	const Asset* m_Asset;
	const LibraryRepresentation* m_LibraryObject;
	UnityGUID m_GUID;
	std::stack<int> m_IndexStack;
	
	bool DoNext (bool enterChildren);
	
	public:
	
	AssetDatabaseProperty ();
	void Reset ();

	virtual bool HasChildren ();

	virtual bool IsExpanded (int* expanded, int expandedSize);
	virtual bool IsMainAssetRepresentation () const { return m_LibraryObject == NULL; }
	virtual UnityGUID GetGUID () { return m_GUID; }
	
	virtual bool HasFullPreviewImage () const;
	virtual BaseHierarchyProperty::IconDrawStyle GetIconDrawStyle () const;
	
	/// The library reprsentation. Null if this is a main asset.
	virtual const LibraryRepresentation* GetChildLibraryRepresentation () { return m_LibraryObject; }
	
	virtual bool Previous (int* expanded, int expandedSize);
	virtual bool Parent ();
	virtual std::vector<int> GetAncestors();
	virtual int GetColorCode ();

	virtual int GetInstanceID () const;
	virtual int GetDepth () { return m_IndexStack.size() - 1; }
	virtual bool IsValid () { return !m_IndexStack.empty(); }

	virtual const AssetLabels* GetLabels ();
	
	virtual Texture* GetCachedIcon ();
	virtual HierarchyType GetHierarchyType () { return kHierarchyAssets; }
	virtual const char* GetName ();
	virtual int GetClassID () const;
	virtual string GetScriptFullClassName();
	virtual bool IsFolder () {return m_Asset->type == kFolderAsset;}

protected:
	virtual bool IsSpecialSearchType (SpecialSearchType specialSearchType) const;
};
