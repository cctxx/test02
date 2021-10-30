#ifndef __AUDIOREVERBFILTER_H__
#define __AUDIOREVERBFILTER_H__

#include "Configuration/UnityConfigure.h"
#if ENABLE_AUDIO_FMOD
#include "AudioSourceFilter.h"
#include "Runtime/Utilities/Utility.h"

/////@TODO: Why does this not have the same parameters as Reverb zone???

class AudioReverbFilter : public AudioFilter
{
public:
	REGISTER_DERIVED_CLASS   (AudioReverbFilter, AudioFilter)
	DECLARE_OBJECT_SERIALIZE (AudioReverbFilter)
	
	AudioReverbFilter (MemLabelId label, ObjectCreationMode mode);

	virtual void CheckConsistency ();
	virtual void Update();
	virtual void AddToManager();
	virtual void Reset();	

	float GetDryLevel() const { return m_DryLevel; }
	void SetDryLevel(const float drylevel) { m_DryLevel = drylevel; Update(); SetDirty();}

	float GetRoom() const { return m_Room; }
	void SetRoom(const float room) { m_Room = room; Update(); SetDirty();}
	
	float GetRoomHF() const { return m_RoomHF; }
	void SetRoomHF(const float roomhf) { m_RoomHF = roomhf; Update(); SetDirty();}
	
	float GetRoomLF() const { return m_RoomLF; }
	void SetRoomLF(const float roomlf) { m_RoomLF = roomlf; Update(); SetDirty();}
	
	float GetRoomRolloff() const { return m_RoomRolloff; }
	void SetRoomRolloff(const float rolloffscale) { m_RoomRolloff = rolloffscale; Update(); SetDirty();}
	
	float GetDecayTime() const { return m_DecayTime; }
	void SetDecayTime(const float decayTime) { m_DecayTime = decayTime; Update(); SetDirty();}
	
	float GetDecayHFRatio() const { return m_DecayHFRatio; }
	void SetDecayHFRatio(const float decayHFRatio) { m_DecayHFRatio = decayHFRatio; Update(); SetDirty(); }
	
	float GetReflectionsLevel() const { return m_ReflectionsLevel; }
	void SetReflectionsLevel(const float reflectionsLevel) { m_ReflectionsLevel = reflectionsLevel; Update(); SetDirty(); }
	
	float GetReflectionsDelay() const { return m_ReflectionsDelay; }
	void SetReflectionsDelay(const float reflectionsDelay) { m_ReflectionsDelay = reflectionsDelay; Update(); SetDirty();}
	
	float GetReverbLevel() const { return m_ReverbLevel; }
	void SetReverbLevel(const float reverbLevel) { m_ReverbLevel = reverbLevel; Update(); SetDirty();}
	
	float GetReverbDelay() const { return m_ReverbDelay; }
	void SetReverbDelay(const float reverbDelay) { m_ReverbDelay = reverbDelay; Update(); SetDirty();}
	
	float GetDiffusion() const { return m_Diffusion; }
	void SetDiffusion(const float diffusion) { m_Diffusion = diffusion; Update(); SetDirty();}
	
	float GetDensity() const { return m_Density; }
	void SetDensity(const float density) { m_Density = density; Update(); SetDirty();}
	
	float GetHFReference() const { return m_HFReference; }
	void SetHFReference(const float hfReference) { m_HFReference = hfReference; Update(); SetDirty();}
	
	float GetLFReference() const { return m_LFReference; }
	void SetLFReference(const float lfReference) { m_LFReference = lfReference; Update(); SetDirty();}	
	
	int GetReverbPreset() const { return m_ReverbPreset; }
	void SetReverbPreset(const int reverbPreset);	
				  
private:		
	float m_DryLevel; // Dry Level : Mix level of dry signal in output in mB. Ranges from -10000.0 to 0.0. Default is 0. 
	float m_Room;    // Room : Room effect level at low frequencies in mB. Ranges from -10000.0 to 0.0. Default is 0.0. 
	float m_RoomHF;	 // Room HF : Room effect high-frequency level re. low frequency level in mB. Ranges from -10000.0 to 0.0. Default is 0.0. 
	float m_RoomRolloff; // Room Rolloff : Like DS3D flRolloffFactor but for room effect. Ranges from 0.0 to 10.0. Default is 10.0 
	float m_DecayTime; // Reverberation decay time at low-frequencies in seconds. Ranges from 0.1 to 20.0. Default is 1.0. 
	float m_DecayHFRatio; // Decay HF Ratio : High-frequency to low-frequency decay time ratio. Ranges from 0.1 to 2.0. Default is 0.5. 
	float m_ReflectionsLevel; // Early reflections level relative to room effect in mB. Ranges from -10000.0 to 1000.0. Default is -10000.0. 
	float m_ReflectionsDelay; // Late reverberation level relative to room effect in mB. Ranges from -10000.0 to 2000.0. Default is 0.0. 
	float m_ReverbLevel; //  Reverb : Late reverberation level relative to room effect in mB. Ranges from -10000.0 to 2000.0. Default is 0.0. 
	float m_ReverbDelay; // Late reverberation delay time relative to first reflection in seconds. Ranges from 0.0 to 0.1. Default is 0.04. 
	float m_Diffusion; //  Reverberation diffusion (echo density) in percent. Ranges from 0.0 to 100.0. Default is 100.0. 
	float m_Density; // Reverberation density (modal density) in percent. Ranges from 0.0 to 100.0. Default is 100.0. 
	float m_HFReference; // HF Reference : Reference high frequency in Hz. Ranges from 20.0 to 20000.0. Default is 5000.0. 
	float m_RoomLF; // Room effect low-frequency level in mB. Ranges from -10000.0 to 0.0. Default is 0.0. 
	float m_LFReference; // Reference low-frequency in Hz. Ranges from 20.0 to 1000.0. Default is 250.0. 
	int m_ReverbPreset; ///< enum { Off = 0, Generic = 1, PaddedCell = 2, Room = 3, Bathroom = 4, Livingroom = 5, Stoneroom = 6, Auditorium = 7, Concerthall = 8, Cave = 9, Arena = 10, Hangar = 11, CarpettedHallway = 12, Hallway = 13, StoneCorridor = 14, Alley = 15, Forest = 16, City = 17, Mountains = 18, Quarry = 19, Plain = 20, Parkinglot = 21, Sewerpipe = 22, Underwater = 23, Drugged = 24, Dizzy = 25, Psychotic = 26, User = 27 }	

	void ChangeProperties();
};

#endif //ENABLE_AUDIO
#endif // __AUDIOREVERBFILTER_H__


