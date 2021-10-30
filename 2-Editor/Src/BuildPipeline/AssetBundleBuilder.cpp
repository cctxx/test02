#include "UnityPrefix.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/BuildPipeline/AssetBundleBuilder.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/SpritePacker/SpritePacker.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/SerializedFile.h"


static LocalIdentifierInFileType GenerateDeterministicIDFromPersistentInstanceID (SInt32 instanceID)
{
	PersistentManager& pm = GetPersistentManager();
	MdFourGenerator mdfourGen;

	SerializedObjectIdentifier identifier;
	pm.InstanceIDToSerializedObjectIdentifier(instanceID, identifier);

	AssertIf(identifier.serializedFileIndex == -1);

	pm.Lock();
	FileIdentifier fileIdentifier = GetGUIDPersistentManager().PathIDToFileIdentifierInternal(identifier.serializedFileIndex);
	pm.Unlock();
	// Feed GUID + type
	int type = fileIdentifier.type;
	if (type == FileIdentifier::kMetaAssetType || type == FileIdentifier::kSerializedAssetType)
	{
		mdfourGen.Feed(GUIDToString(fileIdentifier.guid));
		mdfourGen.Feed(type);
	}
	// Or path
	else
	{
		mdfourGen.Feed(fileIdentifier.pathName);
	}

	// and also the local fileID
	mdfourGen.Feed(identifier.localIdentifierInFile);

	MdFour mdfour = mdfourGen.Finish();


#if LOCAL_IDENTIFIER_IN_FILE_SIZE == 48
	// Extract 32 bits out of 128bit hash (hope it's enough)

	LocalIdentifierInFileType newLocalFileID = *reinterpret_cast<UInt64*> (&mdfour) & 0xFFFFFFFFFFFFULL;
	AssertIf(newLocalFileID >= 1ULL << 48ULL);

#elif LOCAL_IDENTIFIER_IN_FILE_SIZE == 32
	LocalIdentifierInFileType newLocalFileID = *reinterpret_cast<UInt32*> (&mdfour);
#else
#error "not supported"
#endif

	///@TODO: Need to swap endianess for big endian???

	return newLocalFileID;
}


