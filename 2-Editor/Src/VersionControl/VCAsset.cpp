#include "UnityPrefix.h"
#include "VCAsset.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/Word.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Src/VersionControl/VCPluginProtocol.h"
#include <set>

namespace
{
	string GetCurrentProjectPath()
	{
		return AppendPathName(File::GetCurrentDirectory(), "/");
	}
}


VCAsset::VCAsset()
: m_State (kNone)
, m_Meta (false)
{
	SetPath("");
}

VCAsset::VCAsset (std::string const& clientPath)
: m_State(kNone)
, m_Meta (false)
{
	SetPath(clientPath);
}

VCAsset::VCAsset(VCAsset const& other)
{
	*this = other;
}

void VCAsset::SetPath (std::string const& path)
{
	m_Path = path;

	const string projectPath = ::GetCurrentProjectPath();
	if (IsChildOf(projectPath))
		m_Path = m_Path.substr(projectPath.length());

	replace_string(m_Path, "\\", "/");

	if (IsDirectoryCreated(m_Path) && m_Path[m_Path.size() - 1] != '/')
	{
		m_Path += "/";
	}

	m_Meta = EndsWith(m_Path, ".meta");
	if (m_Meta)
		AddState(kMetaFile);
	else
		RemoveState(kMetaFile);
}

string VCAsset::GetMetaPath() const
{
	if (m_Meta)
	{
		return m_Path;
	}
	else
	{
		if (EndsWith(m_Path, "/"))
			return GetTextMetaDataPathFromAssetPath(m_Path.substr(0, m_Path.length() - 1));
		else
			return GetTextMetaDataPathFromAssetPath(m_Path);
	}
}

string VCAsset::GetAssetPath() const
{
	if (m_Meta && EndsWith(m_Path, ".meta"))
	{
		string p = m_Path.substr(0, m_Path.length() - 5);
		if (IsDirectoryCreated(p))
			p += "/";
		return p;
	}
	else
	{
		return m_Path;
	}
}

// Test if this path is a child of a given parent
bool VCAsset::IsChildOf(string const& parent) const
{
	// All paths in the plugin are consistent and end with
	if (!EndsWith(parent, "/") || ToLower(parent) == ToLower(m_Path))
		return false;

	// Assumes case insensitivity for child/parent relationship.  Watch for side effects on OSX
	return BeginsWith(ToLower(m_Path), ToLower(parent));
}

bool VCAsset::IsChildOf(VCAsset const& parent) const
{
	// Parent must be a folder to have a child path relationship
	if (!parent.IsFolder() || parent.m_Path == m_Path)
		return false;

	return BeginsWith(ToLower(m_Path), ToLower(parent.m_Path));
}

bool VCAsset::IsInCurrentProject () const
{
	return IsChildOf(GetCurrentProjectPath());
}

void GetAncestorFolders(const VCAsset& asset, VCAssetSet& ancestors)
{
	vector<string> toks;
	Split(asset.GetPath(), "\\/", toks);
	if (!asset.IsFolder())
		toks.pop_back(); // last entry is definitely not a folder
	
	string projPath = GetCurrentProjectPath();
	
	string path;
	for (vector<string>::const_iterator ti = toks.begin(); ti != toks.end(); ++ti)
	{
		VCAsset a;
		path += *ti + "/";
		a.SetPath(path);
		if (!BeginsWith(a.GetPath(), projPath) || a.GetPath().length() > projPath.length())
			ancestors.insert(a);
	}
}

std::string VCAsset::GetName () const
{
	return DeletePathNameExtension(GetLastPathNameComponent(m_Path));
}

std::string VCAsset::GetFullName () const
{
	return GetLastPathNameComponent(m_Path);
}

void VCAsset::PathAppend (std::string const& append)
{
	if (!IsFolder())
	{
		WarningString("Can only append to a folder");
		return;
	}

	// Change to local state.  Will be changed back to folder if appended path was a folder too
	m_State = kLocal;
	SetPath(m_Path + append);
}

