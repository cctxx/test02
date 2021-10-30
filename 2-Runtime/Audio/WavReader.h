#ifndef __WAVREADER_H__
#define __WAVREADER_H__

typedef UInt32 FOURCC;

struct RIFF_TAG {
	FOURCC RIFF;
	UInt32 size;
};

struct WAV_HEADER {
	FOURCC RIFF;
	UInt32 size;
	FOURCC type;
};

struct WAV_FORMAT {
	FOURCC ID;
	UInt32 size;
	UInt16 format;
	UInt16 channels;
	UInt32 samplerate;
	UInt32 avgBytesSec;
	UInt16 blockalign;
	UInt16 bitsPerSample;	
};

struct WAV_DATA {
	FOURCC ID;
	UInt32 size;
};

#if !UNITY_BIG_ENDIAN
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((UInt32)(UInt8)(ch0) | ((UInt32)(UInt8)(ch1) << 8) |   \
	((UInt32)(UInt8)(ch2) << 16) | ((UInt32)(UInt8)(ch3) << 24 ))
#endif
#define btoll(x) (x)
#define btols(x) (x)
#else
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
((UInt32)(UInt8)(ch0) << 24 | ((UInt32)(UInt8)(ch1) << 16) |   \
((UInt32)(UInt8)(ch2) << 8) | ((UInt32)(UInt8)(ch3) ))
#endif
#define btoll(x) (((x) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | ((x) << 24))
#define btols(x) (((x) >> 8) | ((x&0xff) << 8))
#endif

#define RIFF_RIFF MAKEFOURCC('R','I','F','F')
#define RIFF_WAVE MAKEFOURCC('W','A','V','E')
#define RIFF_FORMAT MAKEFOURCC('f','m','t',' ')
#define RIFF_DATA MAKEFOURCC('d','a','t','a')

static const UInt8* GetRIFFChunk ( FOURCC fourcc, const UInt8* wavRiff )
{
	UInt8* p = const_cast<UInt8*>(wavRiff);
	UInt32 next = *((UInt32*)p);

	if (next != RIFF_RIFF) // not a RIFF file
		return NULL;
	
	UInt32 totalSize = (UInt32) *((UInt32*)(p + 4));

	// next chunk
	p += 12;
	next = *((UInt32*)p);

	while (next != fourcc)
	{
		UInt32 size = (UInt32) *((UInt32*)(p + 4));

		// next chunk 
		p += 8;
		p += size;
		next = *((UInt32*)p);

		if (p - wavRiff >= totalSize)
			return NULL;
	}

	return p;
}

static bool IsWAV(const UInt8* wavRiff)
{
	return ((*((UInt32*)wavRiff) == RIFF_RIFF) && 
			(((WAV_HEADER*)(wavRiff))->type == RIFF_WAVE));
}

// @note return in little endian
static const WAV_HEADER* GetWAVHeader(const UInt8* wavRiff)
{
	 return (WAV_HEADER*)wavRiff;
}

// @note return in little endian
static const WAV_FORMAT* GetWAVFormat(const UInt8* wavRiff)
{
	return (WAV_FORMAT*)GetRIFFChunk(RIFF_FORMAT, wavRiff);
}

// @note return in little endian
static const WAV_DATA* GetWAVData(const UInt8* wavRiff)
{
	return (WAV_DATA*)GetRIFFChunk(RIFF_DATA, wavRiff); 
}

// endian-safe
static bool IsNormalWAV(const UInt8* wavRiff)
{
	return IsWAV(wavRiff) && (GetWAVFormat(wavRiff)->format == 1);
}

// endian-safe
static UInt32 GetWAVSize(const UInt8* wavRiff)
{
	return btoll(GetWAVHeader(wavRiff)->size);
}

static void CreateRIFFTag(RIFF_TAG& tag, FOURCC fourCC, int size)
{
	tag.RIFF = fourCC;
	tag.size = btoll(size);
}


static void CreateWAVHeader( WAV_HEADER &header, int size, int additionalTagSize = 0 ) 
{
	header.RIFF = MAKEFOURCC('R','I','F','F');
	header.size = btoll ( sizeof (WAV_HEADER) + sizeof (WAV_FORMAT) + sizeof (WAV_DATA) + size + additionalTagSize );
	header.type = MAKEFOURCC('W','A','V','E');
}

static void CreateFMTTag( WAV_FORMAT &format, int channels, int frequency, int bitsPerSample ) 
{
	format.ID = MAKEFOURCC('f','m','t',' ');;
	format.size = btoll(16);
	format.format = btols(1);
	format.channels = btols(channels);
	format.samplerate = btoll(frequency);
	format.avgBytesSec = btoll((frequency * bitsPerSample) / 8);
	format.blockalign = btols(bitsPerSample / 8);
	format.bitsPerSample = btols(bitsPerSample);
}



/**
* reconstructing a wav header + alloc size for data
* @param frequency Frequency
* @param size The size of the data
* @param channels channels
* @param bitsPerSamples Bits pr. sample
* @param ppData ptr to data chunk 
* @return ptr to header

**/
static UInt8* CreateWAV(int frequency, int size, int channels, int bitsPerSample, UInt8** ppData)
{
	WAV_HEADER header;
	WAV_FORMAT format;
	WAV_DATA   data; 

	CreateWAVHeader(header, size);
	CreateFMTTag(format, channels, frequency, bitsPerSample);
	CreateRIFFTag((RIFF_TAG&)data, MAKEFOURCC('d','a','t','a'), size);

	UInt8* wav = new UInt8[ sizeof (WAV_HEADER) + sizeof (WAV_FORMAT) + sizeof (WAV_DATA) + size ];

	memcpy(wav, &header, sizeof(WAV_HEADER));
	memcpy(wav + sizeof(WAV_HEADER), &format, sizeof(WAV_FORMAT));
	memcpy(wav + sizeof(WAV_HEADER) + sizeof(WAV_FORMAT), &data, sizeof(WAV_DATA));	

	*ppData = (UInt8*)(wav + sizeof(WAV_HEADER) + sizeof(WAV_FORMAT)) + 8;

	return wav; 	
}


#endif // __WAVREADER_H__
