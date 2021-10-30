#include "UnityPrefix.h"
#include "GizmoRenderer.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Graphics/GeneratedTextures.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include "Runtime/Camera/RenderLayers/GUIText.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/IMGUI/GUIContent.h"

#include "Editor/Src/Utility/ObjectImages.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AnnotationManager.h"
#include "Editor/Src/EditorResources.h"
#include "Editor/Src/Picking.h"

#include <vector>
#include <math.h>
#include "Runtime/Profiler/Profiler.h"
#include "External/shaderlab/Library/intshader.h"

#define BLEND_OCCLUDED_GIZMOS 1


#define DEBUG_GIZMO_STATS 0

struct TextureVertex {
	Vector3f vertex;
	ColorRGBA32 color;
	Vector2f uv;
};

enum GizmoRenderMode {
	kGizmoRenderWire = 0,
	kGizmoRenderWireNoDepthTest,
	kGizmoRenderColor,
	kGizmoRenderLit,
	kGizmoRenderTexture, // only used by icons
	kGizmoRenderColorOcclusion,
	kGizmoRenderText, // only used by labeled icons
	kGizmoRenderModeCount
};

static size_t kRenderModeStrides[kGizmoRenderModeCount] = {
	sizeof(Vector3f),
	sizeof(Vector3f),
	sizeof(gizmos::ColorVertex),
	sizeof(gizmos::LitVertex),
	sizeof(TextureVertex),
	sizeof(gizmos::ColorVertex),
	sizeof(TextureVertex)
};
static UInt32 kRenderModeFormats[kGizmoRenderModeCount] = {
	(1<<kShaderChannelVertex),
	(1<<kShaderChannelVertex),
	(1<<kShaderChannelVertex) | (1<<kShaderChannelColor),
	(1<<kShaderChannelVertex) | (1<<kShaderChannelNormal),
	(1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor),
	(1<<kShaderChannelVertex) | (1<<kShaderChannelColor),
	(1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor)
};


// ----------------------------------------------------------------------
// gizmo materials
static GUIStyle* s_LabelIconGUIStyle = NULL;
static PPtr<Texture2D> s_InspectorLabelIcons [8];
static PPtr<Texture2D> s_SceneViewLabelIcons [8];

static Material* s_GizmoMaterials[kGizmoRenderModeCount];

static const char* kGizmoShaders[kGizmoRenderModeCount] = {
	// simple wire
	"Shader \"Hidden/Editor Gizmo\" {\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Cull Off Fog { Mode Off }\n"
	"	Offset -1, -1\n"
	"   Pass {\n"
	"		ZTest LEqual\n"
	"       SetTexture [_MainTex] { combine primary }\n"
	"   }\n"
	#if BLEND_OCCLUDED_GIZMOS
	"   Pass {\n"
	"		ZTest Greater\n"
	"		SetTexture [_MainTex] { constantColor(1,1,1,0.1) combine constant * primary }\n"
	"   }\n"
	#endif
	"}\n"
	"}",
	// simple wire no depth test
	"Shader \"Hidden/Editor Gizmo\" {\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Cull Off Fog { Mode Off }\n"
	"	Offset -1, -1\n"
	"   Pass {\n"
	"		ZTest Always\n"
	"       SetTexture [_MainTex] { combine primary }\n"
	"   }\n"
	#if BLEND_OCCLUDED_GIZMOS
	"   Pass {\n"
	"		ZTest Never\n"
	"   }\n"
	#endif
	"}\n"
	"}",
	// color
	"Shader \"Hidden/Editor Gizmo Color\" {\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Cull Off Fog { Mode Off }\n"
	"	Offset -1, -1\n"
	"	BindChannels {\n"
	"		Bind \"Vertex\", vertex\n"
	"		Bind \"Color\", color\n"
	"	}\n"
	"   Pass {\n"
	"		ZTest LEqual\n"
	"       SetTexture [_MainTex] { combine primary }\n"
	"   }\n"
	#if BLEND_OCCLUDED_GIZMOS
	"   Pass {\n"
	"		ZTest Greater\n"
	"		SetTexture [_MainTex] { constantColor(1,1,1,0.1) combine constant * primary }\n"
	"   }\n"
	#endif
	"}\n"
	"}",
	// lit
	"Shader \"Hidden/Editor Gizmo Lit\" {\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Fog { Mode Off }\n"
	"	Lighting On\n"
	"	Material { Diffuse(1,1,1,1) Ambient(1,1,1,1) }\n"
	"   Pass {\n"
	"		ZTest LEqual\n"
	"       SetTexture [_MainTex] { combine primary }\n"
	"   }\n"
	#if BLEND_OCCLUDED_GIZMOS
	"   Pass {\n"
	"		ZTest Greater\n"
	"		SetTexture [_MainTex] { constantColor(1,1,1,0.1) combine constant * primary }\n"
	"   }\n"
	#endif
	"}\n"
	"}",
	// textured
	"Shader \"Hidden/Editor Gizmo Textured\" {\n"
	"Properties {\n"
	"	_MainTex (\"\", 2D) = \"white\" {}\n"
	"}\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Fog { Mode Off } Cull Off\n"
	"	Pass {\n"
	"		BindChannels {\n"
	"			Bind \"Vertex\", vertex\n"
	"			Bind \"texcoord\", texcoord\n"
	"			Bind \"Color\", color\n"
	"		}\n"
	"		ZTest LEqual\n"
	"		SetTexture [_MainTex] { combine texture * primary }\n"
	"	}\n"
	#if BLEND_OCCLUDED_GIZMOS
	"	Pass {\n"
	"		ZTest Greater\n"
	"		BindChannels {\n"
	"			Bind \"Vertex\", vertex\n"
	"			Bind \"texcoord\", texcoord\n"
	"			Bind \"Color\", color\n"
	"		}\n"
	"		SetTexture [_MainTex] { combine texture * primary }\n"
	"	}\n"
	#endif
	"}\n"
	"}",
	// color, with occlusion
	"Shader \"Hidden/Editor Gizmo Color Occlusion\" {\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Cull Off Fog { Mode Off }\n"
	"	ZTest LEqual\n"
	"	BindChannels {\n"
	"		Bind \"Vertex\", vertex\n"
	"		Bind \"Color\", color\n"
	"	}\n"
	"	Pass { }\n"
	"	Pass { }\n"
	"}\n"
	"}",
	// text
	"Shader \"Hidden/Editor Gizmo Text\" {\n"
	"Properties {\n"
	"	_MainTex (\"\", 2D) = \"white\" {}\n"
	"}\n"
	"SubShader {\n"
	"	Tags { \"ForceSupported\" = \"True\" \"Queue\" = \"Transparent\" }\n"
	"	Blend SrcAlpha OneMinusSrcAlpha\n"
	"	ZWrite Off Fog { Mode Off } Cull Off\n"
	"	Pass {\n"
	"		ZTest LEqual\n"
	"		SetTexture [_MainTex] { combine texture alpha * primary }\n"
	"	}\n"
	#if BLEND_OCCLUDED_GIZMOS
	"	Pass {\n"
	"		ZTest Greater\n"
	"		SetTexture [_MainTex] { combine texture alpha * primary }\n"
	"	}\n"
	#endif
	"}\n"
	"}"
};


