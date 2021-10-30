#include "UnityPrefix.h"
#include "MovieEncode.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Allocator/MemoryMacros.h"


// Movie encoder code comes pretty much straight from encoder_example in Theora examples

#include <stdio.h>
//#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#if UNITY_OSX
#include <getopt.h>
#endif

#include <time.h>
#include <math.h>

struct VorbisLimit {
	int maxSampleRate;
	int stereoMin, stereoMax;
	int monoMin, monoMax;
};
const VorbisLimit kVorbisLimits[] = {
	 9000,  6000,  32000,  8000,  42000, //  8 khz
	15000,  8000,  44000, 12000,  50000, // 11 khz
	19000, 12000,  86000, 16000,  90000, // 16 khz
	26000, 15000,  86000, 16000,  90000, // 22 khz
	40000, 18000, 190000, 30000, 190000, // 32 khz
	50000, 22500, 250000, 32000, 240000, // 44 khz
};

#if ENABLE_MOVIES
static int channel_remap_matrix[8][8] =
{
	{0},              /* 1.0 mono   */
	{0,1},            /* 2.0 stereo */
	{0,2,1},          /* 3.0 channel ('wide') stereo */
	{0,1,2,3},        /* 4.0 discrete quadraphonic */
	{0,2,1,3,4},      /* 5.0 surround */
	{0,2,1,4,5,3},    /* 5.1 surround */
	{0,2,1,4,5,6,3},  /* 6.1 surround */
	{0,2,1,6,7,4,5,3} /* 7.1 surround (classic theater 8-track) */
};
#endif

static int LimitOggVorbisBitRate( int channels, int sampleRate, int bitrate )
{
	// Ogg Vorbis has limits on the encode bitrate it supports, based on channels and sample rate.
	// The limits are taken from the encoder's modes/setup_*.h files.
	for( int i = 0; i < ARRAY_SIZE(kVorbisLimits); ++i )
	{
		const VorbisLimit& limit = kVorbisLimits[i];
		if( sampleRate < limit.maxSampleRate ) {
			if( channels == 2 ) {
				return clamp<int>( bitrate, limit.stereoMin*2, limit.stereoMax*2 );
			} else {
				return clamp<int>( bitrate, limit.monoMin*channels, limit.monoMax*channels );
			}
		}
	}
	return -1;
}

std::pair<int, int> GetOggVorbisBitRateMinMaxLimit( int channels, int sampleRate  )
{
	// Ogg Vorbis has limits on the encode bitrate it supports, based on channels and sample rate.
	// The limits are taken from the encoder's modes/setup_*.h files.
	for( int i = 0; i < ARRAY_SIZE(kVorbisLimits); ++i )
	{
		const VorbisLimit& limit = kVorbisLimits[i];
		if( sampleRate < limit.maxSampleRate ) {
			if( channels == 2 ) {
				return std::pair<int, int>( limit.stereoMin*2, limit.stereoMax*2 );
			} else {
				return std::pair<int, int>( limit.monoMin, limit.monoMax );
			}
		}
	}
	return std::pair<int, int>(-1, -1);
}


#if ENABLE_MOVIES


const int kAudioEncodeSize = 128*1024;
#define btols(x) (((x) >> 8) | ((x&0xff) << 8))

