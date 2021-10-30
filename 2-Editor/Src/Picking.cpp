#include "UnityPrefix.h"
#include "Picking.h"

#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Camera/Culler.h"
#include "Runtime/Camera/CullResults.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/CameraCullingParameters.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Filters/Particles/ParticleRenderer.h"
#include "Runtime/Filters/Particles/ParticleEmitter.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemRenderer.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Camera/CameraUtil.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Runtime/Graphics/SpriteUtility.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Camera/IntermediateRenderer.h"
#include <limits>
#include <algorithm>
#include "Runtime/Graphics/ImageConversion.h"
#include "Runtime/Interfaces/ITerrainManager.h"
#include "Runtime/Misc/AssetBundle.h"
#include "shaderlab.h"

// Things can't be picked below this alpha value
static float s_SelectionAlphaCutoutValue = 0.01f;
static const Camera* s_CurrentCamera = NULL;
static void RectfToViewPort (const Rectf& r, int* viewPort);
static PPtr<Shader> s_AlphaBasedSelectionShader = 0;
const char *gIDShader = 
"Shader \"__EDITOR_SELECTION\" { \n"
"  SubShader { \n"
"		Tags { \"ForceSupported\" = \"True\" }\n "
"  		Fog { Mode Off } \n"
"  		Pass { } \n"
"  } \n"
"} \n";


Material* GetPickMaterial()
{
	static Material *s_mat = NULL;
	if (s_mat == NULL)
	{
		s_mat = Material::CreateMaterial (gIDShader, Object::kHideAndDontSave);
	}
	return s_mat;
}

Material *GetAlphaPickMaterial() 
{
	static Material *s_matAlpha = NULL;
	if (s_matAlpha == NULL)
	{
		Shader* s_AlphaBasedSelectionShader = GetEditorAssetBundle()->Get<Shader>("SceneView/AlphaBasedSelection.shader");
		s_matAlpha = Material::CreateMaterial(*s_AlphaBasedSelectionShader, Object::kHideAndDontSave);
	}
	return s_matAlpha;
}

static void CullForPicking (Camera& camera, VisibleNodes& visible)
{
	CameraCullingParameters parameters (camera, kCullFlagForceEvenIfCameraIsNotActive);
	CullResults cullResults;
	camera.CustomCull(parameters, cullResults);
	visible = cullResults.nodes;
}


static bool ShouldSkipRendererForPicking(Renderer* r)
{
	if (r->TestHideFlag (Object::kHideInHierarchy))
		return true;
	// skip in locked layers
	if (g_LockedPickingLayers & (1<<r->GetLayer()))
		return true;
	// skip in locked sorting layers
	int layerIndex = GetSortingLayerIndexFromValue(r->GetSortingLayer());
	if (GetSortingLayerLocked(layerIndex))
		return true;

	return false;
}


class SelectionRenderQueue
{
public:
	SelectionRenderQueue( UInt32 layerMask, VisibleNodes& inNodes ): m_Contents (inNodes) { m_LayerMask = layerMask; }
	
	static void EncodeIndex( UInt32 index, UInt8 color[4] )
	{
		// output is R,G,B,A bytes; index is ABGR dword
		color[0] = index & 0xFF;
		color[1] = (index >> 8) & 0xFF;
		color[2] = (index >> 16) & 0xFF;
		color[3] = (index >> 24) & 0xFF;
	} 
	
	static UInt32 DecodeIndex( UInt32 argb )
	{
		// input is color from ARGB32 format pixel; index is ABGR dword
		
		#if UNITY_LITTLE_ENDIAN
		// ARGB32 in memory on little endian looks like BGRA in dword
		return ((argb >> 8) & 0xFFFFFF) | ((argb & 0xFF) << 24);
		#else
		// ARGB32 in memory on bit endian looks like ARGB in dword
		return (argb & 0xFF00FF00) | ((argb&0xFF)<<16) | ((argb>>16)&0xFF);
		#endif
	}
	
