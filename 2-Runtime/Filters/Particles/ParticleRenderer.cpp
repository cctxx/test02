#include "UnityPrefix.h"
#include "ParticleRenderer.h"
#include "ParticleEmitter.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Misc/LineBuilder.h"
#include "Runtime/Camera/Camera.h"
#include "ParticleStruct.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"

IMPLEMENT_CLASS (ParticleRenderer)
IMPLEMENT_OBJECT_SERIALIZE (ParticleRenderer)

// The distance of particle Corner from the center
// sin (45 deg), or sqrt(0.5)
#define kMaxParticleSizeFactor 0.707106781186548f

ParticleRenderer::ParticleRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererParticle, label, mode)
{
	SetVisible (false);
	m_UVFrames = NULL;
}

ParticleRenderer::~ParticleRenderer ()
{
	if (m_UVFrames)
		UNITY_FREE(kMemParticles, m_UVFrames);
}

struct ParticleVertex {
	Vector3f vert;
	Vector3f normal;
	ColorRGBA32	color;
	Vector2f uv;
};

void ParticleRenderer::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{	
	Super::AwakeFromLoad (awakeMode);	
	ParticleRenderer::GenerateUVFrames ();
}

void ParticleRenderer::SetUVFrames (const Rectf *uvFrames, int numFrames)
{
	m_NumUVFrames = numFrames;
	
	if (m_UVFrames)
		UNITY_FREE(kMemParticles, m_UVFrames);

	size_t size;
	size = sizeof(Rectf) * m_NumUVFrames; 
	
	if (m_NumUVFrames != 0 && m_NumUVFrames != size / sizeof(Rectf)) 
	{
		m_UVFrames = NULL;
		m_NumUVFrames = 0;
	} 
	else 
	{
		m_UVFrames = (Rectf*)UNITY_MALLOC(kMemParticles, size);
		memcpy(m_UVFrames, uvFrames, numFrames*sizeof(Rectf));
	}
}

void ParticleRenderer::GenerateUVFrames ()
{
	if(m_UVAnimation.xTile < 1)
		m_UVAnimation.xTile = 1;
	if(m_UVAnimation.yTile < 1)
		m_UVAnimation.yTile = 1;

	m_NumUVFrames = (m_UVAnimation.xTile * m_UVAnimation.yTile);
	float animUScale = 1.0f / m_UVAnimation.xTile;
	float animVScale = 1.0f / m_UVAnimation.yTile;
	
	if (m_UVFrames)
		UNITY_FREE(kMemParticles, m_UVFrames);

	if(m_NumUVFrames == 1)
		m_NumUVFrames = 0;
	SET_ALLOC_OWNER(this);
	m_UVFrames = (Rectf*)UNITY_MALLOC(kMemParticles, m_NumUVFrames * sizeof(Rectf));
	
	for(int index=0;index<m_NumUVFrames;index++)
	{
		int vIdx = index / m_UVAnimation.xTile;
		int uIdx = index - vIdx * m_UVAnimation.xTile;	// slightly faster than index % m_UVAnimation.xTile
		float uOffset = (float)uIdx * animUScale;
		float vOffset = 1.0f - animVScale - (float)vIdx * animVScale;
		
		m_UVFrames[index] = Rectf(uOffset, vOffset, animUScale, animVScale);
	}
}

void ParticleRenderer::SetUVAnimationXTile (int v)
{
	v = std::max(v,1);
	if (v == m_UVAnimation.xTile)
		return;
	m_UVAnimation.xTile = v;
	SetDirty ();
	GenerateUVFrames ();
}
void ParticleRenderer::SetUVAnimationYTile (int v)
{
	v = std::max(v,1);
	if (v == m_UVAnimation.yTile)
		return;
	m_UVAnimation.yTile = v;
	SetDirty ();
	GenerateUVFrames ();
}
void ParticleRenderer::SetUVAnimationCycles (float v)
{
	v = std::max(v,0.0f);
	if (v == m_UVAnimation.cycles)
		return;
	m_UVAnimation.cycles = v;
	SetDirty ();
}