const char *iconPicking = 
"Shader \"__EDITOR_SELECTION\" { \n"
"	SubShader { \n"
"		Tags { \"ForceSupported\" = \"True\" }\n "
"		Fog { Mode Off } \n"
"		Pass {\n"
"			ZTest LEqual\n"
"			ZWrite On\n"
"		}\n"
"		Pass {\n"
"			ZTest Greater\n"
"			ZWrite Off\n"
"		}\n"	
"	} \n"
"} \n";

static GUIStyle* CreateLabelIconGUIStyle ()
{
	GUIStyle* style = new GUIStyle ();
	style->m_Normal.textColor = ColorRGBAf (0.0f, 0.0f, 0.0f, 1.0f);//ColorRGBAf (0.0f, 0.0f, 0.0f, 1.0f);
	style->m_OnNormal.textColor = ColorRGBAf (1.0f, 1.0f, 1.0f, 1.0f);
	style->m_Padding.left = 7;
	style->m_Padding.right = 9;
	style->m_Padding.top = 2;
	style->m_Padding.bottom = 2;
	style->m_Border.left = 8;
	style->m_Border.right = 8;
	const Font *font = GetEditorAssetBundle()->Get<Font> (AppendPathName(EditorResources::kFontsPath, "Lucida Grande small.ttf"));

	if (font)
		style->m_Font = font;
	style->m_FontSize = 0;
	style->m_FontStyle = 0;
	style->m_RichText = false;
	style->m_Alignment = kLowerCenter;
	style->m_Clipping = kOverflow;
	style->m_FixedHeight = 14;
	style->m_FixedWidth = 0;
	style->m_ImagePosition = kTextOnly;
	style->m_WordWrap = false;
	style->m_ContentOffset = Vector2f(0,0);
	style->m_ClipOffset = Vector2f(0,0);
	return style;
}


static Texture2D* GetLabelTexture (const std::string& path)
{
	// We support using our editor project: first try to get the texture the assets of that project
	UnityGUID guid;
	std::string assetPath = "Assets/Editor Default Resources/" + path;
	if (GetGUIDPersistentManager ().PathNameToGUID(assetPath, &guid))
	{
		const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
		if (asset != NULL)
		{
			Texture2D* tex = dynamic_pptr_cast<Texture2D*> (asset->mainRepresentation.object);
			if (tex)
				return tex;
		}
	}

	// If not found try the editor assetbundle
	return GetEditorAssetBundle()->Get<Texture2D> (path);
}


static GUIStyle* GetLabelIconGUIStyle (Texture2D* iconTexture)
{
	if (s_LabelIconGUIStyle == NULL)
	{
		s_LabelIconGUIStyle = CreateLabelIconGUIStyle ();
		for (int i=0; i<8; ++i)
		{
			std::string path = Format (AppendPathName(EditorResources::kIconsPath, "sv_icon_name%d.png").c_str (), i);
			s_InspectorLabelIcons [i] = GetLabelTexture (path);
			if (s_InspectorLabelIcons [i].IsNull ())
				LogString("Missing sv_icon_name");

			path = Format (AppendPathName(EditorResources::kIconsPath, "sv_label_%d.png").c_str (), i);
			s_SceneViewLabelIcons [i] =  GetLabelTexture (path);
			if (s_SceneViewLabelIcons [i].IsNull ())
				LogString("Missing sv_label");
		}
	}

	// Set correct background textures
	GUIStyle* style = NULL;
	for (int i=0; i<8; ++i)
	{
		if (iconTexture == s_SceneViewLabelIcons [i])
		{	
			s_LabelIconGUIStyle->m_Normal.background		= s_SceneViewLabelIcons [i];
			s_LabelIconGUIStyle->m_OnNormal.background	= s_SceneViewLabelIcons [i];
			style = s_LabelIconGUIStyle;
			break;
		}
	}
	return style;
}

