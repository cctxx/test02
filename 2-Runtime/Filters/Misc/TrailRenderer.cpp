#include "UnityPrefix.h"
#include "TrailRenderer.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/GameCode/DestroyDelayed.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"

const float kMinSqrDistance = 0.1f * 0.1f;

IMPLEMENT_CLASS_INIT_ONLY (TrailRenderer)
IMPLEMENT_OBJECT_SERIALIZE (TrailRenderer)

void TrailRenderer::InitializeClass ()
{
	REGISTER_MESSAGE (TrailRenderer, kTransformChanged, TransformChanged, int);
}

TrailRenderer::TrailRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererTrail, label, mode)
,	m_TransformChanged(false)
,	m_WasRendered(false)
,	m_CurrentLength(0)
,	m_Time(0)
,	m_MinVertexDistance(0)
,	m_Autodestruct(false)
{
	m_AABB = MinMaxAABB (Vector3f::zero, Vector3f::zero);
}

TrailRenderer::~TrailRenderer ()
{
}

void TrailRenderer::Reset () {
	Super::Reset ();
	m_Colors[0].Set (255,255,255,255);
	m_Colors[1].Set (255,255,255,255);
	m_Colors[2].Set (255,255,255,255);
	m_Colors[3].Set (255,255,255,255);
	m_Colors[4].Set (255,255,255,0);
	m_Time = 5.0f;
	m_TransformChanged = true;
	m_MinVertexDistance = 0.1F;
	m_Positions.clear();
	m_TimeStamps.clear();
}

void TrailRenderer::UpdateRenderer()
{
	Super::UpdateRenderer();

	float now = GetCurTime ();
	// Remove last vertrices if neccessary
	while (!m_TimeStamps.empty() && now > m_TimeStamps.back() + m_Time) {
		m_Positions.pop_back();
		m_TimeStamps.pop_back();
	}

	// Add a vertex to the object
	if (m_TransformChanged) {
		Vector3f position = GetComponent (Transform).GetPosition ();
		if( m_Positions.empty () || SqrMagnitude (m_Positions.front () - position) > m_MinVertexDistance*m_MinVertexDistance )
		{
			m_Positions.push_front (position);
			m_TimeStamps.push_front (now);
		}

		float halfWidth = GetHalfMaxLineWidth ();
		AABB newPosAABB (m_Positions.front (), Vector3f(halfWidth, halfWidth, halfWidth));

		// Expand the BBox with the new transform position.
		m_AABB.Encapsulate (newPosAABB);
		BoundsChanged ();
	}
	
	if (m_Positions.size() < 2) {
		if (m_Autodestruct && m_WasRendered && IsWorldPlaying ())
			DestroyObjectDelayed (GetGameObjectPtr());
	}
	else
		m_WasRendered = true;

	// Important: update manager state after calling any SetVisible() above. Fixes an issue
	// where trails would stop be rendered when object is disabled or stops moving.
	UpdateManagerState( true );
	
	m_TransformChanged = false;
}

float TrailRenderer::GetHalfMaxLineWidth () const
{
	return std::max (m_LineParameters.endWidth, m_LineParameters.startWidth) * 0.5f;
}

PROFILER_INFORMATION(gTrailRenderProfile, "TrailRenderer.Render", kProfilerRender)
PROFILER_INFORMATION(gSubmitVBOProfileTrail, "Mesh.SubmitVBO", kProfilerRender)

void TrailRenderer::Render (int materialIndex, const ChannelAssigns& channels)
{
	PROFILER_AUTO(gTrailRenderProfile, this)

	int size = m_Positions.size();
	if( size < 2 )
		return;

	Vector3f* trailInVerts = NULL;
	ALLOC_TEMP(trailInVerts, Vector3f, size);
	int idx = 0;
	for (std::list<Vector3f>::iterator j = m_Positions.begin(); j != m_Positions.end(); ++j, ++idx)
	{
		trailInVerts[idx] = *j;
	}
	trailInVerts[0] = GetComponent(Transform).GetPosition();
	
	// Get VBO chunk
	int stripCount = size * 2;
	GfxDevice& device = GetGfxDevice();
	DynamicVBO& vbo = device.GetDynamicVBO();
	LineVertex* vbPtr;
	if( !vbo.GetChunk( (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor),
		stripCount, 0,
		DynamicVBO::kDrawTriangleStrip,
		(void**)&vbPtr, NULL ) )
	{
		return;
	}
	
	// Generate line into the chunk
	MinMaxAABB aabb;
	m_LineParameters.outVertices = vbPtr;
	m_LineParameters.gradient = &m_Colors;
	m_LineParameters.cameraTransform = GetCurrentCamera().GetWorldToCameraMatrix();	
	m_LineParameters.outAABB = &aabb;
	Build3DLine( &m_LineParameters, trailInVerts, size );
	
	vbo.ReleaseChunk( stripCount, 0 );

	aabb.Expand (GetHalfMaxLineWidth ());

	if (!CompareMemory (m_AABB, aabb))
	{
		m_AABB = aabb;
		BoundsChanged ();
	}

	// We can't set the view matrix since that breaks shadow maps (case 490315)
	// Set the world matrix instead so it cancels out the usual view matrix
	// i.e. it transforms from camera space back to world space
	device.SetWorldMatrix(GetCurrentCamera().GetCameraToWorldMatrix().GetPtr());

	if (m_CustomProperties)
		device.SetMaterialProperties (*m_CustomProperties);
	
	PROFILER_BEGIN(gSubmitVBOProfileTrail, this)
	vbo.DrawChunk (channels);
	GPU_TIMESTAMP();
	PROFILER_END
}

void TrailRenderer::TransformChanged (int changeMask)
{
	Renderer::TransformChanged (changeMask);
	m_TransformChanged = true;
}

void TrailRenderer::UpdateTransformInfo()
{
	const Transform& t = GetComponent (Transform);

	TransformType type = t.CalculateTransformMatrix (m_TransformInfo.worldMatrix);
	m_TransformInfo.transformType = type;
	m_TransformInfo.invScale = 1.0f;

	m_TransformInfo.worldAABB = m_AABB;
	InverseTransformAABB( m_TransformInfo.worldAABB, t.GetPosition(), t.GetRotation(), m_TransformInfo.localAABB );
}

template<class TransferFunction> inline
void TrailRenderer::Transfer (TransferFunction& transfer) {
	Super::Transfer (transfer);
	transfer.Transfer (m_Time, "m_Time", kSimpleEditorMask);
	transfer.Transfer (m_LineParameters.startWidth, "m_StartWidth", kSimpleEditorMask);
	transfer.Transfer (m_LineParameters.endWidth, "m_EndWidth", kSimpleEditorMask);
	TRANSFER_SIMPLE (m_Colors);

	TRANSFER(m_MinVertexDistance);
	
	transfer.Transfer (m_Autodestruct, "m_Autodestruct");
	if (transfer.IsReading () && !m_Autodestruct)
		m_WasRendered = false;
}
