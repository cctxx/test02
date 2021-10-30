#include "UnityPrefix.h"
#include "AssetDatabaseProperty.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/Utility/MiniAssetIconCache.h"

#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"

AssetDatabaseProperty::AssetDatabaseProperty ()
{
	Reset ();
}

void AssetDatabaseProperty::Reset ()
{
	AssetDatabase& database = AssetDatabase::Get();
	m_Asset = &database.GetRoot();
	m_GUID = kAssetFolderGUID;
	m_LibraryObject = NULL;
	m_IndexStack = std::stack<int> ();
	m_Row = -1;
}

bool AssetDatabaseProperty::HasChildren ()
{
	if (m_LibraryObject)
		return false;
	else if (m_Asset)
		return !m_Asset->children.empty() || !m_Asset->representations.empty();
	else
		return false;
}

bool AssetDatabaseProperty::DoNext (bool enterChildren)
{	
	AssetDatabase& database = AssetDatabase::Get();

	if (m_LibraryObject != NULL)
	{
		m_IndexStack.top()++;
	
		// Continue iterating library object children
		if (m_IndexStack.top() < m_Asset->representations.size())
		{
			m_LibraryObject = &m_Asset->representations[m_IndexStack.top()];
			return true;
		}
		// Exit iterating children
		else
		{
			m_LibraryObject = NULL;
			m_IndexStack.pop();
		}
	}
	else
	{
		// Enter into child
		if (enterChildren && !m_Asset->children.empty())
		{
			m_IndexStack.push(0);
			m_GUID = m_Asset->children[0];
			m_Asset = database.AssetPtrFromGUID(m_GUID);
			return true;
		}
		else if (enterChildren && !m_Asset->representations.empty())
		{
			m_IndexStack.push(0);
			m_LibraryObject = &m_Asset->representations[0];
			return true;
		}
	}
	
	// Go to next element and keep exiting out of the tree whereever we reach the end of parent's children
	const Asset* parent = database.AssetPtrFromGUID(m_Asset->parent);
	if (parent == NULL)
		return false;
	
	m_IndexStack.top()++;	
	
	while (m_IndexStack.top() == parent->children.size())
	{
		if (m_IndexStack.size() <= 1)
			return false;
		
		m_IndexStack.pop();
		m_IndexStack.top()++;
		parent = database.AssetPtrFromGUID(parent->parent);
		if (parent == NULL)
			return false;
	}
	
	m_GUID = parent->children[m_IndexStack.top()];
	m_Asset = database.AssetPtrFromGUID(m_GUID);
	
	return m_Asset != NULL;
}

const AssetLabels* AssetDatabaseProperty::GetLabels () {
	if ( m_Asset ) 
		return &(m_Asset->labels);
	return NULL;
}

const char* AssetDatabaseProperty::GetName ()
{
	if (m_LibraryObject)
		return m_LibraryObject->name.c_str();
	else if (m_Asset)
		return m_Asset->mainRepresentation.name.c_str();
	else
		return "";
}

bool AssetDatabaseProperty::HasFullPreviewImage () const
{
	UInt16 flags;
	if (m_LibraryObject)
		flags = m_LibraryObject->flags;
	else if (m_Asset)
		flags = m_Asset->mainRepresentation.flags;
	else
		flags = 0;
	return flags & LibraryRepresentation::kHasFullPreviewImage;
}

BaseHierarchyProperty::IconDrawStyle AssetDatabaseProperty::GetIconDrawStyle () const
{
	int classID = GetClassID ();
	if (classID == ClassID(Texture2D) || classID == ClassID(Sprite))
		return kTexture;
	else
		return kNonTexture;
}

int AssetDatabaseProperty::GetInstanceID () const
{
	if (m_LibraryObject)
		return m_LibraryObject->object.GetInstanceID();
	else if (m_Asset)
		return m_Asset->mainRepresentation.object.GetInstanceID();
	else
		return 0;
}

