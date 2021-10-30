#include "UnityPrefix.h"
#include "MoviePlayback.h"

#if ENABLE_MOVIES

#include "MovieTexture.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Audio/AudioManager.h"
#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Audio/AudioSource.h"
#include "Runtime/Misc/ReproductionLog.h"

#if UNITY_EDITOR
//editor uses custom ogg, not the one from fmod, because it also needs the encoding functionality, which is not present in the fmod one.
#include "../../External/Audio/libogg/include/ogg/ogg.h"
#else
#include <ogg/ogg.h>  //rely on include directories to pick the ogg.h from fmod for this specific platform.
#endif

#include "Runtime/Utilities/Utility.h" 

#if !UNITY_EDITOR 
#include <vorbis/window.h>
#define ogg_sync_init FMOD_ogg_sync_init
#define ogg_sync_buffer FMOD_ogg_sync_buffer
#define ogg_sync_wrote FMOD_ogg_sync_wrote
#define ogg_stream_pagein FMOD_ogg_stream_pagein
#define ogg_sync_pageout FMOD_ogg_sync_pageout
#define ogg_page_bos FMOD_ogg_page_bos
#define ogg_stream_init FMOD_ogg_stream_init
#define ogg_stream_pagein FMOD_ogg_stream_pagein
#define ogg_stream_packetout FMOD_ogg_stream_packetout
#define vorbis_synthesis_headerin FMOD_vorbis_synthesis_headerin
#define ogg_stream_clear FMOD_ogg_stream_clear
#define vorbis_info_init FMOD_vorbis_info_init
#define vorbis_comment_init FMOD_vorbis_comment_init
#define ogg_stream_packetout FMOD_ogg_stream_packetout
#define vorbis_synthesis_init FMOD_vorbis_synthesis_init
#define vorbis_block_init FMOD_vorbis_block_init
#define vorbis_info_clear FMOD_vorbis_info_clear
#define vorbis_comment_clear FMOD_vorbis_comment_clear
#define vorbis_synthesis_pcmout FMOD_vorbis_synthesis_pcmout
#define vorbis_synthesis_read FMOD_vorbis_synthesis_read
#define ogg_stream_packetout FMOD_ogg_stream_packetout
#define vorbis_synthesis FMOD_vorbis_synthesis
#define vorbis_synthesis_blockin FMOD_vorbis_synthesis_blockin
#define vorbis_comment_clear FMOD_vorbis_comment_clear
#define vorbis_info_clear FMOD_vorbis_info_clear
#define ogg_stream_clear FMOD_ogg_stream_clear
#define vorbis_block_clear FMOD_vorbis_block_clear
#define vorbis_dsp_clear FMOD_vorbis_dsp_clear
#define ogg_stream_clear FMOD_ogg_stream_clear
#define ogg_sync_clear FMOD_ogg_sync_clear
#define ogg_sync_reset FMOD_ogg_sync_reset
#define ogg_page_serialno FMOD_ogg_page_serialno
#define FMOD_OGG_PRE kFMOD_OGG_context,
#else
#define FMOD_OGG_PRE 
#endif

#if UNITY_EDITOR
//editor uses custom ogg, not the one from fmod, because it also needs the encoding functionality, which is not present in the fmod one.
#include "../../External/Audio/libvorbis/include/vorbis/vorbisfile.h"
#include "../../External/Audio/libvorbis/include/vorbis/codec.h"
#else
#include <vorbis/vorbisfile.h> //rely on include directories to pick the ogg.h from fmod for this specific platform.
#endif

#include "assert.h"

#define DEBUG_MOVIES 0
#define kAudioBufferSize (16 * 1024)

void*	kFMOD_OGG_context = NULL;

#if !UNITY_EDITOR
// FMOD doesn't implement this - and we need it to determine the duration of the movie
char *vorbis_comment_query(vorbis_comment *vc, const char *tag, int count){
	  ogg_int32_t i;
	  int found = 0;
	  int taglen = strlen(tag)+1; /* +1 for the = we append */
	  char *fulltag = (char*)alloca(taglen+ 1);
	
	  strcpy(fulltag, tag);
	  strcat(fulltag, "=");
	
	  for(i=0;i<vc->comments;i++){
	    if(!strncmp(vc->user_comments[i], fulltag, taglen)){
	      if(count == found)
	        /* We return a pointer to the data, not a copy */
	              return vc->user_comments[i] + taglen;
	      else
	        found++;
	    }
	  }
	  return NULL; /* didn't find anything */
	}
