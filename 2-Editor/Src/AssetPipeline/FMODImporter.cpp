/*
 *  FMODImporter.cpp
 *  Xcode
 *
 *  Created by Søren Christiansen on 5/5/09.
 *  Copyright 2009 Unity Technologies. All rights reserved.
 *
 */

#include "UnityPrefix.h"


#include "FMODImporter.h"
#include "Runtime/Audio/AudioManager.h"
#include "Runtime/Utilities/File.h"
#include "MovieEncode.h"
#include "Runtime/Audio/WavReader.h"
#include <assert.h>
#include "Runtime/Utilities/PathNameUtility.h"
#if UNITY_WIN
#include "PlatformDependent/Win/WinUnicode.h"
#include <comdef.h>
#include "Runtime/Serialize/SwapEndianArray.h"
#endif

#include "External/Audio/lame/include/lame.h"
#if UNITY_WIN
#include "External/Audio/xma/include/win/xmaencoder.h"
#include "WiiDSPTool.h"
#endif


#include <iostream>
#include <fstream>
using namespace std;


#define kAudioEncodeSize (128*1024)

double GetTimeSinceStartup ();


FMODImporter::FMODImporter ( const std::string& path, bool forceToMono ) :
	m_Sound ( NULL ),
	m_SampleSize ( 0 ),
	m_Offset ( 0 ),
	m_Bitrate ( 128000 ),
	m_Duration ( 0 ),
	m_ForceToMono ( forceToMono ),
	m_AudioData(kMemAudio)
{
	memset ( &m_ExInfo, 0, sizeof ( m_ExInfo ) );
	open ( path );
}

FMODImporter::FMODImporter() :
	m_Sound ( NULL ),
	m_SampleSize ( 0 ),
	m_Offset ( 0 ),
	m_Bitrate ( 128000 ),
	m_Duration ( 0 ),
	m_ForceToMono ( false )
{
	memset ( &m_ExInfo, 0, sizeof ( m_ExInfo ) );
}

FMODImporter::~FMODImporter()
{
	release();
	Assert(m_AudioData.owns_data());
}


bool FMODImporter::open ( const std::string& path )
{
	m_AssetPath = path;

	if ( !readRAW ( path ) ) return false;

	// create FMOD sound.

	m_ExInfo.cbsize = sizeof ( FMOD_CREATESOUNDEXINFO );
	m_ExInfo.length = m_AudioData.size();
	m_Sound = GetAudioManager().CreateFMODSound ( ( const char* ) &m_AudioData[0], m_ExInfo, FMOD_OPENMEMORY | FMOD_SOFTWARE | FMOD_MPEGSEARCH, false );

	if ( m_Sound )
	{
		UpdateProperties();
	}
	else
	{
		m_ErrorString = GetAudioManager().GetLastError();
		return false;
	}

	if (m_Duration == 0) // do not support 0-length sounds
	{
		m_ErrorString = "Unity doesn't support 0 length audio clips.";
		release();
		return false;
	}
	
	if (m_ForceToMono)
	{
		if (!ForceToMono())
		{
			m_ErrorString = "Unable to force file to mono";
			return false;
		}
	}

	return true;
}

bool FMODImporter::open_only ( const std::string& path )
{
	m_AssetPath = path;

	// open w. FMOD
	// create FMOD sound.
	m_ExInfo.cbsize = sizeof ( FMOD_CREATESOUNDEXINFO );

	m_Sound = GetAudioManager().CreateFMODSound ( path.c_str(), m_ExInfo, FMOD_OPENONLY | FMOD_SOFTWARE | FMOD_MPEGSEARCH, false );

	if ( m_Sound )
	{
		UpdateProperties();
	}
	else
	{
		m_ErrorString = GetAudioManager().GetLastError();
		return false;
	}

	if (m_Duration == 0) // do not support 0-length sounds
	{
		m_ErrorString = "Unity doesn't support 0 length audio clips.";
		release();
		return false;
	}
	
	return true;
}

bool FMODImporter::ForceToMono()
{
	if (m_Channels > 1)
	{
		m_AudioData.clear();
		if ( !DownmixStereoToMono() )
		{
			return false;
		}
		release();

		m_ExInfo.length = m_AudioData.size();
		m_Sound = GetAudioManager().CreateFMODSound ( ( const char* ) &m_AudioData[0], m_ExInfo, FMOD_OPENMEMORY | FMOD_SOFTWARE | FMOD_MPEGSEARCH, false );

		assert ( m_Sound != NULL );

		UpdateProperties();
	}
	return true;
}
void FMODImporter::UpdateProperties()
{
	m_Sound->getFormat (
	    &m_Type, &m_Format, &m_Channels, &m_Bits );

	m_SampleSize = m_Bits / 8;
	m_Sound->getLength ( &m_TotalSize, FMOD_TIMEUNIT_PCMBYTES );

	unsigned lenBytes = 0;
	m_Sound->getLength ( &lenBytes, FMOD_TIMEUNIT_RAWBYTES );
	m_Sound->getLength ( &m_Duration, FMOD_TIMEUNIT_MS );
	m_Bitrate = m_Duration > 0 ? ( lenBytes / m_Duration * 8000 ) : 0;
	float freq;
	m_Sound->getDefaults ( &freq, NULL, NULL, NULL );
	m_Frequency = ( unsigned ) freq;
}

void FMODImporter::release()
{
	if ( m_Sound )
	{
		m_Sound->release();
	}
	m_Sound = NULL;
}

bool FMODImporter::GetNextAudioSamples ( unsigned long *numSamples, void **bufferData, unsigned* sampleSize )
{
	if ( !good() )
		return false;

	void* startPtr, *endPtr;
	unsigned length, endSize;

	if ( *numSamples == 0 ) // grab kAudioEncodeSize bytes
		*numSamples = kAudioEncodeSize / m_SampleSize / m_Channels;


	// read numSamples
	length = *numSamples * m_SampleSize * m_Channels;

	if ( m_Offset + length > m_TotalSize )
		length = m_TotalSize - m_Offset;

	if ( length <= 0 )
	{
		*numSamples = 0;
		return true;
	}

	FMOD_RESULT err = m_Sound->lock ( m_Offset, length, ( void** ) &startPtr, &endPtr, &length, &endSize );

	if ( err != FMOD_OK || length == 0 )
	{
		return false;
	}

	if ( m_ForceToMono && m_Channels > 1 )
	{
		unsigned newLength = *numSamples * ( m_Bits / 8 );

		// @TODO use UNITY_MALLOC - and check ownership with MovieEncode.cpp
		*bufferData = new UInt8[ newLength ];
		
		if (*bufferData == NULL)
		{
			m_ErrorString = "Not enough memory to reformat audio samples";
			m_Sound->unlock ( ( void* ) startPtr, endPtr, length, endSize );
			return false;
		}			
		
		if ( m_Format == FMOD_SOUND_FORMAT_PCM8 )
		{
			downmixer<false, FMOD_SOUND_FORMAT_PCM8> mix;
			mix ( ( UInt8* ) startPtr, ( UInt8* ) startPtr + length, m_SampleSize * 8, m_Channels, 1, ( UInt8* ) *bufferData );
		}
		else if ( m_Format == FMOD_SOUND_FORMAT_PCMFLOAT )
		{
			downmixer<false, FMOD_SOUND_FORMAT_PCMFLOAT> mix;
			mix ( ( UInt8* ) startPtr, ( UInt8* ) startPtr + length, m_SampleSize * 8, m_Channels, 1, ( UInt8* ) *bufferData );
		}
		else
		{
			downmixer<false, FMOD_SOUND_FORMAT_NONE> mix;
			mix ( ( UInt8* ) startPtr, ( UInt8* ) startPtr + length, m_SampleSize * 8, m_Channels, 1, ( UInt8* ) *bufferData );
		}

		*numSamples = length / m_SampleSize / m_Channels;
		*sampleSize = m_Bits / 8;
	}
	else
	{
		// @TODO use UNITY_MALLOC - and check ownership with MovieEncode.cpp
		*bufferData = new UInt8[ length ];
		
		if (*bufferData == NULL)
		{
			m_ErrorString = "Not enough memory to copy audio samples";
			m_Sound->unlock ( ( void* ) startPtr, endPtr, length, endSize );
			return false;
		}	
		
		memcpy ( *bufferData, startPtr, length );
		*numSamples = length / m_SampleSize / m_Channels;
		*sampleSize = m_SampleSize;
	}

	m_Offset += length;

	err = m_Sound->unlock ( ( void* ) startPtr, endPtr, length, endSize );
	if ( err != FMOD_OK )
		return false;

	return true;
}