bool AssetDatabaseProperty::IsSpecialSearchType (SpecialSearchType specialSearchType) const
{
	switch (specialSearchType)
	{
	case kPrefab:	return (GetClassID() == ClassID(GameObject)) && (m_Asset->type == kSerializedAsset);
	case kModel:	return (GetClassID() == ClassID(GameObject)) && (m_Asset->type == kCopyAsset);
	default:		LogString (Format ("IsSpecialSearchType: enum value not handled %d", specialSearchType)); break;
	}
	return false;
}

int AssetDatabaseProperty::GetColorCode ()
{
	return 0;
}
Texture* AssetDatabaseProperty::GetCachedIcon ()
{
	const LibraryRepresentation* rep = m_LibraryObject;
	if (m_LibraryObject)
		rep = m_LibraryObject;
	else if (m_Asset)
		rep = &m_Asset->mainRepresentation;
	else
		return NULL;
	
	return GetCachedAssetDatabaseIcon(m_GUID, *m_Asset, *rep);
}

int AssetDatabaseProperty::GetClassID () const
{
	if (m_LibraryObject)
		return m_LibraryObject->classID;
	else if (m_Asset)
		return m_Asset->mainRepresentation.classID;
	else
		return 0;
}

string AssetDatabaseProperty::GetScriptFullClassName()
{
	if (m_LibraryObject)
		return m_LibraryObject->scriptClassName;
	else if (m_Asset)
		return m_Asset->mainRepresentation.scriptClassName;
	else
		return string();
}

bool AssetDatabaseProperty::IsExpanded (int* expanded, int expandedSize)
{
	if (!HasChildren ())
		return false;

	if (expanded == NULL || m_Asset == &AssetDatabase::Get().GetRoot())
		return true;
	else
	{
		//@TODO: MAKE EXPANDED BE SORTED AND DO BINARY SEARCH
		int instanceID = GetInstanceID();
		for (int i=0;i<expandedSize;i++)
		{
			if (instanceID == expanded[i])
				return true;
		}
		return false;
	}
}

bool AssetDatabaseProperty::Previous (int* expanded, int expandedSize)
{
	const Asset* nextAsset = m_Asset;
	const LibraryRepresentation* nextLibrary = m_LibraryObject;
	int previousInstanceID = 0;
	
	Reset ();
	while (Next (expanded, expandedSize))
	{
		if (m_Asset == nextAsset && m_LibraryObject == nextLibrary)
			return Find(previousInstanceID, expanded, expandedSize);
		
		previousInstanceID = GetInstanceID();
	}
	return false;
}

bool AssetDatabaseProperty::Parent ()
{
	m_Row = -1;
	if (m_LibraryObject)
	{
		m_LibraryObject = NULL;
		Assert(!m_IndexStack.empty());
		m_IndexStack.pop();
		return true;
	}
	else if (m_Asset)
	{
		m_GUID = m_Asset->parent;
		m_Asset = AssetDatabase::Get().AssetPtrFromGUID(m_GUID);
		
		if (m_IndexStack.size() <= 1 || m_Asset == NULL)
			return false;

		Assert(!m_IndexStack.empty());
		m_IndexStack.pop();
		return true;
	}
	else
		return false;
}

vector<int> AssetDatabaseProperty::GetAncestors()
{
	vector<int> result;
	result.reserve(m_IndexStack.size());
	if (m_LibraryObject && m_Asset) 
		result.push_back(m_Asset->mainRepresentation.object.GetInstanceID());
	
	const Asset* ancestorAsset;
	for ( UnityGUID ancestorGuid = m_Asset->parent; ancestorGuid != UnityGUID() && ancestorGuid != kAssetFolderGUID ; ancestorGuid = ancestorAsset->parent )
	{
		ancestorAsset = AssetDatabase::Get().AssetPtrFromGUID(ancestorGuid);
		if (! ancestorAsset )
			break;
		
		result.push_back(ancestorAsset->mainRepresentation.object.GetInstanceID());
	}
	return result;
}

