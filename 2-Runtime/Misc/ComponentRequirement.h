#ifndef COMPONENTREQUIREMENT_H
#define COMPONENTREQUIREMENT_H
#include "Runtime/Utilities/vector_set.h"
#include <string>
#include <map>
#include <list>
#include <vector>

// Returns all components that are required for a component 
// of componentClassID to run.
const vector_set<int>& FindRequiredComponentsForComponent (int componentClassID);
void FindAllRequiredComponentsRecursive (int componentClassID, vector_set<int>& results);
const vector_set<int>& FindConflictingComponents (int classID);

// Can the component with componentClassID, be more than once in a gameobject?
bool DoesComponentAllowMultipleInclusion (int componentClassID);

struct GOComponentDescription
{
	int classID;
	std::string name;
	GOComponentDescription (const std::string& inName, int inClassID) { name = inName; classID = inClassID; }
};

typedef std::list<std::pair<std::string, std::vector<GOComponentDescription> > > ComponentsHierarchy;
// Returns a sorted hierarchy of components
const ComponentsHierarchy& GetComponentsHierarchy ();

int GetAllowComponentReplacementClass (int classID);

void InitComponentRequirements ();

#endif
