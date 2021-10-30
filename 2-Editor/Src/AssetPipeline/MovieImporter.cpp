#include "UnityPrefix.h"
#include "MovieImporter.h"
#include "Runtime/Video/MovieTexture.h"
#include "AssetDatabase.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Audio/AudioClip.h"
#include "QuickTimeMovieImporter.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "AudioVideoImporter.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#if UNITY_WIN
#include "Editor/Platform/Windows/QuicktimeUtils.h"
#include "Editor/Platform/Windows/QuicktimeTools.h"
#endif

const int kMovieImporterVersion = 2;

//16k should be enough for detecting ogg streamsF
#define kOggHeaderParseLength (16*1024)
static const char* kTempFilePath = "Temp/ImportedOgg.ogg";

static int CanLoadPathName (const string& pathName, int* queue);
static int CanLoadPathName (const string& pathName, int* queue)
{
	string ext = ToLower(GetPathNameExtension (pathName));
	bool ok=false;

	#if ENABLE_MOVIES

	if( (strcmp (ext.c_str(), "ogg") == 0) || (strcmp (ext.c_str(), "ogv") == 0) )
	{
		int l=GetFileLength(pathName);
		if(l>kOggHeaderParseLength)
			l=kOggHeaderParseLength;

		UInt8 *data= new UInt8[l];
		if(ReadFromFile(pathName,data,0,l))
		{
			MoviePlayback testPlayback;

			if(testPlayback.LoadMovieData(data,l))
				ok = testPlayback.MovieHasVideo();
		}
		delete []data;
	}
	if(strcmp (ext.c_str(), "mov") == 0
	|| strcmp (ext.c_str(), "avi") == 0
	|| strcmp (ext.c_str(), "asf") == 0
	|| strcmp (ext.c_str(), "mpg") == 0
	|| strcmp (ext.c_str(), "mpeg") == 0
	|| strcmp (ext.c_str(), "mp4") == 0
	)
		ok = true;

	*queue = -10000;

	#endif // ENABLE_MOVIES

	return ok;
}

MovieImporter::MovieImporter(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Quality=0.5;
	m_LinearTexture = false;
}

MovieImporter::~MovieImporter ()
{
}

void MovieImporter::InitializeClass ()
{
	// MovieImporter must come before Audio Importer
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (MovieImporter), kMovieImporterVersion, -20);
}

bool MovieImporter::ImportOggMovie (string pathName)
{
	InputString contents;
	if (!ReadStringFromFile (&contents, pathName))
	{
		LogImportError ("File couldn't be read");
		return false;
	}

	#if ENABLE_MOVIES

	//Try to parse ogg headers to see if we understand this file
	MoviePlayback testPlayback;
	if(!testPlayback.LoadMovieData((UInt8*)&*contents.begin(),contents.size()))
	{
		return false;
	}

	if(testPlayback.MovieHasVideo())
	{
		//create MoveTexture
		MovieTexture *movie = &ProduceAssetObject<MovieTexture> ();
		movie->SetStoredColorSpaceNoDirtyNoApply (m_LinearTexture ? kTexColorSpaceLinear : kTexColorSpaceSRGB);
		movie->SetMovieData ((UInt8*)&contents[0],contents.size());

		//and AudioClip if necessary
		if(testPlayback.MovieHasAudio())
		{
			AudioClip *audio = &ProduceAssetObject<AudioClip> ();
			string name = DeletePathNameExtension(GetLastPathNameComponent(GetAssetPathName ()))+" audio";
			// AudioClip::AwakeFromLoad calls Cleanup/LoadSound which we don't want as audio is initialized
			// by the MoviePlayback. Therefore we call the 'hack' version here to pass the checks for awakes-called.
			audio->HackSetAwakeWasCalled();
			audio->SetName(name.c_str());
			movie->SetMovieAudioClip(audio);
		}
		movie->AwakeFromLoad(kDefaultAwakeFromLoad);

		return true;
	}

	#endif // ENABLE_MOVIES

	return false;
}

