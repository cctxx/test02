#include "UnityPrefix.h"
#include "AudioImporter.h"
#include "AssetDatabase.h"
#include "Runtime/Audio/AudioManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "AssetInterface.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Audio/WavReader.h"

#include "FMODImporter.h"

#include "Runtime/Utilities/WavFileUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "AssetImportState.h"

#define kDurationGetFromImport 0
#define kFrequencyGetFromImport 0
#define kChannelsGetFromImport 0
#define kChannelsForceMono 1
#define kChannelsForceStereo 2
#define kTempAudioPath  "Temp/AudioImport.audio"
#define kTempEditorAudioPath  "Temp/AudioImport.editorAudio"
#define kDefaultQuality 0.5f

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kAudioImporterVersion = 4 };


static int CanLoadPathName ( const string& pathName, int* queue );
double GetTimeSinceStartup ();

// ----------------------------------------------------------
// Platform specific conversion
// ----------------------------------------------------------
AudioImporter::TConversionMap AudioImporter::CreateConversionMap ( int format )
{
	AudioImporter::TConversionMap m;
	// IPhone specific conversions
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_WAV] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_MPEG;
	m[kPlatform_iPhone][FMOD_SOUND_TYPE_AIFF] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_MPEG;

	// Android specific conversions
	m[kPlatformAndroid][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_WAV] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_MPEG;
	m[kPlatformAndroid][FMOD_SOUND_TYPE_AIFF] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_MPEG;

	// WP8 specific conversions

	// BB10 specific conversions
	m[kPlatformBB10][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformBB10][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformBB10][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatformBB10][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatformBB10][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatformBB10][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatformBB10][FMOD_SOUND_TYPE_WAV] = format==AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_MPEG;
	m[kPlatformBB10][FMOD_SOUND_TYPE_AIFF] = format==AudioImporter::kFormatNative  ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_MPEG;

	// Tizen specific conversions
	m[kPlatformTizen][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformTizen][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformTizen][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatformTizen][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatformTizen][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatformTizen][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatformTizen][FMOD_SOUND_TYPE_WAV] = format==AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_MPEG;
	m[kPlatformTizen][FMOD_SOUND_TYPE_AIFF] = format==AudioImporter::kFormatNative  ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_MPEG;

	// PS3 specific conversions
	m[kPlatformPS3][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformPS3][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformPS3][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatformPS3][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatformPS3][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatformPS3][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatformPS3][FMOD_SOUND_TYPE_WAV] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_MPEG;
	m[kPlatformPS3][FMOD_SOUND_TYPE_AIFF] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_MPEG;
	
	// XBOX360 specific conversions
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_XMA;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_XMA;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_WAV] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_XMA;
	m[kPlatformXBOX360][FMOD_SOUND_TYPE_AIFF] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_XMA;
	
	// Wii specific conversions
	m[kPlatformWii][FMOD_SOUND_TYPE_MPEG] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_MPEG : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_OGGVORBIS] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_OGGVORBIS : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_MOD] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_MOD : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_XM] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_XM : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_IT] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_IT : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_S3M] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_S3M : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_WAV] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_GCADPCM;
	m[kPlatformWii][FMOD_SOUND_TYPE_AIFF] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_GCADPCM;

	// flash specific conversions
	m[kPlatformFlash][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_WAV] = FMOD_SOUND_TYPE_MPEG;
	m[kPlatformFlash][FMOD_SOUND_TYPE_AIFF] = FMOD_SOUND_TYPE_MPEG;

	// WebGL specific conversions
	m[kPlatformWebGL][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_WAV] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformWebGL][FMOD_SOUND_TYPE_AIFF] = FMOD_SOUND_TYPE_MPEG;

	// default conversions
	m[kPlatformUnknown][FMOD_SOUND_TYPE_MPEG] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_OGGVORBIS] = FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_MOD] = FMOD_SOUND_TYPE_MOD;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_XM] = FMOD_SOUND_TYPE_XM;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_IT] = FMOD_SOUND_TYPE_IT;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_S3M] = FMOD_SOUND_TYPE_S3M;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_WAV] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_WAV : FMOD_SOUND_TYPE_OGGVORBIS;
	m[kPlatformUnknown][FMOD_SOUND_TYPE_AIFF] = format == AudioImporter::kFormatNative ? FMOD_SOUND_TYPE_AIFF : FMOD_SOUND_TYPE_OGGVORBIS;

	return m;
}