PROFILER_INFORMATION(gParticlesProfile, "ParticleRenderer.Render", kProfilerParticles)
PROFILER_INFORMATION(gSubmitVBOProfileParticle, "Mesh.SubmitVBO", kProfilerRender)

#pragma message ("Optimize particle systems to use templates for inner loops and build optimized inner loops without ifs")

void ParticleRenderer::Render (int/* materialIndex*/, const ChannelAssigns& channels)
{	

	ParticleEmitter* emitter = QueryComponent(ParticleEmitter);
	if( !emitter )
		return;
	
	PROFILER_AUTO_GFX(gParticlesProfile, this)
	
	GfxDevice& device = GetGfxDevice();
	Matrix4x4f matrix;

	ParticleArray& particles = emitter->GetParticles();
	const PrivateParticleInfo& privateInfo = emitter->GetPrivateInfo();
	if( particles.empty() )
		return;

	Camera& cam = GetCurrentCamera ();
	
	Vector3f xSpan, ySpan;
	if(m_StretchParticles == kBillboardFixedHorizontal)
	{
		xSpan = Vector3f(1.0f,0.0f,0.0f);
		ySpan = Vector3f(0.0f,0.0f,1.0f);		
	}

	if(m_StretchParticles == kBillboardFixedVertical)
	{
		ySpan = Vector3f(0.0f,1.0f,0.0f);		
		Vector3f zSpan = RotateVectorByQuat (cam.GetComponent (Transform).GetRotation(), Vector3f(0.0f,0.0f,1.0f));
		xSpan = NormalizeSafe( Cross(ySpan, zSpan) );
	}

	Vector3f cameraVelocity;
	if (privateInfo.useWorldSpace)
	{
		CopyMatrix (device.GetViewMatrix (), matrix.GetPtr());
		cameraVelocity = cam.GetVelocity ();
	}	
	else
	{
		Matrix4x4f mat, temp;
		CopyMatrix (device.GetViewMatrix (), temp.GetPtr());
		mat = GetComponent (Transform).GetLocalToWorldMatrixNoScale();
		MultiplyMatrices4x4 (&temp, &mat, &matrix);
		cameraVelocity = GetComponent (Transform).InverseTransformDirection (cam.GetVelocity ());
	}

	// Constrain the size to be a fraction of the viewport size.
	// In perspective case, max size is (z*factorA). In ortho case, max size is just factorB. To have both
	// without branches, we do (z*factorA+factorB) and set one of factors to zero.
	float maxPlaneScale;
	float maxOrthoSize;
	if (!cam.GetOrthographic()) {
		maxPlaneScale = -cam.CalculateFarPlaneWorldSpaceLength () * m_MaxParticleSize / cam.GetFar ();
		maxOrthoSize = 0.0f;
	} else {
		maxPlaneScale = 0.0f;
		maxOrthoSize = cam.CalculateFarPlaneWorldSpaceLength () * m_MaxParticleSize;
	}
	
	/// Sort the particles
	if (m_StretchParticles == kSortedBillboard && !particles.empty ())
	{
		static vector<float>		   s_Dist;
		if (s_Dist.size() < particles.size()) {
			s_Dist.resize (0);
			s_Dist.resize (particles.size());
		}

		// Calculate all distances
		int i;
		Vector3f distFactor = Vector3f (matrix.Get (2, 0), matrix.Get (2, 1), + matrix.Get (2, 2));
		for (i = 0; i < particles.size(); i++)
			s_Dist[i] = Dot (distFactor, particles[i].position);
		
		// Bubblesort them
		i = 0;
		while (i < particles.size() - 1)
		{
			if (s_Dist[i] > s_Dist[i + 1])
			{
				std::swap (s_Dist[i], s_Dist[i + 1]);
				std::swap (particles[i], particles[i + 1]);
				if (i > 0)
					i -= 2;
			}
			i++;
		}
	}
		
	// Calculate parameters for UV animation
	const int animFullTexCount = int(m_NumUVFrames * m_UVAnimation.cycles);
	
	// Get VBO chunk
	const int particleCount = particles.size();
	DynamicVBO& vbo = device.GetDynamicVBO();
	ParticleVertex* vbPtr;
	if( !vbo.GetChunk( (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor),
		particleCount * 4, 0,
		DynamicVBO::kDrawQuads,
		(void**)&vbPtr, NULL ) )
	{
		return;
	}
	
	int billboardRenderMode = m_StretchParticles;

	if (billboardRenderMode == kSortedBillboard)
		billboardRenderMode = kBillboard;
	if (billboardRenderMode == kBillboardFixedVertical)
		billboardRenderMode = kBillboardFixedHorizontal;
	
	// Fill vertex buffer with particles
	for( int i = 0; i < particleCount; ++i )
	{
		const Particle& p = particles[i];
		Vector3f vert[4];
		Vector2f uv[4];
		
		if (p.rotation != 0)
		{
			if (billboardRenderMode == kBillboard)
				billboardRenderMode = kBillboardRotated;
			else if (billboardRenderMode == kBillboardFixedHorizontal)
				billboardRenderMode = kBillboardFixedRotated;
		}	
		// Positions
		// @TODO: get rid of the branch here
		switch (billboardRenderMode) {
		case kBillboard: 
			{
				// Project point and create quad		
				Vector3f center = matrix.MultiplyPoint3 (p.position);
				
				// Constrain the size to be a fraction of the viewport size. 
				// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
				// Also all valid z's are negative so we just negate the whole equation
				float maxWorldSpaceLength = center.z * maxPlaneScale + maxOrthoSize;
				float size = std::min (p.size, maxWorldSpaceLength);
				float halfSize = size * 0.5f;
				
				vert[0].Set( center.x - halfSize, center.y + halfSize, center.z );
				vert[1].Set( center.x + halfSize, center.y + halfSize, center.z );
				vert[2].Set( center.x + halfSize, center.y - halfSize, center.z );
				vert[3].Set( center.x - halfSize, center.y - halfSize, center.z );
			}
			break;
		case kBillboardFixedHorizontal:
			{
				// Project point and create quad		
				Vector3f center = matrix.MultiplyPoint3 (p.position);
				
				// Constrain the size to be a fraction of the viewport size. 
				// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
				// Also all valid z's are negative so we just negate the whole equation
				float maxWorldSpaceLength = (center.z - p.size * kMaxParticleSizeFactor) * maxPlaneScale + maxOrthoSize;
				float size = std::min (p.size, maxWorldSpaceLength);
				float halfSize = size * 0.5f;
				
				vert[0] = matrix.MultiplyPoint3 ( p.position - halfSize * xSpan + halfSize * ySpan );
				vert[1] = matrix.MultiplyPoint3 ( p.position + halfSize * xSpan + halfSize * ySpan );
				vert[2] = matrix.MultiplyPoint3 ( p.position + halfSize * xSpan - halfSize * ySpan );
				vert[3] = matrix.MultiplyPoint3 ( p.position - halfSize * xSpan - halfSize * ySpan );
			}
			break;			
		case kBillboardRotated:
			{
				// Project point and create quad
				// Particles can be rotated by animation curve and width & height can be animated individually using an animation curve
				Vector3f center = matrix.MultiplyPoint3 (p.position);
								
				// Constrain the size to be a fraction of the viewport size. 
				// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
				// Also all valid z's are negative so we just negate the whole equation
				float maxWorldSpaceLength = center.z * maxPlaneScale + maxOrthoSize;
									
				float s = Sin (p.rotation);
				float c = Cos (p.rotation);
					
				float x00 = c;
				float x01 = s;
				float x10 = -s;
				float x11 = c;
				
				float size = std::min (p.size, maxWorldSpaceLength);
				float halfSize = size * 0.5f;
				
				#define MUL(w,h) center.x + x00 * w + x01 * h, center.y + x10 * w + x11 * h, center.z
					
				vert[0].Set(MUL(-halfSize, halfSize));
				vert[1].Set(MUL( halfSize, halfSize));
				vert[2].Set(MUL( halfSize, -halfSize));
				vert[3].Set(MUL(-halfSize, -halfSize));
				#undef MUL
			}
			break;
		case kBillboardFixedRotated:
			{
				// Project point and create quad		
				Vector3f center = matrix.MultiplyPoint3 (p.position);
				
				// Constrain the size to be a fraction of the viewport size. 
				// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
				// Also all valid z's are negative so we just negate the whole equation
				float maxWorldSpaceLength = center.z * maxPlaneScale + maxOrthoSize;
				
				float s = Sin (p.rotation);
				float c = Cos (p.rotation);
					
				float size = std::min (p.size, maxWorldSpaceLength);
				float halfSize = size * 0.5f;
				
				vert[0] = matrix.MultiplyPoint3 ( p.position + halfSize * xSpan * c + halfSize * ySpan * s );
				vert[1] = matrix.MultiplyPoint3 ( p.position + halfSize * ySpan * c - halfSize * xSpan * s );
				vert[2] = matrix.MultiplyPoint3 ( p.position - halfSize * xSpan * c - halfSize * ySpan * s );
				vert[3] = matrix.MultiplyPoint3 ( p.position - halfSize * ySpan * c + halfSize * xSpan * s );
			}
			break;			

		case kStretch3D:
			{
				Vector3f velocity = p.velocity - cameraVelocity * m_CameraVelocityScale;
				float sqrVelocity = SqrMagnitude (velocity);
				
				float size = p.size;
				
				float lineScale;
				if (sqrVelocity > Vector3f::epsilon)
					lineScale = m_VelocityScale + FastInvSqrt (sqrVelocity) * (m_LengthScale * size);
				else
					lineScale = 0.0F;
				
				Vector3f lineDelta = velocity * lineScale;
				Vector3f begProj = matrix.MultiplyPoint3 (p.position);
				Vector3f endProj = matrix.MultiplyPoint3 (p.position - lineDelta);			
				
				// Constrain the size to be a fraction of the viewport size. 
				// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
				// Also all valid z's are negative so we just negate the whole equation
				float maxWorldSpaceLength = endProj.z * maxPlaneScale + maxOrthoSize;
				size = std::min (size, maxWorldSpaceLength);
				float halfSize = size * 0.5F;
				
				Vector2f delta = Calculate2DLineExtrusion(begProj, begProj - endProj, halfSize);
				
				vert[0].Set( begProj.x + delta.x, begProj.y + delta.y, begProj.z );
				vert[1].Set( endProj.x + delta.x, endProj.y + delta.y, endProj.z );
				vert[2].Set( endProj.x - delta.x, endProj.y - delta.y, endProj.z );
				vert[3].Set( begProj.x - delta.x, begProj.y - delta.y, begProj.z );
			}
			break;
		case kStretch2D:
			{
				Vector3f velocity = p.velocity - cameraVelocity * m_CameraVelocityScale;
				float sqrVelocity = SqrMagnitude (velocity);
				
				float size = p.size;
				
				float lineScale;
				if (sqrVelocity > Vector3f::epsilon)
					lineScale = m_VelocityScale + FastInvSqrt (sqrVelocity) * (m_LengthScale * size);
				else
					lineScale = 0.0F;
				
				Vector3f lineDelta = velocity * lineScale;
				Vector3f begProj = matrix.MultiplyPoint3 (p.position);
				Vector3f endProj = matrix.MultiplyPoint3 (p.position + lineDelta);
				
				float dx = endProj.x - begProj.x;
				float dy = endProj.y - begProj.y;
				
				float sqrProjectedLength = dx*dx + dy*dy;
				
				if (sqrProjectedLength < Vector3f::epsilon)
				{
					dx = 1.0F;
					dy = 0.0F;
					sqrProjectedLength = 1.0F;
				}
				
				// Constrain the size to be a fraction of the viewport size. 
				// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
				// Also all valid z's are negative so we just negate the whole equation
				float maxWorldSpaceLength = endProj.z * maxPlaneScale + maxOrthoSize;
				size = std::min (size, maxWorldSpaceLength);
				float halfSize = size * 0.5F;
				
				float lengthInv = halfSize * FastInvSqrt (sqrProjectedLength);
				// Orthogonal 2D-vector to sdx0
				float sdx1 = -dy * lengthInv;
				float sdy1 =  dx * lengthInv;
				
				// scale with velocity
				vert[0].Set( begProj.x + sdx1, begProj.y + sdy1, begProj.z );
				vert[1].Set( endProj.x + sdx1, endProj.y + sdy1, endProj.z );
				vert[2].Set( endProj.x - sdx1, endProj.y - sdy1, endProj.z );
				vert[3].Set( begProj.x - sdx1, begProj.y - sdy1, begProj.z );
			}
			break;
		}
				
		
		// UVs
		if(m_NumUVFrames > 0)
		{
			const float alpha = 1.0f - clamp01 (p.energy / p.startEnergy);
			unsigned int index = (unsigned int)(alpha * animFullTexCount);
			
			Rectf *r = m_UVFrames+(index % m_NumUVFrames);
			uv[0].Set( r->x, r->y + r->height );
			uv[1].Set( r->x + r->width, r->y + r->height );
			uv[2].Set( r->x + r->width, r->y );
			uv[3].Set( r->x, r->y );
		}
		else
		{
			uv[0].Set( 0, 1 );
			uv[1].Set( 1, 1 );
			uv[2].Set( 1, 0 );
			uv[3].Set( 0, 0 );
		}

		
		// Swizzle color of the renderer requires it
		ColorRGBA32 color = p.color;
		color = device.ConvertToDeviceVertexColor(color);
		
		// Now, write out the vertex structures sequentially (important when writing into dynamic VBO)
		// @TODO: this place seems to be heavy in the profile (even more than branches above), optimize somehow!
		vbPtr[0].vert = vert[0];
		vbPtr[0].normal.Set( 0.0f, 0.0f, 1.0f );
		vbPtr[0].color = color;
		vbPtr[0].uv = uv[0];
		
		vbPtr[1].vert = vert[1];
		vbPtr[1].normal.Set( 0.0f, 0.0f, 1.0f );
		vbPtr[1].color = color;
		vbPtr[1].uv = uv[1];
		
		vbPtr[2].vert = vert[2];
		vbPtr[2].normal.Set( 0.0f, 0.0f, 1.0f );
		vbPtr[2].color = color;
		vbPtr[2].uv = uv[2];
		
		vbPtr[3].vert = vert[3];
		vbPtr[3].normal.Set( 0.0f, 0.0f, 1.0f );
		vbPtr[3].color = color;
		vbPtr[3].uv = uv[3];
		
		// Next four vertices
		vbPtr += 4;
	}
	
	vbo.ReleaseChunk( particleCount * 4, 0 );
	
	// Draw
	float matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity
	
	if (m_CustomProperties)
		device.SetMaterialProperties (*m_CustomProperties);
	
	PROFILER_BEGIN(gSubmitVBOProfileParticle, this)
	vbo.DrawChunk (channels);
	GPU_TIMESTAMP();
	PROFILER_END
	
	device.SetViewMatrix(matView);
}