bool FMODImporter::CanRead ( const std::string& path )
{
	return true;
}

bool decodeFrameSize ( void *data, int *frameSize )
{
	unsigned int head = 0, layer = 0;
	int lsf, bitrate_index, padding, sampling_frequency, mpeg25;
	int gFreqs[9] =
	{
		44100,
		48000,
		32000,
		22050,
		24000,
		16000,
		11025,
		12000,
		8000
	};
	int gTabSel123[2][3][16] =
	{
		{
			{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,},
			{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384,},
			{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,}
		},
		{
			{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256,},
			{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,},
			{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,}
		}
	};

	head = ( unsigned char ) * ( ( unsigned char* ) data + 0 );
	head <<= 8;
	head |= ( unsigned char ) * ( ( unsigned char* ) data + 1 );
	head <<= 8;
	head |= ( unsigned char ) * ( ( unsigned char* ) data + 2 );
	head <<= 8;
	head |= ( unsigned char ) * ( ( unsigned char* ) data + 3 );

	if ( head >> 24 != 0xFF )
	{
		return false;
	}

	if ( ( ( head >> 16 ) & 0xE0 ) != 0xE0 )
	{
		return false;
	}

	if ( head & ( 1 << 20 ) )
	{
		lsf = ( head & ( 1 << 19 ) ) ? 0x0 : 0x1;
		sampling_frequency = ( ( head >> 10 ) & 0x3 ) + ( lsf * 3 );
		mpeg25 = 0;
	}
	else
	{
		lsf = 0;
		sampling_frequency = 6 + ( ( head >> 10 ) & 0x3 );
		mpeg25 = 1;
	}

	layer           = 4 - ( ( head >> 17 ) & 3 );
	bitrate_index   = ( ( head >> 12 ) & 0xf );
	padding         = ( ( head >> 9 ) & 0x1 );

	if ( frameSize )
	{
		switch ( layer )
		{
		case 2:
		{
			*frameSize = ( int ) gTabSel123[lsf][1][bitrate_index] * 144000;
			*frameSize /= gFreqs[sampling_frequency];
			*frameSize += padding; // - 4;  // dont take the header out in this case
			break;
		}
		case 3:
		{
			*frameSize  = ( int ) gTabSel123[lsf][2][bitrate_index] * 144000;
			*frameSize /= gFreqs[sampling_frequency] << ( lsf );
			*frameSize = *frameSize + padding; // - 4;  // dont take the header out in this case
			break;
		}
		default:
		{
			return false;
		}
		};
	}

	return true;
}

double LinInterp ( const double x, const double L0, const double H0 )
{
	return ( L0 + x * ( H0 - L0 ) );
}

// original
inline double HermiteInterp ( double x, double y0, double y1, double y2, double y3 )
{
	// 4-point, 3rd-order Hermite (x-form)
	double c0 = y1;
	double c1 = 0.5f * ( y2 - y0 );
	double c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
	double c3 = 1.5f * ( y1 - y2 ) + 0.5f * ( y3 - y0 );

	return ( ( c3 * x + c2 ) * x + c1 ) * x + c0;
}

inline float ThirdInterp ( const float x, const float L1, const float L0, const
                           float H0, const float H1 )
{
	return
	    L0 +
	    .5f *
	    x * ( H0 - L1 +
	          x * ( H0 + L0 * ( -2 ) + L1 +
	                x * ( ( H0 - L0 ) * 9 + ( L1 - H1 ) * 3 +
	                      x * ( ( L0 - H0 ) * 15 + ( H1 -  L1 ) * 5 +
	                            x * ( ( H0 - L0 ) * 6 + ( L1 - H1 ) * 2 ) ) ) ) );
}

inline double spline5pointInterp ( double x, double p0, double p1, double p2, double p3, double p4, double p5 )
{
	/* 5-point spline*/
	return p2 + 0.04166666666 * x * ( ( p3 - p1 ) * 16.0 + ( p0 - p4 ) * 2.0
	                                  + x * ( ( p3 + p1 ) * 16.0 - p0 - p2 * 30.0 - p4
	                                          + x * ( p3 * 66.0 - p2 * 70.0 - p4 * 33.0 + p1 * 39.0 + p5 * 7.0 - p0 * 9.0
	                                                  + x * ( p2 * 126.0 - p3 * 124.0 + p4 * 61.0 - p1 * 64.0 - p5 * 12.0 + p0 * 13.0
	                                                          + x * ( ( p3 - p2 ) * 50.0 + ( p1 - p4 ) * 25.0 + ( p5 - p0 ) * 5.0 ) ) ) ) );
};

void FMODImporter::Resample ( const SInt16* inBuffer, unsigned inSamples, SInt16* outBuffer, unsigned outSamples, int channels, int& numClippedSamples, double& normalization )
{
	const double clipVal = 32767.0;
	double step = ( double ) inSamples / ( double ) outSamples;
	normalization = 1.0;
	numClippedSamples = 0;
	for(;;)
	{
		int currClippedSamples = 0;
		double d = 0.0, maxPeak = clipVal;
		int n = outSamples;
		SInt16* outPtr = outBuffer;
		while ( n )
		{
			for ( int count = 0; count < channels; count++ )
			{
				int i = ( d );
				double p0 = ( double ) inBuffer[ ( i - 2 ) * channels + count ]  ;
				double p1 = ( double ) inBuffer[ ( i - 1 ) * channels + count ] ;
				double p2 = ( double ) inBuffer[ i * channels + count ];
				double p3 = ( double ) inBuffer[ ( i + 1 ) * channels + count ] ;
				double p4 = ( double ) inBuffer[ ( i + 2 ) * channels + count ] ;
				double p5 = ( double ) inBuffer[ ( i + 3 ) * channels + count ] ;

				double frac = fmod ( d, 1.0 );
				double sample = normalization * spline5pointInterp ( frac, p0, p1, p2, p3, p4, p5 );
				if(sample < -maxPeak)
				{
					maxPeak = -sample;
					currClippedSamples++;
				}
				else if(sample > maxPeak)
				{
					maxPeak = sample;
					currClippedSamples++;
				}
				*outPtr++ = sample;
			}

			n--;
			d += step;
		}
		if(currClippedSamples == 0)
			break;
		// re-run using normalization
		// the +1 makes sure that after the next scaling we will be below the clipping value
		normalization = clipVal / (maxPeak + 1);
		numClippedSamples = max(numClippedSamples, currClippedSamples);
    }
}

