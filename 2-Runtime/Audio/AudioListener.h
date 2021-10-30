#ifndef __AUDIOLISTENER_H__
#define __AUDIOLISTENER_H__

#if ENABLE_AUDIO

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Vector3.h"
#include "AudioBehaviour.h"

class Transform;

class AudioListener : public AudioBehaviour
{
public:
	
	REGISTER_DERIVED_CLASS   (AudioListener, AudioBehaviour)
	DECLARE_OBJECT_SERIALIZE (AudioListener)
	
	AudioListener (MemLabelId label, ObjectCreationMode mode);
	// virtual ~AudioListener (); declared-by-macro
	
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	int GetVelocityUpdateMode() const         { return m_VelocityUpdateMode; }
	void SetVelocityUpdateMode(int update) { m_VelocityUpdateMode=update; }		

	// Behaviour
	virtual void Update();
	virtual void FixedUpdate();
	
	ListNode<AudioListener>& GetNode() { return m_Node; }	

	const Vector3f& GetPosition() const { return m_LastPosition; }
	
	void OnAddComponent();
	
	static void InitializeClass ();	
	static void CleanupClass();	
	
	void Cleanup();	
	
	void SetAlternativeTransform(Transform* t);
	
private:
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	void DoUpdate ();
	void ApplyFilters();

private:
	Vector3f	m_LastPosition;
	int			m_VelocityUpdateMode;

	PPtr<Transform>	m_AltTransform;
	const Transform& GetCurrentTransform() const;
	
	ListNode<AudioListener> m_Node;
	
	friend class AudioManager;
};

#endif //ENABLE_AUDIO
#endif // __AUDIOLISTENER_H__
