#include "UnityPrefix.h"
#include "LineRenderer.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"

IMPLEMENT_CLASS_INIT_ONLY (LineRenderer)
IMPLEMENT_OBJECT_SERIALIZE (LineRenderer)

LineRenderer::LineRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererLine, label, mode)
{
	SetVisible (false);
}

LineRenderer::~LineRenderer ()
{
}

void LineRenderer::InitializeClass ()
{
	RegisterAllowNameConversion (LineRenderer::GetClassStringStatic(), "m_WorldSpace", "m_UseWorldSpace");
}


void LineRenderer::SetVertexCount(int count)
{
	if(count < 0) 
	{
		count = 0;
		ErrorString ("LineRenderer.SetVertexCount: Vertex count can't be set to negative value!");
	}
	UpdateManagerState( true );
	m_Positions.resize(count);
	SetVisible (m_Positions.size() >= 2);
	SetDirty();
	BoundsChanged();
}

void LineRenderer::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	SetVisible (m_Positions.size() >= 2);
	
	if ((awakeMode & kDidLoadFromDisk) == 0)
		BoundsChanged();
}

void LineRenderer::SetPosition (int index, const Vector3f& position)
{
	SetDirty();
	UpdateManagerState( true );
	if (index < m_Positions.size() && index >= 0)
		m_Positions[index] = position;
	else
		ErrorString("LineRenderer.SetPosition index out of bounds!");
	BoundsChanged();
}

PROFILER_INFORMATION(gSubmitVBOProfileLine, "Mesh.SubmitVBO", kProfilerRender)


void LineRenderer::Render (int subsetIndex, const ChannelAssigns& channels)
{
	if( m_Positions.size() < 2 )
		return;
	
	Vector3f* lineInVerts = NULL;
	ALLOC_TEMP(lineInVerts, Vector3f, m_Positions.size());
	
	MinMaxAABB mmAABB = MinMaxAABB(Vector3f::zero, Vector3f::zero);
	
	if (m_UseWorldSpace)
	{
		memcpy (lineInVerts, &m_Positions[0], m_Positions.size()*sizeof(Vector3f));
	}
	else
	{
		Transform& tc = GetComponent(Transform);
		int idx = 0;
		for (PositionVector::iterator j = m_Positions.begin(); j != m_Positions.end(); ++j, ++idx)
		{
			lineInVerts[idx] = tc.TransformPoint(*j);
		}
	}
	
	// Get VBO chunk
	int stripCount = m_Positions.size() * 2;
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
	m_Parameters.outVertices = vbPtr;
	m_Parameters.outAABB = &mmAABB;
	m_Parameters.cameraTransform = GetCurrentCamera().GetWorldToCameraMatrix();	
	Build3DLine (&m_Parameters, lineInVerts, m_Positions.size());
	
	vbo.ReleaseChunk( stripCount, 0 );
	
	// We can't set the view matrix since that breaks shadow maps (case 490315)
	// Set the world matrix instead so it cancels out the usual view matrix
	// i.e. it transforms from camera space back to world space
	device.SetWorldMatrix(GetCurrentCamera().GetCameraToWorldMatrix().GetPtr());
	
	if (m_CustomProperties)
		device.SetMaterialProperties (*m_CustomProperties);
	
	PROFILER_BEGIN(gSubmitVBOProfileLine, this)
	vbo.DrawChunk (channels);
	GPU_TIMESTAMP();
	PROFILER_END
}

void LineRenderer::UpdateTransformInfo ()
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
		MinMaxAABB minmax;
		minmax.Init();
		for (PositionVector::const_iterator i = m_Positions.begin(), itEnd = m_Positions.end(); i != itEnd; ++i)
			minmax.Encapsulate (*i);

		if (m_UseWorldSpace)
		{
			m_TransformInfo.worldAABB = minmax;
			TransformAABB (m_TransformInfo.worldAABB, transform.GetWorldToLocalMatrix(), m_TransformInfo.localAABB);
		}
		else
		{
			m_TransformInfo.localAABB = minmax;
			TransformAABB (m_TransformInfo.localAABB, transform.GetLocalToWorldMatrix(), m_TransformInfo.worldAABB);
		}
	}
}



void LineRenderer::UpdateRenderer()
{
	Super::UpdateRenderer();
	if (m_BoundsDirty)
	{
		BoundsChanged();
	}
}

void LineRenderer::Reset()
{
	Super::Reset();
	m_UseWorldSpace = true;
	m_Positions.clear();
	m_Positions.push_back (Vector3f (0,0,0));
	m_Positions.push_back (Vector3f (0,0,1));
	m_Parameters.color1 = ColorRGBA32(255, 255, 255, 255);
	m_Parameters.color2 = ColorRGBA32(255, 255, 255, 255);
	SetVisible (true);
}

void LineRenderer::SetUseWorldSpace (bool space)
{
	m_UseWorldSpace = space; 
	SetDirty();
	UpdateManagerState( true );
	BoundsChanged();
}

template<class TransferFunction> inline
void LineRenderer::Transfer (TransferFunction& transfer) {
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_Positions);
	TRANSFER_SIMPLE (m_Parameters);
	TRANSFER (m_UseWorldSpace);
}
