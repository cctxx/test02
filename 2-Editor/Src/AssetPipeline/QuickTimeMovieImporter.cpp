#include "UnityPrefix.h"
#include "QuickTimeMovieImporter.h"
#if ENABLE_QTMOVIEIMPORTER
// There is no 64-bit QT on windows yet. Need to wait until QuickTime X is ported.
#if UNITY_WIN
#pragma warning(disable:4005) // macro redefinition, happens in Quicktime headers
#define kAudioDecoderComponentType 'adec' //Defined in AudioUnit.framework on OS X.
#endif

#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "MovieEncode.h"

using namespace std;

// QuickTime defaults an opened movie to 600 "movie units" per second
// and converts only the audio that fills each unit.
// (Movie units only partially filled with audio are discarded.)
//
// This routine makes the movie unit equal to the sample rate,
// so there can be no partially filled audio units.
//
// Based on correspondence with Edward Agabeg <email@hidden>
//
// NOTE: This CHANGES the movie (so it needs to be done only once).
//

struct _trackStartDuration {
	TimeValue movieTrackStart;		// in current movie time	(scale)
	TimeScale movieScale;
	TimeValue mediaDuration;
};
typedef struct _trackStartDuration TrackStartDuration, *TrackStartDurationPtr;

static bool HasComponentForType (OSType type, OSType subType)
{
	ComponentDescription cd;
	cd.componentType = type;
	cd.componentSubType = subType;
	cd.componentManufacturer = 0;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;
	return FindNextComponent(0, &cd) != 0;
}

static bool CanHandleAllMovieCodecs (Movie theMovie)
{
	SInt32 trackCount= GetMovieTrackCount(theMovie);
	SampleDescriptionHandle sampleDesc = (SampleDescriptionHandle)NewHandleClear(0);	
	for (int i = 1; i <= trackCount; i++)
	{
		Track track = GetMovieIndTrackType(theMovie, i, SoundMediaType, movieTrackMediaType);
		if (track)
		{
			Media media = GetTrackMedia(track);
			long count = GetMediaSampleDescriptionCount(media);
			
			for (int i = 1; i <= count; i++) 
			{
				GetMediaSampleDescription(media, 1, sampleDesc);
				OSType subType = (**sampleDesc).dataFormat;
				if (!HasComponentForType (kSoundDecompressor, subType)) 
				{
					if (!HasComponentForType (kAudioDecoderComponentType, subType)) 
					{
						ErrorStringMsg ("Could not find QuickTime audio codec for '%c%c%c%c' on your computer. Please install this codec and reimport the movie.\n", subType>>24, subType>>16, subType>>8, subType);
						return false;
					}
				}
			}
		}
	}

	for (int i = 1; i <= trackCount; i++)
	{
		Track track = GetMovieIndTrackType(theMovie, i, VideoMediaType, movieTrackMediaType);
		if (track)
		{
			Media media = GetTrackMedia(track);
			long count = GetMediaSampleDescriptionCount(media);
			
			for (int i = 1; i <= count; i++) 
			{
				GetMediaSampleDescription(media, 1, sampleDesc);
				OSType subType = (**sampleDesc).dataFormat;
				if (!HasComponentForType (decompressorComponentType, subType)) 
				{
					ErrorStringMsg ("Could not find QuickTime video codec for '%c%c%c%c' on your computer. Please install this codec and reimport the movie.\n", subType>>24, subType>>16, subType>>8, subType);
					return false;
				}
			}
		}
	}
	return true;
}

