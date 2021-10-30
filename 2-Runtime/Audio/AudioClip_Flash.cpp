#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if (UNITY_FLASH || UNITY_WEBGL) && ENABLE_AUDIO


#include "AudioClip.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

#if ENABLE_WWW
#include "Runtime/Export/WWW.h"
#endif

AudioClip::AudioClip (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{
	m_Mp3InMemoryLength = NULL;
}

AudioClip::~AudioClip()
{
#if UNITY_WEBGL
	if (m_Mp3InMemoryLength)
		UNITY_FREE (kMemAudio, m_Mp3InMemory);
#endif
}

template<>
void AudioClip::Transfer<StreamedBinaryRead<false> > (StreamedBinaryRead<false>& transfer) {
	Super::Transfer (transfer);
	
	transfer.Transfer(m_SampleCount,"samplecount");
	transfer.Transfer (m_3D, "m_3D");
#if UNITY_WEBGL
	transfer.Align();
#endif
	transfer.Transfer(m_Mp3InMemoryLength,"mp3length");

#if UNITY_WEBGL
	m_Mp3InMemory = (UInt8*)UNITY_MALLOC(kMemAudio,m_Mp3InMemoryLength);
	transfer.TransferTypelessData (m_Mp3InMemoryLength, &m_Mp3InMemory[0]);
#else
	CachedReader& reader = transfer.GetCachedReader();
	UInt8* ptr = reader.GetCacher()->GetAddressOfMemory();
	m_Mp3InMemory = ptr + reader.GetPosition();
	reader.SetAbsoluteMemoryPosition(m_Mp3InMemory + m_Mp3InMemoryLength);
#endif
	transfer.Align();
}

template<class TransferFunc>
void AudioClip::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
}

bool AudioClip::InitStream (WWW* streamData, void* movie, bool realStream)
{
#if UNITY_WEBGL // No WWW yet on WebGL.
	return false;
#else
	if(realStream)
		ErrorString("Streaming MP3 is currently unsupported on Flash.");

	bool succeeded = false;
	if(streamData != NULL && streamData->GetError() == NULL){
		m_StreamData = streamData;
		if(LoadSoundFrom((void*)m_StreamData->GetData(), m_StreamData->GetSize())){
			m_StreamData->SetAudioClip(this);
			m_IsWWWAudioClip = true;
			succeeded=true;
		}
	}

	if(!succeeded){
		m_StreamData = NULL;
		ErrorString("Failed to load Audio from stream");
	}

	return succeeded;
#endif
}

AudioChannel* AudioClip::CreateChannel(AudioSource* forSource)
{
	return new AudioChannel(m_SoundObject,forSource->GetLoop(),m_SampleCount);
}

void AudioClip::LoadSound()
{
	LoadSoundFrom(m_Mp3InMemory,m_Mp3InMemoryLength);
}

float AudioClip::GetLengthSec()
{
	if(m_SoundObject != NULL){
		return (float)Ext_Sound_Get_Length(m_SoundObject);
	}
	return 0.0f;
}

bool AudioClip::LoadSoundFrom(void* ptr, size_t length)
{
	m_SoundObject = Ext_Sound_Load(ptr, length);
	if(m_SoundObject != NULL){
		double length = Ext_Sound_Get_Length(m_SoundObject);
		if(length != 0){
			return true;
		}
	}
	return false;
}

void AudioClip::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	LoadSound();
}

IMPLEMENT_CLASS (AudioClip)
IMPLEMENT_OBJECT_SERIALIZE (AudioClip)

#endif //UNITY_FLASH
