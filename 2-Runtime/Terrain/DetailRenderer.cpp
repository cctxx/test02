#include "UnityPrefix.h"
#include "DetailRenderer.h"

#if ENABLE_TERRAIN

#include "Runtime/Input/TimeManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Camera/IntermediateRenderer.h"

namespace DetailRenderer_Static
{
static SHADERPROP(Cutoff);
static SHADERPROP(MainTex);
static SHADERPROP(maxDistanceSqr);
static SHADERPROP(CameraPosition);
static SHADERPROP(WaveAndDistance);
static SHADERPROP(WavingTint);
static SHADERPROP(CameraRight);
static SHADERPROP(CameraUp);
} // namespace SplatMaterials_Static

DetailRenderer::DetailRenderer (PPtr<TerrainData> terrain, Vector3f position, int lightmapIndex)
{
	using namespace DetailRenderer_Static;

	m_Database = terrain;
	m_Position = position;
	m_LightmapIndex = lightmapIndex;
		
	const char* shaders[] = {
		"Hidden/TerrainEngine/Details/BillboardWavingDoublePass",
		"Hidden/TerrainEngine/Details/Vertexlit",
		"Hidden/TerrainEngine/Details/WavingDoublePass"
	};

	ScriptMapper& sm = GetScriptMapper ();
	bool shaderNotFound = false;
	for (int i = 0; i < kDetailRenderModeCount; i++)
	{
		Shader* shader = sm.FindShader(shaders[i]);
		if (shader == NULL)
		{
			shaderNotFound = true;
			shader = sm.FindShader("Diffuse");
		}

		m_Materials[i] = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
		m_Materials[i]->SetFloat (kSLPropCutoff, .5f * .75f);
	}

	if (shaderNotFound)
	{
		ErrorString("Unable to find shaders used for the terrain engine. Please include Nature/Terrain/Diffuse shader in Graphics settings.");
	}
	
	m_RenderCount = 0;
	m_LastTime = 0;
}

DetailPatchRender& DetailRenderer::GrabCachedPatch (int x, int y, int lightmapIndex, DetailRenderMode mode, float density)
{
	DetailList &patches = m_Patches[mode];
	UInt32 index = x + y*m_Database->GetDetailDatabase().GetPatchCount();

	DetailPatchRender& render = patches[index];
	if(!render.inited)
	{
		render.mesh = m_Database->GetDetailDatabase().BuildMesh(x, y, m_TerrainSize, lightmapIndex, mode, density);
		render.isMeshNull = render.mesh == NULL;
		render.x = x;
		render.y = y;
		render.inited = true;
	}
	render.lastUsed = m_RenderCount;
	return render;
}
	