static double FixMovieDuration(Movie theMovie)
{
	int i;
	TimeScale mediaScale, maxMediaScale= 0;
	SInt32 trackCount;
	Track theTrack;
	TimeValue movieDuration;
	TimeScale movieScale;
	double movieSeconds;
	TrackStartDurationPtr tsdP, tp;

	trackCount= GetMovieTrackCount(theMovie);
	ALLOC_TEMP(tsdP, TrackStartDuration, trackCount);
	if( tsdP == NULL )
		return 0.0;

	// Delete all the edits in the track.
	// You can't remove the media, but you can remove the edits that	reference it.
	for (i = 1; i <= trackCount; i++)
	{
		theTrack= GetMovieIndTrackType(theMovie, i, SoundMediaType,
			movieTrackMediaType);
		if (theTrack)
		{
			Media theMedia= GetTrackMedia(theTrack);

			tp= tsdP + (i-1);
			tp->movieTrackStart= GetTrackOffset(theTrack);
			// in current movie time
			tp->movieScale= GetMovieTimeScale(theMovie);
			// this is the movie time (until we change it)
			tp->mediaDuration= GetMediaDuration(theMedia);
			mediaScale= GetMediaTimeScale(theMedia);
			if( mediaScale > maxMediaScale )
				maxMediaScale= mediaScale;	// highest sample rate becomes the movie's rate
			DeleteTrackSegment(theTrack, tp->movieTrackStart,
				GetTrackDuration(theTrack));
		}
	}

	// Set the movie time scale to the highest audio rate in the movie
	SetMovieTimeScale(theMovie,maxMediaScale);

	// Insert the media into the track with the media duration
	for (i = 1; i <= trackCount; i++)
	{
		theTrack= GetMovieIndTrackType(theMovie, i, SoundMediaType,
			movieTrackMediaType);
		if (theTrack)
		{
			TimeValue newTrackStart;	// in new movie time
			double startTimeSeconds;
			tp= tsdP + (i-1);
			startTimeSeconds= ((double)tp->movieTrackStart) /
				(double)tp->movieScale; // samples / (samples/sec) = sec
			newTrackStart= floor(0.5 + startTimeSeconds *
				(double)maxMediaScale);	// seconds * samples/sec = samples

			InsertMediaIntoTrack(theTrack, newTrackStart, 0,
				tp->mediaDuration, 1<<16);
		}
	}

	// check the results
	movieDuration= GetMovieDuration(theMovie);	// samples
	movieScale= GetMovieTimeScale(theMovie);	// now set to theaudio rate (8000 Hz, for example)
	movieSeconds= ((double)movieDuration) / (double)movieScale;	//movie units / movie units per second = seconds.

	return movieSeconds;	// the "fixed" value.
}

#if UNITY_WIN

void OffsetRect (Rect *rect, int left, int top)
{
	rect->left -= left;
	rect->top -= top;
	rect->bottom -= top;
	rect->right -= left;
}

void SetRect(Rect* rect, int x, int y, int w, int h)
{
	rect->left = x;
	rect->top = y;
	rect->bottom = y + h;
	rect->right = x + w;
}

#endif

static bool InitAudioExtraction();

// Manually link QT 7 APIs if using 10.3.9 SDK
// We require QT 7 for Audio Importer which is a lot harder to implement with the QT 6 API.
#if !HAS_QT_7_HEADERS

enum {
	  kQTPropertyClass_MovieAudioExtraction_Audio = 'xaud',
	  kQTMovieAudioExtractionAudioPropertyID_AudioStreamBasicDescription = 'asbd',
	  kQTMovieAudioExtractionAudioPropertyID_AudioChannelLayout = 'clay',
	  kQTMovieAudioExtractionComplete = (1L << 0)
};

// define a ProcPtr type for each API
typedef OSStatus (*fpMovieAudioExtractionBeginType) ( Movie m,UInt32 flags,MovieAudioExtractionRef *outSession );
typedef OSStatus (*fpMovieAudioExtractionEndType) ( MovieAudioExtractionRef session );
typedef OSStatus (*fpMovieAudioExtractionGetPropertyType) ( MovieAudioExtractionRef session,QTPropertyClass inPropClass,QTPropertyID inPropID,ByteCount inPropValueSize,QTPropertyValuePtr outPropValueAddress,ByteCount *outPropValueSizeUsed);
typedef OSStatus (*fpMovieAudioExtractionSetPropertyType) ( MovieAudioExtractionRef session,QTPropertyClass inPropClass,QTPropertyID inPropID,ByteCount inPropValueSize,ConstQTPropertyValuePtr inPropValueAddress);
typedef OSStatus (*fpMovieAudioExtractionFillBufferType) ( MovieAudioExtractionRef session,UInt32 *ioNumFrames,AudioBufferList *ioData,UInt32 *outFlags);

// declare storage for each API's function pointers
fpMovieAudioExtractionBeginType MovieAudioExtractionBegin = NULL;
fpMovieAudioExtractionEndType MovieAudioExtractionEnd = NULL;
fpMovieAudioExtractionGetPropertyType MovieAudioExtractionGetProperty = NULL;
fpMovieAudioExtractionSetPropertyType MovieAudioExtractionSetProperty = NULL;
fpMovieAudioExtractionFillBufferType MovieAudioExtractionFillBuffer = NULL;