bool FMODImporter::Downmix(SInt16* srcData16bit, int srcSizeSamples, int fromChannels, int toChannels)
{
	// Downmix 5.1 or 7.1 to to mono or stereo (FMOD style)
	#define SR(from,to,gain) from, to, gain
	static float weights[] =
	{
		-1, // invalid
		SR(0, 0,  1.000f), -1, // mono
		SR(0, 0,  1.000f), SR(1, 1,  1.000f), -1, // stereo
		SR(0, 0,  1.000f), SR(1, 1,  1.000f), -1, // 2.1, discard LFE
		SR(0, 0,  1.000f), SR(1, 1,  1.000f), SR(2, 0,  0.707f), SR(2, 1, -0.707f), SR(3, 0, -0.707f), SR(3, 1,  0.707f), -1, // quadraphonic
		SR(0, 0,  1.000f), SR(1, 1,  1.000f), SR(2, 0,  0.707f), SR(2, 1, -0.707f), SR(3, 0, -0.707f), SR(3, 1,  0.707f), -1, // quadraphonic, ignore LFE
		SR(0, 0,  1.000f), SR(1, 1,  1.000f), SR(2, 0,  0.707f), SR(2, 1,  0.707f), SR(4, 0, -0.872f), SR(5, 0, -0.490f), SR(4, 1,  0.490f), SR(5, 1,  0.872f), -1, // 5.1
		-1, // 5.2? invalid
		SR(0, 0,  1.000f), SR(1, 1,  1.000f), SR(2, 0,  0.707f), SR(4, 0, -0.872f), SR(5, 0, -0.490f), SR(6, 0,  1.000f), SR(2, 1,  0.707f), SR(4, 1,  0.490f), SR(5, 1,  0.872f), SR(7, 1,  1.000f), -1, // 7.1
		-2
	};
	int i = 0;
	for(int n = 0; n < fromChannels; n++)
	{
		while(i < sizeof(weights) / sizeof(float) && weights[i] != -1)
		{
			i += 3;
		}
		if(++i == -2)
			return false;
	}
	if(weights[i] >= 0)
	{
		float w[2][8];
		memset(w, 0, sizeof(w));
		while(weights[i] != -1)
		{
			int src = (int)weights[i + 0];
			int dst = (toChannels == 1) ? 0 : (int)weights[i + 1];
			float amp = weights[i + 2];
			w[dst][src] += amp;
			i += 3;
		}
		float* downmixed = new float [srcSizeSamples * toChannels];
		float maxamp = 0.0f;
		for(int c = 0; c < toChannels; c++)
		{
			for(int n = 0; n < srcSizeSamples; n++)
			{
				float sum = 0.0f;
				for(int i = 0; i < fromChannels; i++)
					sum += srcData16bit[n * fromChannels + i] * w[c][i];
				downmixed[n * toChannels + c] = sum;
				maxamp = max(maxamp, fabsf(sum));
			}
		}
		float scale = 1.0f;
		float clipval = 32767.0f;
		if(maxamp > clipval)
			scale = clipval / maxamp;
		for(int n = 0; n < srcSizeSamples * toChannels; n++)
			srcData16bit[n] = (SInt16)(downmixed[n] * scale);
		delete[] downmixed;
		return true;
	}
	return false;
}

// XXX: temporarily disable on Linux until we compile liblame
#if !UNITY_LINUX
bool FMODImporter::TranscodeToMp3 ( const std::string& outPath , bool seamlessLoop, ProgressbarCallback* callback)
{
	if ( !good() )
		return false;

	FILE *mp3File = fopen ( outPath.c_str(), "wb" );

	Assert ( mp3File );

	// lock data
	UInt8* srcData = NULL, *end = NULL;
	void* startPtr = NULL, *endPtr = NULL;

	unsigned int srcSizeBytes, endSize;
	if ( m_Sound )
	{
		m_Sound->getLength ( &srcSizeBytes, FMOD_TIMEUNIT_PCMBYTES );

		FMOD_RESULT err = m_Sound->lock ( 0, srcSizeBytes, ( void** ) &srcData, &endPtr, &srcSizeBytes, &endSize );
		if ( err != FMOD_OK || srcSizeBytes == 0 )
		{
			ErrorString ( "Locking FMOD data failed ... can't read pcm data for mp3 conversion" );
			return false;
		}
		end = srcData + srcSizeBytes;
		startPtr = srcData;
	}

	int srcSampleSizeBytes = m_Bits / 8;
	int srcSizeSamples = srcSizeBytes / srcSampleSizeBytes / m_Channels;

	// reformat to 16 bit
	UInt8* srcData16bit = NULL;
	AutoFree autoDeleteSrcData;

	srcSizeBytes = srcSizeSamples * m_Channels * 2;
	if ( m_Bits != 16 )
	{
		srcData16bit = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio,srcSizeBytes );
		
		if (srcData16bit == NULL)
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			m_ErrorString = "Not enough memory for reformating to 16 bit";
			return false;
		}
		
		autoDeleteSrcData.Assign( srcData16bit, kMemAudio );

		if ( m_Bits == 8 )
			Reformat8bitsTo16bits ( srcData, srcSizeSamples * m_Channels, ( SInt16* ) srcData16bit );
		else if ( m_Bits == 24 )
			Reformat24bitsTo16bits ( srcData, srcSizeSamples * m_Channels * 3, ( SInt16* ) srcData16bit );
		else if ( m_Bits == 32 )
			Reformat32bitsTo16bits ( (float*)srcData, srcSizeSamples * m_Channels * 4, ( SInt16* ) srcData16bit );
		else
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			ErrorString ( "Bit rate is no supported for MP3 encoding" );
			return false;
		}

		srcData = NULL;
		m_Bits = 16;
		m_SampleSize = 2;
		srcSampleSizeBytes = m_Bits / 8;
		srcSizeSamples = srcSizeBytes / srcSampleSizeBytes / m_Channels;
	}
	else
		srcData16bit = srcData;

	// MP3 only supports mono or stereo, so any multichannel material needs to be downmixed
	short destChannels = ( m_Channels == 1 || m_ForceToMono ) ? 1 : 2;
	if(destChannels < m_Channels)
	{
		if(!Downmix((SInt16*)srcData16bit, srcSizeSamples, m_Channels, destChannels))
		{
			ErrorString(Format("Could not downmix from %d to %d channels", m_Channels, destChannels));
			if(m_Channels > 2)
				return false;
			destChannels = m_Channels;
		}
		srcSizeBytes /= m_Channels;
		srcSizeBytes *= destChannels;
	}

	int write, owrite;
	int frameSizeSamples = 1152;
	int frameSizeBytes = 1152 * destChannels * srcSampleSizeBytes;

	// For cleaning up resample data
	AutoFree autoDelete;

	// Stretch data to fill all frames
	if ( seamlessLoop && ( srcSizeBytes % frameSizeBytes != 0 ) )
	{
		const int destSizeStretchBytes = ( frameSizeSamples - ( srcSizeSamples % frameSizeSamples ) ) * destChannels * srcSampleSizeBytes;
		const int destSizeBytes = srcSizeBytes + destSizeStretchBytes;
		const int destSizeSamples = destSizeBytes / srcSampleSizeBytes / destChannels;

		// ensure we always up-sample
		Assert ( destSizeBytes > srcSizeBytes );

		// allocate resample buffers
		const int overflowSamples = 8;
		const int overflowBytes = srcSampleSizeBytes * destChannels * overflowSamples;

		UInt8* srcResampledData = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, srcSizeBytes + ( 3 * overflowBytes ));
		UInt8* destResampledData = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, destSizeBytes );
		autoDelete.Assign( destResampledData, kMemAudio ); // clean up when out of scope
		
		if (srcResampledData == NULL || destResampledData == NULL)
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			m_ErrorString = "Not enough memory to resample audio data";
			UNITY_FREE(kMemAudio, srcResampledData);
			// destResampledData is cleaned up automatically
			return false;
		}

		// copy data
		// copy [overflowBytes] last samples from source to the beginning of the resample buffer
		// copy [overflowBytes] trailing samples to the end of the resample buffer
		const UInt8* end = srcData16bit + ( srcSizeBytes - overflowBytes );
		memcpy ( srcResampledData, end, overflowBytes );
		memcpy ( srcResampledData + overflowBytes, srcData16bit, srcSizeBytes );
		memcpy ( srcResampledData + overflowBytes + srcSizeBytes, srcData16bit, overflowBytes );

		// Resample
		int numClippedSamples;
		double normalization;
		Resample ( ( SInt16* ) ( srcResampledData + overflowBytes ), srcSizeSamples, ( SInt16* ) destResampledData, destSizeSamples, destChannels, numClippedSamples, normalization );
		if(numClippedSamples > 0)
		{
			// Only display message if the change in attenuation is so strong that it's audible
			double attenuation = 20.0 * log10(normalization);
			if(attenuation < -1.0)
				WarningStringMsg("Sample %s was attenuated by %.3f dB during gapless MP3 encoding because %d/%d samples would have caused clipping. You can fix this by multiplying the playback volume of the AudioSource by %.3f.", m_AssetPath.c_str(), attenuation, numClippedSamples, destSizeSamples * destChannels, 1.0 / normalization);
		}
		
		// Use the resampled buffer
		srcData16bit = ( UInt8* ) destResampledData;
		srcSizeSamples = destSizeSamples;
		srcSizeBytes = destSizeBytes;

		// Clean up
		UNITY_FREE(kMemAudio, srcResampledData);

		// ensure num of samples is a multiple of 576/1152
		Assert ( srcSizeBytes % frameSizeBytes == 0 );
		Assert ( srcSizeSamples % frameSizeSamples == 0 );
	}

	// setup LAME
	GetValidSampleRate ( ( int& ) m_Frequency );
	lame_t lame = lame_init();
	lame_set_preset ( lame, 0 );
	lame_set_mode ( lame, ( (destChannels == 1) ? MONO : JOINT_STEREO ) );
	lame_set_original ( lame, 0 );
	lame_set_num_channels ( lame, destChannels );
	lame_set_in_samplerate ( lame, m_Frequency );
	lame_set_out_samplerate ( lame, m_Frequency );
	lame_set_bWriteVbrTag ( lame, vbr_off );
	lame_set_disable_reservoir ( lame, 1 );
	lame_set_brate ( lame, m_Bitrate / 1000.0f );

	Assert ( lame_init_params ( lame ) != -1 );

	frameSizeSamples = ( lame_get_version ( lame ) == 0 ) ? 576 : 1152;
	frameSizeBytes = frameSizeSamples * destChannels * srcSampleSizeBytes;

	const int mp3BufferSizeBytes = 1.25 * frameSizeSamples + 7200;
	unsigned char* mp3Buffer = (unsigned char*) UNITY_MALLOC_NULL ( kMemAudio, mp3BufferSizeBytes);
	
	if (mp3Buffer == NULL)
	{
		m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
		m_ErrorString = "Not enough memory for mp3 encoding buffer";
		return false;
	}
	
	const int pcmBufferSizeBytes = frameSizeBytes;
	UInt8* pcmBuffer = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, pcmBufferSizeBytes);
	
	if (pcmBuffer == NULL)
	{
		UNITY_FREE(kMemAudio, mp3Buffer);
		m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
		m_ErrorString = "Not enough memory for pcm encoding buffer";
		return false;
	}

	// Skip first frame unless sound is tiny (encoder output lags behind input, first frame bytes written = 0)
	int numFramesToSkip = 0;
	if ( srcSizeBytes > frameSizeBytes )
	{
		numFramesToSkip = 1;
	}

	int numChunks = srcSizeSamples / frameSizeSamples;

	// Prime encoder with 2(mpgeg1) or 4(mpeg2) last frames
	if ( seamlessLoop && ( srcSizeSamples >= ( 1152 * 2 ) ) )
	{
		int frameCount = ( 2 * 1152 ) / frameSizeSamples;

		for ( int i = frameCount; i > 0 ; --i )
		{
			UInt8* frame = srcData16bit + srcSizeBytes - ( i * frameSizeBytes );

			memset ( pcmBuffer, 0, pcmBufferSizeBytes );
			memset ( mp3Buffer, 0, mp3BufferSizeBytes );

			memcpy ( pcmBuffer, frame, pcmBufferSizeBytes );

			if ( destChannels > 1 )
				write = lame_encode_buffer_interleaved ( lame, ( short int* ) pcmBuffer, frameSizeSamples, mp3Buffer, 0 );
			else
				write = lame_encode_buffer ( lame, ( short int* ) pcmBuffer, NULL, frameSizeSamples, mp3Buffer, 0 );

			Assert ( write <= mp3BufferSizeBytes );
		}
	}

	for ( int chunk = 0; chunk < numChunks; ++chunk )
	{
		memset ( pcmBuffer, 0, pcmBufferSizeBytes );
		memset ( mp3Buffer, 0, mp3BufferSizeBytes );

		memcpy ( pcmBuffer, srcData16bit, pcmBufferSizeBytes );

		if ( destChannels > 1 )
			write = lame_encode_buffer_interleaved ( lame, ( short int* ) pcmBuffer, frameSizeSamples, mp3Buffer, 0 );
		else
			write = lame_encode_buffer ( lame, ( short int* ) pcmBuffer, NULL, frameSizeSamples, mp3Buffer, 0 );

		Assert ( write <= mp3BufferSizeBytes );

		if ( chunk >= numFramesToSkip )
		{
			owrite = fwrite ( mp3Buffer, 1, write, mp3File );
			Assert ( owrite == write );
		}

		if ( callback )
			callback ( ( float ) chunk / numChunks , -1.0F, Format ( "Encoding to MP3 %s", m_AssetPath.c_str() ) );

		srcData16bit += pcmBufferSizeBytes;
	}

	write = lame_encode_flush ( lame, mp3Buffer, 0 );
	decodeFrameSize ( mp3Buffer, &write );

	owrite = fwrite ( mp3Buffer, write, 1, mp3File );

	UNITY_FREE(kMemAudio, mp3Buffer);
	UNITY_FREE(kMemAudio, pcmBuffer);

	lame_close ( lame );
	fclose ( mp3File );

	// unlock data
	FMOD_RESULT err = m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
	if ( err != FMOD_OK )
	{
		ErrorString ( "Unlocking FMOD data failed" );
		return false;
	}

	return true;
}
#endif