string AssetBundleBuilder::BuildCustomAssetBundle (PPtr<Object> mainAsset, const vector<PPtr<Object> >& objects, const vector<string>* overridePaths, const string& targetPath, int buildPackageSortIndex, BuildTargetPlatform platform, TransferInstructionFlags transferFlags, BuildAssetBundleOptions assetBundleOptions)
{
	if (overridePaths != NULL && overridePaths->size() != objects.size())
	{
		return "Object names must match the paths.\n";
	}

	string result;
	AssetBundle* assetBundle = NULL;
	try
	{
		const bool buildDeterministicAssetBundle = assetBundleOptions & kAssetBundleDeterministic;

#if ENABLE_SPRITES
		SpritePacker::RebuildAtlasCacheIfNeeded(platform, false, SpritePacker::kSPE_Normal, false);
#endif

		// Gather initial set of object to bundle up and make sure
		// all of them are valid assets.
		BuildInitialObjectSet (mainAsset, objects, buildDeterministicAssetBundle);

		// Create AssetBundle object.
		assetBundle = NEW_OBJECT(AssetBundle);
		assetBundle->Reset();
		assetBundle->HackSetAwakeWasCalled();
		assetBundle->SetHideFlags( 0 );

		// Add it to asset list.
		BuildAsset& assetBundleInfo = AddAssetToBuild (assetBundle->GetInstanceID(), targetPath, buildPackageSortIndex);
		assetBundleInfo.temporaryObjectIdentifier.serializedFileIndex = GetPersistentManager().GetSerializedFileIndexFromPath (targetPath);
		assetBundleInfo.temporaryObjectIdentifier.localIdentifierInFile = 1;

		// Collect any dependencies.
		if (assetBundleOptions & kAssetBundleCollectDependencies)
			CollectDependencies ();

		// Build map of all assets that we are including.
		CollectInstanceIDsAndAssets ();

		// Does user want to include all the objects in the same asset?
		// Do dependency checking by including all assets referenced from that asset if mode wants it
		if (assetBundleOptions & kAssetBundleIncludeCompleteAssets)
			AddInstanceIDsOfCompleteAssets ();

		// Check if all GameObject's components and Transform children are present in object set that
		// is going into the AssetBundle.
		VerifyComponentsAndChildTransformsArePresent ();

		// Add BuildAssets for all the instance IDs we've gathered.
		AddBuildAssets (targetPath, buildPackageSortIndex, buildDeterministicAssetBundle);

		// Add AssetInfos for all assets included in the bundle.
		CreateAllAssetInfos (assetBundle, mainAsset, objects, overridePaths);

		set<int> usedClassIDs;
		ComputeObjectUsage(m_IncludedObjectIDs, usedClassIDs, m_BuildAssets);

		///@TODO: in kDeterministicAssetBundle mode assets will not be sorting by fileID will be very inefficient for load order

		if (!buildDeterministicAssetBundle)
			AssignTemporaryLocalIdentifierInFileForAssets (targetPath, m_BuildAssets);

		// Sort preload index for linear loading
		SortPreloadAssetsByFileID (assetBundle->m_PreloadTable, assetBundle->m_MainAsset.preloadIndex, assetBundle->m_MainAsset.preloadSize, m_BuildAssets);
		for (AssetBundle::iterator i = assetBundle->m_Container.begin (); i != assetBundle->m_Container.end (); ++i)
			SortPreloadAssetsByFileID(assetBundle->m_PreloadTable, i->second.preloadIndex, i->second.preloadSize, m_BuildAssets);

		// Store script compatibility info into the AssetBundle object if typetrees are disabled
		if ((transferFlags & kDisableWriteTypeTree) != 0)
		{
			CreateScriptCompatibilityInfo (*assetBundle, m_BuildAssets, targetPath);

			// Generate hashes for classes of objects that are being written to the AssetBundle
			vector<SInt32> usedClassIDsVector (usedClassIDs.begin (), usedClassIDs.end ());
			assetBundle->FillHashTableForRuntimeClasses (usedClassIDsVector, transferFlags);
		}

		BuildTargetSelection targetSelection(platform,0); //@TODO: take currently set/passed subtarget?
		bool successWriting = WriteSharedAssetFile(targetPath, m_BuildAssets, targetSelection, &BuildingPlayerOrAssetBundleInstanceIDResolveCallback, transferFlags);
		if (!successWriting)
			result = "Failed to write " + targetPath;

		FileSizeInfo info;
		info.AddAssetFileSize(targetPath, TemporaryFileToAssetPathMap(m_BuildAssets));
		info.Print();

		VerifyAllAssetsHaveAssignedTemporaryLocalIdentifierInFile(m_BuildAssets);
	}
	catch (const char* error)
	{
		result = error;
	}
	catch (string& error)
	{
		result = error;
	}

	// Get rid of AssetBundle object.
	if (assetBundle)
	{
		m_BuildAssets.erase (assetBundle->GetInstanceID ());
		DestroySingleObject (assetBundle);
	}

	// Get rid of stream we created.
	GetPersistentManager ().UnloadStream (targetPath);

	return result;
}

