#ifndef CAMERA_H
#define CAMERA_H

#include "Runtime/GameCode/Behaviour.h"
#include <vector>
#include "Runtime/Math/Rect.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Geometry/Ray.h"
#include "Runtime/Graphics/RenderTexture.h" //@TODO remove
#include "Runtime/Camera/RenderLoops/RenderLoop.h" //@TODO remove
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Modules/ExportModules.h"

struct ImageFilter;
class Cubemap;
class Plane;
class IntermediateRenderers;
class Shader;
struct RenderLoop;
struct ShadowCullData;
struct CullResults;
struct CullingParameters;
struct ShaderReplaceData;
struct SceneCullingParameters;
struct CameraCullingParameters;
class Vector2f;

struct DrawGridParameters;

namespace Unity {class Material;}
namespace Umbra { class DebugRenderer; }

struct CameraRenderOldState
{
	void Reset()
	{
		subshaderIndex = 0;
		currentPass = 0;
		memset (&viewport, 0, sizeof(viewport));
		memset (&matWorld, 0, sizeof(matWorld));
		memset (&matView, 0, sizeof(matView));
		matProj = Matrix4x4f::identity;
	}
	PPtr<Material> material;
	PPtr<Shader> shader;
	int subshaderIndex;
	int currentPass;

	int viewport[4];
	PPtr<Camera> camera;
	PPtr<RenderTexture> activeRT;

	float matWorld[16];
	float matView[16];
	Matrix4x4f matProj;
};

struct CameraTemporarySettings
{
	int   renderingPath;
	float fieldOfView;
	float aspect;
	bool  implicitAspect;
};

// The Camera
class EXPORT_COREMODULE Camera : public Behaviour {
public:
	enum SortMode {
		kSortDefault = 0,
		kSortPerspective = 1,
		kSortOrthographic = 2,
	};

	enum { kNumLayers = 32 };

public:
	Camera (MemLabelId label, ObjectCreationMode mode);
	// ~Camera (); declared-by-macro
	REGISTER_DERIVED_CLASS (Camera, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Camera)

	// Tag class as sealed, this makes QueryComponent faster.
	static bool IsSealedClass ()				{ return true; }

	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	virtual void CheckConsistency ();

	enum RenderFlag {
		kRenderFlagStandalone = (1<<0),
		kRenderFlagSetRenderTarget = (1<<1),
		kRenderFlagPrepareImageFilters = (1<<2),
		kRenderFlagDontRestoreRenderState = (1<<3),
		kRenderFlagSetRenderTargetFinal = (1<<4),
		kRenderFlagExplicitShaderReplace = (1<<5),
	};


	// Set up the viewport, render target, load modelview & projection matrices
	void SetupRender (int renderFlags = 0); // bitmask of kRenderFlagXXX

	// Cull with default culling parameters
	void Cull           (CullResults& results);
	void StandaloneCull (Shader* replacementShader, const std::string& replacementTag, CullResults& results);

	// Cull With custom parameters
	void CustomCull (const CameraCullingParameters& params, CullResults& cullResults);


	static void PrepareSceneCullingParameters (const CameraCullingParameters& parameters, RenderingPath renderPath, CullResults& results);
	void CalculateFrustumPlanes(Plane* frustum, const Matrix4x4f& overrideWorldToClip, float overrideFarPlane, float& outBaseFarDistance, bool implicitNearFar) const;
	void CalculateCullingParameters(CullingParameters& cullingParameters) const;
	void CalculateCustomCullingParameters(CullingParameters& cullingParameters, const Plane* planes, int planeCount) const;


	// Clear the camera for how its set up - use skybox if neccessary
	void Clear ();
	void ClearNoSkybox (bool noDepth);
	void RenderSkybox ();

	// Set up the camera transform & render
	void Render (CullResults& cullResults, int renderFlags); // bitmask of kRenderFlagXXX; kRenderFlagPrepareImageFilters always implied

	// Out-of-order cull / rendering.
	void StandaloneRender (UInt32 renderFlags, Shader* replacementShader, const std::string& replacementTag);
	void StandaloneSetup ();
	bool StandaloneRenderToCubemap (RenderTexture* rt, int faceMask);
	bool StandaloneRenderToCubemap (Cubemap* cubemap, int faceMask);