bool FMODImporter::TranscodeToOgg ( const std::string& outPath , ProgressbarCallback* progressCallback, bool importVideo )
{
	if ( !good() )
		return false;

	MovieEncodeState s;
	MovieEncodeParams p;

	p.hasAudio = true;
	p.hasVideo = false;
	p.audioBitrate = m_Bitrate;
	p.audioChannels = m_ForceToMono ? 1 : m_Channels;

	p.audioFreq = ( int ) m_Frequency;

	File outFile;
	p.outfile = &outFile;
	p.outfile->Open ( outPath, File::kWritePermission );
	p.importer = this;
	p.assetPath = m_AssetPath.c_str();

	p.totalDuration = ( double ) m_TotalSize / ( double ) m_SampleSize / ( double ) m_Channels / ( double ) m_Frequency;

	p.progressCallback = progressCallback;

	if ( !InitMovieEncoding ( &s, &p ) )
	{
		p.outfile->Close();
		p.outfile = NULL;
		m_ErrorString = s.errorMessage;
		return false;
	}

	try
	{
		MovieEncodeFrames ( &s );
		MovieEncodeClose ( &s );
	}
	catch ( ... )
	{
		p.outfile->Close();
		p.outfile = NULL;
		return false;
	}

	p.outfile->Close();
	p.outfile = NULL;

	return true;
}