CFBundleRef LoadSystemBundle(const char* frameworkName);
static bool InitAudioExtraction() {
	if (MovieAudioExtractionFillBuffer == NULL)
	{
		CFBundleRef qtBundle = LoadSystemBundle("QuickTime.framework");
	
		MovieAudioExtractionBegin=(fpMovieAudioExtractionBeginType)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("MovieAudioExtractionBegin"));
		if(!MovieAudioExtractionBegin)
			return false;
		MovieAudioExtractionEnd=(fpMovieAudioExtractionEndType)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("MovieAudioExtractionEnd"));
		if(!MovieAudioExtractionEnd)
			return false;
		MovieAudioExtractionGetProperty=(fpMovieAudioExtractionGetPropertyType)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("MovieAudioExtractionGetProperty"));
		if(!MovieAudioExtractionGetProperty)
			return false;
		MovieAudioExtractionSetProperty=(fpMovieAudioExtractionSetPropertyType)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("MovieAudioExtractionSetProperty"));
		if(!MovieAudioExtractionSetProperty)
			return false;
		MovieAudioExtractionFillBuffer=(fpMovieAudioExtractionFillBufferType)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("MovieAudioExtractionFillBuffer"));
		if(!MovieAudioExtractionFillBuffer)
			return false;
	}
	return true;
}

#else
static bool InitAudioExtraction() {return true;}
#endif

// InitializeMovie - Initialize needed movie parts for the offscreen handling.
bool InitializeQTEnvironment () ;

static OSErr				QTStep_GetStartTimeOfFirstVideoSample (Movie theMovie, TimeValue *theTime);
static OSErr				QTStep_DrawVideoSampleAtTime (Movie theMovie, TimeValue theTime);
static OSErr				QTStep_DrawVideoSampleNextOrPrev (Movie theMovie, Fixed theRate);
OSErr						QTStep_GoToFirstVideoSample (Movie theMovie);
OSErr						QTStep_GoToNextVideoSample (Movie theMovie);
OSErr						QTStep_GoToPrevVideoSample (Movie theMovie);
long						QTStep_GetFrameCount (Track theTrack);
long QTStep_GetFrameCount (Movie theMovie);

QuickTimeMovieImporter::QuickTimeMovieImporter ()
{ 
	m_GWorld = NULL; 
	m_FrameIndex = 0; 
	m_HasAudio=false;
	m_HasVideo=false;
	m_OggVideoBitrate=512000;
	m_OggAudioBitrate=128000;
	m_AudioChannelSize=16;
	m_AudioChannels=0;
	m_AudioSamplingRate = 0;
	m_AudioSession = NULL;
	m_Movie = NULL;
	m_FileDataPtr = NULL;
}

QuickTimeMovieImporter::~QuickTimeMovieImporter ()
{
	if (m_GWorld)
	{
		#if UNITY_WIN
		// Windows crashes the second time you import a quicktime movies and the previous importer
		// used DisposeGWorld.
		// This doesnt seem to leak a noticable amount of memory because the image is an offscreen buffer allocated
		// externally.
		#else
		DisposeGWorld (m_GWorld);
		#endif
		m_GWorld = NULL;
	}

	if (m_Movie)
	{
		DisposeMovie (m_Movie);
		m_Movie = NULL;
	}

	UNITY_FREE ( kMemAudio, m_FileDataPtr );

	if (m_AudioSession)
	{
		MovieAudioExtractionEnd(m_AudioSession);
	}
}
Handle createPointerDataRefWithExtensions(
                         void               *data,
                         Size               dataSize,
                         Str255             fileName
                         );
OSStatus PtrDataRef_AddFileNameExtension(
        ComponentInstance dataRefHandler,   /* data ref. handler */
        Str255 fileName);                    /* file name for extension */

Handle MyCreatePointerReferenceHandle(void *data, Size dataSize);