void AssetBundleBuilder::BuildInitialObjectSet (PPtr<Object> mainAsset, const vector<PPtr<Object> >& objects, bool buildDeterministicAssetBundle)
{
	if (mainAsset)
	{
		if (!mainAsset->IsPersistent())
			throw Append ("Assets included in an asset bundle cannot be scene objects, instead place them in a prefab, then export them.\n", mainAsset->GetName());

		m_IncludedObjects.insert(mainAsset);
	}
	for (int i=0;i<objects.size();i++)
	{
		Object* o = objects[i];
		if (o && !o->IsPersistent())
			throw Append ("Assets included in an asset bundle cannot be scene objects, instead place them in a prefab, then export them.\nIncluded scene object", o->GetName());

		if (o)
			m_IncludedObjects.insert(o);
	}

	// Make sure we have at least one asset.
	if (m_IncludedObjects.empty())
		throw "No assets were provided for the asset bundle";

	// Cull editor objects from set.
	for (set<Object*>::iterator i = m_IncludedObjects.begin(), next; i != m_IncludedObjects.end(); i=next)
	{
		next = i; next++;
		Object* obj = *i;

		if (obj != NULL && obj->GetClassID () >= CLASS_SmallestEditorClassID)
		{
			if (IsUnitySceneFile(GetAssetPathFromObject(obj)))
				throw Format("Asset bundles cannot include Scenes: %s\nIf you want to stream a Scene, use BuildPipeline.BuildPlayer with BuildOptions.BuildAdditionalStreamedScenes.", GetAssetPathFromObject(obj).c_str());
			else
			{
				if (!IsDirectoryCreated(GetAssetPathFromObject(obj)))
					WarningString(Format("Asset bundles cannot include Editor Objects (%s): %s.", obj->GetClassName().c_str(), GetAssetPathFromObject(obj).c_str()));
			}

			m_IncludedObjects.erase(i);
		}
	}
}

void AssetBundleBuilder::CollectDependencies ()
{
	set<SInt32> ids;
	GameReleaseCollector collector (&ids);

	for (set<Object*>::const_iterator i = m_IncludedObjects.begin (); i != m_IncludedObjects.end ();i++)
	{
		int instanceID = (**i).GetInstanceID ();
		collector.GenerateInstanceID (instanceID);
	}

	for (set<SInt32>::iterator i = ids.begin();i != ids.end (); i++)
	{
		Object* object = PPtr<Object> (*i);
		if (object)
			m_IncludedObjects.insert(object);
	}
}

void AssetBundleBuilder::CollectInstanceIDsAndAssets ()
{
	for (set<Object*>::iterator i = m_IncludedObjects.begin (); i != m_IncludedObjects.end (); i++)
	{
		Object* o = *i;
		if (!o)
			continue;

		// Add instance ID.
		m_IncludedObjectIDs.insert (o->GetInstanceID());

		// If the object is currently contained in a serialized file, grab
		// its guid and add it to the set (if it's not already on there; we may
		// come across many objects from the same file).
		string path = GetGUIDPersistentManager ().GetPathName (o->GetInstanceID ());
		UnityGUID guid = GetGUIDPersistentManager ().GUIDFromAnySerializedPath (path);
		if (guid != UnityGUID ())
		{
			m_IncludedAssets.insert (guid);
		}
	}
}

void AssetBundleBuilder::AddInstanceIDsOfCompleteAssets ()
{
	for (set<UnityGUID>::iterator guid = m_IncludedAssets.begin (); guid != m_IncludedAssets.end (); guid++)
	{		
		const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID (*guid);
		if (!asset)
			continue;

		GameReleaseDependenciesSameAsset collector (&m_IncludedObjectIDs, *guid);

		// Build resources map
		bool isPrefab = asset->mainRepresentation.classID == ClassID(GameObject);

		int instanceID = asset->mainRepresentation.object.GetInstanceID();
		if (dynamic_instanceID_cast<Object*> (instanceID))
			collector.GenerateInstanceID (instanceID);

		for (int i = 0; i < asset->representations.size (); i++)
		{
			instanceID = asset->representations[i].object.GetInstanceID();

			// We only want the root game object in the resources. Not all child game objects!
			if (isPrefab && asset->representations[i].classID == ClassID(GameObject))
				continue;

			if (dynamic_instanceID_cast<Object*> (instanceID))
				collector.GenerateInstanceID(instanceID);
		}
	}
}