int fetch_and_process_audio(MovieEncodeState *s,ogg_page *audiopage)
{
	int i,j;
	
	// read and process more audio 
	SInt8 *thebuffer = NULL;
	unsigned long sampread = 0;
	unsigned sampleSize = 16;
	
	while(!s->audioflag)
	{
		// process any audio already buffered 
		if(ogg_stream_pageout(&s->vo,audiopage)>0) 
		{
			if (thebuffer)
				delete[] thebuffer;
			return 1;
		}

		if(ogg_stream_eos(&s->vo))
		{
			if (thebuffer)
				delete[] thebuffer;
			return 0;
		}

		if (thebuffer)
			delete[] thebuffer;
		thebuffer = NULL;

		//get audio samples from *the* importer
		if (!s->importer->GetNextAudioSamples(&sampread,(void**) &thebuffer, &sampleSize))
		{
			delete[] thebuffer;
			throw "Error reading from audio file";
		}

		SInt8 *readbuffer = thebuffer;

		float **vorbis_buffer;
		
		if(sampread<=0){
			// end of file.  this can be done implicitly, but it's
			//  easier to see here in non-clever fashion.  Tell the
			// library we're at end of stream so that it can handle the
			// last frame and mark end of stream in the output properly 
			vorbis_analysis_wrote(&s->vd,0);
		}else{
			// split samples into page sizes
			unsigned long samples = sampread;
			int count=0;
			unsigned ss = sampleSize;
			const int *remap = channel_remap_matrix[s->audio_ch > 0 ? (s->audio_ch-1) : 0];
			
			while (samples)
			{				
				vorbis_buffer=vorbis_analysis_buffer(&s->vd,sampread);
				// uninterleave samples 
				for(i=0;i<sampread;i++){
					for(j=0;j<s->audio_ch;j++){
						Assert ( count < (sampread * sampleSize * s->audio_ch) );
						SInt16 sample = 0;
						
						if (ss == 1)
							sample = (((SInt16)readbuffer[count])) << 8;
						else if (ss == 2)
							sample = (SInt16)*((SInt16*)(&readbuffer[count]));
						else if (ss == 4 && s->importer->GetFMODFormat() == FMOD_SOUND_FORMAT_PCMFLOAT)
							sample = (SInt16)(*((float*)(&readbuffer[count])) * (1<<15));
						else if (ss > 2)
						{
							// the std ogg encoder only accepts 16 bit input
							// just truncate - can introduce some distortion 
							// @TODO either change ogg encoder or use some kind of rounding and dither
#if UNITY_BIG_ENDIAN
							sample = (SInt16)*((SInt16*)(&readbuffer[count]));		
#else
							sample = (SInt16)*((SInt16*)(&readbuffer[count + (ss-2)]));
#endif
						}
											
						// convert to little endian on a big endian machine
#if UNITY_BIG_ENDIAN
						sample = btols(sample);
#endif						
						SInt8* p = (SInt8*)&sample;						
						
						vorbis_buffer[remap[j]][i]= ((p[1]<<8)|
												  (0x00ff&(int)p[0]))/32768.f;
						count+=ss;
												
					}
				}
				
				int ret = vorbis_analysis_wrote(&s->vd,sampread);

				if (ret)
				{
					throw "vorbis_analysis_wrote failed";
				}
				 
				samples = samples < sampread?0: samples - sampread;
				sampread = samples < sampread? samples : sampread;				
				readbuffer += sampread * s->audio_ch * ss;
				count=0;

				
			}
		}
		
		
		
		while(vorbis_analysis_blockout(&s->vd,&s->vb)==1){
		
			// analysis, assume we want to use bitrate management
			vorbis_analysis(&s->vb,NULL);
			vorbis_bitrate_addblock(&s->vb);
			
			// weld packets into the bitstream 
			while(vorbis_bitrate_flushpacket(&s->vd,&s->op))
				ogg_stream_packetin(&s->vo,&s->op);
			
		}
	}

	delete[] thebuffer;

	return s->audioflag;
}

//positions of rgba data in decoded quicktime frames
#define kAIndex 0
#define kRIndex 3
#define kGIndex 2
#define kBIndex 1