bool QuickTimeMovieImporter::Open (const std::string & path)
{
	if (!InitializeQTEnvironment ())
		return false;

    short resID     = movieInDataForkResID;
	OSStatus err = noErr;

	m_AssetPath = path;
	
	File file;

	if ( !file.Open ( path, File::kReadPermission ) )
	{
		ErrorString ( "Unable to open file " + path );
		return false; // file problably doesnt exist
	}

	// get the length of the file
	int size = GetFileLength ( path );
	if (!size)
	{
		// Since UNITY_MALLOC_NULL will return NULL if size is zero,
		// an explicit test here prevents a bogus "Not enough memory" message.
		ErrorString ("Movie file contains no data: " + path);
		file.Close ();
		return false;
	}

	// allocate space
	// Is there enough memory?
	m_FileDataPtr = (UInt8*)UNITY_MALLOC_NULL ( kMemAudio, size );
	
	if (!m_FileDataPtr)
	{
		ErrorString ( "Not enough memory to load movie file into memory: " + path );
		file.Close();
		return false;
	}
	
	if ( file.Read ( ( char* ) m_FileDataPtr, size ) != size )
	{
		file.Close();
		ErrorString ( "Unable to read from file " + path );
		return false;
	}
	
	// close the file
	file.Close();
	
	Str255 filename;
    c2pstrcpy(filename, path.c_str());
    
    Handle ptrDataRef = 0;
	ptrDataRef = createPointerDataRefWithExtensions(m_FileDataPtr, size, filename);
	
	err = NewMovieFromDataRef (&m_Movie, newMovieActive, &resID, ptrDataRef, PointerDataHandlerSubType);	

	m_MovieDuration = FixMovieDuration(m_Movie);
	
	if(InitAudioExtraction())
		m_HasAudio = GetMovieIndTrackType (m_Movie, 1, AudioMediaCharacteristic, movieTrackCharacteristic);
	else
	{
		ErrorString ("QuickTime 7 required for Movie & Audio Importing");
		m_HasAudio = false;		
	}
	m_HasVideo = GetMovieIndTrackType (m_Movie, 1, VisualMediaCharacteristic, movieTrackCharacteristic);
	
	bool result = CanHandleAllMovieCodecs(m_Movie);

	return result;
}

bool QuickTimeMovieImporter::TranscodeToOgg (const string& outPath, ProgressbarCallback* progressCallback, bool importVideo)
{
	m_WarningMessage.clear();
	m_ErrorMessage.clear();
	
	MovieEncodeParams p;
	Image *img=NULL;
	p.hasVideo=false;
	if(HasVideo() && importVideo)
	{
		SetupVideo();
		img = new Image(GetWidth (), GetHeight (), kTexFormatARGB32);
		if (!SetRenderingContext (*img))
		{
			m_ErrorMessage = "Internal Quicktime error while setting up movie encoder";
			if(img)
				delete img;
			return false;
		}
		p.hasVideo=true;
		p.width=GetWidth ();
		p.height=GetHeight ();
		p.fps=GetRecommendedFrameRate ();
		p.quality=0;//63*m_OggVideoQuality;
		p.bitrate=m_OggVideoBitrate;
		p.image=img;
	}
	
	p.hasAudio=false;
	if(HasAudio())
	{
		if (SetupAudio())
		{
			p.hasAudio=true;
			p.audioChannels=GetAudioChannelCount ();
			p.audioFreq=GetAudioSamplingRate ();
			p.audioBitrate = m_OggAudioBitrate;
		}
		else
		{
			m_WarningMessage = "Audio for the movie was ignored. Currently audio from .mpg files can not be imported. Please convert to a .mov file first.";
		}
	}
	
	p.totalDuration=GetDuration();

	File outFile;
	p.outfile = &outFile;
	p.outfile->Open( outPath, File::kWritePermission );

	p.importer=this;
	
	p.progressCallback = progressCallback;
	p.assetPath=m_AssetPath;
	
	MovieEncodeState s;

	if(!InitMovieEncoding(&s,&p))
	{
		if(img)
			delete img;
		m_ErrorMessage = s.errorMessage;
		return false;
	}

	MovieEncodeFrames(&s);
		
	MovieEncodeClose(&s);

	p.outfile->Close();
	p.outfile = NULL;
	
	if(img)
		delete img;
	
	return true;
}

bool QuickTimeMovieImporter::SetupVideo ()
{
	// Align the movie rect. Flip the rect!
	GetMovieBox (m_Movie, &m_Rect); 
	OffsetRect (&m_Rect,  -m_Rect.left,  -m_Rect.top);
	m_Rect.top = m_Rect.bottom;
	m_Rect.bottom = 0;
	SetMovieBox (m_Movie, &m_Rect); 
		
	m_FrameCount = QTStep_GetFrameCount (m_Movie);
	m_FrameRate = m_FrameCount/m_MovieDuration;
	return true;
}

