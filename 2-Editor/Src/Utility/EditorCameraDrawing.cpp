#include "UnityPrefix.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/CameraCullingParameters.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Camera/Culler.h"
#include "Runtime/Camera/SceneCulling.h"
#include "Runtime/Camera/ImageFilters.h"
#include "Editor/Src/OcclusionCulling.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Filters/Particles/ParticleRenderer.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/RenderLoops/RenderLoop.h"
#include "Runtime/Math/Color.h"
#include "Editor/Src/Application.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Camera/Renderable.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/AssetBundleUtility.h"
#include "Runtime/Camera/RenderLoops/ReplacementRenderLoop.h"
#include "Runtime/Camera/UnityScene.h"
#include "External/shaderlab/Library/intshader.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Camera/IntermediateRenderer.h"
#include "Editor/Src/OcclusionCullingVisualization.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Editor/Src/OcclusionCullingVisualizationState.h"
#include "Editor/Src/NavMesh/NavMeshVisualization.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Camera/ShadowCulling.h"
#include "Editor/Src/Utility/CustomLighting.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/Utility/DebugPrimitives.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Input/TimeManager.h"

#include <cmath>

#include "Runtime/Mono/MonoManager.h"

// Use lightmap resolution shader from project folder, for debugging
#define USE_LOCAL_LIGHTMAPRES_SHADER 0

#if USE_LOCAL_LIGHTMAPRES_SHADER
#include "Runtime/Shaders/ShaderNameRegistry.h"
#endif

extern ShaderLab::ShaderState* g_EditorPixelShaderOverride;
extern ShaderLab::ShaderState* g_EditorPixelShaderOverrideSM3;


#define TEMP_MATERIAL(getname,x) \
	static Material& getname() { static Material* gMaterial = NULL; if (gMaterial == NULL) { gMaterial = Material::CreateMaterial(x, Object::kHideAndDontSave); } return *gMaterial; }


TEMP_MATERIAL(GetViewColoredTextureMaterial,
	"Shader \"__EDITOR_VIEW_COLORED_TEXTURE\" {\n"
	"  Properties { \n"
	"    _Color (\"\", Color) = (1,1,1,1)\n"
	"    _MainTex (\"\", Any) = \"gray\" {}\n"
	"  }\n"
	"  SubShader {\n"
	"	 Tags { \"ForceSupported\" = \"True\" }\n"
	"    Pass {\n"
	"      SetTexture [_MainTex] { constantColor (1,1,1,0.5) combine constant lerp(constant) texture }\n" // lerp texture towards white
	"      SetTexture [_MainTex] { constantColor [_Color] combine previous * constant }\n" // multiply by color
	"    }\n"
	"  }\n"
	"  SubShader {\n"
	"    Pass {\n"
	"      SetTexture [_MainTex] { constantColor [_Color] combine texture * constant }\n" // multiply by color
	"    }\n"
	"  }\n"
	"}"
);

TEMP_MATERIAL(GetWireMaterial,
			  "Shader \"__EDITOR_WIRE\" { SubShader { Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Overlay\" } Fog { Mode Off } Lighting Off Blend SrcAlpha oneMinusSrcAlpha Cull Off ZTest Always ZWrite Off Color (.7,.7,.7,.9) Pass { } } } ");


class GrayscaleFilterEffect {
	static PPtr<Material> m_Material;
	float m_Fade;
public:
	GrayscaleFilterEffect(float fade)
	{
		m_Fade = fade;
		if (!m_Material.IsValid())
		{
			Material* m = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/SceneViewGrayscaleEffectFade.mat"));

			if (!m)
				m = GetEditorAssetBundle()->Get<Material> ("SceneView/SceneViewGrayscaleEffectFade.mat");
			
			if (m)
				m_Material = m;
		}
		if (!m_Material.IsValid())
			ErrorString("Couldn't find SceneViewGrayscaleEffectFade material!");
	}
	
