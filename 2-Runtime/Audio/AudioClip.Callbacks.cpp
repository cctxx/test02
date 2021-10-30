#include "UnityPrefix.h"
#include "AudioClip.h"
#include "OggReader.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Mono/MonoScopedThreadAttach.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Audio/AudioManager.h"
#if ENABLE_WWW
#include "Runtime/Export/WWW.h"
#endif

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/MonoCompile.h"
#endif

#include "Runtime/Scripting/Scripting.h"

#if ENABLE_AUDIO_FMOD



/**
 * Streaming callbacks
 **/

/**
 * file callback function used by FMOD
 * we're using this to stream data from WWW stream
 **/


#if ENABLE_WWW
FMOD_RESULT F_CALLBACK AudioClip::WWWOpen( 
										  const char *  wwwstream,  
										  int  unicode,  
										  unsigned int *  filesize,  
										  void **  handle,  
										  void **  userdata 
)
{
	WWW* stream = (WWW*) wwwstream;
	
	if (stream)
	{
		// predict filesize
		stream->LockPartialData();		
			// make sure we've read enough to determine filesize
			if (stream->GetPartialSize() == 0)
			{
				stream->UnlockPartialData();
				return FMOD_ERR_NOTREADY;
			}
			*filesize = (unsigned int)((1.0 / stream->GetProgress()) * stream->GetPartialSize()); 
			wwwUserData* ud = new wwwUserData();
			ud->pos = 0;
			ud->filesize = *filesize;
			ud->stream = stream;
			*userdata = (void*)ud;	
			*handle = (void*) wwwstream;
		stream->UnlockPartialData();
		
		return FMOD_OK;
	}	
	
	return FMOD_ERR_INVALID_PARAM;	
}


FMOD_RESULT F_CALLBACK AudioClip::WWWClose( 
											void *  handle,  
											void *  userdata 
)
{
	if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }	
	
	wwwUserData* ud = (wwwUserData*) userdata;	
	delete ud;	
	return FMOD_OK;
}


FMOD_RESULT F_CALLBACK AudioClip::WWWRead( 
										  void *  handle,  
										  void *  buffer,  
										  unsigned int  sizebytes,  
										  unsigned int *  bytesread,  
										  void *  userdata 
)
{	
	if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
	
	wwwUserData* ud = (wwwUserData*) userdata;	
	
	Assert (ud->stream);
	Assert (ud->stream->GetAudioClip());
	
	ud->stream->LockPartialData();
	
	// read sizebytes
	const UInt8* bufferStart = ud->stream->GetPartialData();
	unsigned avail = ud->stream->GetPartialSize();	
	
	if ( ud->pos > avail)
	{
		ud->stream->UnlockPartialData();
		return FMOD_ERR_NOTREADY;
	}
	
	*bytesread = avail - ud->pos<sizebytes?(avail-ud->pos):sizebytes;
	memcpy (buffer, bufferStart + ud->pos, *bytesread);
	
	// update pos
	ud->pos = ud->pos + *bytesread;
	
	ud->stream->UnlockPartialData();
	
	if (*bytesread < sizebytes)
		return FMOD_ERR_FILE_EOF;
	
	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK AudioClip::WWWSeek( 
										  void *  handle,  
										  unsigned int  pos,  
										  void *  userdata 
)
{	
	if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
	
	wwwUserData* ud = (wwwUserData*) userdata;	
	
	Assert( ud->stream );
	Assert( ud->stream->GetAudioClip() );	
	
	if ( ud->stream->GetAudioClip()->IsWWWStreamed() || ud->filesize < pos)
		return FMOD_ERR_FILE_COULDNOTSEEK;
	
	ud->pos = pos;
	
	return FMOD_OK;
}

#endif

/**
 * Buffer streaming callbacks
 **/

/**
 * Callbacks for PCM streaming
 **/
FMOD_RESULT F_CALLBACK AudioClip::pcmread( 
									  FMOD_SOUND *  sound,  
									  void *  data,  
									  unsigned int  datalen 
									  )
{
	FMOD::Sound* pSound = (FMOD::Sound*) sound;
	AudioClip* ac = NULL;
	pSound->getUserData((void**)&ac);
	
	if (ac->GetQueuedAudioData(&data, datalen))
		return FMOD_OK;
	
	return FMOD_ERR_NOTREADY;
}

/**
 * Callbacks for PCM streaming
 **/
FMOD_RESULT F_CALLBACK AudioClip::ScriptPCMReadCallback( 
										  FMOD_SOUND *  sound,  
										  void *  data,  
										  unsigned int  datalen 
										  )
{
#if SUPPORT_MONO_THREADS || UNITY_WINRT	
	
#if UNITY_EDITOR	
	if (IsCompiling())
		return FMOD_OK;
#endif
	
	FMOD::Sound* pSound = (FMOD::Sound*) sound;
	AudioClip* ac = NULL;
	pSound->getUserData((void**)&ac);
	
	Assert (ac);	
#if ENABLE_MONO
	ScopedThreadAttach attach(ac->monoDomain);
#endif
	
	// reuse mono array
	ScriptingArrayPtr array = SCRIPTING_NULL;
	GetAudioManager().GetScriptBufferManager().GetPCMReadArray(datalen / 4, array);
		
	Assert(array != SCRIPTING_NULL);
		
	// invoke
	ScriptingObjectPtr instance = Scripting::ScriptingWrapperFor(ac);
	ScriptingExceptionPtr exception;

	ScriptingInvocation invocation(ac->m_CachedPCMReaderCallbackMethod);
	invocation.AddArray(array);
	invocation.object = instance;
	invocation.Invoke(&exception);	
	if (exception)
	{
		// handle
		Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(instance));
	}
	else
	{
		memcpy(data, &Scripting::GetScriptingArrayElement<float>( array, 0 ), datalen);
	}
#endif // SUPPORT_MONO_THREADS
	
	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK AudioClip::ScriptPCMSetPositionCallback(
													FMOD_SOUND *  sound, 
													int  subsound, 
													unsigned int  position, 
													FMOD_TIMEUNIT  postype)
{
#if SUPPORT_MONO_THREADS || UNITY_WINRT		
	Assert(postype == FMOD_TIMEUNIT_PCM);
	
	FMOD::Sound* pSound = (FMOD::Sound*) sound;
	AudioClip* ac = NULL;
	pSound->getUserData((void**)&ac);
	
	Assert (ac);
#if ENABLE_MONO
	ScopedThreadAttach attach(ac->monoDomain);
#endif
	
	// invoke
	ScriptingObjectPtr instance = Scripting::ScriptingWrapperFor(ac);

	ScriptingExceptionPtr exception;
	ScriptingInvocation invocation(ac->m_CachedSetPositionCallbackMethod);
	invocation.AddInt(position);
	invocation.object = instance;
	invocation.Invoke(&exception);	

	if (exception)
	{
		// handle
		Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(instance));
	}		
#endif // SUPPORT_MONO_THREADS
	
	return FMOD_OK;	
}

#endif //ENABLE_AUDIO