bool QuickTimeMovieImporter::SetupAudio ()
{
	OSStatus err;
	err = MovieAudioExtractionBegin(m_Movie, 0, &m_AudioSession);
	if(err)
		return false;
	
	AudioStreamBasicDescription asbd;

	// Get the default audio extraction ASBD
	err = MovieAudioExtractionGetProperty(m_AudioSession,
			   kQTPropertyClass_MovieAudioExtraction_Audio,
			   kQTMovieAudioExtractionAudioPropertyID_AudioStreamBasicDescription,
			   sizeof (asbd), &asbd, nil);
	if(err)
		return false;
	
	if(asbd.mChannelsPerFrame==0)
		return false;

	//restrain to stereo
	if(asbd.mChannelsPerFrame > 2)
		asbd.mChannelsPerFrame = 2;

	if(m_AudioChannels != 0)
		asbd.mChannelsPerFrame = m_AudioChannels;

	asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	asbd.mBitsPerChannel = m_AudioChannelSize;
	asbd.mBytesPerFrame = m_AudioChannelSize/8 * asbd.mChannelsPerFrame;
	asbd.mBytesPerPacket = asbd.mBytesPerFrame;

	if (m_AudioSamplingRate != 0)
		asbd.mSampleRate = m_AudioSamplingRate;
	else
		m_AudioSamplingRate = (int)asbd.mSampleRate;
	
	m_AudioChannels = asbd.mChannelsPerFrame;
	
	// Set the new audio extraction ASBD
	err = MovieAudioExtractionSetProperty(m_AudioSession,
				kQTPropertyClass_MovieAudioExtraction_Audio,
				kQTMovieAudioExtractionAudioPropertyID_AudioStreamBasicDescription,
				sizeof (asbd), &asbd);	
	if(err)
		return false;

	AudioChannelLayout al;
	memset (&al, 0, sizeof(al));
	if(m_AudioChannels==1)
	{
		al.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
		al.mChannelBitmap = kAudioChannelBit_Center;
		al.mNumberChannelDescriptions = 1;
		al.mChannelDescriptions[0].mChannelLabel = kAudioChannelLabel_Center;
	}
	else
		al.mChannelLayoutTag=kAudioChannelLayoutTag_Stereo;

	err = MovieAudioExtractionSetProperty(m_AudioSession,
				kQTPropertyClass_MovieAudioExtraction_Audio,
				kQTMovieAudioExtractionAudioPropertyID_AudioChannelLayout,
				sizeof (AudioChannelLayout), &al);	
	if(err)
		return false;

	return true;
}

#define kAudioEncodeSize (128*1024)
/**
* Read in next audio samples from the current position.
*
* numSamples : if 0 , proper number of samples are calculated and ppBufferData is allocated (remember to delete it)
*			   numSamples always contains the number of samples read. if 0 on return no more samples are avail. 
* ppBufferData: if numSamples <> NULL, ppBufferData is used as buffer (and not allocated - remember to allocate enough)
*
* returns true/false (SUCCCESS/FAIL)
**/
bool QuickTimeMovieImporter::GetNextAudioSamples(unsigned long *numSamples,void **bufferData, unsigned* sampleSize)
{
	UInt32 flags;
	AudioBuffer buffer;
	// CHANGE: do the buffersize math here instead
	// CHANGE: and allocate buffer
	if (*numSamples == 0)
	{
		// read and process more audio 
		unsigned long sampread=kAudioEncodeSize/2/m_AudioChannels;	
		
		*numSamples = sampread;
		*bufferData = (void*)new char[kAudioEncodeSize];
	}

	buffer.mNumberChannels = m_AudioChannels;
	buffer.mDataByteSize = *numSamples*(m_AudioChannelSize/8)*m_AudioChannels;
	buffer.mData = *bufferData;
	AudioBufferList bufferList;
	bufferList.mNumberBuffers=1;
	bufferList.mBuffers[0]=buffer;
	
	long samplesRead = 0;
	while (1)
	{
		UInt32 numOfSamples = *numSamples;
		OSStatus err = MovieAudioExtractionFillBuffer(m_AudioSession, (MacUInt32*) &numOfSamples, &bufferList, (MacUInt32*) &flags);
		
		samplesRead += numOfSamples;
		
		if (flags & kQTMovieAudioExtractionComplete)
		{
			// extraction complete!
			*numSamples = samplesRead;
			break;
		}
		
		if (samplesRead >= *numSamples)
		{
			// extraction complete!
			*numSamples = samplesRead;
			break;
		}
		
		if(err)
			return false;
	}
	
	
	
	*sampleSize = 2;
	
	return true;
}