FMOD_SOUND_TYPE AudioImporter::EditorAudioType(FMOD_SOUND_TYPE platformType)
{
	switch (platformType)
	{
		case FMOD_SOUND_TYPE_XMA:
		case FMOD_SOUND_TYPE_GCADPCM:
			return FMOD_SOUND_TYPE_OGGVORBIS;
		default:
			return platformType;
	}	
}

FMOD_SOUND_TYPE AudioImporter::GetPlatformConversionType ( FMOD_SOUND_TYPE inType, BuildTargetPlatformGroup platform, int format )
{
	AudioImporter::TConversionMap conversionMap = CreateConversionMap ( format );

	AudioImporter::TConversionMap::const_iterator it = conversionMap.find ( platform );
	if ( it == conversionMap.end() )
		platform = kPlatformUnknown;

	// sanity: *all* types have to be in the map (because std::map otherwise creates them upon lookup)
	Assert ( conversionMap[platform].find ( inType ) != conversionMap[platform].end() );

	return conversionMap[platform][inType];
}

AudioImporter::AudioImporter ( MemLabelId label, ObjectCreationMode mode )
	:	Super ( label, mode )
{
	
	// Reset() - is called from outside
}

void AudioImporter::Reset()
{
	Super::Reset();

	m_Quality = -1.0f;
	m_Format = kFormatNative;
	m_3D = true;
	m_LoadFlag = AudioClip::kCompressedInMemory;
	m_Output.previewData.clear();
	m_ForceToMono = false;
	m_UseHardware = false;
	m_Loopable = false;
}

void AudioImporter::CheckConsistency()
{
	Assert ( ( m_Format == kFormatNative ) || ( m_Format == kFormatCompressed ) );
	Assert ( ( m_LoadFlag == AudioClip::kCompressedInMemory ) || ( m_LoadFlag == AudioClip::kStreamFromDisc ) || ( m_LoadFlag == AudioClip::kDecompressOnLoad ) );
}

void AudioImporter::ClearPreviousImporterOutputs ()
{
	m_Output = Output ();
}

AudioImporter::~AudioImporter ()
{
}

// ----------------------------------------------------------
// InitializeClass
// ----------------------------------------------------------
void AudioImporter::InitializeClass ()
{
	RegisterCanLoadPathNameCallback ( CanLoadPathName, ClassID ( AudioImporter ), kAudioImporterVersion);
	RegisterAllowNameConversion ( AudioImporter::GetClassStringStatic(), "m_UseRuntimeDecompression", "m_DecompressOnLoad" );
	RegisterAllowNameConversion (AudioImporter::GetClassStringStatic(), "3d", "m_3D");
}

// ----------------------------------------------------------
// CanLoadPathName
// ----------------------------------------------------------
int AudioImporter::CanLoadPathName ( const string& pathName, int* q /*=NULL*/ )
{
	string ext = ToLower ( GetPathNameExtension ( pathName ) );
	return  ( ext == "ogg" || ext == "aif" || ext == "aiff" || ext == "wav" || ext == "mp3" || ext == "mod" || ext == "it" || ext == "s3m" || ext == "xm" );
}

int AudioImporter::GetBitrate () const
{
	return ( int ) GetAudioBitrateForQuality ( GetQuality() );
}

void AudioImporter::SetBitrate ( int rate )
{
	SetQuality ( GetAudioQualityForBitrate ( rate ) );
}