void DetailRenderer::Render (Camera* camera, float viewDistance, int layer, float detailDensity)
{
	using namespace DetailRenderer_Static;

	detailDensity = clamp01(detailDensity);

	DetailDatabase& detail = m_Database->GetDetailDatabase();
	m_LastTime += (IsWorldPlaying() ? GetDeltaTime() * detail.GetWavingGrassStrength() * .05f : 0);
	m_RenderCount++;
	

	m_TerrainSize = m_Database->GetHeightmap().GetSize();
	int patchCount = detail.GetPatchCount();
	if (patchCount == 0)
		return;
	
	detail.UpdateDetailPrototypesIfDirty();
	
	for (int i=0;i<kDetailRenderModeCount;i++)
	{
		if (m_Materials[i]->HasProperty(kSLPropMainTex))
			m_Materials[i]->SetTexture (kSLPropMainTex, detail.GetAtlasTexture());
	}

	Transform* camT = camera->QueryComponent (Transform);

	Vector3f position = camT->GetPosition() - m_Position;
	int centerX = RoundfToInt(position.x * patchCount / m_TerrainSize.x);
	int centerY = RoundfToInt(position.z * patchCount / m_TerrainSize.z);
	
	int halfWidth = int(Ceilf(patchCount * viewDistance / m_TerrainSize.x) + 1);
	int halfHeight = int(Ceilf(patchCount * viewDistance / m_TerrainSize.z) + 1);
	
	int minx = centerX - halfWidth;
	if(minx < 0) minx = 0;
	if(minx > patchCount - 1) minx = patchCount - 1;
	
	int miny = centerY - halfHeight;
	if(miny < 0) miny = 0;
	if(miny > patchCount - 1) miny = patchCount - 1;

	int maxx = centerX + halfWidth;
	if(maxx < 0) maxx = 0;
	if(maxx > patchCount - 1) maxx = patchCount - 1;

	int maxy = centerY + halfHeight;
	if(maxy < 0) maxy = 0;
	if(maxy > patchCount - 1) maxy = patchCount - 1;
	
	float sqrViewDistance = viewDistance * viewDistance;
	
	Plane planes[6];
	ExtractProjectionPlanes( camera->GetWorldToClipMatrix(), planes);
	
//	DetailList *newPatches = new DetailList[3];
	int totalVisible = 0;
	int total = 0;
	
	bool supportsBillboards = m_Materials[0]->GetShader()->IsSupported();
	
	// Find and cull all visible patches
	for (int y=miny;y<=maxy;y++)
	{
		for (int x=minx;x<=maxx;x++)
		{
			if (detail.IsPatchEmpty (x,y))
				continue;
			
			for (int i=0;i<kDetailRenderModeCount;i++)
			{
				// Skip billboard rendering if not supported
				if( i == 0 && !supportsBillboards )
					continue;
					
				// Grab the cached patch
				DetailPatchRender &render = GrabCachedPatch (x, y, m_LightmapIndex, (DetailRenderMode)i, detailDensity);

				if (render.isMeshNull)
				{
					render.isCulledVisible = false;
				}
				else
				{
					AABB bounds = render.mesh->GetBounds();

					render.isCulledVisible = true;
					if (CalculateSqrDistance(position,bounds) > sqrViewDistance)
					{
						render.isCulledVisible = false;	
					}
					else
					{
						bounds.GetCenter() += m_Position;
						if (!IntersectAABBFrustumFull(bounds, planes))
							render.isCulledVisible = false;	
					}
					
					if(render.isCulledVisible)
						totalVisible ++;
				}
				total++;
			}
		}
	}
	
	Vector3f up = camT->InverseTransformDirection (Vector3f(0.0f,1.0f,0.0f));
	up.z = 0;
	up = camT->TransformDirection(up);
	up = NormalizeSafe(up);
	
	MaterialPropertyBlock props;
		
	Vector3f right = Cross (camT->TransformDirection(Vector3f(0.0f,0.0f,-1.0f)), up);
	right = NormalizeSafe(right);

	Matrix4x4f matrix;
	matrix.SetTranslate( m_Position );

	for (int r=0;r<kDetailRenderModeCount;r++)
	{
		Material *material = m_Materials[r];
			
		props.Clear();
		props.AddPropertyFloat(kSLPropmaxDistanceSqr, sqrViewDistance);
		Vector4f prop;
		prop[0] = position.x;
		prop[1] = position.y;
		prop[2] = position.z;
		prop[3] = 1.0f / sqrViewDistance;
		props.AddPropertyVector(kSLPropCameraPosition, prop);
		prop[0] = m_LastTime;
		prop[1] = detail.GetWavingGrassSpeed() * .4f;
		// cancel wind on mesh lit
		prop[2] = r == kDetailMeshLit ? 0 : detail.GetWavingGrassAmount() * 6.0f;
		prop[3] = sqrViewDistance;		
		props.AddPropertyVector(kSLPropWaveAndDistance, prop);
		ColorRGBAf color = detail.GetWavingGrassTint();
		props.AddPropertyColor(kSLPropWavingTint, color);
		prop[0] = right.x;
		prop[1] = right.y;
		prop[2] = right.z;
		prop[3] = 0.0f;
		props.AddPropertyVector(kSLPropCameraRight, prop);
		prop[0] = up.x;
		prop[1] = up.y;
		prop[2] = up.z;
		prop[3] = 0.0f;		
		props.AddPropertyVector(kSLPropCameraUp, prop);			


		DetailList &curPatches = m_Patches[r];
		for (DetailList::iterator i = curPatches.begin(); i != curPatches.end(); i++)
		{
			if (i->second.isCulledVisible)
			{				
				IntermediateRenderer* r = AddMeshIntermediateRenderer( matrix, i->second.mesh, material, layer, false, true, 0, camera );		
				r->SetPropertyBlock( props );
				r->SetLightmapIndexIntNoDirty(m_LightmapIndex);
			}
		}
	}

	for (int r=0;r<kDetailRenderModeCount;r++)
	{
		DetailList &curPatches = m_Patches[r];
		DetailList::iterator next;
		for (DetailList::iterator render = curPatches.begin(); render != curPatches.end(); render=next)
		{
			next = render;
			next++;
			if(render->second.lastUsed < m_RenderCount)
				curPatches.erase(render);
		}
	}
}
		
/// Cleanup all the cached render patches
void DetailRenderer::Cleanup ()
{
	for(int i=0;i<kDetailRenderModeCount;i++)
	{
		DestroySingleObject (m_Materials[i]);
		DetailList &curPatches = m_Patches[i];

		for (DetailList::iterator render = curPatches.begin(); render != curPatches.end(); ++render)
		{
			render->second.inited = false;
			DestroySingleObject(render->second.mesh);
			render->second.mesh = NULL;
		}
	}
}
	
void DetailRenderer::ReloadAllDetails ()
{
	for (int i=0;i<kDetailRenderModeCount;i++)
	{
		m_Patches[i].clear();
	}
}
	
void DetailRenderer::ReloadDirtyDetails ()
{
	DetailDatabase& detail = m_Database->GetDetailDatabase();
	for (int i=0;i<kDetailRenderModeCount;i++)
	{
		DetailList &curPatches = m_Patches[i];
		DetailList::iterator next;
		for (DetailList::iterator render = curPatches.begin(); render != curPatches.end(); render=next)
		{
			next = render;
			next++;
			if (detail.IsPatchDirty (render->second.x, render->second.y))
				curPatches.erase(render);
		}
	}
}

#endif // ENABLE_TERRAIN
