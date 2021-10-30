#ifndef RUNTIME_TERRAIN_WIND_H
#define RUNTIME_TERRAIN_WIND_H

#include "Runtime/GameCode/Behaviour.h"
class Vector4f;
class Vector3f;
class AABB;



// --------------------------------------------------------------------------




class WindZone : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS (WindZone, Behaviour)
	DECLARE_OBJECT_SERIALIZE (WindZone)

	enum WindZoneMode
	{
		Directional, // Wind has a direction along the z-axis of the transform
		Spherical // Wind comes from the transform.position and affects in the direction towards the tree
	};

public:

	
	WindZone (MemLabelId label, ObjectCreationMode mode);
	// ~WindZone(); declared by a macro
	
	// Directional / Spherical. Radius is only used in Spherical mode
	inline WindZoneMode GetMode () const		{ return m_Mode; }
	inline void SetMode (WindZoneMode value)	{ m_Mode = value; SetDirty(); }
	
	inline float GetRadius () const				{ return m_Radius; }
	inline void SetRadius (float value)			{ m_Radius = value; SetDirty();}

	
	// Parameters affecting the wind speed, strength and frequency
	inline float GetWindMain () const			{ return m_WindMain; }
	inline float GetWindTurbulence () const		{ return m_WindTurbulence; }
	inline float GetWindPulseMagnitude () const { return m_WindPulseMagnitude; }
	inline float GetWindPulseFrequency () const { return m_WindPulseFrequency; }

	inline void SetWindMain (float value)			{ m_WindMain = value; SetDirty(); }
	inline void SetWindTurbulence (float value)		{ m_WindTurbulence = value; SetDirty();}
	inline void SetWindPulseMagnitude (float value) { m_WindPulseMagnitude = value; SetDirty(); }
	inline void SetWindPulseFrequency (float value) { m_WindPulseFrequency = value; SetDirty(); }

	Vector4f ComputeWindForce (const AABB& bounds, float time) const;

protected:
	virtual void AddToManager();
	virtual void RemoveFromManager();
	
private:
	// Settings
	WindZoneMode m_Mode; ///< enum { Directional = 0, Spherical = 1 }
	
	float m_Radius;
	
	float m_WindMain;
	float m_WindTurbulence;
	
	float m_WindPulseMagnitude;
	float m_WindPulseFrequency;
	
	// Node registered with the Wind Manager
	ListNode<WindZone> m_Node;
};


// --------------------------------------------------------------------------


class WindManager
{
public:
	~WindManager();

	static WindManager& GetInstance();
	
	void AddWindZone(ListNode<WindZone>& node) { m_WindZones.push_back (node); }
	
	Vector4f ComputeWindForce(const AABB& bounds);

	typedef List< ListNode<WindZone> > WindZoneList;
	WindZoneList& GetList ();

private:
	static WindManager s_WindManager;
	
	WindZoneList m_WindZones;
};


// --------------------------------------------------------------------------


#endif