static Material* GetMaterialForRenderMode( GizmoRenderMode mode )
{
	if( !s_GizmoMaterials[mode] ) {
		s_GizmoMaterials[mode] = Material::CreateMaterial (kGizmoShaders[mode], Object::kHideAndDontSave);
	}
	return s_GizmoMaterials[mode];
}

Material* gizmos::GetMaterial() {
	return GetMaterialForRenderMode(kGizmoRenderWire);
}
Material* gizmos::GetColorMaterial() {
	return GetMaterialForRenderMode(kGizmoRenderColor);
}
Material* gizmos::GetLitMaterial() {
	return GetMaterialForRenderMode(kGizmoRenderLit);
}

static Material* GetIconPickMaterial() {
	static Material *s_mat = NULL;
	if (s_mat == NULL)
		s_mat = Material::CreateMaterial (iconPicking, Object::kHideAndDontSave);
	return s_mat;
}

// ----------------------------------------------------------------------
// gizmo object/batch data


struct GizmoObject {
	Vector3f center;
	ColorRGBA32 pickID;
	size_t dataBegin;
	size_t dataEnd;
};


struct GizmoBatch {
	int vertexCount;
	ColorRGBAf color;
	GfxPrimitiveType type;
	GizmoRenderMode mode;
	// followed by vertexCount vertices of appropriate GizmoRenderMode structs
};

struct GizmoIcon {
	Vector2f screenCoord;
	Vector2f screenSize;
	float distance;
	ColorRGBA32 pickID;
	ColorRGBA32 tint;
	Texture2D* icon;
	std::string text;
	bool allowScaling;
};


static std::vector<GizmoObject> s_Gizmos;
static std::vector<GizmoIcon> s_Icons;
static std::vector<UInt8> s_DataBuffer;
static size_t s_LastBatchIndex = size_t(-1);
ColorRGBAf gizmos::g_GizmoColor = ColorRGBAf(1,1,1,1);
ColorRGBA32 gizmos::g_GizmoPickID = ColorRGBA32(0);
bool gizmos::g_GizmoPicking = false;


#if DEBUG_GIZMO_STATS
static int s_GizmoObjects, s_IncomingBatches, s_ActualBatches, s_ShaderChanges;
#endif


static void EndPreviousGizmo()
{
	if( s_Gizmos.empty() )
		return;
	GizmoObject& gizmo = s_Gizmos.back();
	gizmo.dataEnd = s_DataBuffer.size();
	AssertIf( gizmo.dataEnd < gizmo.dataBegin );
}

static GizmoBatch* GetLastPrimitiveBatch()
{
	if( s_LastBatchIndex == size_t(-1) )
		return NULL;
	AssertIf( s_LastBatchIndex + sizeof(GizmoBatch) > s_DataBuffer.size() );
	return reinterpret_cast<GizmoBatch*>( &s_DataBuffer[s_LastBatchIndex] );
}


// ----------------------------------------------------------------------
// public functions


void gizmos::BeginGizmo( const Vector3f& center )
{
	EndPreviousGizmo();

	GizmoObject g;
	g.center = center;
	g.pickID = g_GizmoPicking ? g_GizmoPickID : ColorRGBA32(0);
	g.dataBegin = s_DataBuffer.size();
	g.dataEnd = g.dataBegin;
	s_Gizmos.push_back( g );

	s_LastBatchIndex = size_t(-1);
}


// returns pointer to inserted primitive data
static UInt8* AddPrimitivesImpl( GfxPrimitiveType primType, int vertexCount, const UInt8* verts, GizmoRenderMode mode )
{
	AssertIf( vertexCount < 0 );
	if( vertexCount == 0 || verts == NULL )
		return NULL;

	
	#if DEBUG_GIZMO_STATS
	++s_IncomingBatches;
	#endif

	const int kVertexCountLimit = 60000;

	// check if we can just add this to end of last primitive batch
	const GizmoBatch* lastBatch = GetLastPrimitiveBatch();
	if( lastBatch && lastBatch->type == primType && lastBatch->mode == mode && primType != kPrimitiveLineStrip && lastBatch->vertexCount + vertexCount < kVertexCountLimit )
	{
		if( mode == kGizmoRenderColor || (lastBatch->color.r==gizmos::g_GizmoColor.r && lastBatch->color.g==gizmos::g_GizmoColor.g && lastBatch->color.b==gizmos::g_GizmoColor.b && lastBatch->color.a==gizmos::g_GizmoColor.a) )
		{
			// add data to end of buffer
			size_t insertOffset = s_DataBuffer.size();
			s_DataBuffer.insert( s_DataBuffer.end(),
				verts,
				verts + vertexCount * kRenderModeStrides[mode] );
			// re-fetch pointer to last primitive batch and make it encompass these new primitives
			GizmoBatch* newLastBatch = GetLastPrimitiveBatch();
			newLastBatch->vertexCount += vertexCount;
			return &s_DataBuffer[insertOffset];
		}
	}

	// otherwise, create and add a new batch
	GizmoBatch batch;
	batch.color = gizmos::g_GizmoColor;
	batch.mode = mode;
	batch.type = primType;
	batch.vertexCount = vertexCount;
	// add batch to to end of buffer
	s_LastBatchIndex = s_DataBuffer.size();
	s_DataBuffer.insert( s_DataBuffer.end(),
		reinterpret_cast<const UInt8*>(&batch),
		reinterpret_cast<const UInt8*>(&batch) + sizeof(batch) );
	// add data to to end of buffer
	size_t insertOffset = s_DataBuffer.size();
	s_DataBuffer.insert( s_DataBuffer.end(),
		verts,
		verts + vertexCount*kRenderModeStrides[mode] );
	return &s_DataBuffer[insertOffset];
}

