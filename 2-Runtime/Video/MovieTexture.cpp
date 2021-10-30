#include "UnityPrefix.h"
#include "MovieTexture.h"

#if ENABLE_MOVIES

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "BaseVideoTexture.h"

// --------------------------------------------------------------------------

MovieTexture::MovieTexture (MemLabelId label, ObjectCreationMode mode, WWW *streamData)
:	BaseVideoTexture(label, mode)
{
	m_AudioClip = NULL;
#if ENABLE_WWW
	m_StreamData = NULL;
#endif // ENABLE_WWW
	m_MoviePlayback.SetMovieTexture( this );
	m_MoviePlayback.SetMovieAudioClip( m_AudioClip );
	m_TextureSettings.m_WrapMode = kTexWrapClamp;
}

#if ENABLE_WWW
void MovieTexture::InitStream (WWW * streamData)
{
	AssertIf(m_AudioClip.GetInstanceID() != 0);
	AssertIf(m_StreamData != NULL);
	m_StreamData = streamData;
	if(m_StreamData)
		m_StreamData->Retain();

	if (streamData != NULL)
	{
		m_AudioClip = NEW_OBJECT(AudioClip);
		m_AudioClip->Reset();
		m_AudioClip->InitStream(NULL, &m_MoviePlayback);
	}
	else
		m_AudioClip = NULL;

	m_MoviePlayback.SetMovieAudioClip( m_AudioClip );
	
	// right now we're trying to load movie/sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
}
#endif // ENABLE_WWW

MovieTexture::~MovieTexture ()
{
/*	if( m_ImageBuffer )
	{
		// The allocated buffer for one frame has extra line before the pointer
		// we use in all operations. YUV decoding code operates two lines at a time,
		// goes backwards and thus needs extra line before the buffer in case of odd
		// movie sizes.
		UInt32* realBuffer = m_ImageBuffer - m_TextureWidth;
		delete[] realBuffer;
	}*/
#if ENABLE_WWW
	if(m_StreamData) m_StreamData->Release();
#endif // ENABLE_WWW
}

void MovieTexture::SetMovieData(const UInt8* data,long size)
{
	m_MovieData.assign(data,data+size);

	#if !UNITY_RELEASE
	// right now we're trying to load movie/sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
	#endif
	
}

template<class TransferFunction>
void MovieTexture::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	bool loop = GetLoop();
	transfer.Transfer( loop, "m_Loop", kNoTransferFlags );
	SetLoop(loop);
	transfer.Align();
	transfer.Transfer( m_AudioClip, "m_AudioClip", kNotEditableMask );
	transfer.Transfer( m_MovieData, "m_MovieData", kHideInEditorMask );
	transfer.Transfer( m_ColorSpace, "m_ColorSpace", kHideInEditorMask );
}

bool MovieTexture::ShouldIgnoreInGarbageDependencyTracking()
{
  return false;
}

bool MovieTexture::ReadyToPlay ()	//check if enough of movie loaded to start playing
{
	if (!m_MoviePlayback.DidLoad())
		TryLoadMovie();
	if (m_MoviePlayback.DidLoad())
	{
#if ENABLE_WWW
		if (m_StreamData)
		{
			// Get duration from comment tag
			double totalDuration = m_MoviePlayback.GetMovieTotalDuration();
			// If there's now duration comment (Movie not encoded with unity), estimate using bitrate
			if (totalDuration <= 0)	
			{
				m_StreamData->LockPartialData ();
				int totalSize = m_StreamData->GetPartialSize() / m_StreamData->GetProgress();
				totalDuration = totalSize/(m_MoviePlayback.GetMovieBitrate() / 8.0);
				m_StreamData->UnlockPartialData ();
			}
			
			// Do we have enough to start?
			if (m_StreamData->GetETA() < totalDuration*1.1F)	
				return true;
		}
		else
#endif // ENABLE_WWW
			return true;	//if this isn't a web stream then we can always start
	}
	return false;
}