// ----------------------------------------------------------
// GenerateAssetData
// ----------------------------------------------------------
void AudioImporter::GenerateAssetData ()
{
	std::string pathName = GetAssetPathName ();
	string ext = ToLower ( GetPathNameExtension ( pathName ) );

	MonoPreprocessAudio ( pathName );

	std::auto_ptr<FMODImporter> fmod (new FMODImporter ( pathName, m_Format == kFormatNative ? m_ForceToMono : false ));

	Assert ( fmod.get () );

	if ( !fmod->good() )
	{
		string err = Format ( "The file '%s' does not contain a supported '%s' format (%s)", pathName.c_str(), ext.c_str(), fmod->GetErrorString().c_str() );
		LogImportError ( err );
		return;
	}

	// Platform specific conversion?
	BuildTargetPlatform targetPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
	GetAssetImportState().SetDidImportAssetForTarget ( targetPlatform );

	FMOD_SOUND_TYPE targetType = GetPlatformConversionType ( fmod->m_Type, GetBuildTargetGroup ( targetPlatform ), m_Format );

	AudioClip* newClip = NULL;

	m_Output.previewData.clear();

	// transcode?
	if ( targetType == FMOD_SOUND_TYPE_OGGVORBIS || targetType == FMOD_SOUND_TYPE_MPEG || targetType == FMOD_SOUND_TYPE_XMA || targetType == FMOD_SOUND_TYPE_GCADPCM )
	{
		// is it a wrapped mp3 (in a wav)?
		bool isWrappedMP3 = IsWAV ( &fmod->m_AudioData[0] ) && ( fmod->m_Type == FMOD_SOUND_TYPE_MPEG );

		// if source type is already the right format then just bypass the transcoding
		if ( fmod->m_Type != targetType || isWrappedMP3 )
		{
			if ( isWrappedMP3 )
			{
				LogImportWarning ( "WAV contains a MP3 file - file is unwrapped and reencoded (this might affect quality)." );
			}

			// Calculate correct bitrate
			// if not set by user, use kDefaultQuality
			// if larger that original file, use the original file bitrate
			// if larger than maximum, use maximum
			// if less than minimum, use minimum
			// Don't touch the meta/asset file
			float quality = m_Quality == -1.0f ? kDefaultQuality : m_Quality;
			unsigned bitrate = GetAudioBitrateForQuality( quality );
			bitrate = bitrate < fmod->m_Bitrate ? bitrate : fmod->m_Bitrate;
			bitrate = bitrate < GetMaxBitrate( targetType, fmod->m_Channels, fmod->m_Frequency ) ? bitrate : GetMaxBitrate( targetType, fmod->m_Channels, fmod->m_Frequency );
			bitrate = bitrate > GetMinBitrate( targetType, fmod->m_Channels, fmod->m_Frequency ) ? bitrate : GetMinBitrate( targetType, fmod->m_Channels, fmod->m_Frequency );
			fmod->SetOggBitrate ( bitrate );
			fmod->SetForceToMono ( m_ForceToMono );
			

			// Transcode to OGG that the Editor can play back. If necessary.
			if ( EditorAudioType(targetType) != targetType )
			{
				if ( !fmod->TranscodeToOgg ( kTempEditorAudioPath, GetUpdateProgressCallback(), false ) )
				{
					LogImportError ( "Error compressing Audio: " + GetAssetPathName () + "(" + fmod->GetErrorString() + ")" );
					return;
				}
			}

			if ( targetType == FMOD_SOUND_TYPE_OGGVORBIS )
			{
				if ( !fmod->TranscodeToOgg ( kTempAudioPath, GetUpdateProgressCallback(), false ) )
				{
					LogImportError ( "Error compressing Audio: " + GetAssetPathName () + "(" + fmod->GetErrorString() + ")" );
					return;
				}
			}
			else if ( targetType == FMOD_SOUND_TYPE_MPEG )
			{
				if ( !fmod->TranscodeToMp3 ( kTempAudioPath , m_Loopable, GetUpdateProgressCallback() ) )
				{
					LogImportError ( "Error compressing Audio: " + GetAssetPathName () + "(" + fmod->GetErrorString() + ")" );
					return;
				}
			}
			else if ( targetType == FMOD_SOUND_TYPE_XMA )
			{
#if UNITY_WIN
				if ( !fmod->TranscodeToXMA ( kTempAudioPath, m_Loopable, m_Quality, GetUpdateProgressCallback() ) )
				{
					LogImportError ( "Error compressing Audio: " + GetAssetPathName () + "(" + fmod->GetErrorString() + ")" );
					return;
				}
#else
					LogImportError ( "Error compressing Audio: " + GetAssetPathName () + "(XMA compression is not supported on MacOS)" );
					return;
#endif
			}
			else if (targetType == FMOD_SOUND_TYPE_GCADPCM)
			{
#if UNITY_WIN
				if (!fmod->TranscodeToADPCM (kTempAudioPath , m_Loopable, GetUpdateProgressCallback()))
				{
					LogImportError ("Error compressing Audio to ADPCM: " + GetAssetPathName () + "(" + fmod->GetErrorString() + ")");
					return;
				}
#else
					LogImportError ("Error compressing Audio: " + GetAssetPathName () + "(ADPCM compression is only supported on Windows)");
					return;
#endif
			}

			// release old importer and create a new one for transcoded audio
			fmod.reset (new FMODImporter ( kTempAudioPath, false ));

			// FMOD importer won't be able to load GCADPCM format on the Editor, so it won't correctly setup format and type, do it for him
			if (targetType == FMOD_SOUND_TYPE_GCADPCM)
			{
				fmod->m_Type = FMOD_SOUND_TYPE_GCADPCM;
				fmod->m_Format = FMOD_SOUND_FORMAT_GCADPCM;
			}
		}

		newClip = &ProduceAssetObject<AudioClip> ();
		Assert( newClip );

		// Load Editor audio type (if necessary)
		if (EditorAudioType(targetType) != targetType)
		{
			std::auto_ptr<FMODImporter> editorFMODImporter( new FMODImporter ( kTempEditorAudioPath, targetType == FMOD_SOUND_TYPE_GCADPCM ));
			newClip->SetEditorAudioData(editorFMODImporter->m_AudioData, editorFMODImporter->m_Type, editorFMODImporter->m_Format );
			if ( !editorFMODImporter->GeneratePreview ( PREVIEW_WIDTH, m_Output.previewData ) )
			{
				LogImportWarning ( Format ( "Previews can not be generated for this file '%s' (%s)", pathName.c_str(), fmod->GetErrorString().c_str() ) );
			}
		}
		else
		{
			if ( !fmod->GeneratePreview ( PREVIEW_WIDTH, m_Output.previewData ) )
			{
				LogImportWarning ( Format ( "Previews can not be generated for this file '%s' (%s)", pathName.c_str(), fmod->GetErrorString().c_str() ) );
			}
		}
		newClip->SetAudioDataSwap ( fmod->m_AudioData, m_3D, m_UseHardware, m_LoadFlag, false, targetType, fmod->m_Format );		

		// release fmod mem (not the object itself!)
		fmod->release();
		// On Wii only GCADPCM is really a compressed format, treat all others as native
		if (targetPlatform == kBuildWii) m_Format = targetType == FMOD_SOUND_TYPE_GCADPCM ? kFormatCompressed : kFormatNative;
		else m_Format = kFormatCompressed;
	}
	else if ( targetType == FMOD_SOUND_TYPE_UNKNOWN )
	{
		string err = Format ( "The file '%s' does not contain a supported '%s' format (%s)", pathName.c_str(), ext.c_str(), fmod->GetErrorString().c_str() );
		LogImportError ( err );
		return;
	}
	else
	{
		if ( !fmod->GeneratePreview ( PREVIEW_WIDTH, m_Output.previewData ) )
		{
			LogImportWarning ( Format ( "Previews can not be generated for this file '%s' (%s)", pathName.c_str(), fmod->GetErrorString().c_str() ) );
		}

		newClip = &ProduceAssetObject<AudioClip> ();
		newClip->SetAudioDataSwap ( fmod->m_AudioData, m_3D, m_UseHardware, m_LoadFlag, false, fmod->m_Type, fmod->m_Format );

		m_Format = kFormatNative;
	}

	MonoPostprocessAudio ( *newClip, pathName );
	newClip->AwakeFromLoad ( kDefaultAwakeFromLoad );

	m_Output.importedType = newClip->GetType();
}