#endif // vorbis_comment_query

//Init structures
MoviePlayback::MoviePlayback()
{
	m_InitialisedLoad = false;
	m_VorbisInitialised = false;
	m_VorbisStateInitialised = false;
	m_TheoraInitialised = false;
	m_TheoraStateInitialised = false;
	
	m_AudioBuffer = (ogg_int16_t*)UNITY_MALLOC(kMemAudioData, kAudioBufferSize);
	
#if !UNITY_EDITOR
	_FMOD_vorbis_window_init();
#endif

	/* start up Ogg stream synchronization layer */
	ogg_sync_init(&m_OggSynchState);
		
	m_StartTime = 0.0;	
	m_Texture = NULL;
	m_IsPlaying = false;
	m_Loop = false;
	m_Duration = -1;
	m_AudioChannel = NULL;
	m_AudioClip = NULL;
#if ENABLE_WWW
	m_DataStream = NULL;
#endif
	
#if UNITY_EDITOR
	//shut up gcc warnings.
	UNUSED(OV_CALLBACKS_DEFAULT);
	UNUSED(OV_CALLBACKS_NOCLOSE);
	UNUSED(OV_CALLBACKS_STREAMONLY);
	UNUSED(OV_CALLBACKS_STREAMONLY_NOCLOSE);
#endif
}

// Read data from in into the ogg synch state. returns bytes read.
#define kReadChunkSize 4096
int MoviePlayback::ReadBufferIntoOggStream()
{
	char *buffer = ogg_sync_buffer(FMOD_OGG_PRE &m_OggSynchState, kReadChunkSize);
	unsigned int read = m_Data.size - m_Data.position;
	if (read > kReadChunkSize)
		read = kReadChunkSize;
	memcpy(buffer, m_Data.data + m_Data.position, read);
	ogg_sync_wrote(&m_OggSynchState, read);
	m_Data.position += read;
	return read;
}

/* helper: push a page into the appropriate steam */
/* this can be done blindly; a stream won't accept a page
that doesn't belong to it */
void MoviePlayback::QueueOggPageIntoStream()
{
	if (m_TheoraStateInitialised)
		ogg_stream_pagein(FMOD_OGG_PRE &m_TheoraStreamState, &m_OggPage);
	if (m_VorbisStateInitialised)
		ogg_stream_pagein(FMOD_OGG_PRE &m_VorbisStreamState, &m_OggPage);
}

void MoviePlayback::ChangeMovieData(UInt8 *data, long size)
{
	m_Data.data = data;
	m_Data.size = size;
}

bool MoviePlayback::InitStreams(int &theoraHeadersSeen, int &vorbisHeadersSeen)
{
	m_Data.position = 0;
	ogg_packet op;
	
	/* Ogg file open; parse the headers */
	/* Only interested in Vorbis/Theora streams */
	while (true)
	{
		if (ReadBufferIntoOggStream() == 0)
			return false;
		while (ogg_sync_pageout(&m_OggSynchState, &m_OggPage)>0)
		{
			ogg_stream_state test;
			
			/* is this a mandated initial header? If not, stop parsing */
			if (!ogg_page_bos(&m_OggPage))
			{
				/* don't leak the page; get it into the appropriate stream */
				QueueOggPageIntoStream();
				return true;
			}
			
			if (ogg_stream_init(FMOD_OGG_PRE &test, ogg_page_serialno(&m_OggPage)) != 0)
				return false;
			if (ogg_stream_pagein(FMOD_OGG_PRE &test, &m_OggPage) != 0)
				return false;
			if (ogg_stream_packetout(&test, &op) != 1)
				return false;

			
			/* identify the codec: try theora */
			if (!m_TheoraStateInitialised && theora_decode_header(&m_TheoraInfo, &m_TheoraComment, &op) >= 0)
			{
				/* it is theora */
				memcpy(&m_TheoraStreamState, &test, sizeof(test));
				theoraHeadersSeen = 1;
				m_TheoraStateInitialised = true;
			}else if (!m_VorbisStateInitialised && vorbis_synthesis_headerin(FMOD_OGG_PRE &m_VorbisInfo, &m_VorbisComment, &op) >= 0)
			{
				/* it is vorbis */
				memcpy(&m_VorbisStreamState, &test, sizeof(test));
				vorbisHeadersSeen = 1;
				m_VorbisStateInitialised = true;
			}else{
				/* whatever it is, we don't care about it */
				ogg_stream_clear(FMOD_OGG_PRE &test);
			}
		}
		/* fall through to non-bos page parsing */
	}
}