	Object* GetObject( UInt32 color )
	{
		UInt32 index = DecodeIndex (color);
		if (index == 0)
			return NULL;
		index--;
		int gizmoIndex = index - m_Contents.size ();
		if (index < m_Contents.size ())
		{
			BaseRenderer* r = m_Contents[index].renderer;
			switch (r->GetRendererType())
			{
			case kRendererIntermediate:
				{
					IntermediateRenderer* ir = static_cast<IntermediateRenderer*>(r);
					if (ir->GetInstanceID())
						return PPtr<Object> (ir->GetInstanceID());
					return NULL;
				}
			default:
				return static_cast<Renderer*>( r );
			}
		}
		else if (gizmoIndex < m_Gizmos.size ())
			return PPtr<Object> (m_Gizmos[gizmoIndex]);
		else
			return NULL;
	}
	
	static void BeforePickGizmoFunctor( void* userData, const Object* object )
	{
		Assert(userData && object);
		SelectionRenderQueue* self = reinterpret_cast<SelectionRenderQueue*> (userData);
		
		int gizmoIndex = self->m_Gizmos.size () + self->m_Contents.size ();
		int instanceID = object->GetInstanceID();
		self->m_Gizmos.push_back (instanceID);
		UInt8 encoded[4];
		EncodeIndex( gizmoIndex + 1, encoded );
		gizmos::g_GizmoPickID = ColorRGBA32(encoded[0], encoded[1], encoded[2], encoded[3]);
		self->m_Gizmos.push_back (instanceID);
	}

	void Render( Material& pickingMaterial, Material& alphaPickingMaterial, float alphaCutoff, VisibleNodes& contents )
	{
		GfxDevice& device = GetGfxDevice();
		
		// render objects for picking
		for( int i=0; i < contents.size(); ++i )
		{
			
			VisibleNode& node = contents[i];
			BaseRenderer* baseRenderer = node.renderer;
			switch (baseRenderer->GetRendererType())
			{
			case kRendererIntermediate:
				{
					IntermediateRenderer* renderer = static_cast<IntermediateRenderer*>(baseRenderer);
					Assert (renderer);
					if (renderer->GetInstanceID() == 0)
						continue;
				}
				break;
			default:
				{
					Renderer* renderer = static_cast<Renderer*>( baseRenderer );
					Assert (renderer);
					if (ShouldSkipRendererForPicking (renderer))
						continue;
				}
				break;
			}
						
			float matWorld[16], matView[16];
			CopyMatrix(device.GetViewMatrix(), matView);
			CopyMatrix(device.GetWorldMatrix(), matWorld);
			
			SetupObjectMatrix (node.worldMatrix, node.transformType);
			UInt8 encoded[4];
			EncodeIndex( i + 1, encoded );
			
			ShaderLab::PropertySheet *props = ShaderLab::g_GlobalProperties;

			// uniforms for alphaPickingMaterial (AlphaBasedSelection.shader)
			props->SetVector (ShaderLab::Property("_SelectionID"), ByteToNormalized(encoded[0]), ByteToNormalized(encoded[1]), ByteToNormalized(encoded[2]), ByteToNormalized(encoded[3]));
			props->SetFloat (ShaderLab::Property("_SelectionAlphaCutoff"), alphaCutoff);
			
			const int rendertypeTagID = ShaderLab::GetShaderTagID("RenderType");

			for (int m=0; m<baseRenderer->GetMaterialCount (); m++)
			{
				Material* baseMaterial = baseRenderer->GetMaterial(m);

				bool useAlpha = true;
				if (!baseMaterial)
					useAlpha = false;

				int subshaderTypeID = -1;
				if (baseMaterial)
				{
					Shader* shader = baseMaterial->GetShader();
					if (shader)
						subshaderTypeID = shader->GetShaderLabShader()->GetTag (rendertypeTagID, true);
				}
				if (subshaderTypeID < 0)
					useAlpha = false;
				
				int usedSubshaderIndex = alphaPickingMaterial.GetShader()->GetSubShaderWithTagValue (rendertypeTagID, subshaderTypeID);
				if(usedSubshaderIndex < 0)
					useAlpha = false;

				if (baseMaterial && !baseMaterial->HasProperty(ShaderLab::Property("_MainTex")))
					useAlpha = false;

				// We'll want raster state from original shader, to get the correct culling mode
				const DeviceRasterState* origRasterState = NULL;
				if (baseMaterial && baseMaterial->GetShader())
					origRasterState = baseMaterial->GetShader()->GetShaderLabShader()->GetActiveSubShader().GetPass(0)->GetState().GetRasterState();

				// If original material has _MainTex and RenderType is Transparent or TransparentCutout, we use alphaPickingMaterial (AlphaBasedSelection.shader)
				if (useAlpha)
				{
					DebugAssert(baseMaterial);
					int usedSubshaderIndex = alphaPickingMaterial.GetShader()->GetSubShaderWithTagValue (rendertypeTagID, subshaderTypeID);
					baseRenderer->ApplyCustomProperties(alphaPickingMaterial, alphaPickingMaterial.GetShader(), usedSubshaderIndex);
					ShaderLab::SubShader& subshader = alphaPickingMaterial.GetShader()->GetShaderLabShader()->GetSubShader (usedSubshaderIndex);
					int shaderPassCount = subshader.GetValidPassCount();
					for (int p = 0; p < shaderPassCount; ++p)
					{
						const ChannelAssigns* channel = baseMaterial->SetPassWithShader(p, alphaPickingMaterial.GetShader(), usedSubshaderIndex);
						if (channel)
						{
							if (origRasterState)
								device.SetRasterState (origRasterState);
							baseRenderer->Render( baseRenderer->GetSubsetIndex(m), *channel );
						}
					}
				} 
				else
				{
					const ChannelAssigns* channel = pickingMaterial.SetPass(0);
					device.SetColorBytes( encoded );
					if (channel)
					{
						if (origRasterState)
							device.SetRasterState (origRasterState);
						baseRenderer->Render (baseRenderer->GetSubsetIndex(m), *channel);
					}
				}
			}

			device.SetViewMatrix(matView);
			device.SetWorldMatrix(matWorld);
		}

		device.SetNormalizationBackface( kNormalizationDisabled, false );
		pickingMaterial.SetPass(0);

		// render gizmos for picking
		GizmoManager::Get().DrawAllGizmosWithFunctor( BeforePickGizmoFunctor, this );
	}
	
