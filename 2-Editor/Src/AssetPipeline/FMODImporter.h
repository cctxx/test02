/*
 *  FMODImporter.h
 *  Xcode
 *
 *  Created by Søren Christiansen on 5/5/09.
 *  Copyright 2009 Unity Technologies. All rights reserved.
 *
 */
#ifndef __FMOD_IMPORTER_H__
#define __FMOD_IMPORTER_H__

#include "AudioVideoImporter.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Utilities/dynamic_array.h"

class FMODImporter : IAudioVideoImporter
{
public:	
	FMODImporter ( const std::string& path, bool forceToMono );
	FMODImporter();
	~FMODImporter();

	bool open ( const std::string& path );
	bool open_only ( const std::string& path );

	bool good() const {	return ( m_Sound != NULL );	}
	void release();
	std::string GetErrorString() const	{ return m_ErrorString;	}

	void SetOggBitrate ( unsigned bitrate ) { m_Bitrate = bitrate; }
	void SetForceToMono ( bool force ) { m_ForceToMono = force;	}

	// IAudioVideoImporter implementation
	virtual bool TranscodeToOgg ( const std::string& outPath , ProgressbarCallback*, bool importVideo );
	virtual bool GetNextAudioSamples ( unsigned long *numSamples, void **bufferData, unsigned* sampleSize );
	virtual bool GetNextFrame() { throw "NOT IMPLEMENTED"; 	}
	virtual bool CanRead ( const std::string &path );
	virtual FMOD_SOUND_FORMAT GetFMODFormat() const	{ return m_Format; }
	virtual FMOD_SOUND_TYPE   GetFMODType() const { return m_Type; }

	bool TranscodeToMp3 ( const std::string& outPath , bool seamlessLoop, ProgressbarCallback*);
#if UNITY_WIN
	bool TranscodeToXMA ( const std::string& outPath, bool seamlessLoop, float quality, ProgressbarCallback*);
	bool TranscodeToADPCM (const std::string& outPath , bool looped, ProgressbarCallback*);
#endif

	bool GeneratePreview ( unsigned width, dynamic_array<float>& previewData );


private:
	FMOD::Sound* m_Sound;
	FMOD_CREATESOUNDEXINFO m_ExInfo;
	unsigned m_SampleSize;
	unsigned m_Offset;
	unsigned m_TotalSize;
	unsigned m_Bitrate;
	unsigned m_Frequency;
	unsigned m_ForceToMono;
	unsigned m_Duration;
	std::string m_AssetPath;
	std::string m_ErrorString;
	FMOD_SOUND_TYPE m_Type;
	FMOD_SOUND_FORMAT m_Format;
	int m_Channels;
	int m_Bits;
	dynamic_array<UInt8> m_AudioData;

	bool DownmixStereoToMono();

	template <bool big_endian, FMOD_SOUND_FORMAT format>
	struct downmixer
	{
		void operator() ( UInt8* p, UInt8* pEnd, const unsigned inSampleBitSize, const short inChannels, const short outChannels, UInt8* pOut );
	};
	//void downmix_helper( UInt8* p, UInt8* pEnd, const unsigned inSampleBitSize, const short inChannels, const short outChannels, UInt8* pOut );
	bool readRAW ( const std::string& path );
	bool ForceToMono();
	void UpdateProperties();
	bool SeamlessLoopStrechToFillFrame(const int frameSizeSamples, UInt8** srcData16bit, int* srcSizeSamples, int* srcSampleSizeBytes, unsigned int* srcSizeBytes);

	// helpers
	static int GetValidSampleRate ( int& sampleRate );
	static int GetValidBitRate ( int& bitRate, int sampleRate );
	static std::pair<int, int> GetValidBitRateRange ( int sampleRate );
	static void Reformat8bitsTo16bits ( const UInt8* in, unsigned inBytes, SInt16* out );
	static void Reformat24bitsTo16bits ( const UInt8* in, unsigned inBytes, SInt16* out );
	static void Reformat32bitsTo16bits ( const float* in, unsigned inBytes, SInt16* out );
	static void Resample ( const SInt16* inBuffer, unsigned inSamples, SInt16* outBuffer, unsigned outSamples, int channels, int& numClippedSamples, double& normalization );
	static bool Downmix(SInt16* srcData16bit, int srcSizeSamples, int fromChannels, int toChannels);
	static void Deinterleave16bits( const UInt16* in, unsigned inSamples, short srcChannels, UInt16 startChannel, UInt16 dstChannels, UInt16* out);

	friend class AudioImporter;
};

// Returns NULL on success and an error string on error.
const char* GeneratePreview ( FMOD::Sound* sound, unsigned width, dynamic_array<float>& previewData );


/**
 * Big Endian
 **/
#if UNITY_BIG_ENDIAN
#define btoll(x) (((x) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | ((x) << 24))