void gizmos::AddLinePrimitives( GfxPrimitiveType primType, int vertexCount, const Vector3f* verts, bool depthTest )
{
	AddPrimitivesImpl( primType, vertexCount, reinterpret_cast<const UInt8*>(verts), (depthTest && GetAnnotationManager ().Is3dGizmosEnabled()) ? kGizmoRenderWire : kGizmoRenderWireNoDepthTest);
}

void gizmos::AddColorPrimitives( GfxPrimitiveType primType, int vertexCount, const ColorVertex* verts )
{
	UInt8* batchData = AddPrimitivesImpl( primType, vertexCount, reinterpret_cast<const UInt8*>(verts), kGizmoRenderColor );
	if( gGraphicsCaps.needsToSwizzleVertexColors && batchData )
	{
		ColorVertex* v = reinterpret_cast<ColorVertex*>(batchData);
		for( int i = 0; i < vertexCount; ++i )
			v[i].color = v[i].color.SwizzleToBGRA();
	}
}

void gizmos::AddLitPrimitives( GfxPrimitiveType primType, int vertexCount, const LitVertex* verts )
{
	AddPrimitivesImpl( primType, vertexCount, reinterpret_cast<const UInt8*>(verts), kGizmoRenderLit );
}

void gizmos::AddColorOcclusionPrimitives( GfxPrimitiveType primType, int vertexCount, const ColorVertex* verts )
{
	UInt8* batchData = AddPrimitivesImpl( primType, vertexCount, reinterpret_cast<const UInt8*>(verts), kGizmoRenderColorOcclusion );
	if( gGraphicsCaps.needsToSwizzleVertexColors && batchData )
	{
		ColorVertex* v = reinterpret_cast<ColorVertex*>(batchData);
		for( int i = 0; i < vertexCount; ++i )
			v[i].color = v[i].color.SwizzleToBGRA();
	}
}

void gizmos::ClearGizmos()
{
	s_DataBuffer.clear();
	s_Gizmos.clear();
	s_Icons.clear();
	s_LastBatchIndex = size_t(-1);
	g_GizmoColor = ColorRGBAf(1,1,1,1);

	#if DEBUG_GIZMO_STATS
	s_GizmoObjects = s_IncomingBatches = s_ActualBatches = s_ShaderChanges = 0;
	#endif
}

struct GizmoSorter
{
	bool operator()( const GizmoObject& ga, const GizmoObject& gb ) const
	{
		float da = matrix->MultiplyPoint3(ga.center).z;
		float db = matrix->MultiplyPoint3(gb.center).z;
		return da < db;
	}
	const Matrix4x4f* matrix;
};

struct IconSorter
{
	bool operator()( const GizmoIcon& ga, const GizmoIcon& gb ) const
	{
		return ga.distance < gb.distance;
	}
};


static void SetupLightingForGizmos( const Matrix4x4f& cameraMatrix, const Vector3f& lookDir )
{
	GfxDevice& device = GetGfxDevice();
	float matWorld[16], matView[16];
	CopyMatrix(device.GetViewMatrix(), matView);
	CopyMatrix(device.GetWorldMatrix(), matWorld);
	device.SetViewMatrix(cameraMatrix.GetPtr());
	GfxVertexLight light;
	light.type = kLightDirectional;
	light.position.Set(lookDir.x,lookDir.y,lookDir.z,0);
	light.spotDirection.Set(0,0,1,0);
	light.color.Set(0.67f,0.67f,0.67f,1.0f);
	light.range = 1.0f;
	light.quadAtten = 0.0f;
	light.spotAngle = -1.0f;
	device.SetLight (0, light);
	ColorRGBAf ambient (0.33f, 0.33f, 0.33f, 1.0f);
	ambient = GammaToActiveColorSpace(ambient);
	device.SetAmbient(ambient.GetPtr());
	
	device.DisableLights (1);
	device.SetViewMatrix(matView);
	device.SetWorldMatrix(matWorld);
}

static float g_matWorldDevice[16];
static float g_matViewDevice[16];
static Matrix4x4f g_matProjDevice;