#if UNITY_WIN
// hold up to 4 streams (up 7.1)
#define MAX_XMA_STREAMS 4
bool FMODImporter::TranscodeToXMA( const std::string& outPath, bool seamlessLoop, float quality, ProgressbarCallback *callback)
{
	if (!good())
		return false;

	DWORD compression = clamp<DWORD>((quality+0.005f) * 100, 1, 99); // Range [1;99], do not use 100 for XMA

	FILE *xmaFile = fopen ( outPath.c_str(), "wb" );

	Assert ( xmaFile );

	// lock data
	UInt8* srcData = NULL, *end = NULL;
	void* startPtr = NULL, *endPtr = NULL;

	unsigned int srcSizeBytes, endSize;
	if ( m_Sound )
	{
		m_Sound->getLength ( &srcSizeBytes, FMOD_TIMEUNIT_PCMBYTES );

		FMOD_RESULT err = m_Sound->lock ( 0, srcSizeBytes, ( void** ) &srcData, &endPtr, &srcSizeBytes, &endSize );
		if ( err != FMOD_OK || srcSizeBytes == 0 )
		{
			ErrorString ( "Locking FMOD data failed ... can't read pcm data for XMA conversion" );
			return false;
		}
		end = srcData + srcSizeBytes;
		startPtr = srcData;
	}

	int srcSampleSizeBytes = m_Bits / 8;
	int srcSizeSamples = srcSizeBytes / srcSampleSizeBytes / m_Channels;

	// reformat to 16 bit
	UInt8* srcData16bit = NULL;
	AutoFree autoDeleteSrcData;

	srcSizeBytes = srcSizeSamples * m_Channels * 2;
	if ( m_Bits != 16 )
	{
		srcData16bit = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, srcSizeBytes );
		
		if (srcData16bit == NULL)
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			m_ErrorString = "Not enough memory for reformating to 16 bit";
			return false;
		}
		
		autoDeleteSrcData.Assign( srcData16bit, kMemAudio );

		if ( m_Bits == 8 )
			Reformat8bitsTo16bits ( srcData, srcSizeSamples * m_Channels, ( SInt16* ) srcData16bit );
		else if ( m_Bits == 24 )
			Reformat24bitsTo16bits ( srcData, srcSizeSamples * m_Channels * 3, ( SInt16* ) srcData16bit );
		else if ( m_Bits == 32 )
			Reformat32bitsTo16bits ( (float*)srcData, srcSizeSamples * m_Channels * 4, ( SInt16* ) srcData16bit );
		else
		{
			ErrorString ( "Bit rate is no supported for XMA encoding" );
			return false;
		}

		srcData = NULL;
		m_Bits = 16;
		m_SampleSize = 2;
		srcSampleSizeBytes = m_Bits / 8;
		srcSizeSamples = srcSizeBytes / srcSampleSizeBytes / m_Channels;
	}
	else
		srcData16bit = srcData;

	AutoFree autoDeleteStream[MAX_XMA_STREAMS];
	XMAENCODERSTREAM xmaEncoderStreams[MAX_XMA_STREAMS] = {0};

	AutoFree autoDeleteStreched;
	if (seamlessLoop && srcSizeSamples % 128 != 0)
	{
		if (!SeamlessLoopStrechToFillFrame( 128, &srcData16bit, &srcSizeSamples, &srcSampleSizeBytes, &srcSizeBytes))
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			return false;
		}
		autoDeleteStreched.Assign( srcData16bit, kMemAudio ); // srcData16bit is now at a new location so autodelete this as well.
		Assert ( srcSizeBytes % (128 * m_Channels * srcSampleSizeBytes) == 0 );
	}

	int numStreams = (m_Channels + 1) / 2;

	for (int stream = 0; stream < numStreams; ++stream)
	{
		const int channelsLeft = m_Channels - (stream * 2);
		const int channelsInStream = channelsLeft > 1 ? 2 : 1;
		xmaEncoderStreams[stream].pBuffer = srcData16bit;
		xmaEncoderStreams[stream].BufferSize = (srcSizeBytes / m_Channels) * channelsInStream;

		if (seamlessLoop)
		{
			xmaEncoderStreams[stream].LoopStart = 0;
			xmaEncoderStreams[stream].LoopLength = srcSizeSamples;
		}

		// deinterleave channels if channel > 2 
		if (numStreams > 1)
		{
			UInt8* deinterleavedStream = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, xmaEncoderStreams[stream].BufferSize);
			
			if (deinterleavedStream == NULL)
			{
				m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
				m_ErrorString = "Not enough memory for deinterleaving the XMA streams";
				fclose ( xmaFile );
				return false;
			}
			
			autoDeleteStream[stream].Assign( deinterleavedStream, kMemAudio );

			Deinterleave16bits((const UInt16*)srcData16bit, srcSizeSamples, m_Channels, stream * 2, channelsInStream, (UInt16*)deinterleavedStream);

			xmaEncoderStreams[stream].pBuffer = deinterleavedStream;
		}

		WAVEFORMATEX& format = (WAVEFORMATEX&)xmaEncoderStreams[stream].Format;
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = channelsInStream;
		format.nSamplesPerSec = m_Frequency;
		format.wBitsPerSample = 16;
		format.nBlockAlign = (format.wBitsPerSample * format.nChannels) / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;
	}
	
	// encode the streams
	void* pEncodedBuffer;
	DWORD EncodedBufferSize;
	XMA2WAVEFORMATEX* pEncodedBufferFormat;
	DWORD EncodedBufferFormatSize;
	DWORD* pSeekTable;
	DWORD SeekTableSize;

	HRESULT hresult = XAudio2XMAEncoder(numStreams, 
					  xmaEncoderStreams, 
					  compression,
					  seamlessLoop ? XMAENCODER_LOOP : 0,
					  64,
					  &pEncodedBuffer,
					  &EncodedBufferSize,
					  &pEncodedBufferFormat,
					  &EncodedBufferFormatSize,
					  &pSeekTable,
					  &SeekTableSize);

	if (hresult == S_OK)
	{
		size_t wrote = 0;		
		
		// write WAVE RIFF
		WAV_HEADER header;
		header.RIFF = RIFF_RIFF;
		header.type = RIFF_WAVE;
		header.size = sizeof(WAV_HEADER) + sizeof(XMA2WAVEFORMATEX) + sizeof(RIFF_TAG) + SeekTableSize + sizeof(RIFF_TAG) + EncodedBufferSize;
		wrote = fwrite(&header,1, sizeof(WAV_HEADER), xmaFile);
		Assert(wrote == sizeof(WAV_HEADER));

		// write fmt tag
		RIFF_TAG fmtTag;
		CreateRIFFTag(fmtTag, MAKEFOURCC('f','m','t',' '), EncodedBufferFormatSize);
		wrote = fwrite (&fmtTag, 1, sizeof(RIFF_TAG), xmaFile);
		Assert(wrote == sizeof(RIFF_TAG));
		wrote = fwrite (pEncodedBufferFormat, 1, EncodedBufferFormatSize, xmaFile);
		Assert(wrote == EncodedBufferFormatSize);

		// write (XMA) seek tag
		RIFF_TAG seekTag;
		CreateRIFFTag(seekTag, MAKEFOURCC('s','e','e','k'), SeekTableSize);
		wrote = fwrite (&seekTag, 1, sizeof(RIFF_TAG), xmaFile);
		Assert(wrote == sizeof(RIFF_TAG));
		wrote = fwrite (pSeekTable, 1, SeekTableSize, xmaFile);
		Assert(wrote == SeekTableSize);

		// write data tag
		RIFF_TAG data;
		CreateRIFFTag(data, MAKEFOURCC('d','a','t','a'), EncodedBufferSize);
		wrote = fwrite (&data, 1, sizeof(RIFF_TAG), xmaFile);
		Assert(wrote == sizeof(RIFF_TAG));
		wrote = fwrite (pEncodedBuffer, 1, EncodedBufferSize, xmaFile);
		Assert(wrote == EncodedBufferSize);

	}
	else
	{
		_com_error error(hresult);
		LPCTSTR errorText = error.ErrorMessage();
		ErrorString(errorText);
	}

	// cleanup
	fclose ( xmaFile );

	// unlock data
	FMOD_RESULT err = m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
	if ( err != FMOD_OK )
	{
		ErrorString ( "Unlocking FMOD data failed" );
		return false;
	}

	return true;
}