bool MoviePlayback::LoadMovieData( UInt8 *data, long size )
{
	Cleanup();
	// Should never happen, but better safe than crashing.
	if ( !data )
	{
		ErrorString( "LoadMoveData got NULL!" );
		return false;
	}

	theora_info_init(&m_TheoraInfo);
	theora_comment_init(&m_TheoraComment);
	vorbis_info_init(FMOD_OGG_PRE &m_VorbisInfo);
	vorbis_comment_init(&m_VorbisComment);
	m_InitialisedLoad = true; //Signify we have attempted a load.

	ogg_packet op;
	
	m_Data.data = data;
	m_Data.size = size;

	int theoraHeadersSeen = 0;
	int vorbisHeadersSeen = 0;

	if (!InitStreams(theoraHeadersSeen, vorbisHeadersSeen))
	{
		Cleanup();
		return false;
	}		
	
	/* we're expecting more header packets. */
	while ((m_TheoraStateInitialised && theoraHeadersSeen < 3) || (m_VorbisStateInitialised && vorbisHeadersSeen < 3))
	{
		int ret;
		/* look for further theora headers */
		while (m_TheoraStateInitialised && (theoraHeadersSeen < 3) && (ret = ogg_stream_packetout(&m_TheoraStreamState, &op)))
		{
			if (ret < 0)
			{
				printf_console("Error parsing Theora stream headers; corrupt stream?\n");
				Cleanup();
				return false;
			}
			if (theora_decode_header(&m_TheoraInfo, &m_TheoraComment, &op))
			{
				printf_console("Error parsing Theora stream headers; corrupt stream?\n");
				Cleanup();
				return false;
			}
			theoraHeadersSeen++;
		}
		
		/* look for more vorbis header packets */
		while (m_VorbisStateInitialised && (vorbisHeadersSeen < 3) && (ret = ogg_stream_packetout(&m_VorbisStreamState, &op)))
		{
			if (ret < 0)
			{	
				printf_console("Error parsing Vorbis stream headers; corrupt stream?\n");
				Cleanup();
				return false;
			}
			if (vorbis_synthesis_headerin(FMOD_OGG_PRE &m_VorbisInfo, &m_VorbisComment, &op))
			{
				printf_console("Error parsing Vorbis stream headers; corrupt stream?\n");
				Cleanup();
				return false;
			}
			vorbisHeadersSeen++;
		}
		
		/* The header pages/packets will arrive before anything else we
			care about, or the stream is not obeying spec */
		
		if (ogg_sync_pageout(&m_OggSynchState, &m_OggPage)>0)
		{
			QueueOggPageIntoStream(); /* demux into the appropriate stream */
		}
		else
		{
			if (ReadBufferIntoOggStream() == 0)
			{
				fprintf(stderr, "End of file while searching for codec headers.\n");
				Cleanup();
				return false;
			}
		}
	}
	
	/* and now we have it all.  initialize decoders */
	if (m_TheoraStateInitialised)
	{
		theora_decode_init(&m_TheoraState, &m_TheoraInfo);
		const char *duration = theora_comment_query(&m_TheoraComment, const_cast<char*> ("DURATION"), 0);
		if (duration)
			sscanf(duration, "%f", &m_Duration);
		m_TheoraInitialised = true;
			
	}else{
		/* tear down the partial theora setup */
		theora_info_clear(&m_TheoraInfo);
		theora_comment_clear(&m_TheoraComment);
	}
	if (m_VorbisStateInitialised)
	{
		vorbis_synthesis_init(FMOD_OGG_PRE &m_VorbisState, &m_VorbisInfo);
		vorbis_block_init(FMOD_OGG_PRE &m_VorbisState, &m_VorbisBlock);
		const char *duration = vorbis_comment_query(&m_VorbisComment, const_cast<char*> ("DURATION"), 0);
		if (duration)
			sscanf(duration, "%f", &m_Duration);
		m_VorbisInitialised = true;

	}else{
		/* tear down the partial vorbis setup */
		vorbis_info_clear(FMOD_OGG_PRE &m_VorbisInfo);
		vorbis_comment_clear(FMOD_OGG_PRE &m_VorbisComment);
	}

	m_CanStartPlaying = false;
	m_VideoBufferReady = false;
	m_AudioBufferReady = false;
	m_AudioBufferFill = 0;
	m_AudioBufferGranulePos = -1;
	m_VideoBufferTime = 0;
	m_NoMoreData = false;
	m_LastSampleTime = 0;
	m_AudioBufferTime = 0;
	
	//setup audio
	if (m_AudioClip && !m_AudioChannel)
	{
		m_AudioClip->SetMoviePlayback(this);
		// queue
		// if we have no audio channel ... use the ready audio buffer to init
		return m_AudioClip->ReadyToPlay();
	}

	return true;
}