States VCAsset::GetState () const
{
	return m_State;
}

void VCAsset::SetState (States newState)
{
	m_State = newState;
}

const VCAsset& VCAsset::operator= (VCAsset const& rhs)
{
	m_State = rhs.m_State;
	m_Path = rhs.m_Path;
	m_Action = rhs.m_Action;
	m_Meta = rhs.m_Meta;
	m_guid = rhs.m_guid;
	return *this;
}

void VCAsset::Copy (const VCAsset& other)
{
	*this = other;
}

bool VCAsset::operator<(const VCAsset& o) const
{
	string pa = GetPath();
	if (IsFolder())
		pa.resize(pa.size() - 1);
	
	string pb = o.GetPath();
	if (o.IsFolder())
		pb.resize(pb.size() - 1);
	
	return pa < pb;
}

// @TODO: handle fetching added recursive asset states
void VCAssetList::AddRecursive(const VCAsset& asset)
{
	if (IsAbsoluteFilePath(asset.GetPath()) && !BeginsWith(asset.GetPath(), GetCurrentProjectPath()))
		return; // only add files in the project folder
		
	push_back(asset);

	if (!asset.IsFolder())
	{
		return;
	}

	// Scan directory and add assets
	std::set<std::string> paths;
	GetFolderContentsAtPath(asset.GetPath(), paths);

	for (std::set<std::string>::const_iterator i = paths.begin(); i != paths.end(); ++i)
	{
		const string& path = *i;

		// TODO: Also skip ReadOnly, System and Hidden fiels
		if (path.empty() || path[0] == '.' || (path.length() > 5 && path.substr(path.length()-5) == ".meta")) continue;
		AddRecursive(VCAsset(path));
	}
}

void VCAssetList::ReplaceWithMeta()
{
	VCAssetList tmp;
	GetMeta(tmp);
	swap(tmp);
}

void VCAssetList::GetMeta(VCAssetList& result) const
{
	result.clear();

	for (const_iterator i = begin(); i != end(); ++i)
	{
		const VCAsset& a = *i;
		if (a.IsMeta()) continue;

        UnityStr metaPath = a.GetMetaPath();
        if (metaPath == "Assets.meta" || !BeginsWith(metaPath, "Assets/"))
            continue;
		
        VCAsset meta(metaPath);
		meta.SetState(i->IsUnderVersionControl() ? kOutOfSync : kNone);
		result.push_back(meta);
	}
}

void VCAssetList::IncludeMeta()
{
	VCAssetList tmp;
	CopyWithMeta(tmp);
	swap(tmp);
}

void VCAssetList::GetNonMeta(VCAssetList& result) const
{
	result.clear();
	
	for (const_iterator i = begin(); i != end(); ++i)
	{
		if (!i->IsMeta())
			result.push_back(*i);
	}
}

void VCAssetList::RemoveMeta()
{
	VCAssetList result;
	GetNonMeta(result);
	swap(result);
}

/*
struct VCAssetListComparer
{
	bool operator()(const VCAsset& a, const VCAsset& b) const
	{
		string pa = a.GetPath();
		if (a.IsFolder())
			pa.resize(pa.size() - 1);

		string pb = b.GetPath();
		if (b.IsFolder())
			pb.resize(pb.size() - 1);
		
		return pa < pb;
	}
};
*/
void VCAssetList::SortByPath()
{
	sort(begin(), end());
}

void VCAssetList::RemoveDuplicates()
{
	std::set<VCAsset> unique(begin(), end());
	VCAssetList result;
	result.insert(result.end(), unique.begin(), unique.end());
	swap(result);
}

VCAssetList::const_iterator VCAssetList::FindOldestAncestor(const VCAsset& asset) const
{
	const_iterator result = end();
	
	VCAsset bestMatchSoFar(asset);
	
	for (const_iterator i = begin(); i != end(); ++i)
	{
		if (asset.IsChildOf(*i) && bestMatchSoFar.IsChildOf(*i))
		{
			bestMatchSoFar = *i;
			result = i;
		}
	}
	return result;
}

