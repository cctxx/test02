#include "UnityPrefix.h"
#include "TransferNameConversions.h"
#include "YAMLRead.h"
#include "YAMLWrite.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Serialize/SerializedFile.h"
/*
@TODO:

- Meta file for texture importer has additional unnecessary settings: 
 TextureImporter:
	fileIDToRecycleName: {}

- Add line offsets to SerializedFile ObjectInfo, so we can have proper line numbers in YAML errors.

*/
 
// Text transfer of Unity References:
// If NeedsInstanceIDRemapping() is false or for null references:
// {instanceID: id}
// For local objects in same file:
// {fileID: id}
// For objects from other file with GUID:
// {fileID: id, guid: g, type: t}

template<class T>
void TransferYAMLPtr (T& data, YAMLRead& transfer)
{
	SInt32 instanceID = 0;
	if (!transfer.NeedsInstanceIDRemapping())
	{
		transfer.Transfer (instanceID, "instanceID");
		data.SetInstanceID (instanceID);	
	}
	else
	{
		bool allowLocalIdentifier = (transfer.GetFlags () & kYamlGlobalPPtrReference) == 0;

		LocalIdentifierInFileType fileID = 0;
		TRANSFER (fileID);
		
		if (transfer.HasNode("guid"))
		{
			FileIdentifier id;
			transfer.Transfer (id.guid, "guid");
			transfer.Transfer (id.type, "type");
			
			id.Fix_3_5_BackwardsCompatibility ();
			PersistentManager& pm = GetPersistentManager();
			SInt32 globalIndex = pm.InsertFileIdentifierInternal(id, true);
			SerializedObjectIdentifier identifier (globalIndex, fileID);

			#if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD
			pm.ApplyInstanceIDRemap (identifier);
			#endif
			
			instanceID = pm.SerializedObjectIdentifierToInstanceID (identifier);
		}
		else if (allowLocalIdentifier)
		{
			// local fileID
			LocalSerializedObjectIdentifier identifier;
			identifier.localIdentifierInFile = fileID;
			identifier.localSerializedFileIndex = 0;
			LocalSerializedObjectIdentifierToInstanceID (identifier, instanceID);	
		}
		
		data.SetInstanceID (instanceID);
	}
}

template<class T>
void TransferYAMLPtr (T& data, YAMLWrite& transfer)
{
	transfer.AddMetaFlag(kTransferUsingFlowMappingStyle);
	SInt32 instanceID = data.GetInstanceID();
	if (!transfer.NeedsInstanceIDRemapping())
		transfer.Transfer (instanceID, "instanceID");
	else
	{
		// By default we allow writing self references that exclude guid & type.
		// This way references inside of the file will never be lost even if the guid of the file changes
		bool allowLocalIdentifier = (transfer.GetFlags () & kYamlGlobalPPtrReference) == 0;
		if (allowLocalIdentifier)
		{
			LocalSerializedObjectIdentifier localIdentifier;
			InstanceIDToLocalSerializedObjectIdentifier (instanceID, localIdentifier);
			if (localIdentifier.localSerializedFileIndex == 0)
			{	
				transfer.Transfer (localIdentifier.localIdentifierInFile, "fileID");
				return;
			}
		}
		
		GUIDPersistentManager& pm = GetGUIDPersistentManager();
		pm.Lock();
		SerializedObjectIdentifier identifier;
		if (pm.InstanceIDToSerializedObjectIdentifier(instanceID, identifier))
		{
			FileIdentifier id = pm.PathIDToFileIdentifierInternal(identifier.serializedFileIndex);
			transfer.Transfer (identifier.localIdentifierInFile, "fileID");
			transfer.Transfer (id.guid, "guid");
			transfer.Transfer (id.type, "type");
		}
		else
		{
			instanceID = 0;
			transfer.Transfer (instanceID, "instanceID");
		}
		pm.Unlock();
	}
}

template<>
void TransferYAMLPPtr (PPtr<Object> &data, YAMLRead& transfer) { TransferYAMLPtr (data, transfer); }
template<>
void TransferYAMLPPtr (PPtr<Object> &data, YAMLWrite& transfer) { TransferYAMLPtr (data, transfer); }
template<>
void TransferYAMLPPtr (ImmediatePtr<Object> &data, YAMLRead& transfer) { TransferYAMLPtr (data, transfer); }
template<>
void TransferYAMLPPtr (ImmediatePtr<Object> &data, YAMLWrite& transfer) { TransferYAMLPtr (data, transfer); }


template<>
void YAMLSerializeTraits<UnityGUID>::Transfer (UnityGUID& data, YAMLRead& transfer)
{
	std::string str;
	transfer.TransferStringData(str);
	data = StringToGUID(str);
}

template<>
void YAMLSerializeTraits<UnityGUID>::Transfer (UnityGUID& data, YAMLWrite& transfer)
{
	std::string str = GUIDToString(data);
	transfer.TransferStringData(str);
}