static Rectf SetupScreenCoords()
{
	GfxDevice& device = GetGfxDevice();
	CopyMatrix(device.GetViewMatrix(), g_matViewDevice);
	CopyMatrix(device.GetWorldMatrix(), g_matWorldDevice);
	CopyMatrix(device.GetProjectionMatrix(), g_matProjDevice.GetPtr());

	Rectf r = GetCurrentCamera().GetScreenViewportRect ();
	Matrix4x4f mat;
	float offset = 0.0f;
	if (device.UsesHalfTexelOffset())
		offset = -0.5f;
	mat.SetOrtho(r.x + offset, r.x + r.Width() + offset, r.y + offset, r.y + r.Height() + offset, -1.0f, 1.0f);
	device.SetProjectionMatrix (mat);
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity
	return r;
}
static void TearDownScreenCoords()
{
	GfxDevice& device = GetGfxDevice();
	device.SetViewMatrix(g_matViewDevice);
	device.SetWorldMatrix(g_matWorldDevice);
	device.SetProjectionMatrix(g_matProjDevice);
}
static Vector2f GetScreenSize (Texture2D *texture, float forceSize)
{
	if (forceSize > 0.0f)
		return Vector2f (forceSize, forceSize);

	// Max size
	const float wantedSize = 32.0f;
	float textureWidth = texture->GetDataWidth();
	float textureHeight = texture->GetDataHeight();
	if (textureWidth <= wantedSize && textureHeight <= wantedSize)
	{
		// For textures smaller than our wanted width and height we use the textures size
		return Vector2f (textureWidth, textureHeight);
	}

	// For textures larger than our wanted size we ensure to 
	// keep aspect ratio of the texture and fit it to best match our wanted width and height.
	float relWidth = wantedSize / textureWidth;
	float relHeight = wantedSize / textureHeight;

	float scale = std::min(relHeight, relWidth);

	return Vector2f ((int)(textureWidth * scale), (int)(textureHeight * scale));
}


void gizmos::AddIcon( const Vector3f& center, Texture2D* icon, const char* text /*= 0*/, bool allowScaling /*=true*/, ColorRGBA32 tint /*= ColorRGBA32(255,255,255,255)*/)
{
	const Camera* camera = GetCurrentCameraPtr();
	if( !camera ) {
		AssertString("No camera while adding gizmo icon!");
		return;
	}

	const Matrix4x4f& matrix = camera->GetWorldToCameraMatrix ();
	Vector3f cameraCenter = matrix.MultiplyPoint3 (center);
	float nearPlane = camera->GetNear ();
	if (cameraCenter.z >= -nearPlane)
		return; // behind camera

	// add icon
	GizmoIcon gizmoicon;
	Vector3f screenPos = camera->WorldToScreenPoint (center);
	gizmoicon.screenCoord.Set( screenPos.x, screenPos.y );
	gizmoicon.distance = cameraCenter.z;
	gizmoicon.pickID = g_GizmoPicking ? g_GizmoPickID : ColorRGBA32(0);
	gizmoicon.icon = icon;
	gizmoicon.tint = tint;
	gizmoicon.allowScaling = allowScaling;
	float forceSize = -1.0f;
	if (icon)
	{	
		const char* pixPos = strstr (icon->GetName(), "pix");			
		if (pixPos != NULL)
		{
			int size = atoi (pixPos + 3);
			if (size > 0)
				forceSize = (float)size;
		}
	}
	else
	{
		icon = builtintex::GetWhiteTexture ();
	}
	gizmoicon.screenSize = GetScreenSize (icon, forceSize);

	if (text)
		gizmoicon.text = std::string(text);
	s_Icons.push_back(gizmoicon);
}


PROFILER_INFORMATION(gSubmitVBOProfileGizmo, "Mesh.SubmitVBO", kProfilerRender);


static bool DrawLabelIcon (const Camera& camera, const Matrix4x4f* cameraProj, const GizmoIcon& gizmoIcon, bool picking)
{
	// No text no label icon
	if (gizmoIcon.text.empty())
		return false;

	// Does icon have a corresponding label texture?
	GUIStyle* style = GetLabelIconGUIStyle (gizmoIcon.icon);
	if (style == NULL)
		return false;

	// Calc size of label
	GUIContent displayText;
	UTF8ToUTF16String(gizmoIcon.text.c_str(), displayText.m_Text);
	Vector2f size =  style->CalcSize (displayText);

	GfxDevice& device = GetGfxDevice();
	
	// Center label
	Vector3f screenPos = Vector3f(gizmoIcon.screenCoord.x, gizmoIcon.screenCoord.y, gizmoIcon.distance);
	// +0.3 is to ensure the icon doesn't jump around if at CENTER of viewport.
	screenPos.x = Roundf(screenPos.x - size.x*0.5f + 0.3f);
	screenPos.y = Roundf(screenPos.y + size.y*0.5f + 0.3f);

	if (picking)
	{
		// draw quad for picking
		device.SetColorBytes( gizmoIcon.pickID.GetPtr() );
		device.ImmediateBegin( kPrimitiveQuads );
		device.ImmediateVertex( screenPos.x, screenPos.y, 0.0f );
		device.ImmediateVertex( screenPos.x + size.x, screenPos.y, 0.0f );
		device.ImmediateVertex( screenPos.x + size.x, screenPos.y - size.y, 0.0f );
		device.ImmediateVertex( screenPos.x, screenPos.y - size.y, 0.0f );
		device.ImmediateEnd();
	}
	else
	{
		// draw with style
		Material* mat = GetMaterialForRenderMode(kGizmoRenderTexture);
		Material* matText = GetMaterialForRenderMode(kGizmoRenderText);
		Vector3f postProj;
		cameraProj->PerspectiveMultiplyPoint3(screenPos, postProj);
		Matrix4x4f worldMat = Matrix4x4f::identity;
		worldMat.Get(2,3) = -postProj.z;
		device.SetWorldMatrix (worldMat.GetPtr());
		Rectf screenRect(screenPos.x, screenPos.y, size.x, -size.y);
		DrawGUITexture (screenRect, style->m_Normal.background, style->m_Border.left, style->m_Border.right, style->m_Border.top, style->m_Border.bottom, ColorRGBA32(0xFFFFFFFF), mat);

		worldMat.Get(0,3) = screenRect.x + 8;
		worldMat.Get(1,3) = screenRect.y - 1;
		worldMat.Get(1,1) = -worldMat.Get(1,1);
		device.SetWorldMatrix (worldMat.GetPtr());
		DrawGUIText (gizmoIcon.text, style->m_Font, matText);
	}

	device.SetWorldMatrix (Matrix4x4f::identity.GetPtr());

	return true;
}