	void RenderGrayscaleFade () 
	{
		RenderTexture* current = RenderTexture::GetActive();
		RenderTexture* temp = GetRenderBufferManager().GetTempBuffer (current->GetWidth (), current->GetHeight (), kDepthFormatNone, current->GetColorFormat(), 0, kRTReadWriteDefault);
		if( !temp )
		{
			ErrorString( "Error while generating temporary texture for scene view" );
			return;
		}

		ImageFilters::Blit (current, temp);
		RenderTexture::SetActive( current );

		m_Material->SetTexture (ShaderLab::Property("_MainTex"), temp);
		m_Material->SetFloat (ShaderLab::Property("_Fade"), m_Fade);
		m_Material->SetPass (0);
		DeviceMVPMatricesState preserveMVP;
		GfxDevice& device = GetGfxDevice();

		LoadFullScreenOrthoMatrix();
		device.ImmediateBegin( kPrimitiveQuads );
		device.ImmediateTexCoordAll(0.0f, 0.0f, 0.0f);
		device.ImmediateVertex(0.0f, 0.0f, 0.1f);
		device.ImmediateTexCoordAll(1.0f, 0.0f, 0.0f);
		device.ImmediateVertex(1.0f, 0.0f, 0.1f);
		device.ImmediateTexCoordAll(1.0f, 1.0f, 0.0f);
		device.ImmediateVertex(1.0f, 1.0f, 0.1f);
		device.ImmediateTexCoordAll(0.0f, 1.0f, 0.0f);
		device.ImmediateVertex(0.0f, 1.0f, 0.1f);
		device.ImmediateEnd();
		
		GetRenderBufferManager().ReleaseTempBuffer( temp );
	}
};

PPtr<Material> GrayscaleFilterEffect::m_Material;

ColorRGBAf g_EditorCameraWireframeColor = ColorRGBA32(0x00, 0x00, 0x00, 0x40);
ColorRGBAf g_EditorCameraOverlayColor = ColorRGBA32(0x00, 0x00, 0x00, 0x40);
ColorRGBAf g_EditorCameraActiveColor = ColorRGBA32(125, 176, 250, 95);
ColorRGBAf g_EditorCameraOtherSelectedColor = ColorRGBA32(94, 119, 155, 64);


static void BeginWireframe (const ColorRGBAf& col)
{
	GfxDevice& device = GetGfxDevice();

	device.SetAntiAliasFlag(true);
	device.SetWireframe (true);

	static PPtr<Shader> s_ShaderSceneWire = 0;
	if (!s_ShaderSceneWire)
		s_ShaderSceneWire = GetEditorAssetBundle()->Get<Shader>("SceneView/Wireframe.shader");
	if (s_ShaderSceneWire)
	{
		ShaderLab::SubShader& ss = s_ShaderSceneWire->GetShaderLabShader()->GetActiveSubShader();
		g_EditorPixelShaderOverride = &ss.GetPass(0)->GetState();
		if (ss.GetValidPassCount() >= 2)
			g_EditorPixelShaderOverrideSM3 = &ss.GetPass(1)->GetState();
	}
	ShaderLab::g_GlobalProperties->SetVector(ShaderLab::Property("unity_SceneViewWireColor"), col.GetPtr());
}

static void EndWireframe ()
{
	GfxDevice& device = GetGfxDevice();

	g_EditorPixelShaderOverride = NULL;
	g_EditorPixelShaderOverrideSM3 = NULL;
	device.SetAntiAliasFlag (false);
	device.SetWireframe (false);
}

static const ChannelAssigns* ApplyColoredTextureShader (const ColorRGBAf &col, Material* objectMaterial)
{
	SHADERPROP (Color);
	SHADERPROP (MainTex);
	Material& mat = GetViewColoredTextureMaterial();
	mat.SetColor(kSLPropColor, col);
	if (objectMaterial && objectMaterial->HasProperty (kSLPropMainTex)) {
		mat.SetTexture (kSLPropMainTex, objectMaterial->GetTexture (kSLPropMainTex));
	}
	const ChannelAssigns* channels = mat.SetPass(0);
	return channels;
}