bool QuickTimeMovieImporter::SetRenderingContext (ImageReference& ref)
{
	AssertIf (m_GWorld != NULL);

	if (ref.GetWidth () != GetWidth ())
		return false;
	if (ref.GetHeight () != GetHeight ())
		return false;
	if (ref.GetFormat () != kTexFormatARGB32)
		return false;
		
	int gUsedTextureWidth = GetWidth ();
	int gUsedTextureHeight = GetHeight ();
		
//		MatrixRecord 	movieMatrix;
	Rect 			rectNewMovie;
	
//		SetIdentityMatrix (&movieMatrix);

	SetRect (&rectNewMovie, 0, 0, gUsedTextureWidth, gUsedTextureHeight); // l,t, r, b
	
	QTNewGWorldFromPtr (&m_GWorld, k32ARGBPixelFormat, &rectNewMovie, NULL, NULL, 0, ref.GetImageData (), ref.GetRowBytes ());
	if (m_GWorld == NULL)
		return false;

	SetGWorld(m_GWorld, NULL); 										// set current graphics port to offscreen
	SetMovieGWorld(m_Movie, (CGrafPtr)m_GWorld, NULL);

	PixMapHandle ghPixMap = NULL;
	ghPixMap = GetGWorldPixMap (m_GWorld);
	if (ghPixMap && LockPixels (ghPixMap))
	{
		AssertIf ((UInt8*)GetPixBaseAddr (ghPixMap) != ref.GetImageData ());
		AssertIf (GetPixRowBytes (ghPixMap) != ref.GetRowBytes ());
		return true;
	}
	else
		return false;
}

void QuickTimeMovieImporter::GoToFirstFrame ()
{
	ErrorOSErr (QTStep_GoToFirstVideoSample (m_Movie));
	m_FrameIndex = 0;
}

bool QuickTimeMovieImporter::GetNextFrame ()
{
	QTStep_DrawVideoSampleAtTime(m_Movie,(m_FrameIndex/m_FrameRate)*GetMovieTimeScale(m_Movie));
	m_FrameIndex++;
	if (m_FrameIndex >= m_FrameCount)
		return false;
	
//	ErrorOSErr (QTStep_GoToNextVideoSample (m_Movie));
	return true;
}

#define kBogusStartingTime	-1			// an invalid starting time

//////////
//
// METHOD ONE: Use Movie Toolbox calls to step to interesting times in the movie.
//
//////////

//////////
//
// QTStep_GetStartTimeOfFirstVideoSample
// Return, through the theTime parameter, the starting time of the first video sample of the
// specified QuickTime movie.
//
// The "trick" here is to set the nextTimeEdgeOK flag, to indicate that you want to get the
// starting time of the beginning of the movie.
//
// If this function encounters an error, it returns a (bogus) starting time of -1. Note that
// GetMovieNextInterestingTime also returns -1 as a starting time if the search criteria
// specified in the myFlags parameter are not matched by any interesting time in the movie. 
//
//////////

static OSErr QTStep_GetStartTimeOfFirstVideoSample (Movie theMovie, TimeValue *theTime)
{
	short			myFlags;
	OSType			myTypes[1];
	
	*theTime = kBogusStartingTime;							// a bogus starting time
	if (theMovie == NULL)
		return(invalidMovie);
	
	myFlags = nextTimeMediaSample + nextTimeEdgeOK;			// we want the first sample in the movie
	myTypes[0] = VisualMediaCharacteristic;					// we want video samples

	GetMovieNextInterestingTime(theMovie, myFlags, 1, myTypes, (TimeValue)0, fixed1, theTime, NULL);
	return(GetMoviesError());
}


//////////
//
// QTStep_DrawVideoSampleAtTime
// Draw the video sample of a QuickTime movie at the specified time.
//
//////////