int fetch_and_process_video(MovieEncodeState *s, ogg_page *videopage)
{
	yuv_buffer yuv;
	while( !s->videoflag )
	{
		if(ogg_stream_pageout(&s->to,videopage)>0) return 1;
		if(ogg_stream_eos(&s->to)) return 0;
		
		// read and process more video 
		// video strategy reads one frame ahead so we know when we're
		//  at end of stream and can mark last video frame as such
		// (vorbis audio has to flush one frame past last video frame
		// due to overlap and thus doesn't need this extra work 
		
		// have two frame buffers full (if possible) before
		//   proceeding.  after first pass and until eos, one will
		//   always be full when we get here
		
		for( int fr = s->frameState; fr < 2; ++fr )
		{
			// Decode next frame from quicktime
			if(!s->importer->GetNextFrame())
				break;
				
			int rowBytes = s->image->GetRowBytes();
			UInt8 *rgbline, *rgbline2, *line; 
			UInt8 *rgbBuffer = s->image->GetImageData();
			
			//Get Y data
			line = s->yuvframe[fr] + s->video_x*s->frame_y_offset+s->frame_x_offset;
			rgbline=(UInt8*)rgbBuffer+rowBytes*(s->frame_y-1);
			for(int y=0;y<s->frame_y;y++)
			{
				for(int x=0;x<s->frame_x;x++)
					line[x]=(( 8432*rgbline[4*x+kRIndex] + 16425*rgbline[4*x+kGIndex] +  3176*rgbline[4*x+kBIndex])>>15) + 16;
				rgbline-=rowBytes;
				line+=s->video_x;
			}


			// now get U plane
			line = s->yuvframe[fr] + (s->video_x*s->video_y)+(s->video_x/2)*(s->frame_y_offset/2)+s->frame_x_offset/2;

			rgbline=(UInt8*)rgbBuffer+rowBytes*(s->frame_y-1);
			rgbline2=rgbline-rowBytes;

			for(int y=0;y<s->frame_y/2;y++)
			{
				for(int x=0;x<s->frame_x/2;x++)
				{
					int r=(rgbline[8*x+kRIndex]+rgbline[8*x+4+kRIndex]+rgbline2[8*x+kRIndex]+rgbline2[8*x+4+kRIndex])>>2;
					int g=(rgbline[8*x+kGIndex]+rgbline[8*x+4+kGIndex]+rgbline2[8*x+kGIndex]+rgbline2[8*x+4+kGIndex])>>2;
					int b=(rgbline[8*x+kBIndex]+rgbline[8*x+4+kBIndex]+rgbline2[8*x+kBIndex]+rgbline2[8*x+4+kBIndex])>>2;

					line[x]=(( 14345*r - 12045*g -  2300*b)>>15) + 128;
				}
				rgbline-=rowBytes*2;
				rgbline2-=rowBytes*2;

				line+=s->video_x/2;
			}

			// and the V plane
			line = s->yuvframe[fr] + (s->video_x*s->video_y*5/4)+(s->video_x/2)*(s->frame_y_offset/2)+s->frame_x_offset/2;

			rgbline=(UInt8*)rgbBuffer+rowBytes*(s->frame_y-1);
			rgbline2=rgbline-rowBytes;

			for(int y=0;y<s->frame_y/2;y++)
			{
				for(int x=0;x<s->frame_x/2;x++)
				{
					int r=(rgbline[8*x+kRIndex]+rgbline[8*x+4+kRIndex]+rgbline2[8*x+kRIndex]+rgbline2[8*x+4+kRIndex])>>2;
					int g=(rgbline[8*x+kGIndex]+rgbline[8*x+4+kGIndex]+rgbline2[8*x+kGIndex]+rgbline2[8*x+4+kGIndex])>>2;
					int b=(rgbline[8*x+kBIndex]+rgbline[8*x+4+kBIndex]+rgbline2[8*x+kBIndex]+rgbline2[8*x+4+kBIndex])>>2;

					line[x]=(( -4818*r - 9527*g +  14345*b)>>15) + 128;
				}
				rgbline-=rowBytes*2;
				rgbline2-=rowBytes*2;

				line+=s->video_x/2;
			}
			
			++s->frameState;
		}
		
		if( s->frameState < 1 ) {
			// TODO: error; video had no frames
		}

			
		// Theora is a one-frame-in,one-frame-out system; submit a frame
		// for compression and pull out the packet 

		yuv.y_width=s->video_x;
		yuv.y_height=s->video_y;
		yuv.y_stride=s->video_x;
		
		yuv.uv_width=s->video_x/2;
		yuv.uv_height=s->video_y/2;
		yuv.uv_stride=s->video_x/2;
		
		yuv.y = s->yuvframe[0];
		yuv.u = s->yuvframe[0] + s->video_x*s->video_y;
		yuv.v = s->yuvframe[0] + s->video_x*s->video_y*5/4 ;

		theora_encode_YUVin(&s->td,&yuv);
		
		// if there's only one frame, it's the last in the stream
		theora_encode_packetout( &s->td, s->frameState < 2 ? 1 : 0, &s->op );
		
		ogg_stream_packetin(&s->to,&s->op);
		
		// swap video frames
		{
			unsigned char *temp = s->yuvframe[0];
			s->yuvframe[0] = s->yuvframe[1];
			s->yuvframe[1] = temp;
			s->frameState--;
		}
	}
	return s->videoflag;
}