float GetTexelScreenSizeBeforeDistanceScale (const Camera& camera)
{
	Rectf r = GetCurrentCamera().GetScreenViewportRect ();

	float texelWorldSize =  GetAnnotationManager ().GetIconSize ();
	float multiplier = 1;
	if (camera.GetOrthographic ())
		multiplier = (r.height * 0.5f) / (camera.GetOrthographicSize ());
	else
		multiplier = (r.height * 0.5f) / tanf (Deg2Rad(camera.GetFov () * 0.5f));

	return texelWorldSize * multiplier;
}

// Call only if needed (want distance scaling and perspective camera)
Vector2f CalcIconSize (float texelScreenSizeBeforeDistance, float distanceToCamera, float iconWidth, float iconHeight)
{
	float texelSize = texelScreenSizeBeforeDistance / distanceToCamera;
	
	Vector2f halfSize (texelSize * iconWidth * 0.5f, texelSize * iconHeight * 0.5f);
	
	if (halfSize.x < 4.0f || halfSize.y < 4.0f)
		halfSize.x = halfSize.y = 0.0f;

	return halfSize;
}

inline float InverseLerp01Safe (float from, float to, float v)
{
	Assert (from <= to);
	return clamp01 (v - from) / max(to - from, 0.0001F);
}

static float CalculateAlpha (const Camera& camera, float iconScreenHeight, int pass, bool depthTesting, bool iconScaling)
{
	//Early outs
	if (!iconScaling && depthTesting)
	{
		if (pass == 0)
			return 1.0f;
		else
			return 0.2f;
	}
	else if (!iconScaling && !depthTesting)
{
		return 1.0f;
	}

	float iconScreenRatio = clamp01 (Abs (iconScreenHeight / camera.GetScreenViewportRect ().Height () / Sqrt (camera.GetAspect ())));
	
	// Use lower alpha when occluded.
	float alpha = pass == 0 ? 1.0f : 0.3f;
	
	if (!depthTesting)
		alpha = 1.0f;
	
	// Fade out when big no matter if occluded or not.
	if (iconScreenRatio > 0.25f)
	{
		float fade = (1 - InverseLerp01Safe (0.3f, 1.0f, iconScreenRatio));
		// screen size changes more rapidly when an object is close,
		// so compensate by making the fading less sensitive at the large end.
		alpha *= fade * fade;
	}
	
	// Fade out when small if occluded.
	if (pass == 1 && depthTesting)
	{
		const float fadeOutAtRatio = 0.15f;
		if (iconScreenRatio < fadeOutAtRatio)
		{
			alpha *= iconScreenRatio / fadeOutAtRatio;
		}
	}
	
	return alpha;
}


