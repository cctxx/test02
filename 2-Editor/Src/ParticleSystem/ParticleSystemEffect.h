#ifndef PARTICLESYSTEMEFFECT_H
#define PARTICLESYSTEMEFFECT_H

#include "Runtime/BaseClasses/GameObject.h"

class ParticleSystem;

namespace ParticleSystemEffect
{
	// Legacy emitters
	bool GetIsAnyParticleEmitterActive ();

	// Shuriken
	GameObject* GetRootParticleSystem (GameObject* go);
	std::set<GameObject*> GetActiveParticleSystems ();	
	void UpdateActiveParticleSystems();
	void UpdateParticleSystemRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters);
	void ParticleSystemResimulationRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters);
	void ParticleSystemCollectSubEmittersRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters);
	
	void StopAndClearActive(bool stop = true, bool clear = true);
}


class CircularReferenceChecker
{
public:
	CircularReferenceChecker () : m_Level(0) {}
	~CircularReferenceChecker ();

	// If circular referencing is detected a string with the 
	// path (using gameobject names) will be returned, if no circular references are found an 
	// empty string is returned.
	std::string Check (ParticleSystem* checkThisChild, ParticleSystem* parent, ParticleSystem* root );

private:
	struct Node	
	{
	public:
		Node (ParticleSystem* ps) : particleSystem (ps) {};
		ParticleSystem* particleSystem;
		std::set<Node*> parents; 
		std::set<Node*> children;
	};
	void Clear ();
	bool BuildGraphRecursive (ParticleSystem* ps, Node* parent, std::vector<Node*>& graph);
	Node* GetNode (ParticleSystem* ps, std::vector<Node*>& graph);
	bool ValidateNoCircularReference (Node* node, Node* parent);
	bool ValidateNoCircularReferences_Recursive (Node* checkThisNodeCanBeChild, Node* parent);
	void DebugPrintGraph ();
	std::vector<Node*> m_Graph;
	std::string m_CircularPath;
	unsigned m_Level;
};

#endif
