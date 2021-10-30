#ifndef AUDIOIMPORTER_H
#define AUDIOIMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "MovieEncode.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Audio/correct_fmod_includer.h"

#include <string>

class AudioClip;

#define PREVIEW_WIDTH 1024

class AudioImporter : public AssetImporter
{
	int   m_Format; /* native/compressed (ogg)*/
	float m_Quality; // bitrate = 56+quality*200. Range -0.2 to 1.3 (resulting in 16 to 316 kbps)
	int   m_LoadFlag;
	bool  m_3D;
	bool  m_ForceToMono;
	
	bool  m_UseHardware; /* IPhone only */ 
	bool  m_Loopable; /* for mp3 compression */

public:
	
	REGISTER_DERIVED_CLASS (AudioImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (AudioImporter)

	AudioImporter(MemLabelId label, ObjectCreationMode mode);

	static void InitializeClass ();
	static void CleanupClass () {}
	
	virtual void CheckConsistency ();
	virtual void ClearPreviousImporterOutputs ();
    
	virtual void GenerateAssetData ();
	
	virtual void Reset();

	GET_SET_COMPARE_DIRTY (float, Quality, m_Quality);
	GET_SET_COMPARE_DIRTY (int, LoadFlag, m_LoadFlag);
	GET_SET_COMPARE_DIRTY (bool, 3D, m_3D);
	GET_SET_COMPARE_DIRTY (bool, ForceToMono, m_ForceToMono);
	GET_SET_COMPARE_DIRTY (bool, Hardware, m_UseHardware);
	GET_SET_COMPARE_DIRTY (bool, Loopable, m_Loopable);
	GET_SET_COMPARE_DIRTY (int, Format, m_Format);

	int GetBitrate () const ;
	void SetBitrate (int rate);
		
	bool TranslateOldSettings (int channels, int format); 

	void UpdateOrigData() { m_Output.originalImportData = GetOriginalImportData(); }
	int GetOrigDuration () const { return m_Output.originalImportData.durationMS; }
	int GetOrigFrequency() const { return m_Output.originalImportData.frequency; }
	int GetOrigChannels() const { return m_Output.originalImportData.channels; }
	bool GetOrigIsOgg() const { return m_Output.originalImportData.isOgg; }
	bool GetOrigIsCompressible() const { return m_Output.originalImportData.isCompressible; }
	int GetOrigBitRate() const { return m_Output.originalImportData.bitRate; }
	bool GetOrigIsMonoForcable() const { return m_Output.originalImportData.canForceToMono; }
	FMOD_SOUND_TYPE GetOrigType() const { return m_Output.originalImportData.type; }
	int GetOrigFileSize() const { return m_Output.originalImportData.fileSize; }	
	
	int GetMinBitrate ( FMOD_SOUND_TYPE type, short channels, unsigned frequency ) const;
	int GetMaxBitrate ( FMOD_SOUND_TYPE type, short channels, unsigned frequency ) const;
	
	int GetDefaultBitrate () const;
	
	// preview
	bool HasPreview() const { return !m_Output.previewData.empty(); }
	bool GetPreviewMinMaxSample (float normalizedTime, unsigned channel, unsigned channels, float &min, float &max);
	const float* GetPreviewMinMaxData() const { return m_Output.previewData.begin(); }
	void GetMinMax(UInt32 fromSample, UInt32 toSample, UInt16 channel, UInt16 channels, UInt32 totalSamples, float &min, float &max) const;

	enum
	{
		kFormatNative = -1,
		kFormatCompressed = 0
	};
	
	struct OriginalImportData
	{
		bool isOgg;
		bool isCompressible;
		int  channels;
		int  frequency;
		int  durationMS;
		int  fileSize;
		bool canForceToMono;
		int  bitRate;
		FMOD_SOUND_TYPE type;
	};
	

	OriginalImportData GetOriginalImportData ();	
	
	/// Reimport for current platform
	///static void ReimportAssets(BuildTargetPlatform previousPlatform, BuildTargetPlatform targetPlatform);
	
	/// Does the audio need to be reimported for the targetPlatform or is it already in the right format
	/// @note implement this to support reimport assets
	static bool DoesAssetNeedReimport (const string& assetPath, BuildTargetPlatform targetPlatform, bool unload);	
	
	/// Can this importer import this file?
	/// @note implement this to support reimport assets 
	static int CanLoadPathName(const std::string& path, int* q = NULL);
	
	
	typedef std::map<BuildTargetPlatformGroup, std::map<FMOD_SOUND_TYPE, FMOD_SOUND_TYPE> > TConversionMap;
	static TConversionMap CreateConversionMap ( int format );

	static FMOD_SOUND_TYPE EditorAudioType(FMOD_SOUND_TYPE platformType);
	static FMOD_SOUND_TYPE GetPlatformConversionType ( FMOD_SOUND_TYPE inType, BuildTargetPlatformGroup platform, int format );
	
private:
	
	struct Output
	{
		dynamic_array<float> previewData;
		FMOD_SOUND_TYPE      importedType;

		// Currently not serialized.
		// UpdateOrigData is called before accessing values. It's a bit weird but oh well.
		OriginalImportData   originalImportData; 
		
		Output ();
		
		DECLARE_SERIALIZE(Output)
	};
	
	Output m_Output;
};



/* inlined functions */
inline bool AudioImporter::GetPreviewMinMaxSample (float normalizedTime, unsigned channel, unsigned channels, float &min, float &max)				   
{				   
	if (HasPreview ())
	{
		unsigned numPreviewSamples =  m_Output.previewData.size () / (channels * 2);
		unsigned index = (unsigned)(normalizedTime * numPreviewSamples);
		if (index < m_Output.previewData.size ())
		{
			max = m_Output.previewData[(index * channels * 2) + (channel * 2) ];				   
			min = m_Output.previewData[(index * channels * 2) + (channel * 2) + 1];				   
			return true;
		}
	}	
	return false;
}

inline void AudioImporter::GetMinMax(UInt32 fromSample, UInt32 toSample, UInt16 channel, UInt16 channels, UInt32 totalSamples, float &min, float &max) const	
{
	UInt32 fromIdx = fromSample * ((float)PREVIEW_WIDTH / (float)totalSamples);
	UInt32 toIdx = toSample * ((float)PREVIEW_WIDTH / (float)totalSamples);
	
	max = (m_Output.previewData[(fromIdx * channels * 2) + (channel * 2) ] + m_Output.previewData[(toIdx * channels * 2) + (channel * 2) ] ) / 2;				   
	min = (m_Output.previewData[(fromIdx * channels * 2) + (channel * 2) + 1] + m_Output.previewData[(toIdx * channels * 2) + (channel * 2) + 1] ) / 2;
	
	return;	
}

#endif
