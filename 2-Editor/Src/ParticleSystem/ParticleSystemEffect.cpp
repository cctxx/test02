#include "UnityPrefix.h"
#include "Editor/Src/ParticleSystem/ParticleSystemEffect.h"
#include "Editor/Src/ParticleSystem/ParticleSystemEditor.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/Utility/ActiveEditorTracker.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"
#include "Runtime/Filters/Particles/ParticleEmitter.h"
#include "Runtime/Input/TimeManager.h"

// Legacy:
void UpdateParticleEmitterRecurse (GameObject& go)
{
	if( go.IsActive() )
	{
		ParticleEmitter* emit = go.QueryComponent(ParticleEmitter);
		if( emit )
			emit->UpdateParticleSystem(GetDeltaTime());
	}
	Transform* t = go.QueryComponent (Transform);
	if (t == NULL)
		return;
	for (Transform::iterator i=t->begin ();i != t->end ();i++)
	{
		GameObject& cur = (**i).GetGameObject ();
		UpdateParticleEmitterRecurse (cur);
	}	
}

void UpdateActiveParticleEmitters ()
{
	// Selected particle systems are regarded as active
	TempSelectionSet selection;
	GetSceneTracker().GetSelection(selection);
	for (TempSelectionSet::iterator i=selection.begin ();i != selection.end ();i++) 
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if(go)
			UpdateParticleEmitterRecurse(*go);
	}

	// Locked particle systems are regarded as active
	std::vector<Object*> lockedObjects = ActiveEditorTracker::GetLockedObjects();
	for (unsigned i=0; i<lockedObjects.size(); ++i)
	{
		Object* obj = lockedObjects[i];
		GameObject* go = dynamic_pptr_cast<GameObject*> (obj);
		if(go)
			UpdateParticleEmitterRecurse(*go);
	}
}

bool GetIsParticleEmitterActiveRecursive (GameObject& go)
{
	bool active = false;
	if( go.IsActive() )
	{
		ParticleEmitter* emit = go.QueryComponent(ParticleEmitter);
		if( emit )
			return true;
	}
	Transform* t = go.QueryComponent (Transform);
	if (t == NULL)
		return false;
	for (Transform::iterator i=t->begin ();i != t->end ();i++)
	{
		GameObject& cur = (**i).GetGameObject ();
		active = active || GetIsParticleEmitterActiveRecursive (cur);
	}
	return active;
}

bool ParticleSystemEffect::GetIsAnyParticleEmitterActive ()
{
	// Selected particle systems are regarded as active
	TempSelectionSet selection;
	GetSceneTracker().GetSelection(selection);
	for (TempSelectionSet::iterator i=selection.begin ();i != selection.end ();i++) 
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if(go && GetIsParticleEmitterActiveRecursive(*go))
			return true;
	}

	// Locked particle systems are regarded as active
	std::vector<Object*> lockedObjects = ActiveEditorTracker::GetLockedObjects();
	for (unsigned i=0; i<lockedObjects.size(); ++i)
	{
		Object* obj = lockedObjects[i];
		GameObject* go = dynamic_pptr_cast<GameObject*> (obj);
		if(go && GetIsParticleEmitterActiveRecursive(*go))
			return true;
	}

	return false;
}


// Shuriken:
inline bool HasParticleSystem (GameObject& go)
{
	return go.CountDerivedComponents(CLASS_ParticleSystem) != 0;
}

GameObject* ParticleSystemEffect::GetRootParticleSystem (GameObject* go)
{
	if (go == NULL)
		return NULL;
	
	if (!HasParticleSystem (*go))
		return NULL;
	
	// Find root
	Transform *rootTransform = go->QueryComponent (Transform);
	while (Transform* t = rootTransform->GetParent())
	{
		if (HasParticleSystem(t->GetGameObject()))
			rootTransform = t;
		else
			break;
	}
	
	GameObject& root = rootTransform->GetGameObject ();
	if (!root.IsActive ())
		return NULL;
	
	return &root;
}

std::set<GameObject*> ParticleSystemEffect::GetActiveParticleSystems ()
{
	std::set<GameObject*> result;
	
	// Locked particle system (locked by the Particle Effect Window)
	if (ParticleSystem* lockedParticleSystem = ParticleSystemEditor::GetLockedParticleSystem ())
	{
		GameObject* go = lockedParticleSystem->GetGameObjectPtr ();
		if (go = GetRootParticleSystem(go))
			result.insert (go);
	}


	// Selected particle systems are regarded as active
	TempSelectionSet selection;
	GetSceneTracker().GetSelection(selection);
	for (TempSelectionSet::iterator i=selection.begin ();i != selection.end ();i++) 
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if (go = GetRootParticleSystem(go))
			result.insert (go);
	}
	
	// Locked particle systems are regarded as active
	std::vector<Object*> lockedObjects = ActiveEditorTracker::GetLockedObjects();
	for (unsigned i=0; i<lockedObjects.size(); ++i)
	{
		Object* obj = lockedObjects[i];
		GameObject* go = dynamic_pptr_cast<GameObject*> (obj);
		if (go = GetRootParticleSystem(go))
			result.insert (go);				
	}
	
	return result;	
}