	typedef std::vector<int> Gizmos;
  	Gizmos m_Gizmos;
	VisibleNodes& m_Contents;
  	UInt32 m_LayerMask;
};


Object* PickClosestObject (Camera& cam, UInt32 layers, const Vector2f& pos)
{
	// Find all objects at pickray
	SelectionList selection;
	PickObjects (cam, layers, pos, Vector2f (4, 4), &selection);
	
	// Find object with largest score of all returned objects (most pixels in picking area)
	int bestScore = -1;
	Object* closestObject = NULL;
	for (SelectionList::iterator i= selection.begin ();i != selection.end ();i++)
	{
		if( i->score > bestScore )
		{
			bestScore = i->score;
			closestObject = i->object;
		}
	}
	
	if (closestObject == NULL)
		return NULL;

	return closestObject;
}



GameObject* PickClosestGO (Camera& cam, UInt32 layers, const Vector2f& pos)
{
	Object* object = PickClosestObject (cam, layers, pos);
	if (object == NULL)
		return NULL;
	if (object->IsDerivedFrom (ClassID (GameObject)))
		return static_cast<GameObject*> (object);
	else if (object->IsDerivedFrom (ClassID (Component)))
		return static_cast<Unity::Component*> (object)->GetGameObjectPtr ();
	else
		return NULL;
}

inline float MultiplyPointZ (const Matrix4x4f& m, const Vector3f& v)
{
	return m.m_Data[2] * v.x + m.m_Data[6] * v.y + m.m_Data[10] * v.z + m.m_Data[14];
}

int EvaluateObjectDepth(VisibleNode& node)
{
	int outDistanceForSort;

	Assert(s_CurrentCamera);

	Vector3f center = node.worldAABB.GetCenter();
	if (s_CurrentCamera)
	{
		if (s_CurrentCamera->GetOrthographic())
		{
			const float d = MultiplyPointZ (s_CurrentCamera->GetProjectionMatrix(), center);
			outDistanceForSort = d;
		}
		else
		{
			center -= s_CurrentCamera->GetPosition();
			outDistanceForSort = -SqrMagnitude(center);
		}
	}

	return outDistanceForSort;
}