bool FMODImporter::SeamlessLoopStrechToFillFrame(const int frameSizeSamples, UInt8** srcData16bit, int* srcSizeSamples, int* srcSampleSizeBytes, unsigned int* srcSizeBytes)
{
	// Stretch data to fill all frames
	const int destSizeStretchBytes = ( frameSizeSamples - ( *srcSizeSamples % frameSizeSamples ) ) * m_Channels * *srcSampleSizeBytes;
	const int destSizeBytes = *srcSizeBytes + destSizeStretchBytes;
	const int destSizeSamples = destSizeBytes / *srcSampleSizeBytes / m_Channels;

	// ensure we always up-sample
	Assert ( destSizeBytes > *srcSizeBytes );

	// allocate resample buffers
	const int overflowSamples = 8;
	const int overflowBytes = *srcSampleSizeBytes * m_Channels * overflowSamples;

	UInt8* srcResampledData = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, *srcSizeBytes + ( 3 * overflowBytes ));
	UInt8* destResampledData = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, destSizeBytes );
	
	if (srcResampledData == NULL || destResampledData == NULL)
	{
		m_ErrorString = "Not enough memory to resample audio data";
		UNITY_FREE(kMemAudio, srcResampledData);
		UNITY_FREE(kMemAudio, destResampledData);
		return false;
	}

	// copy data
	// copy [overflowBytes] last samples from source to the beginning of the resample buffer
	// copy [overflowBytes] trailing samples to the end of the resample buffer
	const UInt8* end = *srcData16bit + ( *srcSizeBytes - overflowBytes );
	memcpy ( srcResampledData, end, overflowBytes );
	memcpy ( srcResampledData + overflowBytes, *srcData16bit, *srcSizeBytes );
	memcpy ( srcResampledData + overflowBytes + *srcSizeBytes, *srcData16bit, overflowBytes );

	// Resample
	int numClippedSamples;
	double normalization;
	Resample ( ( SInt16* ) ( srcResampledData + overflowBytes ), *srcSizeSamples, ( SInt16* ) destResampledData, destSizeSamples, m_Channels, numClippedSamples, normalization );
	if(numClippedSamples > 0)
	{
		// Only display message if the change in attenuation is so strong that it's audible
		double attenuation = 20.0 * log10(normalization);
		if(attenuation < -1.0)
			WarningStringMsg("Sample %s was attenuated by %.3f dB during gapless encoding because %d/%d samples would have caused clipping. You can fix this by multiplying the playback volume of the AudioSource by %.3f.", m_AssetPath.c_str(), attenuation, numClippedSamples, destSizeSamples * m_Channels, 1.0 / normalization);
	}

	// Use the resampled buffer
	*srcData16bit = ( UInt8* ) destResampledData;
	*srcSizeSamples = destSizeSamples;
	*srcSizeBytes = destSizeBytes;

	// Clean up
	UNITY_FREE(kMemAudio, srcResampledData);

	// ensure num of samples is a multiple of 576/1152
	Assert ( *srcSizeSamples % frameSizeSamples == 0 );

	return true;
}

bool FMODImporter::TranscodeToADPCM (const std::string& outPath , bool looped, ProgressbarCallback* callback)
{
	if (!good())
		return false;
	if (wii::adpcm::IsLibraryInitialized() == false && wii::adpcm::InitializeLibrary() == false) 
		return false;

	// ADPCM supports only one channel
	if (!ForceToMono())
	{
		m_ErrorString = "Unable to force file to mono";
		return false;
	}

	// lock data
	UInt8* srcData = NULL, *end = NULL;
	void* startPtr = NULL, *endPtr = NULL;

	unsigned int srcSizeBytes, endSize;
	if (m_Sound)
	{
		m_Sound->getLength ( &srcSizeBytes, FMOD_TIMEUNIT_PCMBYTES );

		FMOD_RESULT err = m_Sound->lock ( 0, srcSizeBytes, ( void** ) &srcData, &endPtr, &srcSizeBytes, &endSize );
		if ( err != FMOD_OK || srcSizeBytes == 0 )
		{
			ErrorString ( "Locking FMOD data failed ... can't read pcm data for ADPCM conversion" );
			return false;
		}
		end = srcData + srcSizeBytes;
		startPtr = srcData;
	}

	int srcSampleSizeBytes = m_Bits / 8;
	int srcSizeSamples = srcSizeBytes / srcSampleSizeBytes / m_Channels;

	// reformat to 16 bit
	UInt8* srcData16bit = NULL;
	AutoFree autoDeleteSrcData;

	srcSizeBytes = srcSizeSamples * m_Channels * 2;
	bool ok = true;

	if (m_Bits != 16)
	{
		srcData16bit = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, srcSizeBytes);
		
		if (srcData16bit == NULL)
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			m_ErrorString = "Not enough memory for reformating to 16 bit";
			return false;
		}
		
		autoDeleteSrcData.Assign( srcData16bit, kMemAudio );

		if (m_Bits == 8)
			Reformat8bitsTo16bits (srcData, srcSizeSamples * m_Channels, (SInt16*) srcData16bit);
		else if (m_Bits == 24)
			Reformat24bitsTo16bits (srcData, srcSizeSamples * m_Channels * 3, (SInt16*) srcData16bit);
		else if ( m_Bits == 32 )
			Reformat32bitsTo16bits ( (float*)srcData, srcSizeSamples * m_Channels * 4, ( SInt16* ) srcData16bit );
		else
		{
			m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
			ErrorString ( "Bit rate is no supported for ADPCM encoding" );
			ok &= false;
		}

		if (ok)
		{
			srcData = NULL;
			m_Bits = 16;
			m_SampleSize = 2;
			srcSampleSizeBytes = m_Bits / 8;
			srcSizeSamples = srcSizeBytes / srcSampleSizeBytes / m_Channels;
		}
	}
	else
	{
		srcData16bit = srcData;
	}

	FILE *adpcmFile = NULL;
	UInt8* adpcmData = NULL;
	UInt32 adpcmDataSize = 0;
	if (ok)
	{
		// Based on the source from <RVL_SDK>\build\tools\dspadpcm
		adpcmDataSize = wii::adpcm::GetBytesForAdpcmBuffer(srcSizeSamples);
		adpcmData = (UInt8*)UNITY_MALLOC_ALIGNED(kMemAudioProcessing, adpcmDataSize, 32);

		if (adpcmData == NULL)
		{
			ErrorStringMsg ("Failed to allocate memory for adpcm buffer: %d", adpcmDataSize);
			ok &= false;
		}
	}

	if (adpcmData)
	{
		wii::adpcm::ADPCMINFO adpcminfo;
		memset(&adpcminfo, 0, sizeof(wii::adpcm::ADPCMINFO));
		wii::adpcm::DSPADPCMHEADER adpcmHeader;
		memset(&adpcmHeader, 0, sizeof(wii::adpcm::DSPADPCMHEADER));

		adpcmHeader.num_samples       = srcSizeSamples;
		adpcmHeader.num_adpcm_nibbles = wii::adpcm::GetNibblesForNSamples(srcSizeSamples);
		adpcmHeader.sample_rate       = m_Frequency;

		UInt32 loopStart = 0;
		UInt32 loopEnd = srcSizeSamples - 1;

		if (looped)
		{
			UInt32 nibbleLoopStart, nibbleLoopEnd, nibbleCurrent;
	        
			nibbleLoopStart = wii::adpcm::GetNibbleAddress(loopStart);
			nibbleLoopEnd   = wii::adpcm::GetNibbleAddress(loopEnd);
			nibbleCurrent   = wii::adpcm::GetNibbleAddress(0);
	        
			adpcmHeader.loop_flag = VOICE_TYPE_LOOPED;
			adpcmHeader.format    = DEC_MODE_ADPCM;
			adpcmHeader.sa        = nibbleLoopStart;
			adpcmHeader.ea        = nibbleLoopEnd;
			adpcmHeader.ca        = nibbleCurrent;
		}
		else // not looped
		{
			UInt32 nibbleLoopStart, nibbleLoopEnd, nibbleCurrent;
	        
			nibbleLoopStart = wii::adpcm::GetNibbleAddress(0);
			// if the user specified end address use it
			//nibbleLoopEnd = wii::adpcm::GetNibbleAddress(sampleEndAddr);
			nibbleLoopEnd = wii::adpcm::GetNibbleAddress(srcSizeSamples- 1);
			nibbleCurrent = wii::adpcm::GetNibbleAddress(0);
	        
			adpcmHeader.loop_flag = VOICE_TYPE_NOTLOOPED;
			adpcmHeader.format    = DEC_MODE_ADPCM;
			adpcmHeader.sa        = nibbleLoopStart;
			adpcmHeader.ea        = nibbleLoopEnd;
			adpcmHeader.ca        = nibbleCurrent;
		}
	   
	    
		wii::adpcm::Encode((SInt16*)srcData16bit, adpcmData, &adpcminfo, srcSizeSamples);
	    
		// if the user specified loops get loop context
		if (looped)
			wii::adpcm::GetLoopContext(adpcmData, &adpcminfo, loopStart);
		else
			adpcminfo.loop_pred_scale = adpcminfo.loop_yn1 = adpcminfo.loop_yn2 = 0;
	    
		// put the adpcm info on the dsp_header
		for (int i = 0; i < 16; i++) adpcmHeader.coef[i]  = adpcminfo.coef[i];

		adpcmHeader.gain     = adpcminfo.gain;
		adpcmHeader.ps       = adpcminfo.pred_scale;
		adpcmHeader.yn1      = adpcminfo.yn1;
		adpcmHeader.yn2      = adpcminfo.yn2;
		adpcmHeader.lps      = adpcminfo.loop_pred_scale;
		adpcmHeader.lyn1     = adpcminfo.loop_yn1;
		adpcmHeader.lyn2     = adpcminfo.loop_yn2;

		// Convert to BigEndian

		SwapEndianBytes(adpcmHeader.num_samples);
		SwapEndianBytes(adpcmHeader.num_adpcm_nibbles);
		SwapEndianBytes(adpcmHeader.sample_rate);

		SwapEndianBytes(adpcmHeader.loop_flag);
		SwapEndianBytes(adpcmHeader.format);
		SwapEndianBytes(adpcmHeader.sa);     // loop start address
		SwapEndianBytes(adpcmHeader.ea);     // loop end address
		SwapEndianBytes(adpcmHeader.ca);     // current address

		SwapEndianArray(adpcmHeader.coef, sizeof(u16), 16);
		// start context
		SwapEndianBytes(adpcmHeader.gain);   
		SwapEndianBytes(adpcmHeader.ps);
		SwapEndianBytes(adpcmHeader.yn1);
		SwapEndianBytes(adpcmHeader.yn2);

		// loop context
		SwapEndianBytes(adpcmHeader.lps);    
		SwapEndianBytes(adpcmHeader.lyn1);
		SwapEndianBytes(adpcmHeader.lyn2);

		adpcmFile = fopen ( outPath.c_str(), "wb" );
		if (adpcmFile)
		{
			fwrite (&adpcmHeader, sizeof(wii::adpcm::DSPADPCMHEADER), 1, adpcmFile);
			UInt32 adpcmDataSizeToWrite = wii::adpcm::GetBytesForAdpcmSamples(srcSizeSamples);
			AssertMsg (adpcmDataSizeToWrite <= adpcmDataSize, "GetBytesForAdpcmSamples returned %d which is bigger than GetBytesForAdpcmBuffer %d", adpcmDataSizeToWrite, adpcmDataSize);
			fwrite (adpcmData, adpcmDataSizeToWrite, 1, adpcmFile);
		}
		else
		{
			ErrorStringMsg ("Failed to open %s", outPath.c_str());
			ok &= false;
		}
		UNITY_FREE(kMemAudioProcessing, adpcmData);
	}

	// cleanup
	if (adpcmFile) fclose (adpcmFile);

	// unlock data
	FMOD_RESULT err = m_Sound->unlock ( ( void* ) startPtr, endPtr, srcSizeBytes, endSize );
	if (err != FMOD_OK)
	{
		ErrorString ( "Unlocking FMOD data failed" );
		ok &= false;
	}
	return ok;
}