#endif // ENABLE_MOVIES


void MovieEncodeFrames(MovieEncodeState *s)
{	
	#if ENABLE_MOVIES

    ogg_page audiopage;
    ogg_page videopage;
	
	while(true)
	{
		// is there an audio page flushed?  If not, fetch one if possible 
		if(!s->hasAudio)
			s->audioflag=false;
		else
			s->audioflag = fetch_and_process_audio(s,&audiopage);
			
		// is there a video page flushed?  If not, fetch one if possible 
		if(!s->hasVideo)
			s->videoflag=false;
		else
			s->videoflag = fetch_and_process_video(s,&videopage);
		
		if(!s->audioflag && !s->videoflag)
			break;

		// which is earlier; the end of the audio page or the end of the
		//	video page? Flush the earlier to stream 
		int audio_or_video=-1;
		double audiotime=s->audioflag?vorbis_granule_time(&s->vd,ogg_page_granulepos(&audiopage)):-1;
		double videotime=s->videoflag?theora_granule_time(&s->td,ogg_page_granulepos(&videopage)):-1;
		
		if(!s->audioflag){
			audio_or_video=1;
		} else if(!s->videoflag) {
			audio_or_video=0;
		} else {
			if(audiotime<videotime)
				audio_or_video=0;
			else
				audio_or_video=1;
		}
		
		if(audio_or_video==1){
			// flush a video page
			s->outfile->Write( videopage.header, videopage.header_len );
			s->video_bytesout += videopage.header_len;
			s->outfile->Write( videopage.body, videopage.body_len );
			s->video_bytesout += videopage.body_len;
			s->videoflag=0;
			s->timebase=videotime;
			
		}else{
			// flush an audio page 
			s->outfile->Write( audiopage.header, audiopage.header_len );
			s->audio_bytesout += audiopage.header_len;
			s->outfile->Write( audiopage.body, audiopage.body_len );
			s->audio_bytesout += audiopage.body_len;
			s->audioflag=0;
			s->timebase=audiotime;
		}
		
		if(s->timebase!=-1)
		{
/*			if(audio_or_video)
				s->vkbps=rint(s->video_bytesout*8./s->timebase*.001);
			else
				s->akbps=rint(s->audio_bytesout*8./s->timebase*.001);
			
			printf_console(
			
				   "\r      %d:%02d:%02d.%02d audio: %dkbps video: %dkbps                 ",
				   hours,minutes,seconds,hundredths,s->akbps,s->vkbps);	*/
			if(s->progressCallback && (audio_or_video==1 || !s->hasVideo))
				s->progressCallback(s->totalDuration > 0 ? s->timebase/s->totalDuration : 0.0f, -1.0F, s->assetPath);
		}
	}

	#endif // ENABLE_MOVIES
}

