#ifndef __AUDIOREVERBZONE_H__
#define __AUDIOREVERBZONE_H__

#if ENABLE_AUDIO_FMOD
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Utilities/LinkedList.h"


class AudioReverbZone : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS   (AudioReverbZone, Behaviour)
	DECLARE_OBJECT_SERIALIZE (AudioReverbZone)
	
	/**
	 * Construction/Destruction
	 **/
	AudioReverbZone (MemLabelId label, ObjectCreationMode mode);
	// virtual ~AudioSource (); declared-by-macro
	
	virtual void CheckConsistency ();
	virtual void Update();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void AddToManager();
	virtual void RemoveFromManager();
	
	float GetMinDistance() const; 
	void SetMinDistance(float minDistance);  

	float GetMaxDistance() const; 
	void SetMaxDistance(float maxDistance); 
	
	void SetReverbPreset(int preset);
	int GetReverbPreset() const;
	
	void SetRoom(int room) { m_Room = room; VerifyValues(); SetFMODValues(); SetDirty(); }
	int GetRoom() const { return m_Room; }
	void SetRoomHF(int roomHF) { m_RoomHF = roomHF; VerifyValues(); SetFMODValues(); SetDirty(); }
	int GetRoomHF() const { return m_RoomHF; }	
	void SetDecayTime(float decayTime) { m_DecayTime = decayTime; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetDecayTime() const { return m_DecayTime; }
	void SetDecayHFRatio(float decayHFRatio) { m_DecayHFRatio = decayHFRatio; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetDecayHFRatio() const { return m_DecayHFRatio; }
	void SetReflectionsDelay(float reflections) { m_ReflectionsDelay = (int)reflections; VerifyValues(); SetFMODValues(); SetDirty();}
	void SetReflectionsDelay(int reflections) { m_ReflectionsDelay = reflections; VerifyValues(); SetFMODValues(); SetDirty();}
	int GetReflectionsDelay() const { return (int)m_ReflectionsDelay; } 
	void SetReflections(int reflections) { m_Reflections = reflections; VerifyValues(); SetFMODValues(); SetDirty();}
	int GetReflections() const { return m_Reflections; } 
	void SetReverb(int reverb) { m_Reverb = reverb; VerifyValues(); SetFMODValues(); SetDirty();}
	int GetReverb() const { return m_Reverb; }
	void SetReverbDelay(float reverbDelay) { m_ReverbDelay = reverbDelay; VerifyValues(); SetFMODValues(); SetDirty();}
	int GetReverbDelay() const { return (int)m_ReverbDelay; }
	void SetHFReference(float hfReference) { m_HFReference = hfReference; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetHFReference() const { return m_HFReference; }
	void SetRoomRolloffFactor(float rolloffFactor) { m_RoomRolloffFactor = rolloffFactor; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetRoomRolloffFactor() const { return m_RoomRolloffFactor; } 
	void SetDiffusion(float diffusion) { m_Diffusion = diffusion; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetDiffusion() const { return m_Diffusion; }
	void SetDensity(float density) { m_Density = density; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetDensity() const { return m_Density; }	
	void SetRoomLF(int roomLF) { m_RoomLF = roomLF; VerifyValues(); SetFMODValues(); SetDirty();}
	int GetRoomLF() const { return m_RoomLF; }
	void SetLFReference(float lfReference) { m_LFReference = lfReference; VerifyValues(); SetFMODValues(); SetDirty();}
	float GetLFReference() const { return m_LFReference; }
	
	void Cleanup();
	void Init();
	
	virtual void Reset();
	
private:
	ListNode<AudioReverbZone> m_Node;
	
	void SetFMODValues ();
	void VerifyValues();
	void ChangeProperties();
	
	float m_MinDistance;
	float m_MaxDistance;
	int m_ReverbPreset; ///< enum { Off = 0, Generic = 1, PaddedCell = 2, Room = 3, Bathroom = 4, Livingroom = 5, Stoneroom = 6, Auditorium = 7, Concerthall = 8, Cave = 9, Arena = 10, Hangar = 11, CarpettedHallway = 12, Hallway = 13, StoneCorridor = 14, Alley = 15, Forest = 16, City = 17, Mountains = 18, Quarry = 19, Plain = 20, Parkinglot = 21, Sewerpipe = 22, Underwater = 23, Drugged = 24, Dizzy = 25, Psychotic = 26, User = 27 }	
	
	int  m_Room; // room effect level (at mid frequencies)
	int  m_RoomHF; // relative room effect level at high frequencies	
	int  m_RoomLF; // relative room effect level at low frequencies	
	
	float  m_DecayTime; // reverberation decay time at mid frequencies
	float  m_DecayHFRatio; //  high-frequency to mid-frequency decay time ratio	
	int  m_Reflections; // early reflections level relative to room effect
	float  m_ReflectionsDelay; //  initial reflection delay time
	int  m_Reverb; //  late reverberation level relative to room effect
	float  m_ReverbDelay; //  late reverberation delay time relative to initial reflection 
	float  m_HFReference; // reference high frequency (hz)
	float  m_LFReference; // reference low frequency (hz)
	float  m_RoomRolloffFactor; //  like rolloffscale in global settings, but for reverb room size effect
	float  m_Diffusion; //  Value that controls the echo density in the late reverberation decay
	float  m_Density; // Value that controls the modal density in the late reverberation decay
	
	
	
	FMOD::Reverb* m_FMODReverb;
	
	FMOD_REVERB_PROPERTIES GetReverbProperty(int preset);
	
	friend class AudioManager;
};	

#endif //ENABLE_AUDIO_FMOD
#endif // __AUDIOREVERBZONE_H__
