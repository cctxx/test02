#ifndef MOVIE_ENCODE
#define MOVIE_ENCODE

#include "AudioVideoImporter.h"
#include "Runtime/Graphics/Image.h"

#include "External/theora/include/theora/theora.h"

#if UNITY_EDITOR
//editor uses custom ogg, not the one from fmod, because it also needs the encoding functionality, which is not present in the fmod one.
#include "External/Audio/libvorbis/include/vorbis/codec.h"
#include "External/Audio/libvorbis/include/vorbis/vorbisenc.h"
#else
#include <vorbis/codec.h> //rely on include directories to pick the ogg.h from fmod for this specific platform.
#include <vorbis/vorbisenc.h>
#endif

class File;


struct MovieEncodeState {
	bool hasAudio;	//is there an audio track?
	bool hasVideo;	//is there a video track?
	
	int audio_ch;
	int audio_hz;

	float audio_q;
	int audio_r;

	int video_x;
	int video_y;
	int frame_x;
	int frame_y;
	int frame_x_offset;
	int frame_y_offset;
	int video_hzn;
	int video_hzd;
	int video_an;
	int video_ad;
	
	ogg_stream_state to; /* take physical pages, weld into a logical
		stream of packets */
	ogg_stream_state vo; /* take physical pages, weld into a logical
		stream of packets */
	ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */
	
	theora_state     td;
	theora_info      ti;
	theora_comment   tc;
	
	vorbis_info      vi; /* struct that stores all the static vorbis bitstream
		settings */
	vorbis_comment   vc; /* struct that stores all the user comments */
	
	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */
	
	int audioflag;
	int videoflag;
	int akbps;
	int vkbps;
	
	ogg_int64_t audio_bytesout;
	ogg_int64_t video_bytesout;
	double timebase;
	
	unsigned char* yuvframe[2];
	int frameState;
	
	double totalDuration;

	IAudioVideoImporter *importer;
	Image *image;
	File* outfile;

	IAudioVideoImporter::ProgressbarCallback* progressCallback;
	
	std::string assetPath;
	std::string errorMessage;
};

struct MovieEncodeParams {
	bool hasVideo;	//is there a video track?
	int width;		//pixel dimensions
	int height;
	int bitrate; //bitrate in bps. used if quality is 0. 45000 .. 2000000
	int quality; //0..63. overrides bitrate setting
	float fps;	//frames per seconds
	
	float totalDuration; //total duration of stream (to be encoded in comment for web streaming)
	
	bool hasAudio;	//is there an audio track?
	int audioChannels;	//channels
	int audioFreq;	//sample rate
	int audioBitrate;  //vorbis bitrate in bps.
	
	IAudioVideoImporter *importer; //to get data from
	Image *image;					//rendering image used by qtimport to extract rgb data
	File*	outfile;
	
	IAudioVideoImporter::ProgressbarCallback* progressCallback;
	std::string assetPath;
};

bool InitMovieEncoding(MovieEncodeState *s,MovieEncodeParams *p);
void MovieEncodeFrames(MovieEncodeState *s);
void MovieEncodeClose(MovieEncodeState *s);
std::pair<int, int> GetOggVorbisBitRateMinMaxLimit( int channels, int sampleRate  );

#endif