#if ENABLE_WWW
bool MoviePlayback::LoadMovieData(WWW *stream)
{
	if (m_DataStream != stream)
	{
		if (m_DataStream)
			m_DataStream->Release();
		
		m_DataStream = stream;
		m_DataStream->Retain(); // Make sure the WWW object doesn't dissappear if the mono side of the WWW object is deleted before we are done.
	}
	
	stream->LockPartialData();
	int size = stream->GetPartialSize();

	//require headers before starting
	if (size < 16 * 1024)
	{
		stream->UnlockPartialData();
		return false;
	}

	bool accepted = LoadMovieData((UInt8*)stream->GetPartialData(), stream->GetPartialSize());	
	stream->UnlockPartialData();

	return accepted;
}
#endif

bool MoviePlayback::MovieHasAudio()
{
	return m_VorbisInitialised;
}

bool MoviePlayback::MovieHasVideo()
{
	return m_TheoraInitialised;
}

double MoviePlayback::GetMovieTime(bool useAudio)
{
	TimeManager& timeMgr = GetTimeManager();
	
	double ret;
	//use audio for timing if available
	if (MovieHasAudio() && useAudio)
	{						
		double curTime = timeMgr.GetRealtime();
		
		double d = m_VorbisInfo.rate; // /m_VorbisInfo.channels) * 2;
		double dQ = ((double)(kAudioQueueSize) / (d * 2 * m_VorbisInfo.channels));
		double dG = ((double)(m_AudioBufferGranulePos) / d);
		dG = dG < 0 ? 0 : dG;
		double sDiff = (curTime - m_AudioBufferTime);
		ret = (dG - dQ) + (sDiff);	
	}
	else
	{
		//use real time if audio is not available; unless we run in capture timestep
		double curTime = timeMgr.GetCaptureFramerate() > 0 ? timeMgr.GetCurTime() : timeMgr.GetRealtime();
		curTime -= m_StartTime;
		
		//if time is too far off, reset start time, to resume smooth playback from here
		if (curTime > m_LastSampleTime + 0.1 || curTime < m_LastSampleTime)
		{
			double diff = curTime - m_LastSampleTime - 0.1;
			m_StartTime += diff;
			curTime -= diff;
		}	
		ret = curTime;	
	}
	m_LastSampleTime = ret;
	return ret;
}