static void DoDrawIconsNormal (const Camera& camera)
{
	AssertIf(s_Icons.empty());

	GfxDevice& device = GetGfxDevice();
	DynamicVBO& vbo = device.GetDynamicVBO();
	
	size_t nicons = s_Icons.size();
	
	Material* mat = GetMaterialForRenderMode(kGizmoRenderTexture);
	
	bool orthographic = camera.GetOrthographic ();
	Matrix4x4f cameraProj = camera.GetProjectionMatrix();
	
	bool depthTesting = GetAnnotationManager ().Is3dGizmosEnabled();
	
	int passCount = mat->GetPassCount();
	if (passCount != 2) // icon shader needs to have two passes
		return;

	for (int ip = 0; ip < passCount; ++ip)
	{
		for( size_t i = 0; i < nicons; /* nothing */ )
		{
			AssertIf( i >= nicons );
			
			bool useIconScaling = depthTesting && s_Icons[i].allowScaling;
	
			float texelScreenSizeBeforeDistance = 1.0f;
			if (useIconScaling)
				texelScreenSizeBeforeDistance = GetTexelScreenSizeBeforeDistanceScale (camera);

			// Try to draw this icon as a label icon (returns true if it was drawn)
			if (DrawLabelIcon (camera, &cameraProj, s_Icons[i], false))
			{
				i++;
				continue;
			}

			// Figure out current texture
			Texture2D* texture = s_Icons[i].icon;		
			if (!texture)
				texture = builtintex::GetWhiteTexture ();
			
			float dataWidth = s_Icons[i].screenSize.x;
			float dataHeight = s_Icons[i].screenSize.y;
			
			Vector2f uvExtents = Vector2f(1.0F, 1.0F);

			// look ahead and count how many icons are using the same texture
			size_t j = i+1;
			while( j < nicons && s_Icons[j].icon == texture )
				++j;
			size_t iconCount = j - i;

			// get VBO chunk
			TextureVertex* vbPtr;
			if( !vbo.GetChunk( kRenderModeFormats[kGizmoRenderTexture], iconCount * 4, 0, DynamicVBO::kDrawQuads, (void**)&vbPtr, NULL ) )
				break;

			// fill in vertex data for all icons with this texture
			do
			{
				Vector2f p2 = s_Icons[i].screenCoord;
				Vector3f p(p2.x, p2.y, s_Icons[i].distance);
				Vector3f postProj;
				cameraProj.PerspectiveMultiplyPoint3(p, postProj);
				p.z = -postProj.z;

				// +0.3 is to ensure the icon doesn't jump around if at CENTER of viewport.
				p.x = Roundf(p.x + 0.3f);
				p.y = Roundf(p.y + 0.3f);

				// Calc icon size (scale by distance)
				Vector2f halfSize (dataWidth * 0.5f, dataHeight * 0.5f);
				if (useIconScaling)
				{
					if (orthographic)
						halfSize = CalcIconSize (texelScreenSizeBeforeDistance, 1, dataWidth, dataHeight);
					else
						halfSize = CalcIconSize (texelScreenSizeBeforeDistance, -s_Icons[i].distance, dataWidth, dataHeight);
				}

				//Figure out icon alpha based on pass / distance and depth testing settings
				float alpha = CalculateAlpha (camera, (halfSize.y * 2.0f), ip, depthTesting, useIconScaling);
				
				ColorRGBA32 vertexColor = ColorRGBA32 (s_Icons[i].tint.r, s_Icons[i].tint.g, s_Icons[i].tint.b, (UInt8)(alpha * 255));
				vertexColor = GetGfxDevice().ConvertToDeviceVertexColor (vertexColor);

				vbPtr[0].vertex.Set(p.x + halfSize.x, p.y + halfSize.y, p.z);
				vbPtr[0].uv.Set(uvExtents.x, uvExtents.y);
				vbPtr[0].color.Set(vertexColor);
				vbPtr[1].vertex.Set(p.x + halfSize.x, p.y - halfSize.y, p.z);
				vbPtr[1].uv.Set(uvExtents.x, 0.0f);
				vbPtr[1].color.Set(vertexColor);
				vbPtr[2].vertex.Set(p.x - halfSize.x, p.y - halfSize.y, p.z);
				vbPtr[2].uv.Set(0.0f, 0.0f);
				vbPtr[2].color.Set(vertexColor);
				vbPtr[3].vertex.Set(p.x - halfSize.x, p.y + halfSize.y, p.z);
				vbPtr[3].uv.Set(0.0f, uvExtents.y);
				vbPtr[3].color.Set(vertexColor);
				vbPtr += 4;

				++i;
			}
			while( i < j );
			
			// draw chunk
			vbo.ReleaseChunk( iconCount * 4, 0 );

			HandleGUITextureProps (&mat->GetWritableProperties(), texture);

			const ChannelAssigns* channels = mat->SetPass(ip);

			float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			device.SetColor( white );
			PROFILER_BEGIN(gSubmitVBOProfileGizmo, NULL);
			vbo.DrawChunk (*channels);
			PROFILER_END
			
		}
	}
}

static void DoDrawIconsPicking (const Camera& camera)
{
	AssertIf(s_Icons.empty());

	GfxDevice& device = GetGfxDevice();

	size_t nicons = s_Icons.size();

	bool orthographic = camera.GetOrthographic ();
	Matrix4x4f cameraProj = camera.GetProjectionMatrix();

	//Need to render the picking buffer twice to match the 'normal' icon rendering.
	Material* mat = GetIconPickMaterial();
	int passCount = mat->GetPassCount();
	if (passCount != 2)
		return;
	
	bool depthTesting = GetAnnotationManager ().Is3dGizmosEnabled();

	//Decrement loop as we want to render zfail first
	for (int ip = passCount - 1; ip >= 0; --ip)
	{
		mat->SetPass (ip);
		for( size_t i = 0; i < nicons; ++i )
		{
			bool useIconScaling = depthTesting && s_Icons[i].allowScaling;

			float texelScreenSizeBeforeDistance = 1.0f;
			if (useIconScaling)
				texelScreenSizeBeforeDistance = GetTexelScreenSizeBeforeDistanceScale (camera);

			// Try to draw this icon as a labelled icon (returns true if it was drawn)
			if (DrawLabelIcon (camera, NULL, s_Icons[i], true))
				continue;

			Vector2f p2 = s_Icons[i].screenCoord;
			Vector3f p(p2.x, p2.y, s_Icons[i].distance);
			Vector3f postProj;
			cameraProj.PerspectiveMultiplyPoint3(p, postProj);
			p.z = -postProj.z;

			// +0.3 is to ensure the icon doesn't jump around if at CENTER of viewport.
			p.x = Roundf(p.x + 0.3f);
			p.y = Roundf(p.y + 0.3f);

			// Setup current texture
			Texture2D* texture = s_Icons[i].icon;		
			if (!texture)
				texture = builtintex::GetWhiteTexture ();

			float dataWidth = s_Icons[i].screenSize.x;
			float dataHeight = s_Icons[i].screenSize.y;
			
			// Calc icon size (scale by distance)
			Vector2f halfSize (dataWidth * 0.5f, dataHeight * 0.5f);
			if (useIconScaling)
			{
				if (orthographic)
					halfSize = CalcIconSize (texelScreenSizeBeforeDistance, 1, dataWidth, dataHeight);
				else
				halfSize = CalcIconSize (texelScreenSizeBeforeDistance, -s_Icons[i].distance, dataWidth, dataHeight);
			}

			float alpha = CalculateAlpha (camera, (halfSize.y * 2.0f), ip, depthTesting, useIconScaling);
			if (alpha < 0.01 && useIconScaling ) continue;

			// draw quad (Rotated 45 degrees and slightly scaled down)
			halfSize *= 0.84f;
			device.SetColorBytes (s_Icons[i].pickID.GetPtr ());
			device.ImmediateBegin (kPrimitiveQuads);
			device.ImmediateVertex (p.x + halfSize.x, p.y, p.z);
			device.ImmediateVertex (p.x, p.y - halfSize.y, p.z);
			device.ImmediateVertex (p.x - halfSize.x, p.y, p.z);
			device.ImmediateVertex (p.x, p.y + halfSize.y, p.z);
			device.ImmediateEnd ();
		}
	}

	//Restore the default picking material
	GetPickMaterial ()->SetPass (0);
}