int Sort(VisibleNode lhs, VisibleNode rhs)
{
	bool result;
	if (!CompareGlobalLayeringData(lhs.renderer->GetGlobalLayeringData(), rhs.renderer->GetGlobalLayeringData(), result))
	{
		result = EvaluateObjectDepth(lhs) < EvaluateObjectDepth(rhs);
	}

	return result;
}

void SortVisibleNodes(VisibleNodes& visible, const Camera& camera)
{
	s_CurrentCamera = &camera;
	std::sort(visible.begin(), visible.end(), Sort);
	s_CurrentCamera = NULL;
}

int PickObjects (Camera& cam, UInt32 layers, const Vector2f& pos, const Vector2f& size, SelectionList* selection)
{
	gizmos::g_GizmoPicking = true;
	
	//@TODO: Try removing this...
	Camera* oldCurrentCamera = GetRenderManager ().GetCurrentCameraPtr ();
	GetRenderManager().SetCurrentCamera (&cam);
	
	RenderManager::UpdateAllRenderers();

	size_t originalIntermRendererSize = cam.GetIntermediateRenderers().GetRendererCount();

	VisibleNodes visible;
	CullForPicking(cam, visible);

	SortVisibleNodes(visible, cam);

	SelectionRenderQueue renderqueue (layers, visible);
	
	GfxDevice& device = GetGfxDevice();

	// Make sure we are in  
	RenderTexture::SetActive (NULL);

	// Setup viewport
	int viewPort[4];
	RectfToViewPort (cam.GetScreenViewportRect (), viewPort);
	
	int minx = RoundfToInt (pos.x - size.x / 2.0F);
	int miny = RoundfToInt (pos.y - size.y / 2.0F);
	int maxx = RoundfToInt (pos.x + size.x / 2.0F);
	int maxy = RoundfToInt (pos.y + size.y / 2.0F);
	minx = std::max( minx, viewPort[0] );
	miny = std::max( miny, viewPort[1] );
	maxx = std::min( maxx, viewPort[0]+viewPort[2]-1 );
	maxy = std::min( maxy, viewPort[1]+viewPort[3]-1 );
	int xSize = maxx - minx;
	int ySize = maxy - miny;

	if( xSize >= 1 && ySize >= 1 )
	{
		FlipScreenRectIfNeeded( device, viewPort );
		device.SetViewport( viewPort[0], viewPort[1], viewPort[2], viewPort[3] );	
		
		// Setup pickmatrix
		device.SetProjectionMatrix (cam.GetProjectionMatrix());
		device.SetViewMatrix( cam.GetWorldToCameraMatrix().GetPtr() );
		
		int oldScissorRect[4];
		device.GetScissorRect( oldScissorRect );
		bool oldScissor = device.IsScissorEnabled();
		
		int scissorRect[4] = { minx, miny, xSize, ySize };
		FlipScreenRectIfNeeded( device, scissorRect );
		device.SetScissorRect( scissorRect[0], scissorRect[1], scissorRect[2], scissorRect[3] );

		// Clear buffer
		device.Clear (kGfxClearAll, ColorRGBAf(0,0,0,0).GetPtr(), 1.0f, 0);

		Assert(device.IsInsideFrame());
			
		// Render
		renderqueue.Render( *GetPickMaterial(), *GetAlphaPickMaterial(), s_SelectionAlphaCutoutValue, renderqueue.m_Contents );

		device.FinishRendering();

		if (oldScissor)
		{
			device.SetScissorRect( oldScissorRect[0], oldScissorRect[1], oldScissorRect[2], oldScissorRect[3] );
		}
		else
		{
			device.DisableScissor();
		}
		
		
		Image image( xSize, ySize, kTexFormatARGB32 );
		bool readOk = device.ReadbackImage( image, minx, miny, xSize, ySize, 0, 0 );
		Assert(readOk);
		
		const UInt32* pixel = (const UInt32*)image.GetImageData();
		for( int i = 0; i < xSize * ySize; ++i )
		{
			SelectionEntry entry;
			entry.score = 1;
			entry.object = renderqueue.GetObject( pixel[i] );
						
			if (entry.object)
			{
				Unity::Component *component = dynamic_pptr_cast<Unity::Component*> (entry.object);
				if (component)
				{
					GameObject *go = component->GetGameObjectPtr();
					while (go->IsMarkedVisible() == GameObject::kVisibleAsChild)
					{
						Transform *parent = go->GetComponent(Transform).GetParent();
						if (parent)
						{
							go = parent->GetGameObjectPtr();
							entry.object = go;
						}
						else
							break;
					}
				}

				SelectionList::iterator found = selection->insert (entry).first;
				SelectionEntry& foundEntry = const_cast<SelectionEntry&> (*found);
				++foundEntry.score;
			}
		}
	}

	cam.ClearIntermediateRenderers (originalIntermRendererSize);
	
	gizmos::g_GizmoPicking = false;
	
	GetRenderManager ().SetCurrentCamera (oldCurrentCamera);
	
	return selection->size ();
}

