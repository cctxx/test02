#ifndef TRANSFERBASE_H
#define TRANSFERBASE_H

#include "Runtime/Serialize/SerializeTraits.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"


struct ReduceCopyData;
struct StreamingInfo;


class EXPORT_COREMODULE TransferBase
{
public:

	TransferBase ()
		: m_Flags (0)
		, m_UserData (NULL) {}

	/// @name Predicates
	/// @{

	/// Get the TransferInstructionFlags.
	/// Commonly used to special case transfer functions for specific operations.
	int GetFlags () { return m_Flags; }

	/// If true, the transfer is reading data from a source. (Could be fread from a file or reading from a block of memory)
	/// @note There are transfers for which neither IsReading() nor IsWriting() is true (for example when generating a typetree).
	///	IsReading is NOT the inverse of IsWriting.
	bool IsReading () { return false; }

	/// If true, the transfer reads PPtrs (Object references)
	/// This is true when reading from a memory stream or file but also when using RemapPPtrTransfer (A generic way of iterating all object references)
	bool IsReadingPPtr () { return false; }

	/// Whether the last Transfer() resulted in a value store, i.e. had actual data
	/// transfered from the stream.
	/// It is important to use this function instead of IsReading when 
	/// When reading from a stream that does not define all the data, the desired behaviour is that default values from constructor are fully preserved.
	/// All transfer functions do this internally (transferred properties are left untouched when the data does not exist for example in a Yaml file)
	/// But when the serialized property needs to be manually converted in the Transfer function, then it is important to check if the value was actually read.
	/// CODE EXAMPLE:
	/// bool enabled;
	/// if (transfer.IsWriting ())
	/// 	enabled = m_Flags == 1;
	/// TRANSFER (enabled);
	/// if (transfer.DidReadLastProperty ())
	/// 	m_Flags = enabled ? 1 : 0;
	bool DidReadLastProperty () const { return false; }

	/// Same as DidReadLastProperty, but only returns true when reading PPtr properties.
	/// A compile time optimization necessary for removing generated code by RemapPPtrTransfer.
	bool DidReadLastPPtrProperty () const { return false; }


	/// If true, the transfer is writing out data.
	bool IsWriting () { return false; }
	/// If true, the transfer is writing out PPtr data. This is true when writing to a memory stream or file, but also when using RemapPPtrTransfer.
	bool IsWritingPPtr () { return false; }

	/// Are we reading data from a data source that is not guaranteed to have the same data layout as the Transfer function.
	/// eg. StreamedBinaryRead always returns false. YamlRead & SafeBinaryRead return true.
	bool IsReadingBackwardsCompatible () { return false; }

	/// When writing or reading from disk we need to translate instanceID
	/// to LocalIdentifierInFile & LocalSerializedFileIndex or in the case of Yaml files, guids + LocalIdentifierInFile.
	/// This returns true when remapping of the instanceID should be performed.
	bool NeedsInstanceIDRemapping () { return false; }

	/// Are we transferring data with endianess swapping. (We might neither endianess swap on write or read based on IsReading / IsWriting)
	/// The endianess conversion is done by the TransferBackend, but there are some special cases where you might want to handle it yourself.
	/// (For example a texture data is transferred a single UInt8* array, so all endianess swapping is the responsibiltiy of the texture transfer function.)
	bool ConvertEndianess () { return false; }
	
	/// Are we reading/writing a .meta file (Asset importers use it to differentiate reading/writing of a Library/metadata cached file. )
	/// @TODO: We should rename Library/metadata to Library/cachedata and cleanup the usage of metadata vs .meta file. It is confusing.
	bool AssetMetaDataOnly () { return false; }

	/// Is this a RemapPPtrTransfer backend. Commonly used to do very specialized code when generating dependencies using RemapPPtrTransfer.
	bool IsRemapPPtrTransfer () { return false; }

	/// Return true if we are writing the data for a player.
	bool IsWritingGameReleaseData () { return false; }
	
	/// Are we serializing data for use by the player.
	/// This includes reading/writing/generating typetrees. And can be when creating data from the editor for player or when simply reading/writing data in the player.
	/// Commonly used to not serialize data that does not exist in the player.
	bool IsSerializingForGameRelease ()
	{
		#if UNITY_EDITOR
		return m_Flags & kSerializeGameRelease;
		#else
		return true;
		#endif
	}

	/// @}

	/// @name Build Targets
	/// @{

	/// Returns true in the editor when writing the data for a player of the specified target platform.
	bool IsBuildingTargetPlatform (BuildTargetPlatform) { return false; }
	/// Returns the target platform we are building for. Only returns the target platform when writing data.
	BuildTargetSelection GetBuildingTarget () { return BuildTargetSelection::NoTarget (); }

	#if UNITY_EDITOR
	/// BuildUsageTag carries information generated by the build process about the object being serialized.
	/// For example the buildpipeline might instruct the transfer system to strip normals and tangents from a Mesh,
	/// because it knows that no renderers & materials in all scenes actually use them.
	BuildUsageTag GetBuildUsage () {  return BuildUsageTag (); }
	#endif

	/// @}

	/// @name Versioning
	/// @{

	/// Sets the "version of the class currently transferred"
	void SetVersion (int) {}

	/// Returns if the transferred data's version is the version used by the source code
	bool IsVersionSmallerOrEqual (int /*version*/) { return false; }

	/// Deprecated: use IsVersionSmallerOrEqual instead.
	bool IsOldVersion (int /*version*/) { return false; }
	bool IsCurrentVersion () { return true; }

	/// @}

	/// @name Transfers
	/// @{

	/// Alignment in the serialization system is done manually.
	/// The serialization system only ever cares about 4 byte alignment.
	/// When writing data that has an alignment of less than 4 bytes, followed by data that has 4 byte alignment,
	/// then Align must be called before the 4 byte aligned data.
	/// TRANSFER (1byte);
	/// TRANSFER (1byte);
	/// transfer.Align ();
	/// TRANSFER (4byte);
	void Align () {}


	/// Internal function. Should only be called from SerializeTraits
	template<class T>
	void TransferBasicData (T&) { }

	/// Internal function. Should only be called from SerializeTraits
	template<class T>
	void TransferPtr (bool, ReduceCopyData*) {}

	/// Internal function. Should only be called from SerializeTraits
	template<class T>
	void ReduceCopy (const ReduceCopyData&){}
	/// @}

	/// user data.
	void* GetUserData () { return m_UserData; }
	void  SetUserData (void* userData) { m_UserData = userData; }

	void AddMetaFlag(int /*mask*/) {}

	/// Deprecated
	void BeginMetaGroup (std::string /*name*/) {}
	void EndMetaGroup () {}
	void EnableResourceImage (ActiveResourceImage /*targetResourceImage*/) {}
	bool ReadStreamingInfo (StreamingInfo* /*streamingInfo*/) { return false; }
	bool NeedNonCriticalMetaFlags () { return false; }

protected:

	int	m_Flags;
	void* m_UserData;
};

#endif // !TRANSFER_BASE