static const ChannelAssigns* ApplyLightmapsResolutionShader (float UVMult, Material* objectMaterial)
{
	SHADERPROP (Checkerboard);
	SHADERPROP (UVMult);
	AssetBundle* editorAssetBundle = GetEditorAssetBundle();
	static Material* s_ShowLightmapMaterial = NULL;
	if (s_ShowLightmapMaterial == NULL)
	{
		#if USE_LOCAL_LIGHTMAPRES_SHADER
		Shader* shader = GetScriptMapper().FindShader ("Hidden/Show Lightmap Resolution");
		#else
		Shader* shader = editorAssetBundle->Get<Shader> ("SceneView/SceneViewShowLightmap.shader");
		#endif
		s_ShowLightmapMaterial = Material::CreateMaterial(*shader, Object::kHideAndDontSave);
		// try getting the texture from the assets folder (a hook for easy editing)
		Texture2D* checkerboardTex = dynamic_pptr_cast<Texture2D*>(GetMainAsset("Assets/Editor Default Resources/SceneView/Textures/lightmapChecker.psd"));
		// otherwise get the real thing from the editor resources
		if (checkerboardTex == NULL)
			checkerboardTex = editorAssetBundle->Get<Texture2D> ("SceneView/Textures/lightmapChecker.psd");
		s_ShowLightmapMaterial->SetTexture(kSLPropCheckerboard, checkerboardTex);
	}
	Vector2f UVScale(UVMult*0.5f, UVMult*0.5f);
	s_ShowLightmapMaterial->SetTextureScale(kSLPropCheckerboard, UVScale);
	const ChannelAssigns* channels = s_ShowLightmapMaterial->SetPass(0);
	return channels;
}

static void RenderSelectedObject (Renderer *r, const SceneCullingParameters& culling)
{
	if (r == NULL || !r->CanSelectedWireframeBeRendered () || r->IsDerivedFrom(ClassID (ParticleRenderer)) || r->IsDerivedFrom(ClassID (SpriteRenderer)) || r->GetGameObjectPtr () == GetActiveGO())
		return;

	// Check if the object is visible (LOD & PVS)
	SceneHandle handle = r->GetSceneHandle();
	if (handle == kInvalidSceneHandle || !IsNodeVisible (GetScene().GetRendererNode(handle), GetScene().GetRendererAABB(handle), culling))
		return;

	const TransformInfo& xformInfo = r->GetTransformInfo ();
	SetupObjectMatrix (xformInfo.worldMatrix, xformInfo.transformType);
	for (int m=0;m<r->GetMaterialCount ();m++)
	{
		Material* mat = r->GetMaterial(m);
		if (!mat)
			continue;
		const ChannelAssigns* channels = mat->SetPass(0);
		if (!channels)
			continue;
		r->Render (r->GetSubsetIndex(m), *channels);
	}
}

static void RenderSelectedObjectRecursive (GameObject& go, GameObject* activeGO, const std::set<GameObject*>& selection, const SceneCullingParameters& parameters)
{
	// Render active game objects
	// But don't render the active selected object, since that gets rendered with another shader
	if (go.IsActive() && activeGO != &go) 
	{
		RenderSelectedObject (go.QueryComponent (Renderer), parameters);
	}
	
	// Render all children
	Transform* transform = go.QueryComponent (Transform);
	if (transform != NULL)
	{
		for (Transform::iterator i = transform->begin(); i != transform->end(); i++)
		{
			GameObject& childGO = (**i).GetGameObject();
			
			// Only recurse into objects that are not part of the selection
			// since they will already be rendered the loop that iterates through the Selected objects and calls RenderObjectRecursive on each one
			if (selection.count(&childGO) == 0)
			{
				RenderSelectedObjectRecursive (childGO, activeGO, selection, parameters);
			}
		}
	}
}



/// * Renders the active Mesh in bold wireframe
/// * Renders all selected game objects and all their children in light wireframe.
/// (The active mesh is not drawn twice obviously)
/// (If you select a parent and a child the child also doesn't get twice obviously)
static void DoRenderSelected (const CameraCullingParameters& culling, RenderingPath renderPath)
{
	CullResults renderSelectedCullResults;
	Camera::PrepareSceneCullingParameters (culling, renderPath, renderSelectedCullResults);

	BeginWireframe (g_EditorCameraOtherSelectedColor);

	GfxDevice& device = GetGfxDevice();
	
	float matWorld[16];
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	
	std::set<GameObject *> selection = GetGameObjectSelection ();
	GameObject* activeGO = GetActiveGO();
	
	// Draw selected children objects except the active game object
	for (std::set<GameObject *>::iterator i = selection.begin(); i != selection.end(); i++)
	{
		GameObject* go = *i;
		RenderSelectedObjectRecursive (*go, activeGO, selection, renderSelectedCullResults.sceneCullParameters);
	}
	
	// Draw the active game object
	if (activeGO && activeGO->IsActive())
	{
		Renderer *r = activeGO->QueryComponent (Renderer);
		if (r && r->CanSelectedWireframeBeRendered ())
		{
			BeginWireframe (g_EditorCameraActiveColor);
			const TransformInfo& xformInfo = r->GetTransformInfo ();
			SetupObjectMatrix (xformInfo.worldMatrix, xformInfo.transformType);
			for (int m = 0; m < r->GetMaterialCount(); m++)
			{
				Material* mat = r->GetMaterial(m);
				if (!mat)
					continue;
				const ChannelAssigns* channels = mat->SetPass(0);
				if (!channels)
					continue;
				r->Render (r->GetSubsetIndex (m), *channels);
			}
		}
	}

	device.SetWorldMatrix(matWorld);

	EndWireframe ();
}