	void SetFov (float deg);
	float GetFov () const           { return m_FieldOfView; }
	void SetNear (float n);
	float GetNear () const          { return m_NearClip; }
	void SetFar (float f);
	float GetFar () const           { return m_FarClip; }

	// Projection's near and far plane (can differ from GetNear/Far() for custom projection matrix)
	float GetProjectionNear () const;
	float GetProjectionFar () const;

	void SetHDR (bool enable)		{ m_HDR = enable; SetDirty(); }
	bool GetUsingHDR () const		{ return m_UsingHDR; } // Cached when setting up the render
	// A recompute is needed if Camera.hdr is called from script before rendering (case 483603)
	bool CalculateUsingHDR () const;

	void SetRenderingPath (int rp) { m_RenderingPath = rp; SetDirty(); }
	RenderingPath GetRenderingPath () const { return static_cast<RenderingPath>(m_RenderingPath); }

	void SetOrthographicSize (float f);
	float GetOrthographicSize () const		{ return m_OrthographicSize; }

	bool GetOrthographic() const		{ return m_Orthographic; }
	void SetOrthographic (bool v);

	void GetTemporarySettings (CameraTemporarySettings& settings) const;
	void SetTemporarySettings (const CameraTemporarySettings& settings);

	float GetAspect() const			{ return m_Aspect; }
	void SetAspect (float aspect);
	void ResetAspect ();


	bool IsValidToRender() const;

	void SetNormalizedViewportRect (const Rectf& normalizedRect);
	Rectf GetNormalizedViewportRect () const				{ return  m_NormalizedViewPortRect; }

	// The screen view port rect of the camera.
	// If the cameras normalized viewport rect is set to be the fullscreen, then this will always go from
	// 0, 0 to width, height.
	Rectf GetScreenViewportRect () const { return GetCameraRect(true); }
	// Similar to GetScreenViewportRect, except this can have non-zero origin even for fullscreen cameras.
	// This only ever happens in editor's game view when using forced aspect ratio or size.
	Rectf GetPhysicalViewportRect() const { return GetCameraRect(false); }

	void SetScreenViewportRect (const Rectf& pixelRect);

	// Get the final in-rendertarget render rectangle.
	// This takes into account any render texture setup we may have.
	Rectf GetRenderRectangle() const;


	/// The Camera render order is determined by sorting all cameras by depth.
	/// Small depth camera are rendered first, big depth cameras last.
	float GetDepth () const { return m_Depth; }
	void SetDepth (float depth);

	/// Set the background color of the camera.
	void SetBackgroundColor (const ColorRGBAf& color);
	ColorRGBAf GetBackgroundColor () const				{ return m_BackGroundColor; }

	// The clearing mode used for the camera.
	enum ClearMode {
		kSkybox = 1,
		kSolidColor = 2,
		kDepthOnly = 3,
		kDontClear = 4
		// Watch out for check consistency when changing this!
	};

	void SetClearFlags (int flags);
	ClearMode GetClearFlags () const { return static_cast<ClearMode>(m_ClearFlags); }

	void SetCullingMask (UInt32 cullingMask);
	UInt32 GetCullingMask () const						{ return m_CullingMask.m_Bits; }

	void SetEventMask (UInt32 cullingMask);
	UInt32 GetEventMask () const						{ return m_EventMask.m_Bits; }

	Vector3f GetPosition () const;

	void SetUseOcclusionCulling (bool occlusionCull)    { m_OcclusionCulling = occlusionCull; }
	bool GetUseOcclusionCulling () const                { return m_OcclusionCulling; }

	/// A screen space point is defined in pixels.
	/// The left-bottom of the screen is (0,0). The right-top is (screenWidth,screenHeight)
	/// The z position is between 0...1. 0 is on the near plane. 1 is on the far plane

	/// A viewport space point is normalized and relative to the camera
	/// The left-bottom of the camera is (0,0). The top-right is (1,1)
	/// The z position is between 0...1. 0 is on the near plane. 1 is on the far plane

	/// Projects a World space point into screen space.
	/// on return: canProject is true if the point could be projected to the screen (The point is inside the frustum)
	Vector3f WorldToScreenPoint (const Vector3f& worldSpacePoint, bool* canProject = NULL) const;
	/// Unprojects a screen space point into world space
	Vector3f ScreenToWorldPoint (const Vector3f& screenSpacePoint) const;

