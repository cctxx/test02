#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

class MoveTask : public VCTask
{
public:
	MoveTask(const VCAsset& src, const VCAsset& dst, bool noLocalFileMove = false);

	void Execute();
	void Done();
	
	const std::string& GetDescription() const;
	
private:
	VCAsset m_src;
	VCAsset m_dst;
	bool m_NoLocalFileMove;
};