#endif

inline
float GetNormalizedSample ( int i, int ch, unsigned sampleCount, int channels, FMOD_SOUND_FORMAT format, UInt8* data );

const char* GeneratePreview ( FMOD::Sound* sound, unsigned width, dynamic_array<float>& previewData )
{
	if (sound == NULL)
		return "Cannot generate preview from null sound";

	unsigned from = 0;
	unsigned sampleCount;
	sound->getLength ( &sampleCount, FMOD_TIMEUNIT_PCM );
	unsigned sampleStep = ( sampleCount ) / width;

	FMOD_SOUND_TYPE soundType;
	FMOD_SOUND_FORMAT format;
	int channels;
	int bits;
	FMOD_RESULT result = sound->getFormat( &soundType, &format, &channels, &bits);
	
	if (result != FMOD_OK) return "Cannot get format of sound";
	
	// outrule some formats
	if ( ( soundType == FMOD_SOUND_TYPE_MOD ) ||
		( soundType == FMOD_SOUND_TYPE_S3M ) ||
		( soundType == FMOD_SOUND_TYPE_MIDI ) ||
		( soundType == FMOD_SOUND_TYPE_XM ) ||
		( soundType == FMOD_SOUND_TYPE_IT ) ||
		( soundType == FMOD_SOUND_TYPE_SF2 ) )
	{
		return "This audio format does not contain PCM data to preview";
	}
	
	float* temp = (float*) UNITY_MALLOC_NULL ( kMemAudio, width * channels * 2 * sizeof(float) );
	if (temp == NULL)
	{
		return "Not enough memory to generate preview data.";
	}

	previewData.assign_external(temp, temp + (width * channels * 2));
	previewData.set_owns_data (true);
	
	// interleave channels and min&max values:
	//
	// MaxMin:		MmMm..Mm|MmMm..
	// Channel:		0011..Ch|0011..
	float* p = previewData.begin();

	// lock data
	UInt8* data = NULL;
	unsigned int startSize, endSize = 0;
	void* endPtr;
	
	sound->getLength ( &startSize, FMOD_TIMEUNIT_PCMBYTES );;
	bool locked = false;
	FMOD_RESULT err = sound->lock ( 0, startSize, ( void** ) &data, &endPtr, &startSize, &endSize );
	if ( !(err != FMOD_OK || startSize == 0 || data == NULL) )
	{
		// ErrorString ( "Locking FMOD data failed" );
		locked = true;
	} 
	else 
	{
		sound->getLength ( &startSize, FMOD_TIMEUNIT_PCMBYTES );;
		data = (UInt8*) UNITY_MALLOC_NULL ( kMemAudio, startSize );
		sound->seekData(0);
		err = sound->readData(data, startSize, &endSize);
		if (err != FMOD_OK)
			return "Reading FMOD data failed";
	}
	
	for ( int x = 0; x < width; x++ )
	{
		for ( unsigned int channel = 0; channel < channels; ++channel )
		{
			float max;
			float min;
			//getMinMaxSample(clip, from, , channel, min, max);
			min = 1.0f;
			max = -1.0f;
			unsigned _from = from;
			unsigned _to = _from + sampleStep;

			while ( _from < _to )
			{
				float sample = GetNormalizedSample ( _from, channel, sampleCount, channels, format, data );
				if ( sample < min )
					min = sample;
				if ( sample > max )
					max = sample;
				++_from;
			}
			p[ 0 ] = max;
			p[ 1 ] = min;

			p += 2;
		}

		from += sampleStep;
	}

	Assert (p == previewData.end());

	// unlock data
	if (locked) 
	{
		err = sound->unlock ( ( void* ) data, endPtr, startSize, endSize );
		if ( err != FMOD_OK )
		{
			// ErrorString ( "Unlocking FMOD data failed" );
			return "Unlocking FMOD data failed";
		}
	}
	else
	{
		UNITY_FREE ( kMemAudio, data );
	}
	return NULL;
}

bool FMODImporter::GeneratePreview ( unsigned width, dynamic_array<float>& previewData )
{	
	m_ErrorString.clear();
	
	if (const char * str = ::GeneratePreview ( m_Sound, width, previewData ))
	{
		m_ErrorString = str;
		return false;
	}
	return true;
}