static OSErr QTStep_DrawVideoSampleAtTime (Movie theMovie, TimeValue theTime)
{
	OSErr			myErr = noErr;
	
	if (theMovie == NULL)
		return(invalidMovie);
	
	// make sure that the specified time lies within the movie's temporal bounds
	if ((theTime < 0) || (theTime > GetMovieDuration(theMovie)))
		return(paramErr);
	
	SetMovieTimeValue(theMovie, theTime);
	myErr = GetMoviesError();
	if (myErr != noErr)
		goto bail;
		
	// the following calls to UpdateMovie and MoviesTask are not necessary
	// if you are handling movie controller events in your main event loop
	// (by passing the event to MCIsPlayerEvent); they don't hurt, however.
	
	// redraw the movie immediately by calling UpdateMovie and MoviesTask
	UpdateMovie(theMovie);
	myErr = GetMoviesError();
	if (myErr != noErr)
		goto bail;
		
	MoviesTask(theMovie, 0L);
	myErr = GetMoviesError();

bail:
	return(myErr);
}


//////////
//
// QTStep_DrawVideoSampleNextOrPrev
// Draw the next or previous video sample of a QuickTime movie.
// If theRate is 1, the next video sample is drawn; if theRate is -1, the previous sample is drawn.
//
//////////

static OSErr QTStep_DrawVideoSampleNextOrPrev (Movie theMovie, Fixed theRate)
{
	TimeValue		myCurrTime;
	TimeValue		myNextTime;
	short			myFlags;
	OSType			myTypes[1];
	OSErr			myErr = noErr;
	
	if (theMovie == NULL)
		return(invalidMovie);
	
	myFlags = nextTimeStep;									// we want the next frame in the movie's media
	myTypes[0] = VisualMediaCharacteristic;					// we want video samples
	myCurrTime = GetMovieTime(theMovie, NULL);

	GetMovieNextInterestingTime(theMovie, myFlags, 1, myTypes, myCurrTime, theRate, &myNextTime, NULL);
	myErr = GetMoviesError();
	if (myErr != noErr)
		return(myErr);
		
	myErr = QTStep_DrawVideoSampleAtTime(theMovie, myNextTime);
	
	return(myErr);
}


//////////
//
// QTStep_GoToFirstVideoSample
// Draw the first video sample of a QuickTime movie.
//
//////////

OSErr QTStep_GoToFirstVideoSample (Movie theMovie)
{
	TimeValue		myTime;
	OSErr			myErr = noErr;
	
	if (theMovie == NULL)
		return(invalidMovie);
		
	myErr = QTStep_GetStartTimeOfFirstVideoSample(theMovie, &myTime);
	if (myErr != noErr)
		return(myErr);
		
	myErr = QTStep_DrawVideoSampleAtTime(theMovie, myTime);
	return(myErr);
}


//////////
//
// QTStep_GoToNextVideoSample
// Draw the next video sample of a QuickTime movie.
//
//////////

OSErr QTStep_GoToNextVideoSample (Movie theMovie)
{
	return(QTStep_DrawVideoSampleNextOrPrev(theMovie, fixed1));
}

long QTStep_GetFrameCount (Track theTrack)
{	
	long		myCount = -1;
	short		myFlags;
	TimeValue	myTime = 0;
	
	if (theTrack == NULL)
		goto bail;
		
	// we want to begin with the first frame (sample) in the track
	myFlags = nextTimeMediaSample + nextTimeEdgeOK;

	while (myTime >= 0) {
		myCount++;
		
		// look for the next frame in the track; when there are no more frames,
		// myTime is set to -1, so we'll exit the while loop
		GetTrackNextInterestingTime(theTrack, myFlags, myTime, fixed1, &myTime, NULL);
		
		// after the first interesting time, don't include the time we're currently at
		myFlags = nextTimeStep;
	}

bail:
	return(myCount);
}

long QTStep_GetFrameCount (Movie theMovie)
{	
	long		myCount = -1;
	short		myFlags;
	TimeValue	myTime = 0;
	OSType			myTypes[1] = {VisualMediaCharacteristic};
	
	if (theMovie == NULL)
		goto bail;
		
	// we want to begin with the first frame (sample) in the track
	myFlags = nextTimeMediaSample + nextTimeEdgeOK;

	while (myTime >= 0) {
		myCount++;
		
		// look for the next frame in the track; when there are no more frames,
		// myTime is set to -1, so we'll exit the while loop
		GetMovieNextInterestingTime(theMovie, myFlags, 1, myTypes, myTime, fixed1, &myTime, NULL);
		
		// after the first interesting time, don't include the time we're currently at
		myFlags = nextTimeStep;
	}

bail:
	return(myCount);
}

