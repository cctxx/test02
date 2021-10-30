#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#include "WavFileUtility.h"
#include <stdio.h>

static inline int fput2 (unsigned short v, FILE* f)
{
	return fwrite (&v, 2, 1, f);
}

static inline int fput4 (unsigned v, FILE* f)
{
	return fwrite (&v, 4, 1, f);
}

bool WriteWaveFile (char const* filename, int channels, int samplebits, int freq, void* data, int samples)
{
	FILE* wf = fopen (filename, "wb+");
	if (!wf)
		return false;
		
	size_t datasize = channels * (samplebits / 8) * samples;

#define C(x) if (x < 0) goto err

	// RIFF chunk
	C((fputs ("RIFF", wf)));
	C((fput4 (datasize + (12-8) + 24 + 8, wf)));
	C((fputs ("WAVE", wf)));
	
	// FORMAT chunk
	C((fputs ("fmt ", wf)));
	C((fput4 (0x10, wf)));
	C((fput2 (0x01, wf)));
	C((fput2 (channels, wf)));
	C((fput4 (freq, wf)));
	C((fput4 (channels * samplebits * freq, wf)));
	C((fput2 (samplebits >> 3, wf)));
	C((fput2 (samplebits, wf)));
	
	// DATA chunk
	C((fputs ("data", wf)));
	C((fput4 (datasize, wf)));
	C((fwrite (data, datasize, 1, wf)));
#undef C
	
	fclose (wf);
	return true;
	
err:
	fclose (wf);
	return false;
}