AudioImporter::OriginalImportData AudioImporter::GetOriginalImportData ()
{
	OriginalImportData data;
	memset ( &data, 0, sizeof ( OriginalImportData ) );

	FMODImporter fmod;

	if ( fmod.open_only ( GetAssetPathName () ) )
	{
		data.channels = fmod.m_Channels;
		data.durationMS = fmod.m_Duration;
		data.type = fmod.m_Type;
		data.isOgg = ( data.type == FMOD_SOUND_TYPE_OGGVORBIS );
		data.isCompressible = ( data.type != FMOD_SOUND_TYPE_MOD ) &&
		                      ( data.type != FMOD_SOUND_TYPE_S3M ) &&
		                      ( data.type != FMOD_SOUND_TYPE_MIDI ) &&
		                      ( data.type != FMOD_SOUND_TYPE_XM ) &&
		                      ( data.type != FMOD_SOUND_TYPE_IT ) &&
		                      ( data.type != FMOD_SOUND_TYPE_SF2 ) &&
		                      ( fmod.m_Format != FMOD_SOUND_FORMAT_PCM32 ) &&
		                      ( data.type != FMOD_SOUND_TYPE_MPEG ) &&
		                      ( data.type != FMOD_SOUND_TYPE_OGGVORBIS );
		data.frequency = fmod.m_Frequency;
		data.fileSize = GetFileLength ( GetAssetPathName() ) ;
		data.bitRate = fmod.m_Bitrate;
		data.canForceToMono = ( data.type == FMOD_SOUND_TYPE_WAV ) || ( data.type == FMOD_SOUND_TYPE_AIFF );
	}

	return data;
}


