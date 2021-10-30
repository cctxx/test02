#ifndef WIIDSPTOOL_H
#define WIIDSPTOOL_H

#if UNITY_WIN

typedef UInt8 u8;
typedef SInt16 s16;
typedef UInt16 u16;
typedef UInt32 u32;

namespace wii
{
namespace adpcm
{

#define VOICE_TYPE_NOTLOOPED    0x0000     // sample is not looped        
#define VOICE_TYPE_LOOPED       0x0001     // sample is indeed looped

#define DEC_MODE_ADPCM          0x0000     // ADPCM mode
#define DEC_MODE_PCM16          0x000A     // 16-bit PCM mode
#define DEC_MODE_PCM8           0x0009     // 8-bit PCM mode (UNSIGNED)

typedef struct
{
	// start context
	s16 coef[16];
	u16 gain;
	u16 pred_scale;
	s16 yn1;
	s16 yn2;

	// loop context
	u16 loop_pred_scale;
	s16 loop_yn1;
	s16 loop_yn2;

} ADPCMINFO;


typedef struct
{
    u32 num_samples;
    u32 num_adpcm_nibbles;
    u32 sample_rate;

    u16 loop_flag;
    u16 format;
    u32 sa;     // loop start address
    u32 ea;     // loop end address
    u32 ca;     // current address

    u16 coef[16];

    // start context
    u16 gain;   
    u16 ps;
    u16 yn1;
    u16 yn2;

    // loop context
    u16 lps;    
    u16 lyn1;
    u16 lyn2;

    u16 pad[11];

} DSPADPCMHEADER;


u32 GetBytesForAdpcmBuffer(u32) { return 0; }
u32 GetBytesForAdpcmSamples(u32) { return 0; }
u32 GetBytesForPcmBuffer(u32) { return 0; }
u32 GetBytesForPcmSamples(u32) { return 0; }
u32 GetSampleForAdpcmNibble(u32) { return 0; }
u32 GetNibbleAddress(u32) { return 0; }
u32 GetNibblesForNSamples(u32) { return 0; }
u32 GetBytesForAdpcmInfo() { return 0; }
void Encode(s16*, u8*, ADPCMINFO*, u32) {}
void Decode(u8*, s16*, ADPCMINFO*, u32) {}
void GetLoopContext(u8*, ADPCMINFO*, u32) {}

bool InitializeLibrary() { return false; }
void ShutdownLibrary() {}
bool IsLibraryInitialized() { return false; }

}
}


#endif

#endif