	/// Projects a world space point into viewport space
	Vector3f WorldToViewportPoint (const Vector3f &worldSpace) const;
	/// Unprojects a view port space into world space
	Vector3f ViewportToWorldPoint (const Vector3f &viewPort) const;

	/// Unprojects a view port space into camera space
	Vector3f ViewportToCameraPoint (const Vector3f &viewPort) const;

	// Converts a screen point into a world space ray
	Ray ScreenPointToRay (const Vector2f& screenPos) const;
	// Converts a viewport point into a world space ray
	Ray ViewportPointToRay (const Vector2f& viewportPos) const;

	// Converts a point between screen space and viewport space
	Vector3f ScreenToViewportPoint (const Vector3f& screenPos) const;
	Vector3f ViewportToScreenPoint (const Vector3f& viewPortPos) const;

	// Calculates the distance between the left and right
	// edges of the frustum pyramid at the far plane
	float CalculateFarPlaneWorldSpaceLength () const;

	// Calculates the distance between the left and right
	// edges of the frustum pyramid at the near plane
	float CalculateNearPlaneWorldSpaceLength () const;

	void WindowSizeHasChanged ();

	void AddImageFilter (const ImageFilter& filter);
	void RemoveImageFilter (const ImageFilter& filter);

	void ClearIntermediateRenderers( size_t startIndex = 0 );
	IntermediateRenderers& GetIntermediateRenderers() { return *m_IntermediateRenderers; }

	const Vector3f& GetVelocity () const { return m_Velocity; }

	const Matrix4x4f& GetWorldToCameraMatrix () const;
	Matrix4x4f GetCameraToWorldMatrix () const;

	const Matrix4x4f& GetProjectionMatrix () const;
	void GetImplicitProjectionMatrix (float overrideNearPlane, Matrix4x4f& outMatrix) const;
	void GetImplicitProjectionMatrix (float overrideNearPlane, float overrideFarPlane, Matrix4x4f& outMatrix) const;

	void GetClipToWorldMatrix( Matrix4x4f& outMatrix ) const;
	const Matrix4x4f& GetWorldToClipMatrix() const;

	void SetWorldToCameraMatrix (const Matrix4x4f& matrix);
	void SetProjectionMatrix (const Matrix4x4f& matrix);

	void ResetWorldToCameraMatrix () { m_ImplicitWorldToCameraMatrix = true; m_DirtyWorldToCameraMatrix = true; m_DirtyWorldToClipMatrix = true; }
	void ResetProjectionMatrix () { m_ImplicitProjectionMatrix = true; m_DirtyProjectionMatrix = true; m_DirtyWorldToClipMatrix = true; }
	bool IsImplicitWorldToCameraMatrix() const { return m_ImplicitWorldToCameraMatrix; }
	bool IsImplicitProjectionMatrix() const { return m_ImplicitProjectionMatrix; }

	void SetReplacementShader( Shader* shader, const std::string& replacementTag );
	void ResetReplacementShader() { m_ReplacementShader.SetInstanceID(0); m_ReplacementTag.clear(); }
	Shader *GetReplacementShader() const { return m_ReplacementShader; }
	string GetReplacementShaderTag() const {return m_ReplacementTag; }

	// Get/Set the texture to render into.
	RenderTexture *GetTargetTexture () const {  return m_TargetTexture; }
	void SetTargetTexture (RenderTexture *tex);

	void SetTargetBuffers (int colorCount, RenderSurfaceHandle* color, RenderSurfaceHandle depth, RenderTexture* originatedFrom);
	void SetTargetBuffersScript (int colorCount, const ScriptingRenderBuffer* color, ScriptingRenderBuffer* depth);

	// TODO: mrt support?
	RenderSurfaceHandle GetTargetColorBuffer() const { return m_TargetColorBuffer[0]; }
	RenderSurfaceHandle GetTargetDepthBuffer() const { return m_TargetDepthBuffer; }

	const float *GetLayerCullDistances() const { return m_LayerCullDistances; }
	void SetLayerCullDistances(float *layerCullDistances) {memcpy(m_LayerCullDistances,layerCullDistances,sizeof(float)*kNumLayers);}