bool InitMovieEncoding(MovieEncodeState *s,MovieEncodeParams *p)
{
	s->yuvframe[0] = NULL;
	s->yuvframe[1] = NULL;
	s->frameState = 0;
	s->hasAudio=p->hasAudio;
	s->hasVideo=p->hasVideo;
	s->audio_ch=p->audioChannels;
	s->audio_hz=p->audioFreq;
	s->audio_q=-99;
	s->audio_r=-1;
	s->video_x=0;
	s->video_y=0;
	s->frame_x=p->width;
	s->frame_y=p->height;
	s->frame_x_offset=0;
	s->frame_y_offset=0;
	s->video_hzn=p->fps*1000;
	s->video_hzd=1000;
	s->video_an=1;
	s->video_ad=1;
	s->audioflag=0;
	s->videoflag=0;
	s->akbps=0;
	s->vkbps=0;
	
	s->audio_bytesout=0;
	s->video_bytesout=0;
	
	s->outfile = NULL;

	#if ENABLE_MOVIES
		
	/* yayness.  Set up Ogg output stream */
	srand(time(NULL));
	/* need two inequal serial numbers */
	int serial1, serial2;
	serial1 = rand();
	serial2 = rand();
	if (serial1 == serial2) serial2++;
	ogg_stream_init(&s->to,serial1);
	ogg_stream_init(&s->vo,serial2);
	
	if(s->hasVideo)
	{
		/* Theora has a divisible-by-sixteen restriction for the encoded video size */
		/* scale the frame size up to the nearest /16 and calculate offsets */
		s->video_x=((s->frame_x + 15) >>4)<<4;
		s->video_y=((s->frame_y + 15) >>4)<<4;
		/* We force the offset to be even.
			This ensures that the chroma samples align properly with the luma
			samples. */
		s->frame_x_offset=((s->video_x-s->frame_x)/2)&~1;
		s->frame_y_offset=((s->video_y-s->frame_y)/2)&~1;
		
		theora_info_init(&s->ti);
		s->ti.width=s->video_x;
		s->ti.height=s->video_y;
		s->ti.frame_width=s->frame_x;
		s->ti.frame_height=s->frame_y;
		s->ti.offset_x=s->frame_x_offset;
		s->ti.offset_y=s->frame_y_offset;
		s->ti.fps_numerator=s->video_hzn;
		s->ti.fps_denominator=s->video_hzd;
		s->ti.aspect_numerator=s->video_an;
		s->ti.aspect_denominator=s->video_ad;
		s->ti.colorspace=OC_CS_UNSPECIFIED;
		s->ti.pixelformat=OC_PF_420;
		s->ti.target_bitrate=p->bitrate;
		s->ti.quality=p->quality;
		
		s->ti.dropframes_p=0;
		s->ti.quick_p=1;
		s->ti.keyframe_auto_p=1;
		s->ti.keyframe_frequency=64;
		s->ti.keyframe_frequency_force=64;
		s->ti.keyframe_data_target_bitrate=p->bitrate*1.5;
		s->ti.keyframe_auto_threshold=80;
		s->ti.keyframe_mindistance=8;
		s->ti.noise_sensitivity=1;
		s->image=p->image;
		
		if(theora_encode_init(&s->td,&s->ti)){
			s->errorMessage = "Could not setup Ogg Theora encoder";
			return false;
		}
		theora_info_clear(&s->ti);
	}
	
	s->outfile=p->outfile;
	s->importer=p->importer;
	s->progressCallback=p->progressCallback;
	s->assetPath=p->assetPath;
	s->totalDuration = p->totalDuration;
	
	/* initialize Vorbis too, assuming we have audio to compress. */
	if(s->hasAudio){
		int ret;
		vorbis_info_init(&s->vi);
		int rate = LimitOggVorbisBitRate( s->audio_ch, s->audio_hz, p->audioBitrate );
		if( rate < 0 ) {
			s->errorMessage = Format("Could not setup Ogg Vorbis encoder (invalid audio sampling rate %i)", s->audio_hz );
			return false;
		}
		ret = vorbis_encode_init(&s->vi,s->audio_ch,s->audio_hz,-1,rate,-1);
		if(ret) {
			s->errorMessage = "Could not setup Ogg Vorbis encoder (quality out of range?)";
			return false;
		}
		
		vorbis_comment_init(&s->vc);
		if(p->totalDuration>0)
		{
			char durationComment[128];
			sprintf(durationComment,"%.2f",p->totalDuration);
			vorbis_comment_add_tag(&s->vc, const_cast<char*> ("DURATION"),durationComment);
		}
		ret = vorbis_analysis_init(&s->vd,&s->vi);
		if(ret) {
			s->errorMessage = "Could not setup Ogg Vorbis encoder (quality out of range?)";
			return false;
		}
		ret = vorbis_block_init(&s->vd,&s->vb);
		if(ret) {
			s->errorMessage = "Could not setup Ogg Vorbis encoder (quality out of range?)";
			return false;
		}
	}

	/* write the bitstream header packets with proper page interleave */
	
	if(s->hasVideo)
	{
		/* first packet will get its own page automatically */
		theora_encode_header(&s->td,&s->op);
		ogg_stream_packetin(&s->to,&s->op);
		if(ogg_stream_pageout(&s->to,&s->og)!=1){
			s->errorMessage = "Internal Ogg library error";
			return false;
		}
		s->outfile->Write( s->og.header, s->og.header_len );
		s->outfile->Write( s->og.body, s->og.body_len );
		
		/* create the remaining theora headers */
		theora_comment_init(&s->tc);
		if(p->totalDuration>0)
		{
			char durationComment[128];
			sprintf(durationComment,"%.2f",p->totalDuration);
			theora_comment_add_tag(&s->tc, const_cast<char*> ("DURATION"),durationComment);
		}
		theora_encode_comment(&s->tc,&s->op);
		ogg_stream_packetin(&s->to,&s->op);
		free(s->op.packet);
		
		theora_encode_tables(&s->td,&s->op);
		ogg_stream_packetin(&s->to,&s->op);
	}
	
	if(s->hasAudio){
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;
		
		vorbis_analysis_headerout(&s->vd,&s->vc,&header,&header_comm,&header_code);
		ogg_stream_packetin(&s->vo,&header); // automatically placed in its own page 
		if(ogg_stream_pageout(&s->vo,&s->og)!=1){
			s->errorMessage = "Internal Ogg library error";
			return false;
		}
		s->outfile->Write( s->og.header, s->og.header_len );
		s->outfile->Write( s->og.body, s->og.body_len );
		
		// remaining vorbis header packets 
		ogg_stream_packetin(&s->vo,&header_comm);
		ogg_stream_packetin(&s->vo,&header_code);
	}
	
	/* Flush the rest of our headers. This ensures
		the actual data in each stream will start
		on a new page, as per spec. */
	while(1){
		int result = ogg_stream_flush(&s->to,&s->og);
		if(result<0){
			/* can't get here */
			s->errorMessage = "Internal Ogg library error";
			return false;
		}
		if(result==0)break;
		s->outfile->Write( s->og.header, s->og.header_len );
		s->outfile->Write( s->og.body, s->og.body_len );
	}
	if(s->hasAudio){
		while(1){
			int result=ogg_stream_flush(&s->vo,&s->og);
			if(result<0){
				// can't get here 
				s->errorMessage = "Internal Ogg library error";
				return false;
			}
			if(result==0)break;
			s->outfile->Write( s->og.header, s->og.header_len );
			s->outfile->Write( s->og.body, s->og.body_len );
		}
	}
	
	// initialize the double frame buffer 
	s->yuvframe[0] = (unsigned char *)malloc(s->video_x*s->video_y*3/2);
	s->yuvframe[1] = (unsigned char *)malloc(s->video_x*s->video_y*3/2);
		
	// clear initial frame as it may be larger than actual video data 
	// fill Y plane with 0x10 and UV planes with 0X80, for black data 
	memset(s->yuvframe[0], 0x10, s->video_x*s->video_y);
	memset(s->yuvframe[0] + s->video_x*s->video_y, 0x80, s->video_x*s->video_y/2);
	memset(s->yuvframe[1], 0x10, s->video_x*s->video_y);
	memset(s->yuvframe[1] + s->video_x*s->video_y, 0x80, s->video_x*s->video_y/2);

	return true;

	#else // ENABLE_MOVIES

	s->errorMessage = "Movie importing not supported";
	return false;

	#endif // ENABLE_MOVIES
}

void MovieEncodeClose(MovieEncodeState *s)
{
	#if ENABLE_MOVIES

	if( s->yuvframe[0] )
		free( s->yuvframe[0] );
	if( s->yuvframe[1] )
		free( s->yuvframe[1] );
	/* clear out state */
	
	if(s->hasAudio){
		vorbis_block_clear(&s->vb);
		vorbis_dsp_clear(&s->vd);
		vorbis_comment_clear(&s->vc);
		vorbis_info_clear(&s->vi);
	}
	if(s->hasVideo){
		theora_clear(&s->td);
	}	
	ogg_stream_clear(&s->vo);
	ogg_stream_clear(&s->to);

	#endif // ENABLE_MOVIES
}