inline
float GetNormalizedSample ( int i, int ch, unsigned sampleCount, int channels, FMOD_SOUND_FORMAT format, UInt8* data )
{
	Assert ( i < sampleCount );

	float normSample = 0.0f;

	// floats
	if ( format == FMOD_SOUND_FORMAT_PCMFLOAT )
	{
		const UInt8* p = data + ( i * sizeof ( float ) * channels ) + ( ch * sizeof ( float ) );
		normSample = * ( ( float* ) p );
	}
	else
		// 24-bit
		if ( format == FMOD_SOUND_FORMAT_PCM24 )
		{
			const UInt8* p = data + ( i * 3 * channels ) + ( ch * 3 );
			SInt32 ui = * ( SInt32* ) p;
			ui = ( ui << 8 );
			normSample = ( ( float ) ui / ( 1 << 31 ) ) ;
		}
		else
			// 32-bit
			if ( format == FMOD_SOUND_FORMAT_PCM32 )
			{
				const UInt8* p = data + ( i * sizeof ( float ) * channels ) + ( ch * sizeof ( float ) );
				SInt32 ui = * ( ( SInt32* ) p );
				normSample = ( float ) ui / ( 1 << 31 );
			}
			else
				// 16-bit
				if ( format == FMOD_SOUND_FORMAT_PCM16 )
				{
					const UInt8* p = data + ( i * sizeof ( short ) * channels ) + ( ch * sizeof ( short ) );
					short s = * ( ( short* ) p );

					normSample = ( ( ( float ) s ) / ( 1 << 15 ) );
				}
				else
					// 8-bit
					if ( format == FMOD_SOUND_FORMAT_PCM8 )
					{
						const UInt8* p = data + ( i * channels ) + ch;

						signed char ui = * ( ( signed char* ) p );
						normSample = ( ( float ) ui ) / ( 1 << 7 );
					}

	return normSample;
}

template <typename T, unsigned B>
inline T signextend ( const T x )
{
	struct
	{
T x:
		B;
	} s;
	return s.x = x;
}



/************************************************************************/
/* downmix 2 channels to 1												*/
/* @TODO do this in-place												*/
/* @TODO preserve markers, cue, tags ...								*/
/************************************************************************/
bool FMODImporter::DownmixStereoToMono()
{
	assert ( m_Sound != NULL );
	// get data from fmod
	UInt8* pStart, *pEnd;
	unsigned len1, len2;
	m_Sound->lock ( 0, m_TotalSize, ( void** ) &pStart, ( void** ) &pEnd, &len1, &len2 );

	// create output wav
	UInt8* pOut;
	UInt8* wav = CreateWAV ( m_Frequency, m_TotalSize / m_Channels , 1, m_Bits, &pOut );

	if ( m_Format == FMOD_SOUND_FORMAT_PCM8 )
	{
		downmixer<UNITY_BIG_ENDIAN, FMOD_SOUND_FORMAT_PCM8> mix;
		mix ( pStart, pStart + len1, m_Bits, m_Channels, 1, pOut );
	}
	else if ( m_Format == FMOD_SOUND_FORMAT_PCMFLOAT )
	{
		downmixer<UNITY_BIG_ENDIAN, FMOD_SOUND_FORMAT_PCMFLOAT> mix;
		mix ( pStart, pStart + len1, m_Bits, m_Channels, 1, pOut );
	}
	else
	{
		downmixer<UNITY_BIG_ENDIAN, FMOD_SOUND_FORMAT_NONE> mix;
		mix ( pStart, pStart + len1, m_Bits, m_Channels, 1, pOut );
	}

	m_Sound->unlock ( pStart, pEnd, len1, len2 );

	// wav now contains the down-mixed data
	UInt32 size = GetWAVSize ( wav );

	m_AudioData.clear();
	m_AudioData.assign ( wav, wav + size );
	delete[] wav;

	assert ( m_AudioData.size() == size );

	return true;
}

bool FMODImporter::readRAW ( const std::string &path )
{
	// first load entire file into byte array
	// open the file
	File file;

	if ( !file.Open ( path, File::kReadPermission ) )
	{
		m_ErrorString = "Unable to open file " + path;
		return false; // file problably doesnt exist
	}

	// get the length of the file
	int size = GetFileLength ( path );

	// allocate space
	// Is there enough memory?
	UInt8* ptr = (UInt8*)UNITY_MALLOC_NULL ( kMemAudio, size );
	
	if (!ptr)
	{
		m_ErrorString = "Not enough memory to load audio file into memory";
		file.Close();
		return false;
	}
	
	m_AudioData.assign_external(ptr, ptr + size);
	m_AudioData.set_owns_data (true);

	if ( file.Read ( ( char* ) &m_AudioData[0], size ) != size )
	{
		file.Close();
		m_ErrorString = "Unable to read from file " + path;
		return false;
	}

	Assert ( m_AudioData.size() == size );

	// close the file
	file.Close();

	return true;
}

// MP3 helpers
const int version_table[] =
{
	2, 2, 2, 0, 0, 0, 1, 1, 1
};

const int sample_rate_table[] =
{
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

const int bit_rate_table[3][16] =
{
	{8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
	{32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
	{8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
};


int FMODImporter::GetValidSampleRate ( int& sampleRate )
{
	for ( int i = 0; i < 8; ++i )
		if ( ( sample_rate_table[i] <= sampleRate ) && ( sampleRate <= sample_rate_table[i + 1] ) )
		{
			// pick closes
			int idx = sampleRate - sample_rate_table[i] < sample_rate_table[i + 1] - sampleRate ?
			          i : i + 1;
			sampleRate = sample_rate_table[idx];
			return idx;
		}
	int idx = sampleRate <= sample_rate_table[0] ? 0 : 8;
	sampleRate = sample_rate_table[idx];
	return idx;
}

int FMODImporter::GetValidBitRate ( int& bitRate, int sampleRate )
{
	const int version = version_table[GetValidSampleRate ( sampleRate )];

	for ( int i = 0; i < 13; ++i )
		if ( ( bit_rate_table[version][i] <= bitRate ) && ( bitRate <= bit_rate_table[version][i + 1] ) )
		{
			// pick closes
			int idx = bitRate - bit_rate_table[version][i] < bit_rate_table[version][i + 1] - bitRate ? i : i + 1;
			bitRate = bit_rate_table[version][idx];
			return idx;
		}
	int idx = bitRate <= bit_rate_table[version][0] ? 0 : 13;
	bitRate = bit_rate_table[version][idx];
	return idx;
}

std::pair<int, int> FMODImporter::GetValidBitRateRange ( int sampleRate )
{
	const int version = version_table[GetValidSampleRate ( sampleRate )];

	return std::make_pair<int, int> ( bit_rate_table[version][0] * 1000, bit_rate_table[version][12] * 1000 );
}

// make sure <out> can hold {inBytes * 2} bytes!
void FMODImporter::Reformat8bitsTo16bits ( const UInt8* in, unsigned inBytes, SInt16* out )
{
	for ( int b = 0; b < inBytes; ++b )
	{
		out[b] = ( ( SInt16 ) in[b] << 8 );
	}
}

// make sure <out> can hold {inBytes * 2/3} bytes!
void FMODImporter::Reformat24bitsTo16bits ( const UInt8* in, unsigned inBytes, SInt16* out )
{
	for ( int b = 0, p = 0; p < inBytes; b++, p += 3 )
	{
		out[b]  = ( in[p + 2] << 8 );
		out[b] |= ( in[p + 1] );
	}
}

// make sure <out> can hold {inBytes * 1/2} bytes!
void FMODImporter::Reformat32bitsTo16bits ( const float* in, unsigned inBytes, SInt16* out )
{
	for ( int b = 0; b < inBytes / 4; ++b )
	{
		out[b]  = (SInt16) ( *in * ( 1 << 15 ) );
		in++;
	}
}

void FMODImporter::Deinterleave16bits( const UInt16* in, unsigned inSamples, short srcChannels, UInt16 startChannel, UInt16 dstChannels, UInt16* out )
{
	if (srcChannels == dstChannels)
	{
		memcpy(out, in, inSamples * 2);
		return;
	}
	
	in += (startChannel * 2); 
	for ( int i = 0; i < inSamples; ++i)
	{
		memcpy(out, in, dstChannels * 2);
		out += dstChannels;
		in += srcChannels;
	}
}