static void RectfToViewPort (const Rectf& r, int* viewPort)
{
	viewPort[0] = RoundfToInt (r.x);
	viewPort[1] = RoundfToInt (r.y);
	viewPort[2] = RoundfToInt (r.Width ());
	viewPort[3] = RoundfToInt (r.Height ());
}


bool BoundsInRect (const AABB& bounds, const Rectf& rect, const Matrix4x4f& objectMatrix )
{
    Vector3f min = bounds.GetMin();
    Vector3f max = bounds.GetMax();
    Vector3f p;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(min.x, min.y, min.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(max.x, min.y, min.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(min.x, max.y, min.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(max.x, max.y, min.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(min.x, min.y, max.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(max.x, min.y, max.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(min.x, max.y, max.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    objectMatrix.PerspectiveMultiplyPoint3(Vector3f(max.x, max.y, max.z), p);
    if (!rect.Contains(p.x, p.y))
        return false;
    return true;
}

struct UserData
{
    std::set<GameObject*>* list;
    Camera *camera;
    Rectf *viewRect;
};

static void CheckGizmosInRect( void* userData, const Object* object )
{
    UserData *data = (UserData*)userData;
    GameObject *go = dynamic_pptr_cast<GameObject*> (object);
    
	if (!go)
    {
        Unity::Component *comp = dynamic_pptr_cast<Unity::Component*> (object);
        if (comp)
            go = &comp->GetGameObject();
    }

    if (go)
    {
		if (!go->IsMarkedVisible())
			return;

        Vector3f p =data->camera->WorldToViewportPoint(go->GetComponent(Transform).GetPosition());
        if ((p.z >= 0 || data->camera->GetOrthographic ()) && data->viewRect->Contains(p.x, p.y))
        {
            if (data->list->find (go) == data->list->end())
            data->list->insert (go);
        }
    }
}

void PickRectObjects (Camera& cam, const Rectf& rect, std::set<GameObject*>* result, bool selectPrefabParentsOnly)
{
	VisibleNodes visible;
	CullForPicking(cam, visible);
	SortVisibleNodes(visible, cam);
	
    Matrix4x4f worldToView;
	MultiplyMatrices4x4 (&cam.GetProjectionMatrix(), &cam.GetWorldToCameraMatrix(), &worldToView);
    Rectf viewRect = rect;
    viewRect.y = 1 - viewRect.y - viewRect.height;
    Rectf modRect = rect;
    modRect.x = modRect.x * 2.0f - 1.0f;
    modRect.y = -(modRect.y * 2.0f - 1.0f);
    modRect.width *=2.0f;
    modRect.height*=2.0f;
    modRect.y -= modRect.height;
    
    std::set<GameObject*>* selection;
    if (selectPrefabParentsOnly) {
        selection = new std::set<GameObject*> ();
    } else {
        selection = result;
    }
    
    for (VisibleNodes::iterator i = visible.begin(); i != visible.end(); i++)
    {
        BaseRenderer* br = i->renderer;
        if (br->GetRendererType() == kRendererIntermediate)
            continue;
        Renderer* r = static_cast<Renderer*>(br);
		if (ShouldSkipRendererForPicking (r))
			continue;

		GameObject *go = r->GetGameObjectPtr();
		if (!go->IsMarkedVisible())
			continue;

		while (go->IsMarkedVisible() == GameObject::kVisibleAsChild)
		{
			Transform *parent = go->GetComponent(Transform).GetParent();
			if (parent)
				go = parent->GetGameObjectPtr();
			else
				break;
		}
        
        // select meshes
        MeshFilter* mf = r->QueryComponent(MeshFilter);        
        if (mf)
        {
            Mesh* mesh = mf->GetSharedMesh();
            if (mesh)
            {
                AABB bounds = mesh->GetBounds();
                Matrix4x4f objectMatrix;
				Matrix4x4f localToWorldMatrix = r->GetComponent(Transform).GetLocalToWorldMatrix();
				MultiplyMatrices4x4 (&worldToView, &localToWorldMatrix, &objectMatrix);
                if (BoundsInRect(bounds, modRect, objectMatrix))
                    selection->insert(go);
				else 
				{
					// If the object is in our list, we check against the verts (it has already be broadphase-culled)
					bool found = true;
					for (StrideIterator<Vector3f> i = mesh->GetVertexBegin (); i != mesh->GetVertexEnd (); i++)
					{
						Vector3f v;
						objectMatrix.PerspectiveMultiplyPoint3 (*i, v);
						if (!modRect.Contains (v.x, v.y))
						{
							found = false;
							break;
						}
					}
					if (found)
						selection->insert(go);
				}
                continue;
            }
        }
        // select misc renderers
		const bool isParticleRenderer = (r->QueryComponent(ParticleRenderer) || r->QueryComponent(ParticleSystemRenderer));
        if (!isParticleRenderer && BoundsInRect(i->worldAABB, modRect, worldToView))
        {
            selection->insert(go);
            continue;
        }
    }
    // select particles
    vector<SInt32> objects;
	Object::FindAllDerivedObjects (ClassID(ParticleRenderer), &objects);
    
    for (int i=0;i<objects.size ();i++)
	{
		ParticleRenderer& object = *PPtr<ParticleRenderer> (objects[i]);
        GameObject* go = object.GetGameObjectPtr();
		if (object.TestHideFlag (Object::kDontSave) || ( object.IsPersistent() || !go->IsActive()) || !object.GetEnabled())
            continue;

		if (!go->IsMarkedVisible())
			continue;
			
		while (go->IsMarkedVisible() == GameObject::kVisibleAsChild)
		{
			Transform *parent = go->GetComponent(Transform).GetParent();
			if (parent)
				go = parent->GetGameObjectPtr();
			else
				break;
		}

        ParticleEmitter *emit = object.QueryComponent(ParticleEmitter);
        if (emit && emit->IsEmitting())
        {
            Vector3f p = cam.WorldToViewportPoint(emit->GetComponent(Transform).GetPosition());
            if (p.z >= 0 && viewRect.Contains(p.x, p.y))
                selection->insert (go);
        }
    }
    
    UserData data;
    data.camera = &cam;
    data.list = selection;
    data.viewRect = &viewRect;
    GizmoManager::Get().CallFunctorForGizmos ( CheckGizmosInRect, &data );
	
    for (std::set<GameObject*>::iterator i = selection->begin(); i != selection->end(); i++)
    {
		result->insert (FindPrefabRoot (*i));
    }
}

struct ObjectWithDistance 
{
	float distance;
	GameObject *go;
	
	ObjectWithDistance (float dist, GameObject *_go)
	{
		distance = dist;
		go = _go;
	}
};

static bool CompareObjectDistances (const ObjectWithDistance &v1, const ObjectWithDistance &v2) 
{
	return (v1.distance < v2.distance);
}

static bool AddToSortedObjectsIfCanFindVerticesOnRenderer (Transform* t, Renderer* r, std::set<Transform*>* ignoreObjects, Ray& cameraRay, std::vector<ObjectWithDistance>& sortedObjects)
{
	MeshFilter* mf = r->QueryComponent(MeshFilter);        
	if (!mf || !mf->GetSharedMesh())
	{	
#if ENABLE_SPRITES
		SpriteRenderer* sr = r->QueryComponent(SpriteRenderer);
		if (!sr || !sr->GetSprite())
#endif
			return false;
	}

	if (ignoreObjects && ignoreObjects->find(t) != ignoreObjects->end())
		return false;

	AABB worldAABB;
	r->GetWorldAABB (worldAABB);
	sortedObjects.push_back(ObjectWithDistance (cameraRay.SqrDistToPoint(worldAABB.GetCenter()) - SqrMagnitude(worldAABB.GetExtent ()), &r->GetGameObject()));

	return true;
}

static void ProcessVertexForPicking (const Vector3f& worldV, const Vector2f& screenPoint, Camera& camera, float& minDist, bool& found, Vector3f *point)
{
	Vector3f p = camera.WorldToScreenPoint(worldV);
	// skip points behind the camera
	if (p.z > 0.0f)
	{
		float dist = SqrMagnitude(Vector2f (p.x, p.y) - screenPoint);
		if (dist < minDist) 
		{
			minDist = dist;
			*point = worldV;
			found = true;
		}
	}
}

bool FindNearestVertex (std::vector<Transform*> *objectsToSearchIn, std::set<Transform*> *ignoreObjects, Vector2f screenPoint, Camera &camera, Vector3f *point)
{
	std::vector<ObjectWithDistance> sortedObjects;
	
	Ray cameraRay = camera.ScreenPointToRay (screenPoint);
	if (objectsToSearchIn == NULL)
	{
		VisibleNodes visible;
		CullForPicking(camera, visible);
		
		for (VisibleNodes::iterator i = visible.begin(); i != visible.end(); i++)
		{
			BaseRenderer* br = i->renderer;
			if (br->GetRendererType() == kRendererIntermediate)
				continue;
			Renderer* r = static_cast<Renderer*>(br);
			AddToSortedObjectsIfCanFindVerticesOnRenderer(&r->GetComponent (Transform), r, ignoreObjects, cameraRay, sortedObjects);
	  }
	} else {
		// should this only be for objects in the selection AFTER a proper camera culling?
		for (std::vector<Transform*>::iterator i = objectsToSearchIn->begin(); i != objectsToSearchIn->end(); i++)
		{	
			Renderer* r = (*i)->QueryComponent (Renderer);			
			if (!r)
				continue;
			AddToSortedObjectsIfCanFindVerticesOnRenderer(*i, r, ignoreObjects, cameraRay, sortedObjects);
		}	
	}
	
	std::sort (sortedObjects.begin(), sortedObjects.end(), CompareObjectDistances);
		
	float minDist = std::numeric_limits<float>::infinity ();
	bool found = false;

	for (vector<ObjectWithDistance>::iterator i = sortedObjects.begin (); i != sortedObjects.end(); i++)
	{
		Matrix4x4f localToWorldMatrix = i->go->GetComponent(Transform).GetLocalToWorldMatrix();

		MeshFilter* mf = i->go->QueryComponent(MeshFilter);
#if ENABLE_SPRITES
		if (mf)
		{
#endif
			Mesh* mesh = mf->GetSharedMesh ();
	
			// go over all mesh points
			for (StrideIterator<Vector3f> v = mesh->GetVertexBegin (), end = mesh->GetVertexEnd (); v != end; ++v)
			{
				Vector3f worldV = localToWorldMatrix.MultiplyPoint3(*v);
				ProcessVertexForPicking (worldV, screenPoint, camera, minDist, found, point);
			}
#if ENABLE_SPRITES
		}
		else
		{
			// If there is no MeshFilter, then there must be a SpriteRenderer
			SpriteRenderer* sr = i->go->QueryComponent(SpriteRenderer);
			AABB localAABB;
			sr->GetLocalAABB(localAABB);

			// Go over 
			Vector3f v[4];
			GetAABBVerticesForSprite(localAABB, v);
			for (int i = 0; i < 4; ++i)
			{
				Vector3f worldV = localToWorldMatrix.MultiplyPoint3(v[i]);
				ProcessVertexForPicking (worldV, screenPoint, camera, minDist, found, point);
			}
		}
#endif
	}
	
	return found;
}