bool MoviePlayback::MovieStreamImage()
{
	//This may happen if WWW stream is cancelled before movie is stopped.
	if (m_Data.data == NULL && !DidLoad())
		return false;
		
	//can we use audio for timing?
	bool canPlayAudio = false;
	if (m_AudioClip && m_AudioChannel)
		m_AudioChannel->isPlaying(&canPlayAudio);
	
	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
		canPlayAudio = false; 
	#endif
		
	bool didWriteBuffer = false;
	while (!didWriteBuffer)
	{
		/* we want a video and audio frame ready to go at all times.  If
			we have to buffer incoming, buffer the compressed data (ie, let
																	ogg do the buffering) */
		while (MovieHasAudio() && canPlayAudio && !m_AudioBufferReady)
		{
			int ret;
			float **pcm;
			/* if there's pending, decoded audio, grab it */
			if ((ret = vorbis_synthesis_pcmout(&m_VorbisState, &pcm))>0)
			{
				int count = m_AudioBufferFill / 2;
				int maxSamples = (kAudioBufferSize - m_AudioBufferFill) / 2 / m_VorbisInfo.channels;
				maxSamples = std::min (maxSamples, ret);
				for (int i = 0;i < maxSamples;i++)
				{
					for (int j = 0;j < m_VorbisInfo.channels;j++)
					{
						// TODO: implement fast RoundfToInt for intel!
						int val = RoundfToInt(pcm[j][i]*32767.f);
						if (val > 32767)
							val = 32767;
						if (val<-32768)
							val = -32768;
						assert (count + 1 < kAudioBufferSize);
						m_AudioBuffer[count++] = val;
					}
				}
				vorbis_synthesis_read(&m_VorbisState, maxSamples);
				m_AudioBufferFill += maxSamples * m_VorbisInfo.channels * 2;

				if (m_AudioBufferFill == kAudioBufferSize)
					m_AudioBufferReady = true;
				if (m_VorbisState.granulepos >= 0)
					m_AudioBufferGranulePos = m_VorbisState.granulepos - ret + maxSamples;
				else
					m_AudioBufferGranulePos += maxSamples;
					
				m_AudioBufferTime = GetTimeManager().GetRealtime();
			}
			else
			{
				ogg_packet op;
				/* no pending audio; is there a pending packet to decode? */
				if (ogg_stream_packetout(&m_VorbisStreamState, &op)>0)
				{
					if (vorbis_synthesis(FMOD_OGG_PRE &m_VorbisBlock, &op) == 0) /* test for success! */
						vorbis_synthesis_blockin(&m_VorbisState, &m_VorbisBlock);
				}
				else   /* we need more data; break out to suck in another page */
					break;
			}
		}

		while (MovieHasVideo() && !m_VideoBufferReady)
		{
			ogg_packet op;
			/* theora is one in, one out... */
			if (ogg_stream_packetout(&m_TheoraStreamState, &op)>0)
			{
				int ret = theora_decode_packetin(&m_TheoraState, &op);
				AssertIf (ret != 0);
				ogg_int64_t videobuf_granulepos = m_TheoraState.granulepos;

				m_VideoBufferTime = theora_granule_time(&m_TheoraState, videobuf_granulepos);

				/* is it already too old to be useful?  This is only actually
				useful cosmetically after a SIGSTOP.  Note that we have to
				decode the frame even if we don't show it (for now) due to
				keyframing.  Soon enough libtheora will be able to deal
				with non-keyframe seeks.  */

				if (ret == 0 && m_VideoBufferTime >= GetMovieTime(canPlayAudio))
					m_VideoBufferReady = true;

			}else
				break;
		}		

		if (!m_VideoBufferReady && (!m_AudioBufferReady || !canPlayAudio) && m_Data.position >= m_Data.size)
		{
			m_NoMoreData = true;
			return false;
		}
		
		if ((!m_VideoBufferReady && MovieHasVideo()) || (!m_AudioBufferReady && MovieHasAudio() && canPlayAudio))
		{
			/* no data yet for somebody.  Grab another page */
			ReadBufferIntoOggStream();
			while (ogg_sync_pageout(&m_OggSynchState, &m_OggPage)>0)
			{
				QueueOggPageIntoStream();
				m_NoMoreData = false;
			}
		}
		
		/* If playback has begun, top audio buffer off immediately. */
		if (m_CanStartPlaying && MovieHasAudio() && canPlayAudio && m_AudioBufferReady) 
		{
			if (m_AudioClip->QueueAudioData(m_AudioBuffer, kAudioBufferSize))
			{
				m_AudioBufferFill = 0;
				m_AudioBufferReady = false;
			}		
		}
		
		/* are we at or past time for this video frame? */
		if (m_CanStartPlaying && m_VideoBufferReady && m_VideoBufferTime <= GetMovieTime(canPlayAudio))
		{
			if (m_Texture && m_Texture->GetImageBuffer())
			{
				yuv_buffer yuv;
				int ret = theora_decode_YUVout(&m_TheoraState, &yuv);
				if (ret == 0)
				{
					YuvFrame yuvFrame;
					yuvFrame.y = yuv.y;
					yuvFrame.u = yuv.u;
					yuvFrame.v = yuv.v;
					yuvFrame.width = m_TheoraInfo.frame_width;
					yuvFrame.height = m_TheoraInfo.frame_height;
					yuvFrame.y_stride = yuv.y_stride;
					yuvFrame.uv_stride = yuv.uv_stride;
					yuvFrame.uv_step = 1; // non-interleaved UV data
					yuvFrame.offset_x = m_TheoraInfo.offset_x;
					yuvFrame.offset_y = m_TheoraInfo.offset_y;
					m_Texture->YuvToRgb (&yuvFrame);
				}
			}
			didWriteBuffer = true;
			m_VideoBufferReady = false;
		}

		if (m_CanStartPlaying &&
		   (m_AudioBufferReady || !(MovieHasAudio() && canPlayAudio)) &&
		   (m_VideoBufferReady || !MovieHasVideo()) 
		)
		{
			/* we have an audio frame ready (which means the audio buffer is
			full), it's not time to play video, so wait until one of the
	audio buffer is ready or it's near time to play video */
			
			return didWriteBuffer;
		}

		/* if our buffers either don't exist or are ready to go,
		   we can begin playback */
		if ((!MovieHasVideo() || m_VideoBufferReady) && (!(MovieHasAudio() && canPlayAudio) || m_AudioBufferReady ))
			m_CanStartPlaying = true;
			
		/* same if we've run out of input */
		if (m_Data.position >= m_Data.size)
			m_CanStartPlaying = true;
	}

	return didWriteBuffer;
}

