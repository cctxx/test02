#ifndef UNITY_WAV_FILE_UTILITY_H_
#define UNITY_WAV_FILE_UTILITY_H_

bool WriteWaveFile (char const* filename, int channels, int samplebits, int freq, void* data, int samples);

#endif // UNITY_WAV_FILE_UTILITY_H_