void ParticleSystemEffect::UpdateParticleSystemRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters)
{
	if( go.IsActive() )
	{
		ParticleSystem* shuriken = go.QueryComponent(ParticleSystem);
		if (shuriken)
		{
			const bool playing = ParticleSystemEditor::GetPlaybackIsPlaying() && !ParticleSystemEditor::GetIsScrubbing();
			float dt = playing ? GetDeltaTime() * ParticleSystemEditor::GetSimulationSpeed() : 0.0f;
			if(std::find(shurikenSubEmitters.begin(), shurikenSubEmitters.end(), shuriken) == shurikenSubEmitters.end())
				ParticleSystemEditor::UpdatePreview (shuriken, dt);
		}
	}
	Transform* t = go.QueryComponent (Transform);
	if (t == NULL || !go.QueryComponent(ParticleSystem))
		return;
	for (Transform::iterator i=t->begin ();i != t->end ();i++)
	{
		GameObject& cur = (**i).GetGameObject ();
		UpdateParticleSystemRecurse (cur, shurikenSubEmitters);
	}	
}

void ParticleSystemEffect::ParticleSystemResimulationRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters)
{
	if( go.IsActive() )
	{
		ParticleSystem* shuriken = go.QueryComponent(ParticleSystem);
		if (shuriken)
			if(std::find(shurikenSubEmitters.begin(), shurikenSubEmitters.end(), shuriken) == shurikenSubEmitters.end())
				ParticleSystemEditor::PerformCompleteResimulation (shuriken);
	}
	Transform* t = go.QueryComponent (Transform);
	if (t == NULL || !go.QueryComponent(ParticleSystem))
		return;
	for (Transform::iterator i=t->begin ();i != t->end ();i++)
	{
		GameObject& cur = (**i).GetGameObject ();
		ParticleSystemResimulationRecurse (cur, shurikenSubEmitters);
	}	
}

void ParticleSystemEffect::ParticleSystemCollectSubEmittersRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters)
{
	if( go.IsActive() )
	{
		ParticleSystem* shuriken = go.QueryComponent(ParticleSystem);
		if (shuriken)
			ParticleSystemEditor::CollectSubEmittersRec (shuriken, shurikenSubEmitters);
	}
	Transform* t = go.QueryComponent (Transform);
	if (t == NULL)
		return;
	for (Transform::iterator i=t->begin ();i != t->end ();i++)
	{
		GameObject& cur = (**i).GetGameObject ();
		ParticleSystemCollectSubEmittersRecurse (cur, shurikenSubEmitters);
	}	
}