	bool GetLayerCullSpherical() const { return m_LayerCullSpherical; }
	void SetLayerCullSpherical(bool enable) { m_LayerCullSpherical = enable; }

	SortMode GetSortMode() const { return m_SortMode; }
	void SetSortMode (SortMode m) { m_SortMode = m; }

	// Get the current target. This can be different than the textureTarget if some image filters require
	// A temporary buffer to render into.
	RenderTexture *GetCurrentTargetTexture () const { return m_CurrentTargetTexture; }
	void SetCurrentTargetTexture (RenderTexture* rt) { m_CurrentTargetTexture = rt; }

	void TransformChanged ();
	static void InitializeClass ();
	static void CleanupClass () {}

	void CopyFrom( const Camera& other );

	void CalculateFarCullDistances (float* farCullDistances, float baseFarDistance) const;

	enum DepthTextureModes {
		kDepthTexDepthBit = (1<<0),
		kDepthTexDepthNormalsBit = (1<<1),
	};
	UInt32 GetDepthTextureMode() const { return m_DepthTextureMode; }
	void SetDepthTextureMode (UInt32 m) { m_DepthTextureMode = m; }

	bool GetClearStencilAfterLightingPass() const { return m_ClearStencilAfterLightingPass; }
	void SetClearStencilAfterLightingPass (bool clear) { m_ClearStencilAfterLightingPass = clear; }

	RenderingPath CalculateRenderingPath() const;
	bool CalculateNeedsToRenderIntoRT() const;
	int CalculateAntiAliasingForRT() const;

	bool GetUsesScreenForCompositing (bool forceIntoRT) const;


	// Get the resolved skybox material we want to render with.
	Material *GetSkyboxMaterial () const;

	// Implementations in EditorCameraDrawing.cpp
	#if UNITY_EDITOR

	enum EditorDrawingMode {
		kEditorDrawTextured				= 0,
		kEditorDrawWire					= 1,
		kEditorDrawTexturedWire			= 2,
		kEditorDrawRenderPaths			= 3,
		kEditorDrawLightmapResolution	= 4,
		kEditorDrawModeCount
	};


	void ClearEditorCamera (); // clears
	void RenderEditorCamera (EditorDrawingMode mode, const DrawGridParameters* gridParam); // renders camera content (does not clear)
	void FinishRenderingEditorCamera ();
	void SetOnlyRenderIntermediateObjects() { m_OnlyRenderIntermediateObjects = true; }
	static bool ShouldShowChannelErrors (const Camera* ptr);
	void RenderEditorCameraFade (float fade);
	void SetAnimateMaterials (bool animate);
	bool GetAnimateMaterials () const { return m_AnimateMaterials; }
	void SetAnimateMaterialsTime (float time);
	
	bool IsFiltered (Unity::GameObject& gameObject) const;
	void SetFilterMode (int filterMode)	{ m_FilterMode = filterMode; }
	int  GetFilterMode () const			{ return m_FilterMode; }

	#endif

//	Texture* GetUmbraOcclusionBufferTexture ();

private:

	typedef void PerformRenderFunction (Camera& camera, RenderLoop& loop, CullResults& contents);
 	void DoRender( CullResults& cullResults, PerformRenderFunction* customRender, int renderFlags ); // Render all objects

	void DoRenderPostLayers();			// Render any post-layers (before image effects)
	void DoRenderGUILayer();

 	void DoClear (UInt32 gfxClearFlags);

	// Behaviour stuff
	virtual void AddToManager ();
	virtual void RemoveFromManager ();

	void SetCameraShaderProps();

	void UpdateDepthTextures (const CullResults& cullResults);
	void RenderDepthTexture (const CullResults& cullResults, RenderTexture** rt, RenderTextureFormat format, Shader* shader, const ColorRGBAf& clearColor, ShaderLab::FastPropertyName name);

	void CleanupDepthTextures ();

	void DisplayHDRWarnings() const;

	Rectf GetCameraRect (bool zeroOrigin) const;

	bool GetRenderImmediateObjects () const;

private:

	mutable Matrix4x4f  m_WorldToCameraMatrix;
	mutable Matrix4x4f  m_ProjectionMatrix;
	mutable Matrix4x4f	m_WorldToClipMatrix;