void MoviePlayback::Cleanup()
{
	if(!m_InitialisedLoad)
	{
		return;
	}

	if(m_VorbisInitialised)
	{
		vorbis_block_clear(FMOD_OGG_PRE &m_VorbisBlock);
		vorbis_dsp_clear(FMOD_OGG_PRE &m_VorbisState);
	}
	if (m_VorbisStateInitialised)
	{
		ogg_stream_clear(FMOD_OGG_PRE &m_VorbisStreamState);
	}

	if (m_TheoraInitialised)
	{
		theora_clear(&m_TheoraState);
	}
	if (m_TheoraStateInitialised)
	{
		ogg_stream_clear(FMOD_OGG_PRE &m_TheoraStreamState);
	}

	vorbis_comment_clear(FMOD_OGG_PRE &m_VorbisComment);
	vorbis_info_clear(FMOD_OGG_PRE &m_VorbisInfo);
	theora_comment_clear(&m_TheoraComment);
	theora_info_clear(&m_TheoraInfo);

	m_InitialisedLoad = false;
	m_VorbisInitialised = false;
	m_VorbisStateInitialised = false;
	m_TheoraInitialised = false;
	m_TheoraStateInitialised = false;
}

MoviePlayback::~MoviePlayback()
{
	Cleanup();
	
	if (m_AudioClip)
		if (m_AudioClip->GetMoviePlayback() == this)
			m_AudioClip->SetMoviePlayback(NULL);
		
	UNITY_FREE(kMemAudioData, m_AudioBuffer);
	ogg_sync_clear(FMOD_OGG_PRE &m_OggSynchState);
	
#if ENABLE_WWW
	if (m_DataStream)
		m_DataStream->Release();
#endif
}

int MoviePlayback::GetMovieBitrate()
{
	int bitrate = 0;
	if (MovieHasVideo())
	{
		if (m_TheoraInfo.target_bitrate)
			bitrate += m_TheoraInfo.target_bitrate;
		else	//find a good way to guess average bitrate of unknown encodings
			bitrate += 500000;
	}
	if (MovieHasAudio())
	{
		if (m_VorbisInfo.bitrate_nominal > 0)
			bitrate += m_VorbisInfo.bitrate_nominal;
		else if (m_VorbisInfo.bitrate_upper > 0)
			bitrate += m_VorbisInfo.bitrate_upper;
	}
	return bitrate;
}

int MoviePlayback::GetMovieWidth()
{
	if (MovieHasVideo())
		return m_TheoraInfo.frame_width;
	else
		return 0;
}

int MoviePlayback::GetMovieHeight()
{
	if (MovieHasVideo())
		return m_TheoraInfo.frame_height;
	else
		return 0;
}

int MoviePlayback::GetMovieAudioRate()
{
	if (MovieHasAudio())
		return m_VorbisInfo.rate;
	else
		return 0;
}

