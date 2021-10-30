#ifndef PARTICLERENDERER_H
#define PARTICLERENDERER_H

#include <vector>
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/AnimationCurve.h"
using std::vector;
class ParticleEmitter;
class MinMaxAABB;




enum ParticleRenderMode {
	kBillboard = 0,
	kStretch2D = 1,
	kStretch3D = 3,
	kSortedBillboard = 2,
	kBillboardFixedHorizontal = 4,
	kBillboardFixedVertical = 5,
	
	/// Internal modes
	kBillboardRotated = 1000,
	kBillboardFixedRotated = 1001
};

class ParticleRenderer : public Renderer {
public:
	REGISTER_DERIVED_CLASS (ParticleRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (ParticleRenderer)

	ParticleRenderer (MemLabelId label, ObjectCreationMode mode);
	// ~ParticleRenderer(); declared-by-macro
	
	virtual void Render (int materialIndex, const ChannelAssigns& channels);
	
	// Can operate in either local or world space, so we need to fill whole transform info ourselves
	virtual void UpdateTransformInfo();
		
	virtual void CheckConsistency ();
	virtual void Reset ();

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	GET_SET_DIRTY (ParticleRenderMode, RenderMode, m_StretchParticles) ;
	GET_SET_DIRTY (float, LengthScale, m_LengthScale) ;
	GET_SET_DIRTY (float, VelocityScale, m_VelocityScale) ;
	GET_SET_DIRTY (float, CameraVelocityScale, m_CameraVelocityScale) ;
	GET_SET_DIRTY (float, MaxParticleSize, m_MaxParticleSize) ;
	int GetUVAnimationXTile() const { return m_UVAnimation.xTile; }
	int GetUVAnimationYTile() const { return m_UVAnimation.yTile; }
	float GetUVAnimationCycles() const { return m_UVAnimation.cycles; }
	void SetUVAnimationXTile (int v);
	void SetUVAnimationYTile (int v);
	void SetUVAnimationCycles (float v);
		
	void UpdateParticleRenderer();

	Rectf *GetUVFrames() {return m_UVFrames;};
	int GetNumUVFrames() {return m_NumUVFrames;};
	
	void SetUVFrames(const Rectf *uvFrames, int numFrames);
	
private:
	// from Renderer
	virtual void UpdateRenderer();
	
	void AdjustBoundsForStretch( const ParticleEmitter& emitter, MinMaxAABB& aabb ) const;

protected:
	struct UVAnimation {
		int xTile;			///< Number of texture tiles in the X direction.
		int yTile;			///< Number of texture tiles in the Y direction.
		float cycles;		///< Number of cycles over a particle's life span.
		DECLARE_SERIALIZE (UVAnimation)
	};
	void SetBufferSize (int particleCount);
	void GenerateUVFrames ();

	int          m_StretchParticles;		///< enum { Billboard = 0, Stretched = 3, Sorted Billboard = 2, Horizontal Billboard = 4, Vertical Billboard = 5 } Should the particles be stretched along their velocity?
	float        m_LengthScale;		///< When Stretch Particles is enabled, defines the length of the particle compared to its width.
	float        m_VelocityScale;		///< When Stretch Particles is enabled, defines the length of the particle compared to its velocity.
	float        m_MaxParticleSize;		///< How large is a particle allowed to be on screen at most? 1 is entire viewport. 0.5 is half viewport.
	UVAnimation  m_UVAnimation; 	///< Tiled UV settings.

	float        m_CameraVelocityScale;	///< How much the camera motion is factored in when determining particle stretching
	
	int			 m_NumUVFrames;		//uv tiles for uv animation
	Rectf		*m_UVFrames;
};

template<class TransferFunc>
void ParticleRenderer::UVAnimation::Transfer (TransferFunc& transfer) {
	transfer.Transfer (xTile, "x Tile", kSimpleEditorMask);
	transfer.Transfer (yTile, "y Tile", kSimpleEditorMask);
	TRANSFER_SIMPLE (cycles);
}

#endif
