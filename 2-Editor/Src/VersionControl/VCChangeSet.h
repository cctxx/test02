#pragma once
#include <string>
#include <vector>
#include "Runtime/Mono/MonoTypes.h"

typedef std::string VCChangeSetID;

extern const std::string kDefaultChangeSetDescription;
extern const std::string kDefaultChangeSetID;
extern const std::string kNewChangeSetID;

class VCPluginSession;

class VCChangeSet
{
public:
	VCChangeSet();
	VCChangeSet(VCChangeSet const& other);
	explicit VCChangeSet(std::string const& description);
	const VCChangeSet& operator=(VCChangeSet const& rhs);

	std::string GetDescription() const {return m_Description;}
	void SetDescription(std::string const& description) {m_Description = description;} 

	VCChangeSetID GetID() const {return m_ID;}
	void SetID(VCChangeSetID id) {m_ID = id;}

private:
	VCChangeSetID m_ID;
	std::string m_Description;
};

typedef std::vector<VCChangeSet> VCChangeSets;
typedef std::vector<VCChangeSetID> VCChangeSetIDs;

void ExtractChangeSetIDs(const VCChangeSets& src, VCChangeSetIDs& dst);

VCPluginSession& operator<<(VCPluginSession& p, const VCChangeSet& list);
VCPluginSession& operator>>(VCPluginSession& p, VCChangeSet& cl);

VCPluginSession& operator<<(VCPluginSession& p, const VCChangeSetID& id);
VCPluginSession& operator>>(VCPluginSession& p, VCChangeSetID& id);