int MoviePlayback::GetMovieAudioChannelCount()
{
	if (MovieHasAudio())
		return m_VorbisInfo.channels;
	else
		return 0;
}

void MoviePlayback::Play()
{
	m_IsPlaying = true;
	if (m_AudioClip && m_AudioChannel)
	{
		m_AudioChannel->setPaused(false);
	}
}

void MoviePlayback::Stop()
{
	m_IsPlaying = false;
	if (m_AudioClip && m_AudioChannel)
	{
		m_AudioClip->ClearQueue();		
		PauseAudio();		
	}
	Rewind();
}

void MoviePlayback::Pause()
{
	m_IsPlaying = false;
	PauseAudio();
}

void MoviePlayback::SetLoop (bool loop) 
{
	m_Loop=loop; 	
	if (m_AudioChannel)	
	{
		FMOD_MODE mode;
		m_AudioChannel->getMode(&mode);
		mode = (mode & ~(FMOD_LOOP_NORMAL | FMOD_LOOP_OFF)) | (m_Loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
		m_AudioChannel->setMode(mode);
	}
}

void MoviePlayback::SetAudioChannel(FMOD::Channel* channel) 
{
	m_AudioChannel = channel; 
	if(m_AudioChannel)
	{
		FMOD_MODE mode;
		m_AudioChannel->getMode(&mode);
		mode = (mode & ~(FMOD_LOOP_NORMAL | FMOD_LOOP_OFF | FMOD_3D)) | (m_Loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_2D;
		m_AudioChannel->setMode(mode);
	}
}

void MoviePlayback::Rewind()
{
	// Destroy and recreate streams. Just setting file position to 0 may break, because we first have to parse
	// the headers.
	Cleanup();
	
	ogg_sync_reset(&m_OggSynchState);
	
#if ENABLE_WWW
	if (m_DataStream)
	{	
		//update download position
		//lock stream, so it cannot reallocate
		m_DataStream->LockPartialData();
		ChangeMovieData((UInt8*)m_DataStream->GetPartialData(), m_DataStream->GetPartialSize());
	}
#endif

	LoadMovieData(m_Data.data, m_Data.size);	

#if ENABLE_WWW
	if (m_DataStream)
		m_DataStream->UnlockPartialData();
#endif
	
	m_StartTime = GetCurTime();
}

bool MoviePlayback::Update()
{	
	if (!MovieHasVideo() && !MovieHasAudio())
		return false;

	bool videoChanged = false;
	if (m_IsPlaying)
	{
#if ENABLE_WWW
		if (m_DataStream)
		{	
			//update download position
			//lock stream, so it cannot reallocate
			m_DataStream->LockPartialData();
			ChangeMovieData((UInt8*)m_DataStream->GetPartialData(), m_DataStream->GetPartialSize());
		}
#endif

		if (MovieStreamImage())
		{
			if (m_Texture && m_Texture->GetImageBuffer())
				videoChanged = true;
		}

		bool finished = m_NoMoreData;
#if ENABLE_WWW
		if (m_DataStream)
		{
			m_DataStream->UnlockPartialData();

			// if we are still downloading, we probably aren't really at the end
			finished &= m_DataStream->IsDone();
		}
#endif

		// rewind if looping and movie is at end
		if (finished)
		{
			if (m_Loop)
				Rewind();
			else 
			{
				m_IsPlaying = false;
				if (m_AudioChannel)
				{
					PauseAudio();
					if (m_AudioClip) 
						m_AudioClip->ClearQueue();
				}
			}
		}
	}

	return videoChanged;
}

void MoviePlayback::PauseAudio()
{
	// get AudioSource and pause the sound
	if (m_AudioChannel)
	{
		AudioSource* audioSource;
		m_AudioChannel->getUserData((void**) &audioSource);
		if (audioSource)
			audioSource->Pause();
		else
			m_AudioChannel->setPaused(true);
	}	
}

#else // ENABLE_MOVIES
// dummy implementation coded in .h file
#endif

#if UNITY_EDITOR
bool PlayFullScreenMovie (std::string const& path,
                          ColorRGBA32 const& backgroundColor,
                          unsigned long controlMode, unsigned long scalingMode)
{
	return true;
}
#endif
