#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "TreeDatabase.h"
#include "Runtime/Math/Rect.h"
#include <vector>


class RenderTexture;
class Camera;


class ImposterRenderTexture
{
public:
	ImposterRenderTexture(const TreeDatabase& treeDB);
	~ImposterRenderTexture();
		
	const Rectf& GetArea(int index) const {
		return m_Areas[index];
	}	

	RenderTexture* GetTexture() const { return m_Texture; }
	bool GetSupported() const { return m_Supported; }

	void UpdateImposters(const Camera& mainCamera);

	void InvalidateAngles () 
	{
		m_AngleX = std::numeric_limits<float>::infinity();
		m_AngleY = std::numeric_limits<float>::infinity();
	}

	const Matrix4x4f& getCameraOrientation() const { return m_CameraOrientationMatrix; }

	void GetBillboardParams(float& angleFactor, float& offsetFactor) const;
private:
	void UpdateImposter(const Rectf& rect, const TreeDatabase::Prototype& prototype);

private:
	const TreeDatabase&	m_TreeDatabase;

	std::vector<Rectf> m_Areas;
	Camera* m_Camera;
	RenderTexture* m_Texture;
	bool		m_Supported;

	float    m_AngleX;
	float    m_AngleY;

	int 		    m_ImposterHeight;
	int 		    m_MaxImposterSize;

	Matrix4x4f m_CameraOrientationMatrix;

	static void InvalidateRenderTexture(void* userData) {
		((ImposterRenderTexture*)userData)->InvalidateAngles();
	}
};

#endif // ENABLE_TERRAIN