static void DoDrawIcons (const Camera& camera)
{
	if( s_Icons.empty() )
		return;

	if (GetAnnotationManager ().GetIconSize () == 0.0f)
		return;

	// sort icons back to front
	std::sort( s_Icons.begin(), s_Icons.end(), IconSorter() );

	// setup screen space coordinates
	Rectf screenRect = SetupScreenCoords ();

	if( !gizmos::g_GizmoPicking )
		DoDrawIconsNormal (camera);
	else
		DoDrawIconsPicking (camera);

	TearDownScreenCoords();
}

static void RenderGizmoBatches (int passIndex)
{
	GizmoRenderMode previousMode = kGizmoRenderModeCount;
	GfxDevice& device = GetGfxDevice();
	size_t ngizmos = s_Gizmos.size();
	for( size_t i = 0; i < ngizmos; ++i )
	{
		#if DEBUG_GIZMO_STATS
		++s_GizmoObjects;
		#endif
		const GizmoObject& gizmo = s_Gizmos[i];
		AssertIf( gizmo.dataBegin > s_DataBuffer.size() || gizmo.dataEnd > s_DataBuffer.size() );

		if( gizmos::g_GizmoPicking )
			device.SetColorBytes( gizmo.pickID.GetPtr() );

		size_t batchOffset = gizmo.dataBegin;
		while( batchOffset != gizmo.dataEnd )
		{
			AssertIf( batchOffset > gizmo.dataEnd );

			const GizmoBatch* batch = reinterpret_cast<GizmoBatch*>( &s_DataBuffer[batchOffset] );

			if (!gizmos::g_GizmoPicking)
			{
				// change material if needed
				if( batch->mode != previousMode )
				{
					device.SetAntiAliasFlag( batch->mode != kGizmoRenderLit );
					GetMaterialForRenderMode(batch->mode)->SetPass(passIndex);
					#if DEBUG_GIZMO_STATS
					++s_ShaderChanges;
					#endif
					previousMode = batch->mode;
				}

				if( batch->mode == kGizmoRenderWire || batch->mode == kGizmoRenderWireNoDepthTest) {
					device.SetColor( batch->color.GetPtr() );
				} else if( batch->mode == kGizmoRenderLit ) {
					static float kZero[4] = {0,0,0,0};
					device.SetMaterial( batch->color.GetPtr(), batch->color.GetPtr(), kZero, kZero, 1.0f );
				}
			}

			// draw batch
			#if DEBUG_GIZMO_STATS
			++s_ActualBatches;
			#endif
			device.DrawUserPrimitives(
				batch->type,
				batch->vertexCount,
				kRenderModeFormats[batch->mode],
				reinterpret_cast<const void*>( batch + 1 ),
				kRenderModeStrides[batch->mode] );

			// advance to next batch
			batchOffset += sizeof(GizmoBatch);
			batchOffset += batch->vertexCount * kRenderModeStrides[batch->mode];
		}
	}

	device.SetAntiAliasFlag( false );
}

void gizmos::RenderGizmos ()
{
	EndPreviousGizmo();

	const Camera* camera = GetCurrentCameraPtr();
	if( !camera ) {
		ClearGizmos();
		return;
	}
	const Matrix4x4f& cameraMatrix = camera->GetWorldToCameraMatrix ();
	Vector3f lookDir = camera->GetComponent(Transform).TransformDirection( Vector3f(0,0,1) );


	// sort gizmos back to front
	GizmoSorter sorter;
	sorter.matrix = &cameraMatrix;
	std::sort( s_Gizmos.begin(), s_Gizmos.end(), sorter );

	GetGfxDevice().SetWorldMatrix (Matrix4x4f::identity.GetPtr());

	// setup lighting
	if( !g_GizmoPicking )
		SetupLightingForGizmos(cameraMatrix, lookDir);
	

	// go over gizmo objects and render their batches
	RenderGizmoBatches (0);
	#if BLEND_OCCLUDED_GIZMOS
	if (!g_GizmoPicking)
		RenderGizmoBatches (1);
	#endif

	// draw icons
	DoDrawIcons( *camera );

	#if DEBUG_GIZMO_STATS
	printf_console( "gizmos: objects=%i batches=%i actualbatches=%i shaderchanges=%i data=%i\n", s_GizmoObjects, s_IncomingBatches, s_ActualBatches, s_ShaderChanges, s_DataBuffer.size() );
	#endif

	ClearGizmos();
}