void AssetBundleBuilder::VerifyComponentsAndChildTransformsArePresent ()
{
	for (set<SInt32>::iterator it = m_IncludedObjectIDs.begin(); it != m_IncludedObjectIDs.end(); ++it)
	{
		GameObject* obj = dynamic_instanceID_cast<GameObject*> (*it);
		if (!obj)
			continue;

		VerifyComponentsArePresent (obj);
		VerifyChildTransformsArePresent (obj);
	}
}

void AssetBundleBuilder::VerifyComponentsArePresent (GameObject* obj)
{
	// Make sure components are included
	int componentsMissing = 0;
	for (int i = 0; i < obj->GetComponentCount(); ++i)
	{
		Unity::Component* component = &obj->GetComponentAtIndex (i);

		if (!IsClassIDSupportedInBuild (component->GetClassID ()))
			continue;

		set<SInt32>::iterator it = m_IncludedObjectIDs.find (component->GetInstanceID());
		if (it == m_IncludedObjectIDs.end ())
			componentsMissing++;
	}

	if (componentsMissing != 0)
		throw Format ("%d component(s) of GameObject '%s' are missing in the AssetBundle.\nPlease specify BuildAssetBundleOptions.CollectDependencies or collect GameObject's components and pass as 'assets' parameter.",
		componentsMissing, obj->GetName());
}

void AssetBundleBuilder::VerifyChildTransformsArePresent (GameObject* obj)
{
	// In case GameObject has a transform, make sure children are included
	int childTransformMissing = 0;
	if (Transform* transform = obj->QueryComponentT<Transform> (ClassID(Transform)))
	{
		for (int i=0; i<transform->GetChildrenCount (); ++i)
		{
			Transform* child = &transform->GetChild(i);
			set<SInt32>::iterator it = m_IncludedObjectIDs.find (child->GetInstanceID());
			if (it == m_IncludedObjectIDs.end ())
				childTransformMissing++;
		}
	}

	if (childTransformMissing != 0)
		throw Format ("%d child transform(s) of '%s' are missing in the AssetBundle hierarchy.\nPlease specify BuildAssetBundleOptions.CollectDependencies or collect child Transforms or break the parent-child connection.",
		childTransformMissing, obj->GetName());
}

void AssetBundleBuilder::AddBuildAssets (const string& targetPath, int buildPackageSortIndex, bool buildDeterministicAssetBundle)
{
	set<LocalIdentifierInFileType> ensureUniqueMdFour;
	ensureUniqueMdFour.insert(1);
	ensureUniqueMdFour.insert(0);

	for (set<SInt32>::iterator i = m_IncludedObjectIDs.begin (); i != m_IncludedObjectIDs.end(); i++)
	{
		LocalIdentifierInFileType newLocalFileID = 0;
		if (buildDeterministicAssetBundle)
		{
			newLocalFileID = GenerateDeterministicIDFromPersistentInstanceID (*i);

			if (!ensureUniqueMdFour.insert (newLocalFileID).second)
			{
				Object* obj = dynamic_instanceID_cast<Object*> (*i);
				throw Format ("Building AssetBundle failed because hash collisions were detected in the deterministic id generation.\nAsset path: '%s'", GetAssetPathFromObject(obj).c_str());
			}
		}

		AddBuildAssetInfoChecked (*i, targetPath, buildPackageSortIndex, m_BuildAssets, newLocalFileID, false);
	}
}

void AssetBundleBuilder::CreateAllAssetInfos (AssetBundle* assetBundle, PPtr<Object> mainAsset, const vector<PPtr<Object> >& objects, const vector<string>* overridePaths)
{
	assetBundle->m_MainAsset.asset = mainAsset;
	if (mainAsset)
		AddPreloadTableEntries (assetBundle, assetBundle->m_MainAsset);

	if (overridePaths == NULL)
	{
		for (set<UnityGUID>::const_iterator asset = m_IncludedAssets.begin (); asset != m_IncludedAssets.end (); ++asset)
			AddAssetInfoForAssetFromDatabase (assetBundle, *asset);
	}
	else
	{
		// Generate paths from explicit assignments in overridePaths
		for (int i = 0; i < objects.size(); i++)
		{
			Object* object = objects[i];
			if (!object)
				continue;

			AssetBundle::AssetInfo& assetInfo = AddAssetInfoToContainer (assetBundle, object, overridePaths->at (i));
			AddPreloadTableEntries (assetBundle, assetInfo);
		}
	}
}