void MovieTexture::TryLoadMovie ()
{
#if ENABLE_WWW
	if(m_StreamData)
		m_MoviePlayback.LoadMovieData(m_StreamData);	
	else
#endif // ENABLE_WWW
		m_MoviePlayback.LoadMovieData(&*m_MovieData.begin(),m_MovieData.size());	
		
	if(m_MoviePlayback.DidLoad())
	{	
	//if movie loaded, init texture structures
		int width = m_MoviePlayback.GetMovieWidth();
		int height = m_MoviePlayback.GetMovieHeight();

		InitVideoMemory(width, height);
	}
	
	#if !UNITY_RELEASE
	// right now we're trying to load movie/sound in AwakeFromLoad (and do only that) - so we skip the call
	HackSetAwakeWasCalled();
	#endif
}

void MovieTexture::Play ()
{
	if(!m_MoviePlayback.DidLoad())
		TryLoadMovie();
	if(m_MoviePlayback.DidLoad())
	{
		m_MoviePlayback.Play();
		BaseVideoTexture::Play();
	}
}

void MovieTexture::Pause ()
{
	if(m_MoviePlayback.DidLoad())
	{
		m_MoviePlayback.Pause();
		BaseVideoTexture::Pause();
	}
}

void MovieTexture::Stop ()
{
	if(m_MoviePlayback.DidLoad())
	{
		m_MoviePlayback.Stop();
		BaseVideoTexture::Stop();
	}
}

void MovieTexture::Rewind ()
{
	if(m_MoviePlayback.DidLoad())
		m_MoviePlayback.Rewind();
}

void MovieTexture::Update()
{
	if(m_MoviePlayback.DidLoad())
	{
		if(m_MoviePlayback.Update())
			UploadTextureData();
	}
}

bool MovieTexture::IsPlaying ()
{
	if(m_MoviePlayback.DidLoad())
		return m_MoviePlayback.IsPlaying();
	return false;
}

void MovieTexture::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	if (!m_AudioClip.IsNull())
	{	
		m_AudioClip->SetMoviePlayback(&m_MoviePlayback);
		m_MoviePlayback.SetMovieAudioClip( m_AudioClip );
	}
	TryLoadMovie();
}

void MovieTexture::SetMovieAudioClip(AudioClip *clip)
{ 
	m_AudioClip=clip;
	m_AudioClip->SetMoviePlayback(&m_MoviePlayback);
	m_MoviePlayback.SetMovieAudioClip( m_AudioClip );
}

void MovieTexture::UnloadFromGfxDevice (bool forceUnloadAll)
{
	if (!m_MoviePlayback.DidLoad())
		return;

	// Here we want to unload strictly GFX device-specific data,
	// and since the image buffer is not GFX device specific, it should
	// be left untouched. We do, however, delete the GFX texture.
	GetGfxDevice().DeleteTexture(GetTextureID());
}

void MovieTexture::UploadToGfxDevice ()
{
	if (!m_MoviePlayback.DidLoad())
		return;

	int width				= m_MoviePlayback.GetMovieWidth();
	int height			= m_MoviePlayback.GetMovieHeight();

	int texwidth		= GetDataWidth();
	int texheight		= GetDataHeight();

	if (GetImageBuffer() == NULL || width != texwidth || height != texheight)
	{
		// This accommodates the change in width and height of the image buffer,
		// since in such a case we want to recreate the image buffer.
		ReleaseVideoMemory();
		InitVideoMemory(width, height);
	}
	else
	{
		// Recreate the Gfx structure and upload the previously allocated image buffer back
		CreateGfxTextureAndUploadData(false);
	}

	UploadTextureData();
}


IMPLEMENT_CLASS (MovieTexture)
IMPLEMENT_OBJECT_SERIALIZE (MovieTexture)

#endif
