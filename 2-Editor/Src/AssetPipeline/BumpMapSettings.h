#ifndef BUMPMAPSETTINGS_H
#define BUMPMAPSETTINGS_H

namespace Unity { class Material; }


class BumpMapSettings
{
private:
	typedef std::set<int> UnmarkedBumpMapTextureImporters;
	UnmarkedBumpMapTextureImporters  m_UnmarkedBumpMapTextureImporters;

public:
	static BumpMapSettings& Get();	

	void PerformBumpMapCheck(Unity::Material& material);
	void PerformUnmarkedBumpMapTexturesFixing();
	void PerformUnmarkedBumpMapTexturesFixingAfterDialog(int result);

	bool static BumpMapTextureNeedsFixing(Unity::Material& material);
	void static FixBumpMapTexture(Unity::Material& material);
};

#endif