void AssetBundleBuilder::AddAssetInfoForAssetFromDatabase (AssetBundle* assetBundle, const UnityGUID& guid)
{
	// Get asset data from database.
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID (guid);
	if (!asset)
	{
		// Not an asset managed by the database.  This is probably the
		// default or extra resources bundle.
		return;
	}

	SInt32 mainInstanceID = asset->mainRepresentation.object.GetInstanceID();

	const string path = GetAssetPathFromGUID (guid);
	const string name = GetLastPathNameComponent(DeletePathNameExtension (ToLower (path)));
	const bool isPrefab = asset && asset->mainRepresentation.classID == ClassID(GameObject);

	vector<AssetBundle::AssetInfo*> addedInfos;

	// Add main object if included
	if (dynamic_instanceID_cast<Object*> (mainInstanceID)
		&& m_IncludedObjectIDs.find (mainInstanceID) != m_IncludedObjectIDs.end ())
	{
		addedInfos.push_back (&AddAssetInfoToContainer (assetBundle, PPtr<Object> (mainInstanceID), name));
	}

	// Add secondary objects if included
	for (int i=0;i<asset->representations.size();i++)
	{
		// We only want the root game object in the resources. Not all child game objects!
		if (isPrefab && asset->representations[i].classID == ClassID(GameObject))
			continue;

		SInt32 secondaryInstanceID = asset->representations[i].object.GetInstanceID();
		if (dynamic_instanceID_cast<Object*> (secondaryInstanceID)
			&& m_IncludedObjectIDs.find (secondaryInstanceID) != m_IncludedObjectIDs.end ())
		{
			addedInfos.push_back (&AddAssetInfoToContainer (assetBundle, PPtr<Object> (secondaryInstanceID), name));
		}
	}

	AddPreloadTableEntries(assetBundle, addedInfos.data (), addedInfos.size ());
}

void AssetBundleBuilder::AddPreloadTableEntries (AssetBundle* assetBundle, AssetBundle::AssetInfo** assetInfos, int numAssetInfos)
{
	set<SInt32> preloadIds;
	GameReleaseCollector sortOrderCollector (&preloadIds);

	for (int i = 0; i < numAssetInfos; ++i)
		sortOrderCollector.GenerateInstanceID (assetInfos[i]->asset.GetInstanceID ());

	// Reduce preloadIDs to what is actually included in the resource file
	vector<PPtr<Object> > preloadPPtrs;
	for (set<SInt32>::iterator i = preloadIds.begin (); i != preloadIds.end(); i++)
	{
		if (m_IncludedObjectIDs.find (*i) != m_IncludedObjectIDs.end ())
			preloadPPtrs.push_back (PPtr<Object> (*i));
	}

	// Setup allPreloadAssets and indices into AssetBundle::AssetInfo.
	// One asset is loaded is one complete unit, thus all game objects / meshes / animation clips
	// share the same preloadIndex array.
	for (int i = 0; i < numAssetInfos; ++i)
	{
		assetInfos[i]->preloadIndex = assetBundle->m_PreloadTable.size ();
		assetInfos[i]->preloadSize = preloadPPtrs.size ();
	}

	assetBundle->m_PreloadTable.insert (assetBundle->m_PreloadTable.end (), preloadPPtrs.begin (), preloadPPtrs.end ());

}

AssetBundle::AssetInfo& AssetBundleBuilder::AddAssetInfoToContainer (AssetBundle* assetBundle, PPtr<Object> object, const string& path)
{
	string pathLowerCase = ToLower (path);

	AssetBundle::AssetInfo info;
	info.asset = object;

	return assetBundle->m_Container.insert (make_pair (pathLowerCase, info))->second;
}
