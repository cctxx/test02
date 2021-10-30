#ifndef PARTICLEANIMATOR_H
#define PARTICLEANIMATOR_H

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Math/Color.h"
#include "ParticleStruct.h"



class ParticleAnimator : public Unity::Component
{
public:
	REGISTER_DERIVED_CLASS (ParticleAnimator, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (ParticleAnimator)

	ParticleAnimator(MemLabelId label, ObjectCreationMode mode);
	
	static void InitializeClass ();
	static void CleanupClass ();
	
	enum { kColorKeys = 5 };

	GET_SET_DIRTY (Vector3f, WorldRotationAxis, m_WorldRotationAxis)
	GET_SET_DIRTY (Vector3f, LocalRotationAxis, m_LocalRotationAxis)
	GET_SET_DIRTY (Vector3f, RndForce, m_RndForce)
	GET_SET_DIRTY (Vector3f, Force, m_Force)
	GET_SET_DIRTY (float, Damping, m_Damping)
	GET_SET_DIRTY (float, SizeGrow, m_SizeGrow)
	GET_SET_DIRTY (bool, Autodestruct, m_Autodestruct)
	GET_SET_DIRTY (bool, DoesAnimateColor, m_DoesAnimateColor)
	GET_SET_DIRTY (bool, stopSimulation, m_StopSimulation)

	void GetColorAnimation(ColorRGBAf *col) const;
	void SetColorAnimation(ColorRGBAf *col);
	
	void UpdateAnimator( ParticleArray& particles, PrivateParticleInfo& privateInfo, float deltaTime );
	
	bool WillAutoDestructIfNoParticles( const PrivateParticleInfo& privateInfo ) const;

private:
	void UpdateParticles (ParticleArray& particles, PrivateParticleInfo& privateInfo, float deltaTime) const;

private:
	Vector3f		m_WorldRotationAxis;	// axis around which the particle rotates, worldspace
	Vector3f		m_LocalRotationAxis;	// axis around which the particle rotates, localspace
	Vector3f		m_RndForce;// rnd force
	Vector3f		m_Force;			// gravity value
	float           m_Damping;		// damping value
	float           m_SizeGrow;		// Grows the size of the particle by sizeGrow per second. A value of 1.0 doubles the particle size every second.
	ColorRGBA32     m_ColorAnimation[kColorKeys]; // Animates the color through the color keys
	int             m_Autodestruct;
	bool            m_DoesAnimateColor;
	bool            m_StopSimulation;
	float           m_EnergylossFraction;
};

#endif