bool InitializeQTEnvironment () 
{
	OSErr anErr = noErr;
	long qtVersion = 0L;
	
	if(noErr != Gestalt (gestaltQuickTime, &qtVersion))
		return false;

	if( (qtVersion >> 16 ) < 0x400 ) 
		return false;
		
	anErr = EnterMovies();
	if(anErr != noErr) 
		return false;
	
	return true;
}

//
// createPointerDataRefWithExtensions
//
// Given a pointer to some movie data, it creates a
// pointer data reference with extensions.
//
// Parameters
//
//   data             A pointer to your movie data
//   dataSize         The actual size of the movie data
//                    specified by the data pointer
//   fileName         If you know the original file name
//                    you should pass it here to help
//                    QuickTime determine which importer
//                    to use. Pass NULL if you do not wish
//                    to specify the fileName
 
Handle createPointerDataRefWithExtensions(
                         void               *data,
                         Size               dataSize,
                         Str255             fileName
                         )
{
    OSStatus  err = noErr;
    Handle dataRef = NULL;
    ComponentInstance dataRefHandler = NULL;

    // First create a data reference handle for our data
    dataRef = MyCreatePointerReferenceHandle(data, dataSize);
    if (!dataRef) goto bail;

    //  Get a data handler for our data reference
    err = OpenADataHandler(
            dataRef,                    /* data reference */
            PointerDataHandlerSubType,  /* data ref. type */
            NULL,                       /* anchor data ref. */
            (OSType)0,                  /* anchor data ref. type */
            NULL,                       /* time base for data handler */
            kDataHCanRead,              /* flag for data handler usage */
            &dataRefHandler);           /* returns the data handler */
    if (err) goto bail;

    // We can add the filename to the data ref to help
    // importer finding process. Find uses the extension.
    // If we add a filetype or mimetype we must add a
    // filename -- even if it is an empty string
    if (fileName)
    {
        err = PtrDataRef_AddFileNameExtension(
            dataRefHandler, /* data ref. handler */
            fileName);      /* file name for extension */

        if (err) goto bail;
    }

    
    /* dispose old data ref handle because
        it does not contain our new changes */
    DisposeHandle(dataRef);
    dataRef = NULL;
    
    /* re-acquire data reference from the
        data handler to get the new
        changes */
    err = DataHGetDataRef(dataRefHandler, &dataRef);

    return dataRef;

bail:
    if (dataRef)
    {
        // make sure and dispose the data reference handle
        // once we are done with it
        DisposeHandle(dataRef);
    }

    return NULL;
}

//////////
//
// PtrDataRef_AddFileNameExtension
//
// Tell the data handler to set
// the file name extension in the
// data reference.
//
//////////

OSStatus PtrDataRef_AddFileNameExtension(
        ComponentInstance dataRefHandler,   /* data ref. handler */
        Str255 fileName)                    /* file name for extension */
{
    OSStatus anErr = noErr;
    unsigned char myChar = 0;
    Handle fileNameHndl = NULL;
    
    /* create a handle with our file name string */
    
    /* if we were passed a null string, then we
       need to add this null string (a single 0
       byte) to the handle */
    
    if (fileName == NULL)
        anErr = PtrToHand(&myChar, &fileNameHndl, sizeof(myChar));
    else
        anErr = PtrToHand(fileName, &fileNameHndl, fileName[0] + 1);
    if (anErr != noErr) goto bail;
    
    /* set the data ref extension for the
        data ref handler */
    anErr = DataHSetDataRefExtension(
            dataRefHandler,         /* data ref. handler */
            fileNameHndl,           /* data ref. extension to add */
            kDataRefExtensionFileName);

bail:
    if (fileNameHndl)
        /* no longer need this */
        DisposeHandle(fileNameHndl);
    
    return anErr;

}


//////////
//
// MyCreatePointerReferenceHandle
// Create a pointer data reference handle.
//
// The pointer data reference handle contains
// a record specifying a pointer to a block of
// movie data along with a size value.
//
//////////

Handle MyCreatePointerReferenceHandle(void *data, Size dataSize)
{
 Handle dataRef = NULL;
 PointerDataRefRecord ptrDataRefRec;
 OSErr err;

 
 ptrDataRefRec.data = data;
 ptrDataRefRec.dataLength = dataSize;

 // create a data reference handle for our data
 err = PtrToHand( &ptrDataRefRec, &dataRef, sizeof(PointerDataRefRecord));

 return dataRef;
}
#endif