void ParticleSystemEffect::UpdateActiveParticleSystems()
{
	if (IsWorldPlaying())
		return;

	// Old particle system
	UpdateActiveParticleEmitters();

	// Update all in edit mode
	if(ParticleSystemEditor::GetUpdateAll())
	{
		ParticleSystemEditor::UpdateAll();
		return;
	}

	// Update selected target particlesystem
	std::set<GameObject*> activeParticleSystems = ParticleSystemEffect::GetActiveParticleSystems ();
	bool resimulate = ParticleSystemEditor::GetPerformCompleteResimulation();
	ParticleSystemEditor::SetPerformCompleteResimulation(false);
		
	bool playing = false;
	bool isPaused = false;

	// Collect all sub emitters
	std::vector<ParticleSystem*> shurikenSubEmitters;
	for (std::set<GameObject*>::iterator i=activeParticleSystems.begin ();i != activeParticleSystems.end ();i++)
	{
		GameObject* go = *i;
		if (go)
		{
			ParticleSystem* shuriken = go->QueryComponent(ParticleSystem);
			if (shuriken)
			{
				ParticleSystemCollectSubEmittersRecurse(*go, shurikenSubEmitters);
				playing |= shuriken->IsPlaying();
				isPaused |= shuriken->IsPaused ();
			}
			
			if (go->QueryComponent(ParticleSystem))
			{
				Transform* t = go->QueryComponent (Transform);
				if (t == NULL)
					continue;
				t = t->GetParent ();
				if (t == NULL || t == &t->GetRoot ())
					continue;
				for (Transform::iterator i=t->begin ();i != t->end ();i++)
				{
					GameObject& cur = (**i).GetGameObject ();
					if (activeParticleSystems.find (&cur) == activeParticleSystems.end())
					{
						ParticleSystem* shuriken = cur.QueryComponent(ParticleSystem);
						if (shuriken)
						{
							ParticleSystemCollectSubEmittersRecurse(cur, shurikenSubEmitters);
							playing |= shuriken->IsPlaying();
							isPaused |= shuriken->IsPaused ();
						}
					}
				}	
			}
		}
	}
	
	size_t numSubEmitters = shurikenSubEmitters.size();
	for(int i = 0; i < numSubEmitters; i++)
	{
		ParticleSystem* system = shurikenSubEmitters[i]->QueryComponent(ParticleSystem);
		playing |= system->IsAlive();
	}

	playing = playing && !isPaused;
	ParticleSystemEditor::SetPlaybackIsPlaying (playing);
	ParticleSystemEditor::SetPlaybackIsPaused (isPaused);

	// Reset playback time when all systems have stopped (note: playing is false when paused)
	if (!playing && !isPaused)
	{
		ParticleSystemEditor::SetPlaybackTime (0.f);
	}
	
	// Update systems that are not referenced as sub emitters
	for (std::set<GameObject*>::iterator i=activeParticleSystems.begin ();i != activeParticleSystems.end ();i++)
	{
		GameObject* go = *i;
		if (go)
		{
			if(resimulate)
				ParticleSystemEffect::ParticleSystemResimulationRecurse (*go, shurikenSubEmitters);
			ParticleSystemEffect::UpdateParticleSystemRecurse (*go, shurikenSubEmitters);
				
			if (go->QueryComponent(ParticleSystem))
			{
				Transform* t = go->QueryComponent (Transform);
				if (t == NULL)
					continue;
				t = t->GetParent ();
				if (t == NULL || t == &t->GetRoot ())
					continue;
				for (Transform::iterator i=t->begin ();i != t->end ();i++)
				{
					GameObject& cur = (**i).GetGameObject ();
					if (activeParticleSystems.find (&cur) == activeParticleSystems.end())
					{
						if(resimulate)
							ParticleSystemEffect::ParticleSystemResimulationRecurse (cur, shurikenSubEmitters);
						ParticleSystemEffect::UpdateParticleSystemRecurse (cur, shurikenSubEmitters);
					}
				}	
			}
		}
	}
	if (playing && !isPaused && !ParticleSystemEditor::GetIsScrubbing())
	{
		const float time = ParticleSystemEditor::GetPlaybackTime() + GetDeltaTime() * ParticleSystemEditor::GetSimulationSpeed();
		ParticleSystemEditor::SetPlaybackTime(time);
	}
}

void StopAndClearRecurse (GameObject& go, std::vector<ParticleSystem*>& shurikenSubEmitters, bool stop, bool clear)
{
	if( go.IsActive() )
	{
		ParticleSystem* shuriken = go.QueryComponent(ParticleSystem);
		if (shuriken)
		{
			if(stop)
				shuriken->Stop();
			if(clear)
				shuriken->Clear();
		}
	}
	Transform* t = go.QueryComponent (Transform);
	if (t == NULL || !go.QueryComponent(ParticleSystem))
		return;
	for (Transform::iterator i=t->begin ();i != t->end ();i++)
	{
		GameObject& cur = (**i).GetGameObject ();
		StopAndClearRecurse (cur, shurikenSubEmitters, stop, clear);
	}	
}


void ParticleSystemEffect::StopAndClearActive(bool stop, bool clear)
{
	std::set<GameObject*> activeParticleSystems = ParticleSystemEffect::GetActiveParticleSystems ();
	
	// Collect all sub emitters
	std::vector<ParticleSystem*> shurikenSubEmitters;
	for (std::set<GameObject*>::iterator i=activeParticleSystems.begin ();i != activeParticleSystems.end ();i++)
	{
		GameObject* go = *i;
		if (go)
		{
			ParticleSystem* system = go->QueryComponent(ParticleSystem);
			if (system)
			{
				ParticleSystemCollectSubEmittersRecurse(*go, shurikenSubEmitters);
				StopAndClearRecurse (*go, shurikenSubEmitters, stop, clear);

				Transform* t = go->QueryComponent (Transform);
				if (t == NULL)
					continue;
				t = t->GetParent ();
				if (t == NULL || t == &t->GetRoot ())
					continue;
				for (Transform::iterator i=t->begin ();i != t->end ();i++)
				{
					GameObject& cur = (**i).GetGameObject ();
					if (activeParticleSystems.find (&cur) == activeParticleSystems.end())
					{
						ParticleSystem* system = cur.QueryComponent(ParticleSystem);
						if (system)
						{
							ParticleSystemCollectSubEmittersRecurse(*go, shurikenSubEmitters);
							StopAndClearRecurse (cur, shurikenSubEmitters, stop, clear);
						}
					}
				}	
			}
		}
	}
	
	size_t numSubEmitters = shurikenSubEmitters.size();
	for(int i = 0; i < numSubEmitters; i++)
	{
		ParticleSystem* system = shurikenSubEmitters[i]->QueryComponent(ParticleSystem);
		if(stop)
			system->Stop();
		if(clear)
			system->Clear();
	}
}