AudioImporter::Output::Output ()
: previewData(kMemAudio)
{
	importedType = FMOD_SOUND_TYPE_UNKNOWN;
}

template<class T>
void AudioImporter::Output::Transfer ( T& transfer )
{
	TRANSFER(previewData);
	TRANSFER_ENUM(importedType);
}


///@TODO: This seems fairly brittle code. Make a simple test that imports a couple of old data files
//        with .meta and check that the upgraded data is correct
bool AudioImporter::TranslateOldSettings ( int channels, int format )
{
	OriginalImportData data = GetOriginalImportData();

	// old format --> new format
	m_ForceToMono = ( channels == 1 && data.channels != 1 );

	if ( data.channels < 2 || m_ForceToMono )
		m_3D = true;
	else
		m_3D = false;

	m_Format = kFormatNative;
	if ( format == 1 )
		m_Format = kFormatCompressed;

	// U1.7: Apple native (mp3, m4a, caf ...) - we only support mp3 in 3.0
	if ( format == 2 )
	{
		// Old IPhone 1.7
		// Bail on m4a and caf files (not supported in 3.0)
		if ( data.type == FMOD_SOUND_TYPE_UNKNOWN ) // m4a or caf
		{
			return false;
		}

		m_Format = kFormatCompressed;
		m_UseHardware = true;
	}

	// fixup bitrate
	// never set higher bitrate than original file
	SetBitrate ( GetBitrate() < data.bitRate ? GetBitrate() : data.bitRate );

	return true;
}