void ParticleRenderer::AdjustBoundsForStretch( const ParticleEmitter& emitter, MinMaxAABB& aabb ) const
{
	Assert(m_StretchParticles == kStretch3D);
	
	const ParticleArray& particles = emitter.GetParticles();
	const size_t particleCount = particles.size();
	const float velocityScale = m_VelocityScale;
	const float lengthScale = m_LengthScale;
	const Particle* p = &particles[0];
	
	for( size_t i = 0; i < particleCount; ++i, ++p ) {
		float sqrVelocity = SqrMagnitude (p->velocity);
		if (sqrVelocity > Vector3f::epsilon) {
			float scale = velocityScale + FastInvSqrt (sqrVelocity) * lengthScale * p->size;
			aabb.Encapsulate( p->position - p->velocity * scale );
		}
	}
}

void ParticleRenderer::UpdateTransformInfo ()
{
	const Transform& transform = GetTransform();
	if (m_TransformDirty)
	{
		m_TransformInfo.invScale = 1.0f;
		// will return a cached matrix most of the time
		m_TransformInfo.transformType = transform.CalculateTransformMatrix (m_TransformInfo.worldMatrix);;
	}

	if (m_BoundsDirty)
	{
		ParticleEmitter* emitter = QueryComponent(ParticleEmitter);
		if (!emitter)
		{
			m_TransformInfo.localAABB.SetCenterAndExtent(Vector3f::zero, Vector3f::zero);
			m_TransformInfo.worldAABB.SetCenterAndExtent(Vector3f::zero, Vector3f::zero);
			return;
		}

		const PrivateParticleInfo& info = emitter->GetPrivateInfo();
		MinMaxAABB aabb = info.aabb;
		if (m_StretchParticles == kStretch3D)
			AdjustBoundsForStretch (*emitter, aabb);
		aabb.Expand (info.maxParticleSize * kMaxParticleSizeFactor);

		if (info.useWorldSpace)
		{
			m_TransformInfo.worldAABB = aabb;
			InverseTransformAABB (m_TransformInfo.worldAABB, transform.GetPosition(), transform.GetRotation(), m_TransformInfo.localAABB);
		}
		else
		{
			m_TransformInfo.localAABB = aabb;
			TransformAABB (m_TransformInfo.localAABB, transform.GetPosition(), transform.GetRotation(), m_TransformInfo.worldAABB);
		}
	}
}


