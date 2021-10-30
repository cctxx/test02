#ifndef __UNITY_AUDIOVIDEOIMPORTER_H__
#define __UNITY_AUDIOVIDEOIMPORTER_H__

#if ENABLE_AUDIO

#include <string>
#include "Runtime/Audio/correct_fmod_includer.h"

inline double GetAudioBitrateForQuality(double f) { return (56000+200000*(f)); }
inline double GetVideoBitrateForQuality(double f) { return (100000+8000000*(f)); }
inline double GetAudioQualityForBitrate(double f) { return (f - 56000) / 200000; }
inline double GetVideoQualityForBitrate(double f) { return (f - 100000) / 8000000; }


// abstract base class for different audio(/later video) importers
class IAudioVideoImporter
{
public:

	typedef void ProgressbarCallback (float individual, float overrideTotalProgress, const std::string& text);
	
	/**
	* Read in next audio samples from the current position.
	*
	* numSamples : if 0 , proper number of samples are calculated and ppBufferData is allocated (remember to delete it)
	* ppBufferData: if numSamples <> NULL, ppBufferData is used as buffer (and not allocated - remember to allocate enough)
	*				NOTE: *ppBufferData can be reallocated 
	*
	*			  samples are interleaved like this (2ch stereo)
	* sample:	  0011223344
	* ch		  0101010101 	
	* returns true/false (SUCCCESS/FAIL)
	**/
	virtual bool GetNextAudioSamples(unsigned long *numSamples,void **ppBufferData, unsigned* sampleSize) = 0;
	
	// next video frame
	virtual bool GetNextFrame() = 0;

	virtual bool TranscodeToOgg (const std::string& outPath , ProgressbarCallback* progressbar, bool importVideo) = 0;
	// ... add more formats here

	/**
	* Can the importer read this file (path)?
	**/
	virtual bool CanRead(const std::string& path) = 0;
	
	virtual FMOD_SOUND_FORMAT GetFMODFormat() const = 0;
	virtual FMOD_SOUND_TYPE GetFMODType() const  = 0;
};

#endif // ENABLE_AUDIO
#endif // __UNITY_AUDIOVIDEOIMPORTER_H__