template<class T>
void AudioImporter::Transfer ( T& transfer )
{
	Super::Transfer ( transfer );

	// version 4: Stream replacing decompressOnLoad, UseHardware (for IPhone)
	// version 3: 3D and hardware properties
	// version 2: preview data added
	// version 1: ...
	transfer.SetVersion ( 4 );

	TRANSFER ( m_Format );
	TRANSFER ( m_Quality );
	transfer.Transfer ( m_LoadFlag, "m_Stream" );
	TRANSFER ( m_3D );
	TRANSFER ( m_ForceToMono );
	TRANSFER ( m_UseHardware );
	TRANSFER ( m_Loopable );
	
	if ( transfer.IsOldVersion ( 3 ) )
	{
		bool decompressOnLoad;
		transfer.Transfer ( decompressOnLoad, "m_DecompressOnLoad", kNotEditableMask );
		m_LoadFlag = !decompressOnLoad;

		transfer.Align();
		TRANSFER ( m_3D );
		TRANSFER ( m_ForceToMono );

		///@TODO: This is definately wrong. Why should the transfer function change a value based on the input value?
		////      GenerateAsset should interpret bitrate correctly and clamp it no???
		
		// fixup bitrate
		// never set higher bitrate than original file
		OriginalImportData data = GetOriginalImportData();
		SetBitrate ( GetBitrate() < data.bitRate ? GetBitrate() : data.bitRate );

	}
	else if ( transfer.IsVersionSmallerOrEqual ( 2 ) )
	{
		unsigned channels = 0;

		transfer.Transfer ( channels, "m_Channels", kNotEditableMask );

		bool decompressOnLoad;
		transfer.Transfer ( decompressOnLoad, "m_DecompressOnLoad", kNotEditableMask );
		m_LoadFlag = !decompressOnLoad;

		transfer.Align();

		if ( !TranslateOldSettings ( channels, m_Format ) )
		{
			string pathName = GetAssetPathName ();
			string ext = ToLower ( GetPathNameExtension ( pathName ) );
			ErrorString ( Format ( "The format '%s' is not supported anymore. Check the documentation for supported formats.", ext.c_str() ) );
		}
	}

	
	/// This is strictly cached data,
	/// optimally it shouldn't be part of the texture importer
	if (!transfer.AssetMetaDataOnly())
	{
		TRANSFER(m_Output);
	}

	PostTransfer (transfer);
}

/// Reimport for current platform
bool AudioImporter::DoesAssetNeedReimport ( const string& path, BuildTargetPlatform targetPlatform, bool unload )
{
	bool reimport = false;
	if ( !CanLoadPathName ( path ) )
		return false;

	string metaDataPath = GetMetaDataPathFromAssetPath ( path );
	AudioImporter* importer = dynamic_pptr_cast<AudioImporter*> ( FindAssetImporterAtPath ( metaDataPath ) );

	if ( importer && importer->m_Format != -1 )
	{
		const OriginalImportData& info = importer->GetOriginalImportData ();
		const FMOD_SOUND_TYPE toType = GetPlatformConversionType ( info.type, GetBuildTargetGroup( targetPlatform ), importer->GetFormat() );

		if ( importer->m_Output.importedType != toType )
			reimport = true;
	}

	if ( unload )
	{
		if ( importer && !importer->IsPersistentDirty() )
			UnloadObject ( importer );
		
		GetPersistentManager().UnloadStream(metaDataPath);
	}

	return reimport;
}

int AudioImporter::GetMinBitrate ( FMOD_SOUND_TYPE type, short channels, unsigned frequency ) const
{
	if ( type == FMOD_SOUND_TYPE_OGGVORBIS )
		return GetOggVorbisBitRateMinMaxLimit ( channels , frequency ).first;
	else // MP3
		return FMODImporter::GetValidBitRateRange ( frequency ).first;
}

int AudioImporter::GetMaxBitrate ( FMOD_SOUND_TYPE type, short channels, unsigned frequency ) const
{
	if ( type == FMOD_SOUND_TYPE_OGGVORBIS )
		return GetOggVorbisBitRateMinMaxLimit ( channels , frequency ).second;
	else // MP3
		return FMODImporter::GetValidBitRateRange ( frequency ).second;
}

int AudioImporter::GetDefaultBitrate () const
{
	return GetAudioBitrateForQuality ( kDefaultQuality );
}

IMPLEMENT_CLASS_HAS_INIT ( AudioImporter )
IMPLEMENT_OBJECT_SERIALIZE ( AudioImporter )