void ParticleRenderer::UpdateRenderer()
{
	ParticleEmitter* emitter = QueryComponent(ParticleEmitter);
	if( emitter )
	{
		bool empty = emitter->GetParticles().empty();
		SetVisible( !empty );
		if( !empty )
			BoundsChanged();
	}
	else
	{
		UpdateManagerState( false );
	}

	Super::UpdateRenderer();
}

void ParticleRenderer::UpdateParticleRenderer()
{
	UpdateManagerState( true );
}


template<class TransferFunction> inline
void ParticleRenderer::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	TRANSFER (m_CameraVelocityScale);
	TRANSFER_SIMPLE (m_StretchParticles);
	TRANSFER_SIMPLE (m_LengthScale);
	TRANSFER (m_VelocityScale);
	TRANSFER (m_MaxParticleSize);
	
	if (transfer.IsCurrentVersion()) {
		transfer.Transfer (m_UVAnimation, "UV Animation");
	} else {
		transfer.Transfer (m_UVAnimation.xTile, "m_AnimatedTextureCount");
	}
}

void ParticleRenderer::CheckConsistency ()
{
	Super::CheckConsistency ();
	m_MaxParticleSize = std::max (0.0F, m_MaxParticleSize);
	m_UVAnimation.xTile = std::max (1, m_UVAnimation.xTile);
	m_UVAnimation.yTile = std::max (1, m_UVAnimation.yTile);
	m_UVAnimation.cycles = std::max (0.0F, m_UVAnimation.cycles);
}

void ParticleRenderer::Reset ()
{
	Super::Reset ();
	m_StretchParticles = kBillboard;
	m_LengthScale = 2.0F;
	m_VelocityScale = 0.0F;
	m_MaxParticleSize = 0.25F;
	m_UVAnimation.xTile = 1;
	m_UVAnimation.yTile = 1;
	m_UVAnimation.cycles = 1;
	m_CameraVelocityScale = 0.0;
}