	// NOTE: whenever adding new camera properties, make sure they are copied as
	// appropriate in CopyFrom(), and extend CameraCopyFromWorks runtime test
	// to cover it.


	RenderLoop*			m_RenderLoop;

	PPtr<RenderTexture> m_TargetTexture;	///< The texture to render this camera into

	RenderSurfaceHandle	m_TargetColorBuffer[kMaxSupportedRenderTargets];
	int					m_TargetColorBufferCount;
	RenderSurfaceHandle	m_TargetDepthBuffer;
	// this is here to set as active RT along with render buffers
	// dx uses current rt to check if we render onscreen (and tweak projection/offsets/etc)
	// image filters use target RT in calculations of rt to draw to
	// while ideally we want simple IsRenderingOnscreen + image effects to use render buffers
	//   we want it to work naow
	RenderTexture*		m_TargetBuffersOriginatedFrom;

	RenderTexture*		m_DepthTexture;
	RenderTexture*		m_DepthNormalsTexture;

	RenderTexture* 		m_CurrentTargetTexture;	// The texture we're rendering into _right now_

	PPtr<Shader>		m_ReplacementShader;
	std::string			m_ReplacementTag;

	IntermediateRenderers*	m_IntermediateRenderers;

	unsigned int          m_ClearFlags; ///< enum { Skybox = 1, Solid Color = 2, Depth only = 3, Don't Clear = 4 }
	ColorRGBAf            m_BackGroundColor;	///< The color to which camera clears the screen
	Rectf                 m_NormalizedViewPortRect;

	BitField              m_CullingMask;	///< Which layers the camera does render
	BitField			m_EventMask;	///< Which layers receive events

	float                 m_Depth; ///< A camera with a larger depth is drawn on top of a camera with a smaller depth range {-100, 100}
	Vector3f              m_Velocity;
	Vector3f              m_LastPosition;

	float                 m_OrthographicSize;
	float                 m_FieldOfView;	///< Field of view of the camera range { 0.00001, 179 }
	float                 m_NearClip;		///< Near clipping plane
	float                 m_FarClip;		///< Far clipping plane
	int                   m_RenderingPath;	///< enum { Use Player Settings = -1, Vertex Lit=0, Forward=1, Deferred Lighting=2 } Rendering path to use.

	float				  m_LayerCullDistances[kNumLayers];
	float                 m_Aspect;
	SortMode				m_SortMode;

	CubemapFace			m_CurrentTargetFace;	// current cubemap face we're rendering into (only used while rendering into a cubemap)

	UInt32				m_DepthTextureMode;

	mutable bool		m_DirtyWorldToCameraMatrix;
	mutable bool        m_DirtyProjectionMatrix;
	mutable bool        m_DirtyWorldToClipMatrix;
	bool                  m_ImplicitWorldToCameraMatrix;
	bool                  m_ImplicitProjectionMatrix;
	bool                  m_ImplicitAspect;
	bool                  m_Orthographic;	///< Is camera orthographic?
	bool                  m_OcclusionCulling;
	bool				m_LayerCullSpherical;
	bool				m_HDR;
	bool				m_UsingHDR;
	bool				m_IsRendering;
	bool				m_ClearStencilAfterLightingPass;


	// NOTE: whenever adding new camera properties, make sure they are copied as
	// appropriate in CopyFrom(), and extend CameraCopyFromWorks runtime test
	// to cover it.


	void SetTargetTextureBuffers(RenderTexture* tex, int colorCount, RenderSurfaceHandle* color, RenderSurfaceHandle depth, RenderTexture* rbOrigin);

	#if UNITY_EDITOR
	int				m_FilterMode;
	bool			m_OnlyRenderIntermediateObjects;
	bool			m_IsSceneCamera;
	CameraRenderOldState m_OldCameraState;
	bool			m_AnimateMaterials;
	float			m_AnimateMaterialsTime;
	#endif
};

void StoreRenderState (CameraRenderOldState& state);
void RestoreRenderState (CameraRenderOldState& state);

Shader* GetCameraDepthTextureShader ();
Shader* GetCameraDepthNormalsTextureShader ();

void ClearWithSkybox (bool clearDepth, Camera const* camera);

Rectf GetCameraOrWindowRect (const Camera* camera);

#endif