static void RenderSceneRenderPaths (Camera& camera, RenderLoop& loop, CullResults& contents)
{
	GfxDevice& device = GetGfxDevice();

	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	const RenderingPath cameraRP = camera.CalculateRenderingPath();

	VisibleNodes::iterator nodeIt, nodeEnd = contents.nodes.end();
	for (nodeIt = contents.nodes.begin(); nodeIt != nodeEnd; ++nodeIt)
	{
		VisibleNode& node = *nodeIt;
		BaseRenderer* r = node.renderer;

		SetupObjectMatrix (node.worldMatrix, node.transformType);

		int matCount = r->GetMaterialCount();
		for (int m = 0; m < matCount; ++m)
		{
			Material* mat = r->GetMaterial (m);
			ColorRGBAf color (0.5f,0.5f,0.5f,1.0f);
			if (mat)
			{
				const ShaderLab::IntShader* shader = mat->GetShader()->GetShaderLabShader();
				if (cameraRP == kRenderPathPrePass && shader->GetDefaultSubshaderIndex (kRenderPathExtPrePass) == 0)
					color.Set (0.5f, 1.0f, 0.5f, 1.0f);
				else if ((cameraRP == kRenderPathPrePass || cameraRP == kRenderPathForward) && shader->GetDefaultSubshaderIndex (kRenderPathExtForward) == 0)
					color.Set (1.0f, 1.0f, 0.5f, 1.0f);
				else
					color.Set (1.0f, 0.5f, 0.5f, 1.0f);
			}
			
			const ChannelAssigns* channels = ApplyColoredTextureShader (color, mat);
			r->Render (r->GetSubsetIndex(m), *channels);
		}
	}

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

static void RenderSceneLightmapResolution (Camera& camera, RenderLoop& loop, CullResults& contents)
{
	// render the scene view normally
	DoRenderLoop (loop, camera.CalculateRenderingPath(), contents, false);
	CleanupAfterRenderLoop (loop);

	// then render all objects with the 'Show Lightmap Resolution' shader, alpha-blended
	GfxDevice& device = GetGfxDevice();

	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);

	LightmapEditorSettings& les = GetLightmapEditorSettings();
	float lightmapResolution = les.GetResolution();
	int maxLightmapSize = std::min(les.GetTextureWidth(), les.GetTextureHeight());

	VisibleNodes::iterator nodeIt, nodeEnd = contents.nodes.end();
	for (nodeIt = contents.nodes.begin(); nodeIt != nodeEnd; ++nodeIt)
	{
		VisibleNode& node = *nodeIt;
		BaseRenderer* r = node.renderer;
		MeshRenderer* mr = dynamic_cast<MeshRenderer*>(r);
		SkinnedMeshRenderer* smr = NULL;
		IntermediateRenderer* ir = NULL;

		if (!mr)
			smr = dynamic_cast<SkinnedMeshRenderer*>(r);
		if (!mr && !smr)
			ir = dynamic_cast<IntermediateRenderer*>(r);

		// care only about mesh renderers, skinned mesh renderers and terrains
		if (!mr && !smr && !ir)
			continue;

		// ignore non-static objects
		if (mr && !mr->GetGameObject().AreStaticEditorFlagsSet (kLightmapStatic))
			continue;
		if (smr && !smr->GetGameObject().AreStaticEditorFlagsSet (kLightmapStatic))
			continue;
		// TODO: non-static terrains (ir) won't be ignored, but maybe that's ok

		float uvMult = 1.0f;
		if (mr || smr)
		{
			float cachedSurfaceArea = mr ? mr->GetCachedSurfaceArea () : smr->GetCachedSurfaceArea ();
			uvMult = std::min(sqrtf (cachedSurfaceArea) * lightmapResolution * r->GetScaleInLightmap (), (float)maxLightmapSize);
		}
		else if(ir)
		{
			// terrain sets scaleInLightmap on its patches to its lightmapSize value,
			// other intermediate renderers are left with the default -1.0f value
			uvMult = ir->GetScaleInLightmap ();
			if (uvMult < 0)
				continue;
		}

		SetupObjectMatrix (node.worldMatrix, node.transformType);

		int matCount = r->GetMaterialCount ();
		for (int m = 0; m < matCount; ++m)
		{
			Material* mat = r->GetMaterial (m);
			
			const ChannelAssigns* channels = ApplyLightmapsResolutionShader (uvMult, mat);
			r->Render (r->GetSubsetIndex (m), *channels);
		}
	}

	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

//------------------------------------------------------------------------------ 
// grid drawing

// TODO: different cpp?

struct DrawGridParameters
{
	Vector3f   pivot;
	ColorRGBAf color;
	float	 size;
	float	 alphaX;
	float	 alphaY;
	float	 alphaZ;
};

struct
GridPart
{
	int   planeIndex; // x, y, z - 0, 1, 2
	float extents[4];
	float step;
	float alpha;
};


void PrepareGridPart( const Camera* camera, const DrawGridParameters* gridParam, const int planeIndex[3], GridPart* gridPart )
{
	AssertIf(camera==0);
	AssertIf(planeIndex[0]<0 || planeIndex[0]>2);
	AssertIf(planeIndex[1]<0 || planeIndex[1]>2);
	AssertIf(planeIndex[2]<0 || planeIndex[2]>2);
	AssertIf(gridPart==0);

	gridPart->planeIndex = planeIndex[0];

	// Grid size is based on distance to scene view pivot AND
	// camera's distance to the grid plane.
	
	const int   kGridCells = 50;
	const float kMinGridSize = 1.0f;
	const float kMaxGridSize = 1000.0f;

	const float kGridDistanceFactor		= 0.35f;
	const float kPivotDistanceInfluence = 0.4f; // pivot distance 40%, distance to grid 60%
	
	Vector3f dir = camera->GetComponent (Transform).TransformDirection (Vector3f(0,0,1));

	float step = gridParam->size;
	if (!camera->GetOrthographic())
	{
		Vector3f pos			= camera->GetPosition();
		float    pivotDistance	= Magnitude(pos - gridParam->pivot);
		
		step  = Abs(pos[planeIndex[0]]) * (1-kPivotDistanceInfluence) + pivotDistance * kPivotDistanceInfluence;
		step *= kGridDistanceFactor;
	}

	if( step < kMinGridSize )	step = kMinGridSize;
	if( step > kMaxGridSize )	step = kMaxGridSize;


	// constrain grid sizes on 10^N boundaries
	// take fractional portion of log10 as alpha
	float gridSizeLog	= std::log10(step);
	float alpha			= std::ceil(gridSizeLog) - gridSizeLog;
	
	step = Pow(10.0f, std::floor(gridSizeLog));

	// center grid onto editor's pivot
	Vector3f pos = gridParam->pivot;
	
	pos[planeIndex[1]] = std::floor (pos[planeIndex[1]] / step) * step;
	pos[planeIndex[2]] = std::floor (pos[planeIndex[2]] / step) * step;
	gridPart->extents[0] = pos[planeIndex[1]] - kGridCells * step;
	gridPart->extents[1] = pos[planeIndex[1]] + kGridCells * step;
	gridPart->extents[2] = pos[planeIndex[2]] - kGridCells * step;
	gridPart->extents[3] = pos[planeIndex[2]] + kGridCells * step;
	
	// At grazing angles, fade out the minor lines more so that it does not appear too dense.
	const float kGrazingAngle = 15.0f;

	float angleToGrid = Rad2Deg( std::asin ( Abs(dir[planeIndex[0]]) ) );
	if (angleToGrid < kGrazingAngle)
	{
		float a = angleToGrid/kGrazingAngle;
		if(a < 0.3f) a = 0.3f;
		alpha *= a;
	}
	
	gridPart->alpha = alpha;	
	gridPart->step  = step;
}

void DrawLines( const int coord[2], const float extents1[2], const float extents2[2], Vector3f offset, float step,
				const ColorRGBAf& colorMinor, const ColorRGBAf& colorMajor
			  )
{
	GetGfxDevice().ImmediateColor(colorMinor.r, colorMinor.g, colorMinor.b, colorMinor.a);
		
	Vector3f vertex(0, 0, 0);

	int majorCounter = ((int)(std::floor(extents1[0] / step))) % 10;
	for (float g = extents1[0]; g < extents1[1]; g += step)
	{
		vertex[coord[0]] = g; 

		if(majorCounter == 0) // major line - set major color
			GetGfxDevice().ImmediateColor(colorMajor.r, colorMajor.g, colorMajor.b, colorMajor.a);

		vertex[coord[1]] = extents2[0];
		GetGfxDevice().ImmediateVertex(vertex.x + offset.x, vertex.y + offset.y, vertex.z + offset.z);
		vertex[coord[1]] = extents2[1];
		GetGfxDevice().ImmediateVertex(vertex.x + offset.x, vertex.y + offset.y, vertex.z + offset.z);
		
		if(majorCounter == 0) // major line - restore minor color
			GetGfxDevice().ImmediateColor(colorMinor.r, colorMinor.g, colorMinor.b, colorMinor.a);

		if(++majorCounter == 10)
			majorCounter = 0;
	}
}



void DoDrawGrid( const Camera* camera, const DrawGridParameters* gridParam, const int planeIndex[3], float totalAlpha )
{
	AssertIf(camera==0);
	AssertIf(planeIndex[0]<0 || planeIndex[0]>2);
	AssertIf(planeIndex[1]<0 || planeIndex[1]>2);
	AssertIf(planeIndex[2]<0 || planeIndex[2]>2);


	// when camera is too far away, do not even draw the grid
	if( Abs( camera->GetPosition()[planeIndex[0]] ) > camera->GetFar() )
		return;

	// offset grid slightly to avoid z-fighting with objects placed at zero
	// while this is not perfect solution - now it is more difficult to "find" position to get z-fighting
	// Offset along the camera forward direction to minimize error and artefacts.
	
	static const float kGridOffsetValue = -10.0f / 1000.0f;
	Vector3f dir = camera->GetComponent (Transform).TransformDirection (Vector3f(0,0,1));
	Vector3f gridOffset = dir * kGridOffsetValue * Abs(camera->GetPosition()[planeIndex[0]]);
	

	GridPart part;
	PrepareGridPart(camera, gridParam, planeIndex, &part);
	
	ColorRGBAf colorMajor =  gridParam->color;
	colorMajor.a *= totalAlpha;
	
	ColorRGBAf colorMinor =  gridParam->color;
	colorMinor.a *= (part.alpha*totalAlpha);
	
	static PPtr<Material> s_GridMaterial = 0;
	if( !s_GridMaterial )
	{
		s_GridMaterial = dynamic_pptr_cast<Material*>(GetMainAsset ("Assets/Editor Default Resources/SceneView/Grid.mat"));

		if( !s_GridMaterial )
			s_GridMaterial = GetEditorAssetBundle ()->Get<Material> ("SceneView/Grid.mat");

		if( !s_GridMaterial )
			return;
	}
	
	
	
	for( int passI = 0 ; passI < s_GridMaterial->GetPassCount () ; ++passI )
	{
		s_GridMaterial->SetPass(passI);
		GetGfxDevice().ImmediateBegin(kPrimitiveLines);
		
		{
			int   coords[] = {planeIndex[1], planeIndex[2]};
			float ext1[]   = {part.extents[0], part.extents[1]};
			float ext2[]   = {part.extents[2], part.extents[3]};

			DrawLines(coords, ext1, ext2, gridOffset, part.step, colorMinor, colorMajor);
		}
		
		{
			int   coords[] = {planeIndex[2], planeIndex[1]};
			float ext1[]   = {part.extents[2], part.extents[3]};
			float ext2[]   = {part.extents[0], part.extents[1]};

			DrawLines(coords, ext1, ext2, gridOffset, part.step, colorMinor, colorMajor);
		}
		
		GetGfxDevice().ImmediateEnd();
	}
		
}

void DrawGrid( const Camera* camera, const DrawGridParameters* gridParam )
{
	AssertIf(camera==0);

	const int planeIndexX[3] = { 0, 1, 2 };
	const int planeIndexY[3] = { 1, 0, 2 };
	const int planeIndexZ[3] = { 2, 0, 1 };
	
	GetGfxDevice().SetAntiAliasFlag(true);
	
	if (gridParam->alphaX > 0.0F)
		DoDrawGrid(camera, gridParam, planeIndexX, gridParam->alphaX);
	if (gridParam->alphaY > 0.0F)
		DoDrawGrid(camera, gridParam, planeIndexY, gridParam->alphaY);
	if (gridParam->alphaZ > 0.0F)
		DoDrawGrid(camera, gridParam, planeIndexZ, gridParam->alphaZ);
		
	GetGfxDevice().SetAntiAliasFlag(false);
}



//------------------------------------------------------------------------------ 


void Camera::ClearEditorCamera()
{
	CameraRenderOldState state;
	if (m_TargetTexture.IsValid())
		StoreRenderState(state);

	m_IsSceneCamera = true;
	
	GetRenderManager().SetCurrentCamera (this);
	WindowSizeHasChanged ();
	SetupRender (kRenderFlagSetRenderTarget | kRenderFlagPrepareImageFilters);
	
	// Skybox rendering in ortho mode does not quite work.
	// So when clearing we always setup perspective, and restore it afterwards.
	bool ortho = m_Orthographic;
	float nearClip = m_NearClip;
	float farClip = m_FarClip;
	m_Orthographic = false;
	m_NearClip = 1.0f;
	m_FarClip = 1000.0f;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
	
	// If we have a skybox that uses a material we need to set 
	// the correct sRGB flags for the clear.
	bool needResetSRGB = false;
	if (GetClearFlags() == kSkybox && GetSkyboxMaterial())
	{
		RenderTexture* rt = RenderTexture::GetActive();
		if(rt)
			GetGfxDevice().SetSRGBWrite(rt->GetSRGBReadWrite());
		else
			GetGfxDevice().SetSRGBWrite(GetActiveColorSpace ()==kLinearColorSpace);
		needResetSRGB = true;
	}

	Clear();
	
	m_Orthographic = ortho;
	m_NearClip = nearClip;
	m_FarClip = farClip;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;

	// Setup the right matrices/props now. Something (e.g. grid drawing) might depend on them.
	GfxDevice& device = GetGfxDevice();
	device.SetProjectionMatrix (GetProjectionMatrix());
	device.SetViewMatrix( GetWorldToCameraMatrix().GetPtr() );
	
	SetCameraShaderProps();

	if (needResetSRGB)
		GetGfxDevice().SetSRGBWrite(false);

	if (m_TargetTexture.IsValid())
		RestoreRenderState(state);
}

float GetGameViewAspectRatio (Camera& camera)
{
	ScriptingInvocation invocation(MONO_COMMON.getGameViewAspectRatio);
	invocation.AddObject(Scripting::ScriptingWrapperFor(&camera));
	ScriptingObject* result = invocation.Invoke();
	if (result == NULL)
		return -1.0F;
	
	return ExtractMonoObjectData<float> (result);
}

void Camera::RenderEditorCamera (EditorDrawingMode drawMode, const DrawGridParameters* gridParam)
{
	if (!IsValidToRender())
		return;

	if (GetAnimateMaterials())
		SetAnimateMaterialsTime(GetTimeSinceStartup());

	GetApplication().PerformDrawHouseKeeping();
	RenderManager::UpdateAllRenderers();	
	
	if (m_TargetTexture.IsValid())
		StoreRenderState(m_OldCameraState);

	GetRenderManager().SetCurrentCamera (this);
	WindowSizeHasChanged ();
	
	GetScene().BeginCameraRender();
	
	// Cull
	Camera* cullingCamera = this;
	bool occlusionCull = false;

	float customAspect = -1.0F;
	if (GetOcclusionCullingVisualization()->GetShowOcclusionCulling()
		&& !GetOcclusionCullingVisualization()->GetShowPreVis())
	{
		Camera* occlusionCullingCamera = FindPreviewOcclusionCamera();
		if (occlusionCullingCamera)
		{
			cullingCamera = occlusionCullingCamera;
			occlusionCull = GetOcclusionCullingVisualization()->GetShowGeometryCulling();
			
			customAspect = GetGameViewAspectRatio(*cullingCamera);
		}
	}

	// Setup custom aspect ratio based on GameView
	CameraTemporarySettings oldSettings;
	cullingCamera->GetTemporarySettings (oldSettings);
	if (customAspect > 0.0F)
		cullingCamera->SetAspect (customAspect);
	
	CullResults cullResults;
	CameraCullingParameters cullingParams (*cullingCamera, kCullFlagNeedsLighting | kCullFlagForceEvenIfCameraIsNotActive);
	if (occlusionCull)
		cullingParams.cullFlag |= kCullFlagOcclusionCull;

	CustomCull(cullingParams, cullResults);

	cullingCamera->SetTemporarySettings (oldSettings);
	
	// Update depth textures if needed
	UpdateDepthTextures (cullResults);
		
	SetupRender (kRenderFlagSetRenderTarget);

	GfxDevice& device = GetGfxDevice();
	
	// we don't want to clear again
	Camera::ClearMode oldClearFlags = GetClearFlags();
	SetClearFlags( kDontClear );

	switch (drawMode)
	{
	case kEditorDrawTextured:
		DoRender( cullResults, NULL, 0 );
		DoRenderSelected(cullingParams, GetRenderingPath());
		break;
	case kEditorDrawWire:
		BeginWireframe (g_EditorCameraWireframeColor);
		DoRender (cullResults, NULL, 0);
		EndWireframe ();
		DoRenderSelected (cullingParams, GetRenderingPath());
		break;
	case kEditorDrawTexturedWire:
		DoRender (cullResults, NULL, 0);
		BeginWireframe (g_EditorCameraWireframeColor);
		// entering render loop for 2nd time; cleanup any previous stuff
		CleanupAfterRenderLoop (*m_RenderLoop);
		DoRender (cullResults, NULL, 0);
		EndWireframe ();
		DoRenderSelected(cullingParams, GetRenderingPath());
		break;
	case kEditorDrawRenderPaths:
		DoRender( cullResults, RenderSceneRenderPaths, 0 );
		DoRenderSelected(cullingParams, GetRenderingPath());
		break;
	case kEditorDrawLightmapResolution:
		DoRender( cullResults, RenderSceneLightmapResolution, 0 );
		DoRenderSelected(cullingParams, GetRenderingPath());
		break;
	default:
		AssertString ("unknown scene camera draw mode");
	}

	GetWireMaterial().SetPass(0);

	if( gridParam )
		DrawGrid(this, gridParam);
	
	DebugPrimitives::Get()->Draw(this);
	
	// restore clear flags
	SetClearFlags( oldClearFlags );	

	device.SetProjectionMatrix (GetProjectionMatrix());

	// TODO
	DoRenderPostLayers();

	GetScene().EndCameraRender();
}

void Camera::FinishRenderingEditorCamera ()
{
	GetGfxDevice().SetProjectionMatrix (GetProjectionMatrix());
	GetGfxDevice().SetViewMatrix (GetWorldToCameraMatrix().GetPtr());

	GizmoManager::Get ().DrawAllGizmos ();

	RenderImageFilters (*m_RenderLoop, m_TargetTexture, false);

	DoRenderGUILayer();

	// Clear camera's intermediate renderers here
	ClearIntermediateRenderers();

	// Cleanup after all rendering
	CleanupAfterRenderLoop (*m_RenderLoop);

	CleanupDepthTextures ();
	
	if (m_TargetTexture.IsValid())
		RestoreRenderState(m_OldCameraState);
}

void Camera::RenderEditorCameraFade (float fade)
{
	CameraRenderOldState state;
	StoreRenderState(state);

	// if camera's viewport rect is empty or invalid, do nothing
	if( !IsValidToRender() )
		return;
		
	GetRenderManager ().SetCurrentCamera (this);
	
	SetupRender (m_TargetTexture.IsValid()?kRenderFlagSetRenderTarget:0);
	
	int viewcoord[4];
	RectfToViewport( Rectf (0, 0, m_CurrentTargetTexture->GetWidth(), m_CurrentTargetTexture->GetHeight()), viewcoord );
	GetGfxDevice().SetViewport( viewcoord[0], viewcoord[1], viewcoord[2], viewcoord[3] );

	GrayscaleFilterEffect* fx = NULL;
	if (fade > 0 && m_TargetTexture.IsValid())
	{
		fx = new GrayscaleFilterEffect(fade);
	}

	DoRenderPostLayers ();		// Handle any post-layer

	if (fx != NULL)
	{
		fx->RenderGrayscaleFade ();
		delete fx;
	}

	RestoreRenderState(state);
}

bool Camera::ShouldShowChannelErrors (const Camera* ptr)
{
	if (ptr == NULL)
		return true;

	// We use shader replacement with normals when drawing outlines of objects in the search view.
	// Use this to suppress error messages when meshes don't have normals.
	return !(ptr->m_IsSceneCamera && ptr->m_ReplacementShader.IsValid()); 
}

void Camera::SetAnimateMaterials (bool animate)
{
	m_AnimateMaterials = animate;
}

void Camera::SetAnimateMaterialsTime (float time)
{
	m_AnimateMaterialsTime = time;
}
