#ifndef __ASSET_SERVER_MONO_UTILITY_H
#define __ASSET_SERVER_MONO_UTILITY_H

#include "ASConfiguration.h"
#include "ASCache.h"
#include "ASController.h"
#include "ASAdmin.h"
#include "Runtime/Utilities/GUID.h"
#include "Editor/Src/AssetServer/Backend/ASBackend.h"

struct MonoString;
struct MonoArray;
struct ImportPackageAsset;



namespace AssetServer 
{
	struct MonoChangeset
	{
		int changeset;
		MonoString *message;
		MonoString *date;
		MonoString *owner;
		MonoArray* items;
	};


	struct MonoChangesetItem
	{
		MonoString *fullPath;
		MonoString *guid;
		MonoString *assetOperations;
		int assetIsDir;
		int changeFlags;
	};

	struct MonoDeletedAsset {
		int changeset;
		MonoString *guid;
		MonoString *parent;
		MonoString *name;
		MonoString *fullPath;
		MonoString *date;
		int assetIsDir;
	};

	struct ChangesetDummy
	{
		std::vector<Item> items;
		Changeset srcChangeset;
	};

	struct MonoMaintUserRecord
	{
		int enabled;
		MonoString *userName;
		MonoString *fullName;
		MonoString *email;
	};

	struct MonoMaintDatabaseRecord 
	{
		MonoString *name;
		MonoString *dbName;
	};

	struct MonoAssetsItem
	{
		MonoString *guid;
		MonoString *parentGuid;
		MonoString *pathName;
		MonoString *message;
		MonoString *exportedAssetPath;
		MonoString *guidFolder;
		int enabled;
		int assetIsDir;
		int changeFlags;
		MonoString *previewPath;
		int exists;

	};

	class ASMonoUtility 
	{
	public:
		/// Singleton object
		static ASMonoUtility & Get();

		ASMonoUtility();

		enum ASRefreshState { kASRefreshUpdate = 1, kASRefreshCommit = 2 };

		bool GetRefreshUpdate();
		bool GetRefreshCommit();

		void SetNeedsToRefreshUpdate() { m_RefreshUpdate = true; }
		void SetNeedsToRefreshCommit() { m_RefreshCommit = true; }

		// this tells up to what changeset items are shown currently in mono update window
		void SetLatestPassedChangeset(int changeset) {m_LatestChangesetPassedToMono = changeset;}

		void ClearRefreshUpdate() {m_RefreshUpdate = false;}
		void ClearRefreshCommit() {m_RefreshCommit = false;}

		void CheckForServerUpdates();


	private:
		// this marks when update and commit windows need to refresh
		// values are set from different places. 
		bool m_RefreshCommit;
		bool m_RefreshUpdate;
		int m_LatestChangesetPassedToMono;
	};

	MonoArray* ConvertToGUIDs (const std::set<UnityGUID>& guids);
	std::set<UnityGUID> ConvertToGUIDs (MonoArray* array);
	std::map<UnityGUID, DownloadResolution> ConvertToResolutions (MonoArray* guids, MonoArray* resolutions);
	std::map<UnityGUID, CompareInfo> ConvertToCompareInfo (MonoArray* guids, MonoArray* compInfos);

	void AssetsItemToMono (const ImportPackageAsset &src, MonoAssetsItem &dest);
	void AssetsItemToCpp( MonoAssetsItem & src, ImportPackageAsset & dst);
	void AssetsItemToMono (const AssetServerCache::DeletedItem &src, MonoAssetsItem &dest);
	void AssetsItemToMono (const UnityGUID &guid, const Item &item, MonoAssetsItem &dest);
	void ImportAssetsItemToMono (const ImportPackageAsset &src, MonoAssetsItem &dest);

	void ChangesetHeadersToMono (const Changeset &src, MonoChangeset &dest);
	void ChangesetToMono (const ChangesetDummy &src, MonoChangeset &dest);
	void ChangesetMapToMono(const int &changeset, const ChangesetDummy &src, MonoChangeset &dest);
	void CollectSelection(std::set<UnityGUID>* guids);
	void DeletedAssetToMono (const DeletedAsset &src, MonoDeletedAsset &dest);
	void MaintDatabaseRecordToMono(const MaintDatabaseRecord &src, MonoMaintDatabaseRecord &dest);
	void MaintUserRecordToMono(const MaintUserRecord &src, MonoMaintUserRecord &dest);

	extern std::map<UnityGUID, DownloadResolution> g_DownloadResolutions;
	extern std::vector<UnityGUID> g_NameConflicts;

} // namespace

#endif
