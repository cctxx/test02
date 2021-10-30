#include "UnityPrefix.h"
#include "VCChangeSet.h"
#include "VCProvider.h"
#include "VCPlugin.h"

const string kDefaultChangeSetDescription = "default";
const string kDefaultChangeSetID = "-1";
const string kNewChangeSetID = "-2";

VCChangeSet::VCChangeSet ()
: m_ID(kDefaultChangeSetID)
{
}

VCChangeSet::VCChangeSet (VCChangeSet const& other)
: m_Description(other.m_Description)
, m_ID(other.m_ID)
{
}

VCChangeSet::VCChangeSet (std::string const& description)
: m_Description(description)
, m_ID(kDefaultChangeSetID)
{
}

const VCChangeSet& VCChangeSet::operator= (VCChangeSet const& rhs)
{
	m_Description = rhs.m_Description;
	m_ID = rhs.m_ID;
	return *this;
}

void ExtractChangeSetIDs(const VCChangeSets& src, VCChangeSetIDs& dst)
{
	dst.clear();
	dst.reserve(src.size());
	for (VCChangeSets::const_iterator i = src.begin(); i != src.end(); i++)
	{
		dst.push_back(i->GetID());
	}
}

VCPluginSession& operator<<(VCPluginSession& p, const VCChangeSet& list)
{
	string id = list.GetID() == kDefaultChangeSetID ? kDefaultChangeSetID : list.GetID();
	p.WriteLine(id);
	p.WriteLine(list.GetDescription());
	return p;
}

VCPluginSession& operator>>(VCPluginSession& p, VCChangeSet& cl)
{
	cl.SetID(p.ReadLine());
	cl.SetDescription(p.ReadLine());
	return p;
}

VCPluginSession& operator<<(VCPluginSession& p, const VCChangeSetID& id)
{
	p.WriteLine(id);
	return p;
}

VCPluginSession& operator>>(VCPluginSession& p, VCChangeSetID& id)
{
	id = p.ReadLine();
	return p;
}