#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"
#include "Runtime/Graphics/ParticleSystem/Modules/SubModule.h"

CircularReferenceChecker::~CircularReferenceChecker ()
{
	Clear ();
}

bool CircularReferenceChecker::ValidateNoCircularReferences_Recursive( Node* checkThisChild, Node* parent)
{
	if (!checkThisChild  || !parent)
	{
		LogString (Format("ERROR: checkThisChild %p || parent %p", checkThisChild, parent));
		return false;
	}

	m_CircularPath += std::string(parent->particleSystem->GetName ()) + " - ";

	// Referencing itself is not allowed
	if (checkThisChild == parent)
	{
		m_CircularPath += checkThisChild->particleSystem->GetName();
		return false;
	}

	for (std::set<Node*>::iterator it = parent->parents.begin(); it != parent->parents.end(); it++)
	{
		// Found that child is already an ancestor of its parent: NOW that is degenerate and is not allowed
		if (checkThisChild == *it)
		{
			m_CircularPath += checkThisChild->particleSystem->GetName ();
			return false;
		}

		m_Level++;

		// Check ancestors
		if (!ValidateNoCircularReferences_Recursive(checkThisChild, *it))
			return false;

		m_Level--;

		if (m_Level == 0)
			m_CircularPath = "";
	}
	return true;
}



bool CircularReferenceChecker::ValidateNoCircularReference (Node* node, Node* parent)
{
	m_Level = 0;
	m_CircularPath = "";
	return ValidateNoCircularReferences_Recursive (node, parent);
}


CircularReferenceChecker::Node* CircularReferenceChecker::GetNode (ParticleSystem* ps, std::vector<Node*>& graph)
{
	// Already have the node
	for (unsigned i=0; i<graph.size(); ++i)
		if (graph[i]->particleSystem == ps)
			return graph[i];

	// Then create and add new node
	Node* newNode = new Node (ps);
	graph.push_back (newNode);
	return newNode;	
}



// parent is null first iteration (ps is root and has no parent)
bool CircularReferenceChecker::BuildGraphRecursive (ParticleSystem* ps, Node* parent, std::vector<Node*>& graph)
{
	// Get node (create and add node to graph if not found)
	Node* node = GetNode (ps, graph);
	if (parent)
		node->parents.insert (parent);

	// Check for circular reference before continuing
	if (parent && !ValidateNoCircularReference(node, parent))
		return false;

	// Now add subemitter nodes
	ParticleSystem* subEmitters[kParticleSystemMaxSubTotal];
	ParticleSystemEditor::GetSubEmitterPtrs(*ps, &subEmitters[0]);
	SubModule::RemoveDuplicatePtrs(&subEmitters[0]);
	for(int i = 0; i < kParticleSystemMaxSubTotal; i++)
	{
		if(subEmitters[i])
		{
			// Add child
			node->children.insert (GetNode (subEmitters[i], graph));

			// Add subemitters of subemitter
			if (!BuildGraphRecursive(subEmitters[i], node, graph))
				return false;
		}
	}

	return true;
}


void CircularReferenceChecker::DebugPrintGraph ()
{
	for (unsigned i=0; i<m_Graph.size(); ++i)
	{
		std::string text = m_Graph[i]->particleSystem->GetGameObject().GetName();
		text += ": Children:";
		for (std::set<Node*>::iterator it = m_Graph[i]->children.begin(); it != m_Graph[i]->children.end(); ++it)
		{
			Node* node = *it;
			text += std::string(" ") + node->particleSystem->GetGameObject().GetName();
		}
		text += ": Parents:";

		for (std::set<Node*>::iterator it = m_Graph[i]->parents.begin(); it != m_Graph[i]->parents.end(); ++it)
		{
			Node* node = *it;
			text += std::string(" ") + node->particleSystem->GetGameObject().GetName();
		}
		LogString(text);
	}
}

void CircularReferenceChecker::Clear ()
{
	m_Level = 0;
	m_CircularPath = "";
	for (unsigned i=0; i<m_Graph.size(); ++i)
		delete m_Graph[i];
	m_Graph.clear();
}

// static
std::string CircularReferenceChecker::Check (ParticleSystem* checkThisChild, ParticleSystem* parent, ParticleSystem* root)
{
	Clear ();
	if (BuildGraphRecursive (root, NULL, m_Graph))
	{
		if (ValidateNoCircularReference (GetNode(checkThisChild, m_Graph), GetNode(parent, m_Graph)))
			m_CircularPath = ""; // happy
	}
	
	//DebugPrintGraph ();
	return m_CircularPath;
}