void MovieImporter::ImportQuickTimeMovie (string pathName)
{
	#if ENABLE_QTMOVIEIMPORTER
	#if UNITY_WIN
	if( !InitializeQuicktime() )
	{
		LogImportError ("Movie importing requires Quicktime to be installed.");
		return;
	}
	#endif

	QuickTimeMovieImporter qtimport;
	if (!qtimport.Open (pathName))
	{
		LogImportError ("Error while reading Movie: " + GetAssetPathName ());
		return;
	}

	qtimport.SetOggVideoBitrate(GetVideoBitrateForQuality(m_Quality));
	qtimport.SetOggAudioBitrate(GetAudioBitrateForQuality(m_Quality));

	if(!qtimport.TranscodeToOgg(kTempFilePath, GetUpdateProgressCallback(), true))
	{
		LogImportError ("Movie encoding error [" + GetAssetPathName() + "]: " + qtimport.GetErrorMessage());
		return;
	}

	if (!qtimport.GetWarningMessage().empty())
		LogImportWarning( qtimport.GetWarningMessage() + " [" + GetAssetPathName() + "]" );

#elif UNITY_WIN // WIN64
	std::string app = AppendPathName (GetApplicationContentsPath(), "Tools/QuicktimeTools.exe");
	std::string output;
	std::vector<std::string> arguments;
	arguments.push_back("transcode");
	arguments.push_back(Format("-input  \"%s\"", pathName.c_str()));
	arguments.push_back(Format("-output \"%s\"", kTempFilePath));
	arguments.push_back(Format("-vbr %f", GetVideoBitrateForQuality(m_Quality)));
	arguments.push_back(Format("-abr %f", GetAudioBitrateForQuality(m_Quality)));
	bool quoteArguments = false;
	UInt32 exitCode = 0;
	bool appStarted = LaunchTaskArray (app, &output, arguments, quoteArguments, std::string(), &exitCode);
	if (!appStarted)
	{
		LogImportError ("Failed to start importing movie [" + GetAssetPathName() + "]: " + output);
	}
	switch(exitCode)
	{
		case QTT_NEED_QUICKTIME:
			LogImportError ("Movie importing requires Quicktime to be installed.");
			return;
		case QTT_UNABLE_TO_READ:
			LogImportError ("Error while reading Movie: " + GetAssetPathName ());
			return;
		case QTT_UNABLE_TO_ENCODE:
			LogImportError ("Movie encoding error [" + GetAssetPathName() + "]: " + output);
			return;
		case QTT_NO_ERRORS_OR_WARNINGS:
			// everything's fine
			break;
		case QTT_WARNINGS_WHILE_ENCODING:
			LogImportWarning( output + " [" + GetAssetPathName() + "]" );
			break;
		case QTT_GENERIC_ERROR:
		default:
			LogImportError ("Movie import error [" + GetAssetPathName() + "]: " + output);
			return;
	}
#endif //WIN64

	if( !ImportOggMovie(kTempFilePath) )
	{
		LogImportError ("Error reading encoded Movie (buggy encoder?) [" + GetAssetPathName () + "]");
	}
	DeleteFile(kTempFilePath);
}


void MovieImporter::GenerateAssetData ()
{
	string pathName = GetAssetPathName ();
	string ext = ToLower(GetPathNameExtension (pathName));

	if (LicenseInfo::Flag (lf_pro_version))
	{
		if( (strcmp (ext.c_str(), "ogg") == 0) || (strcmp (ext.c_str(), "ogv") == 0) )
		{
			// read ogg files directly
			ImportOggMovie(pathName);
		}
		else
		{
			// everything else is transcoded first
			ImportQuickTimeMovie(pathName);
		}
	}
	else
	{
		LogImportWarning("Movie Playback is only possible with Unity Pro.");
	}
}

float MovieImporter::GetDuration() const
{
#if ENABLE_QTMOVIEIMPORTER

#if UNITY_WIN
	if( !InitializeQuicktime() )
		return 0.0f;
#endif

	// @note this is slow. cache the result somewhere
	QuickTimeMovieImporter qtimport;
	if (!qtimport.Open (GetAssetPathName ()))
		return 0.0f;
	return qtimport.GetDuration();

#elif UNITY_WIN //WIN64

	std::string app = AppendPathName (GetApplicationContentsPath(), "Tools/QuicktimeTools.exe");
	std::string output;
	std::vector<std::string> arguments;
	arguments.push_back("duration");
	arguments.push_back(Format("-input  \"%s\"", GetAssetPathName().c_str()));
	bool quoteArguments = false;
	UInt32 exitCode = 0;
	bool appStarted = LaunchTaskArray (app, &output, arguments, quoteArguments, std::string(), &exitCode);
	if (!appStarted)
		ErrorStringMsg("Failed to launch QuicktimeTools : '%s'", output.c_str());
	float ret = 0.0f;
	if (appStarted && exitCode == QTT_NO_ERRORS_OR_WARNINGS)
		ret = atof(output.c_str());
	return ret;

#endif //WIN64

	return 0.0f;
}



template<class T>
void MovieImporter::Transfer (T& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (1);
	TRANSFER (m_Quality);
	TRANSFER (m_LinearTexture);
	PostTransfer (transfer);
}

IMPLEMENT_CLASS_HAS_INIT (MovieImporter)
IMPLEMENT_OBJECT_SERIALIZE (MovieImporter)

bool ExtractOggFile(Object* obj, string path)
{
	#if ENABLE_MOVIES
	MovieTexture *m = dynamic_pptr_cast<MovieTexture*> (obj);
	if(m != NULL)
	{
		string str;
		int l = m->GetMovieData()->size();
		str.resize(l);
		memcpy(&*str.begin(),&*m->GetMovieData()->begin(),l);
		return WriteStringToFile(str,path,kProjectTempFolder,0);
	}
	else
	{
		AudioClip *c = dynamic_cast<AudioClip*> (obj);
		if(c != NULL)
		{
			return c->WriteRawDataToFile(path);
		}
	}
	#endif // ENABLE_MOVIES
	return false;
}
