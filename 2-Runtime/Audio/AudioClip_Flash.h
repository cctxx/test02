#ifndef AUDIOCLIP_FLASH_H
#define AUDIOCLIP_FLASH_H
#include "Configuration/UnityConfigure.h"

#if (UNITY_FLASH || UNITY_WEBGL) && ENABLE_AUDIO

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Audio/AudioSource.h"
#include "PlatformDependent/FlashSupport/cpp/AudioChannel.h"

#if ENABLE_WWW
class WWW;
#endif

class AudioClip : public NamedObject
{

public:
	REGISTER_DERIVED_CLASS 	 (AudioClip, NamedObject)
	DECLARE_OBJECT_SERIALIZE (AudioClip)

	AudioClip (MemLabelId label, ObjectCreationMode mode);
	// virtual ~AudioClip (); - declared-by-macro 
	
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	float GetLengthSec();

	bool ReadyToPlay() { return true; }
	int GetFrequency() { return 44100; }
	unsigned int GetSampleCount() { return m_SampleCount; }
	int GetChannelCount() { return 2; }
	AudioChannel* CreateChannel(AudioSource* forSource = NULL);
	
	bool Get3D() const { return m_3D; }
	void Set3D(bool threeD) { m_3D = threeD; }	

	void SetData(const float* data, unsigned lengthSamples, unsigned offsetSamples = 0) {}
	void GetData(float* data, unsigned lengthSamples, unsigned offsetSamples = 0) const {}

	void SetReadAllowed (bool allowed) { m_ReadAllowed = allowed; }
	bool GetReadAllowed () const { return m_ReadAllowed; }
	
	bool InitStream (WWW* streamData, void* movie, bool realStream = false);

private:
	void LoadSound();
	bool LoadSoundFrom(void* ptr, size_t length);
	
	bool			m_IsWWWAudioClip;
	bool			m_ReadAllowed;
	bool			m_3D;
	UInt8*			m_Mp3InMemory;
	int				m_Mp3InMemoryLength;
	unsigned int	m_SampleCount;
#if UNITY_FLASH
	AS3Handle		m_SoundObject;
#elif UNITY_WEBGL
	int				m_SoundObject;
#endif
	WWW*			m_StreamData;
	


	friend class AudioManager;

};

#endif //UNITY_FLASH
#endif