VCPluginSession& operator<<(VCPluginSession& p, const VCAsset& v)
{
	p.WriteLine(AppendPathName(::GetCurrentProjectPath(), v.GetPath()));
	p.WriteLine(IntToString(v.GetState()));
	return p;
}

VCPluginSession& operator>>(VCPluginSession& p, VCAsset& asset)
{
	asset.SetPath(p.ReadLine());
	int state = StringToInt(p.ReadLine());
	asset.SetState((States) state);
	return p;
}

void VCAssetList::CopyWithMeta(VCAssetList& result) const
{
	result.clear();

	VCAssetSet unique(begin(), end());

	for (const_iterator i = begin(); i != end(); ++i)
	{
		const VCAsset& a = *i;
		result.push_back(a);

		// Insert a meta file for all assets in the list that
		// doesn't already have its meta file in the list.
		UnityStr metaPath = a.GetMetaPath();
		if (metaPath == "Assets.meta" || !BeginsWith(metaPath, "Assets/"))
			continue;

		VCAsset meta(metaPath);
		if (!a.IsMeta() && unique.find(meta) == unique.end())
		{
			meta.SetState(a.IsUnderVersionControl() ? kOutOfSync : kNone);
			result.push_back(meta);
		}
	}
}

void VCAssetList::Filter(VCAssetList& result, bool includeFolder,
						 int includeStates, int excludeStates) const
{
	result.clear();

	if (!includeFolder && includeStates == kNone)
		return;

	for (const_iterator i = begin(); i != end(); ++i)
	{
		const bool folder = i->IsFolder();
		const bool a = (i->GetState() & includeStates) || includeStates == kAny; // last clause there because kNone == 0
		const bool b = !(i->GetState() & excludeStates);

		if ( (folder && includeFolder) || (!folder && a && b ) )
			result.push_back(*i);
	}
}

size_t VCAssetList::FilterCount(bool includeFolder,
								int includeStates, int excludeStates) const
{
	if (!includeFolder && includeStates == kNone)
		return size();

	size_t num = 0;
	for (const_iterator i = begin(); i != end(); ++i)
	{
		bool folder = i->IsFolder();
		const bool a = (i->GetState() & includeStates) || includeStates == kAny; // last clause there because kNone == 0
		const bool b = !(i->GetState() & excludeStates);

		if ( (folder && includeFolder) || (!folder && a && b ) )
			++num;
	}
	return num;
}

void VCAssetList::FilterChildren(VCAssetList& result) const
{
	result.clear();

	VCAssetSet unique(begin(), end());

	for (const_iterator i = begin(); i != end(); ++i)
	{
		// Only folders can contain child assets
		if (!i->IsFolder())
			continue;

		for (const_iterator j = begin(); j != end(); ++j)
		{
			if (j->IsChildOf(*i))
				unique.erase(*j);
		}
	}

	result.insert(result.begin(), unique.begin(), unique.end());
}

void VCAssetList::IncludeFolders()
{
	VCAssetSet unique(begin(), end());
	VCAssetList result;
	
	for (const_iterator i = begin(); i != end(); ++i)
	{
		if (i->IsMeta())
		{
			string nonMetaName = i->GetAssetPath();
			VCAsset a(nonMetaName);
			if (EndsWith(nonMetaName, "/") && unique.find(a) == unique.end())
				result.push_back(a);
		}
		result.push_back(*i);
	}
	
	result.swap(*this);
}

void VCAssetList::IncludeAncestors()
{
	VCAssetSet unique(begin(), end());
	GetAncestors(unique);
	clear();
	insert(begin(), unique.begin(), unique.end());
}

void VCAssetList::GetAncestors(VCAssetSet& unique) const
{
	for (const_iterator i = begin(); i != end(); ++i)
		GetAncestorFolders(*i, unique);
}