template<>
struct FMODImporter::downmixer<true, FMOD_SOUND_FORMAT_NONE>
{
	void operator() ( UInt8* pStart, UInt8* pEnd, const unsigned inSampleBitSize, const short inChannels, const short outChannels, UInt8* pOut )
	{
		Assert ( ( outChannels == 1 ) && ( inSampleBitSize > 0 ) ); // only support X -> 1 now

		const unsigned inSampleSize = ( inSampleBitSize < 8 ? 1 : inSampleBitSize / 8 );

		while ( pStart != pEnd )
		{
			SInt32 L = 0, R = 0, A = 0;
			// only mix left and right channels (even for >2 ch input)
			memcpy ( &L, pStart , inSampleSize );
			memcpy ( &R, pStart + inSampleSize, inSampleSize );

			L = ( L >> ( 32 - inSampleBitSize ) );
			R = ( R >> ( 32 - inSampleBitSize ) );

			A = ( L >> 1 ) + ( R >> 1 );

			A = btoll ( *reinterpret_cast<UInt32*> ( &A ) );
			memcpy ( pOut, &A, inSampleSize );

			pStart += ( inSampleBitSize * inChannels ) / 8;
			pOut += ( inSampleBitSize * outChannels ) / 8;
		}
	}
};
#endif

/**
 * Little endian
 **/
template<>
struct FMODImporter::downmixer<false, FMOD_SOUND_FORMAT_NONE>
{
	void operator() ( UInt8* pStart, UInt8* pEnd, const unsigned inSampleBitSize, const short inChannels, const short outChannels, UInt8* pOut  )
	{
		Assert ( ( outChannels == 1 ) && ( inSampleBitSize > 0 ) ); // only support X -> 1 now

		const unsigned inSampleSize = ( inSampleBitSize < 8 ? 1 : inSampleBitSize / 8 );

		while ( pStart != pEnd )
		{
			SInt32 L = 0, R = 0;
			// only mix left and right channels (even for >2 ch input)
			memcpy ( &L, pStart , inSampleSize );
			memcpy ( &R, pStart + inSampleSize, inSampleSize );


			bool signL = ( L << ( 32 - inSampleBitSize ) ) & 0x80000000;
			bool signR = ( R << ( 32 - inSampleBitSize ) ) & 0x80000000;

			if ( signL )
				L = L | ( 0xff << inSampleBitSize );
			if ( signR )
				R = R | ( 0xff << inSampleBitSize );

			SInt32 A;
			A = ( L >> 1 ) + ( R >> 1 );

			memcpy ( pOut, &A, inSampleSize );

			pStart += ( inSampleBitSize * inChannels ) / 8;
			pOut += ( inSampleBitSize * outChannels ) / 8;
		}
	}
};


/**
 * 8 bit
 **/
template<bool big_endian>
struct FMODImporter::downmixer<big_endian, FMOD_SOUND_FORMAT_PCM8>
{
	void operator() ( UInt8* pStart, UInt8* pEnd, const unsigned inSampleBitSize, const short inChannels, const short outChannels, UInt8* pOut  )
	{
		Assert ( ( outChannels == 1 ) && ( inSampleBitSize > 0 ) ); // only support X -> 1 now

		const unsigned inSampleSize = ( inSampleBitSize < 8 ? 1 : inSampleBitSize / 8 );

		while ( pStart != pEnd )
		{
			SInt8 L = 0, R = 0;
			// only mix left and right channels (even for >2 ch input)
			memcpy ( &L, pStart , 1 );
			memcpy ( &R, pStart + 1, 1 );

			SInt8 A;
			A = ( L >> 1 ) + ( R >> 1 );

#if !UNITY_WIN
			if ( inSampleSize == 1 )
				A = A - 128;
#endif
			memcpy ( pOut, &A, inSampleSize );

			pStart += ( inSampleBitSize * inChannels ) / 8;
			pOut += ( inSampleBitSize * outChannels ) / 8;
		}
	}
};

/**
 * 32 bit float
 **/
template<bool big_endian>
struct FMODImporter::downmixer<big_endian, FMOD_SOUND_FORMAT_PCMFLOAT>
{
	void operator() ( UInt8* pStart, UInt8* pEnd, const unsigned inSampleBitSize, const short inChannels, const short outChannels, UInt8* pOut  )
	{
		Assert ( ( outChannels == 1 ) && ( inSampleBitSize > 0 ) ); // only support X -> 1 now

		const unsigned inSampleSize = ( inSampleBitSize < 8 ? 1 : inSampleBitSize / 8 );

		while ( pStart != pEnd )
		{
			float L = 0, R = 0;
			// only mix left and right channels (even for >2 ch input)
			memcpy ( &L, pStart , inSampleSize );
			memcpy ( &R, pStart + inSampleSize, inSampleSize );

			float A;
			A = ( L / 2.0f ) + ( R / 2.0f );

			// convert to 32 bit int
			SInt32 I = A * ( 1 << 31 );

			memcpy ( pOut, &I, inSampleSize );

			pStart += ( inSampleBitSize * inChannels ) / 8;
			pOut += ( inSampleBitSize * outChannels ) / 8;
		}
	}
};

#endif // __FMOD_IMPORTER_H__
