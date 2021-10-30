#include "UnityPrefix.h"
#include "CameraUtil.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Rect.h"
#include "RenderManager.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Geometry/Plane.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/ScreenManager.h"

void FlipScreenRectIfNeeded( const GfxDevice& device, int screenviewcoord[4] )
{
	// Flip viewport rect vertically for D3D or dashboard widgets.
	// But only do it when rendering to screen, not a texture!
	if( (!device.UsesOpenGLTextureCoords() || device.GetInvertProjectionMatrix()) && device.GetActiveRenderTexture() == NULL )
	{
		int height;
		#if GFX_SUPPORTS_D3D9
		// On D3D, always use height of current actual render target. Others may be off, particularly in editor's game view
		if (device.GetRenderer() == kGfxRendererD3D9 || device.GetRenderer() == kGfxRendererD3D11)
			height = device.GetCurrentTargetHeight();
		else
		#endif
			// Use screen height, not render manager's window height. In editor's GameView, the window height might be smaller.
			height = GetScreenManager().GetHeight();

		int miny = height - screenviewcoord[1];
		int maxy = height - (screenviewcoord[3] + screenviewcoord[1]);
		if (maxy < miny)
			std::swap(maxy, miny);
		miny = std::max(miny, 0);
		screenviewcoord[1] = miny;
		screenviewcoord[3] = maxy - miny;
		DebugAssertIf( screenviewcoord[1] < 0 || screenviewcoord[3] < 0 );
	}
	#if GFX_USES_VIEWPORT_OFFSET
	if( device.GetActiveRenderTexture() == NULL )
	{
		float xOffs, yOffs;
		device.GetViewportOffset(xOffs, yOffs);
		screenviewcoord[0] += xOffs;
		screenviewcoord[1] += yOffs;
	}
	#endif
	#if UNITY_WP8
	ScreenOrientation const screenOrientation = GetScreenManager().GetScreenOrientation();
	bool renderToTexture = device.GetActiveRenderTexture() != NULL;
	bool rotated = (screenOrientation == ScreenOrientation::kLandscapeLeft) || (screenOrientation == ScreenOrientation::kLandscapeRight);
	if (rotated && !renderToTexture)
	{
		float x = screenviewcoord[0];
		float y = screenviewcoord[1];

		if (screenOrientation == ScreenOrientation::kLandscapeRight)
		{
			float width = screenviewcoord[2];
			float targetWidth = device.GetActiveRenderTexture() ? device.GetCurrentTargetWidth() : GetScreenManager().GetWidth();
			screenviewcoord[0] = y;
			screenviewcoord[1] = targetWidth - x - width;
		}
		else if (screenOrientation == ScreenOrientation::kLandscapeLeft)
		{
			float height = screenviewcoord[3];
			float targetHeight = device.GetActiveRenderTexture() ? device.GetCurrentTargetHeight() : GetScreenManager().GetHeight();
			screenviewcoord[0] = targetHeight - y - height;
			screenviewcoord[1] = x;
		}

		std::swap(screenviewcoord[2], screenviewcoord[3]);
	}
	#endif
}

#if UNITY_WP8

void RotateScreenIfNeeded(Matrix4x4f& projection)
{
	ScreenOrientation const screenOrientation = GetScreenManager().GetScreenOrientation();
	if (screenOrientation == ScreenOrientation::kLandscapeLeft)
	{
		Vector4f const x = projection.GetRow(0);
		Vector4f const y = projection.GetRow(1);
		projection.SetRow(0, y);
		projection.SetRow(1, -x);
	}
	else if (screenOrientation == ScreenOrientation::kLandscapeRight)
	{
		Vector4f const x = projection.GetRow(0);
		Vector4f const y = projection.GetRow(1);
		projection.SetRow(0, -y);
		projection.SetRow(1, x);
	}
}

static void RotatePointIfNeeded(Vector3f& point, bool unrotate)
{
	ScreenOrientation const screenOrientation = GetScreenManager().GetScreenOrientation();
	if (screenOrientation == ScreenOrientation::kLandscapeLeft)
	{
		auto const x = point.x;
		auto const y = point.y;

		if (unrotate)
		{
			point.x = y;
			point.y = -x;
		}
		else
		{
			point.x = -y;
			point.y = x;
		}
	}
	else if (screenOrientation == ScreenOrientation::kLandscapeRight)
	{
		auto const x = point.x;
		auto const y = point.y;

		if (unrotate)
		{
			point.x = -y;
			point.y = x;
		}
		else
		{
			point.x = y;
			point.y = -x;
		}
	}
}

#endif

void CalcPixelMatrix (const Rectf& screenRect, Matrix4x4f &out) 
{
	out.SetOrtho( screenRect.x, screenRect.GetRight(), screenRect.y, screenRect.GetBottom(), -1.0f, 100.0f );
	#if UNITY_WP8
	RotateScreenIfNeeded(out);
	#endif
}

void ApplyTexelOffsetsToPixelMatrix( bool invertYTexelOffset, Matrix4x4f& matrix )
{
	float offsetX, offsetY;
	GetHalfTexelOffsets( offsetX, offsetY );
	if( invertYTexelOffset )
		offsetY = -offsetY;

	matrix.Get(0,3) -= offsetX * matrix.Get(0,0);
	matrix.Get(1,3) -= offsetY * matrix.Get(1,1);
}


void LoadPixelMatrix( const Rectf& screenRect, GfxDevice& device, bool setMatrix, bool invertYTexelOffset )
{
	Matrix4x4f ortho;
	CalcPixelMatrix( screenRect, ortho );
	ApplyTexelOffsetsToPixelMatrix( invertYTexelOffset, ortho );
	device.SetProjectionMatrix (ortho);
	if( setMatrix )
		device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity
}

void GetHalfTexelOffsets( float& outx, float& outy )
{
	GfxDevice& device = GetGfxDevice();
	if( device.UsesHalfTexelOffset() )
	{
		outx = 0.5f;
		outy = 0.5f;
		if( device.GetActiveRenderTexture() == NULL )
			outy = -outy;
	}
	else
	{
		outx = outy = 0.0f;
	}
}

void LoadFullScreenOrthoMatrix( float nearPlane, float farPlane, bool forceNoHalfTexelOffset )
{
	GfxDevice& device = GetGfxDevice();
	float offsetX, offsetY;
	if( device.UsesHalfTexelOffset() && !forceNoHalfTexelOffset)
	{
		// viewport of the device should always have the correct size of
		// the render target (both when rendering to screen and to a 
		// render texture), so calc half texel offset from that.
		int viewport[4];
		device.GetViewport(viewport);
		int width = viewport[2];
		int height = viewport[3];
		offsetX = width ? (0.5f / width) : 0.0f;
		offsetY = height ? (0.5f / height) : 0.0f;
		if( device.GetActiveRenderTexture() == NULL )
			offsetY = -offsetY;
	}
	else
	{
		offsetX = offsetY = 0.0f;
	}
	Matrix4x4f matrix;
	matrix.SetOrtho( offsetX, 1.0f + offsetX, offsetY, 1.0f + offsetY, nearPlane, farPlane );
	device.SetProjectionMatrix (matrix);
	device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity
}

void SetupPixelCorrectCoordinates()
{
	GfxDevice& device = GetGfxDevice();

	int viewcoords[4];
	Rectf r = GetRenderManager ().GetWindowRect();
	RectfToViewport( r, viewcoords );
	FlipScreenRectIfNeeded( device, viewcoords );
	device.SetViewport( viewcoords[0], viewcoords[1], viewcoords[2], viewcoords[3] );

	LoadPixelMatrix( r, device, true, false );
}

void RectfToViewport( const Rectf& r, int viewPort[4] )
{
	// We have to take care that the viewport doesn't exceed the buffer size (case 569703).
	// Bad rounding to integer makes D3D11 crash and burn.
	viewPort[0] = RoundfToInt (r.x);
	viewPort[1] = RoundfToInt (r.y);
	viewPort[2] = RoundfToIntPos (r.GetRight ()) - viewPort[0];
	viewPort[3] = RoundfToIntPos (r.GetBottom ()) - viewPort[1];
}


bool CameraProject( const Vector3f& p, const Matrix4x4f& cameraToWorld, const Matrix4x4f& worldToClip, const int viewport[4], Vector3f& outP )
{
	Vector3f clipPoint;
	if( worldToClip.PerspectiveMultiplyPoint3( p, clipPoint ) )
	{
		Vector3f cameraPos = cameraToWorld.GetPosition();
		Vector3f dir = p - cameraPos;
		// The camera/projection matrices follow OpenGL convention: positive Z is towards the viewer.
		// So negate it to get into Unity convention.
		Vector3f forward = -cameraToWorld.GetAxisZ();
		float dist = Dot( dir, forward );

		#if UNITY_WP8
		RotatePointIfNeeded(clipPoint, false);
		#endif
		
		outP.x = viewport[0] + (1.0f + clipPoint.x) * viewport[2] * 0.5f;
		outP.y = viewport[1] + (1.0f + clipPoint.y) * viewport[3] * 0.5f;
		//outP.z = (1.0f + clipPoint.z) * 0.5f;
		outP.z = dist;

		return true;
	}
	
	outP.Set( 0.0f, 0.0f, 0.0f );
	return false;
}

bool CameraUnProject( const Vector3f& p, const Matrix4x4f& cameraToWorld, const Matrix4x4f& clipToWorld, const int viewport[4], Vector3f& outP )
{
	// pixels to -1..1
	Vector3f in;
	in.x = (p.x - viewport[0]) * 2.0f / viewport[2] - 1.0f;
	in.y = (p.y - viewport[1]) * 2.0f / viewport[3] - 1.0f;
	// It does not matter where the point we unproject lies in depth; so we choose 0.95, which
	// is further than near plane and closer than far plane, for precision reasons.
	// In a perspective camera setup (near=0.1, far=1000), a point at 0.95 projected depth is about
	// 5 units from the camera.
	in.z = 0.95f;

	#if UNITY_WP8
	RotatePointIfNeeded(in, true);
	#endif
	
	Vector3f pointOnPlane;
	if( clipToWorld.PerspectiveMultiplyPoint3( in, pointOnPlane ) )
	{
		// Now we have a point on the plane perpendicular to the viewing direction. We need to return the one that is on the line
		// towards this point, and at p.z distance along camera's viewing axis.
		Vector3f cameraPos = cameraToWorld.GetPosition();
		Vector3f dir = pointOnPlane - cameraPos;
		
		// The camera/projection matrices follow OpenGL convention: positive Z is towards the viewer.
		// So negate it to get into Unity convention.
		Vector3f forward = -cameraToWorld.GetAxisZ();
		float distToPlane = Dot( dir, forward );
		if( Abs(distToPlane) >= 1.0e-6f )
		{
			bool isPerspective = (clipToWorld.m_Data[3] != 0.0f || clipToWorld.m_Data[7] != 0.0f || clipToWorld.m_Data[11] != 0.0f || clipToWorld.m_Data[15] != 1.0f);
			if( isPerspective )
			{
				dir *= p.z / distToPlane;
				outP = cameraPos + dir;
			}
			else
			{
				outP = pointOnPlane - forward * (distToPlane - p.z);
			}
			return true;
		}
	}
	
	outP.Set( 0.0f, 0.0f, 0.0f );
	return false;
}

void SetGLViewport (const Rectf& pixelRect)
{
	Rectf tempPixelRect (pixelRect);
	int viewport[4];

	GfxDevice& device = GetGfxDevice();

#if UNITY_EDITOR
	// Handle game view's aspect ratio dropdown, but only if we're not rendering into a render
	// texture.
	if( device.GetActiveRenderTexture() == NULL )
	{
		Rectf renderRect = GetRenderManager().GetWindowRect();
		tempPixelRect.x += renderRect.x;
		tempPixelRect.y += renderRect.y;
		tempPixelRect.Clamp (renderRect);
	}
#endif
	
	viewport[0] = RoundfToInt( tempPixelRect.x );
	viewport[1] = RoundfToInt( tempPixelRect.y );
	viewport[2] = RoundfToIntPos( tempPixelRect.Width() );
	viewport[3] = RoundfToIntPos( tempPixelRect.Height() );
	FlipScreenRectIfNeeded( device, viewport );
	device.SetViewport( viewport[0], viewport[1], viewport[2], viewport[3] );
	
}

void ExtractProjectionPlanes( const Matrix4x4f& finalMatrix, Plane* outPlanes )
{
	float tmpVec[4];
	float otherVec[4];
	
	tmpVec[0] = finalMatrix.Get (3, 0);
	tmpVec[1] = finalMatrix.Get (3, 1);
	tmpVec[2] = finalMatrix.Get (3, 2);
	tmpVec[3] = finalMatrix.Get (3, 3);
	
	otherVec[0] = finalMatrix.Get (0, 0);
	otherVec[1] = finalMatrix.Get (0, 1);
	otherVec[2] = finalMatrix.Get (0, 2);
	otherVec[3] = finalMatrix.Get (0, 3);
	
	// left & right
	outPlanes[kPlaneFrustumLeft].SetABCD ( otherVec[0] + tmpVec[0],  otherVec[1] + tmpVec[1],  otherVec[2] + tmpVec[2],  otherVec[3] + tmpVec[3]);
	outPlanes[kPlaneFrustumLeft].NormalizeUnsafe();
	outPlanes[kPlaneFrustumRight].SetABCD (-otherVec[0] + tmpVec[0], -otherVec[1] + tmpVec[1], -otherVec[2] + tmpVec[2], -otherVec[3] + tmpVec[3]);
	outPlanes[kPlaneFrustumRight].NormalizeUnsafe();
	
	// bottom & top
	otherVec[0] = finalMatrix.Get (1, 0);
	otherVec[1] = finalMatrix.Get (1, 1);
	otherVec[2] = finalMatrix.Get (1, 2);
	otherVec[3] = finalMatrix.Get (1, 3);
	
	outPlanes[kPlaneFrustumBottom].SetABCD ( otherVec[0] + tmpVec[0],  otherVec[1] + tmpVec[1],  otherVec[2] + tmpVec[2],  otherVec[3] + tmpVec[3]);
	outPlanes[kPlaneFrustumBottom].NormalizeUnsafe();
	outPlanes[kPlaneFrustumTop].SetABCD (-otherVec[0] + tmpVec[0], -otherVec[1] + tmpVec[1], -otherVec[2] + tmpVec[2], -otherVec[3] + tmpVec[3]);
	outPlanes[kPlaneFrustumTop].NormalizeUnsafe();
	
	otherVec[0] = finalMatrix.Get (2, 0);
	otherVec[1] = finalMatrix.Get (2, 1);
	otherVec[2] = finalMatrix.Get (2, 2);
	otherVec[3] = finalMatrix.Get (2, 3);
	
	// near & far
	outPlanes[kPlaneFrustumNear].SetABCD ( otherVec[0] + tmpVec[0],  otherVec[1] + tmpVec[1],  otherVec[2] + tmpVec[2],  otherVec[3] + tmpVec[3]);
	outPlanes[kPlaneFrustumNear].NormalizeUnsafe();
	outPlanes[kPlaneFrustumFar].SetABCD (-otherVec[0] + tmpVec[0], -otherVec[1] + tmpVec[1], -otherVec[2] + tmpVec[2], -otherVec[3] + tmpVec[3]);
	outPlanes[kPlaneFrustumFar].NormalizeUnsafe();
}

void ExtractProjectionNearPlane( const Matrix4x4f& finalMatrix, Plane* outPlane )
{
	float tmpVec[4];
	float otherVec[4];
	
	tmpVec[0] = finalMatrix.Get (3, 0);
	tmpVec[1] = finalMatrix.Get (3, 1);
	tmpVec[2] = finalMatrix.Get (3, 2);
	tmpVec[3] = finalMatrix.Get (3, 3);
	
	otherVec[0] = finalMatrix.Get (2, 0);
	otherVec[1] = finalMatrix.Get (2, 1);
	otherVec[2] = finalMatrix.Get (2, 2);
	otherVec[3] = finalMatrix.Get (2, 3);
	
	// near
	outPlane->SetABCD ( otherVec[0] + tmpVec[0],  otherVec[1] + tmpVec[1],  otherVec[2] + tmpVec[2],  otherVec[3] + tmpVec[3]);
	outPlane->NormalizeUnsafe();
}


void SetClippingPlaneShaderProps()
{
	GfxDevice& device = GetGfxDevice();
	BuiltinShaderParamValues& params = device.GetBuiltinParamValues();

	const Matrix4x4f* viewMatrix = (const Matrix4x4f*)device.GetViewMatrix();
	const Matrix4x4f* deviceProjMatrix = (const Matrix4x4f*)device.GetDeviceProjectionMatrix();
	Matrix4x4f viewProj;
	MultiplyMatrices4x4 (deviceProjMatrix, viewMatrix, &viewProj);
	Plane planes[6];
	ExtractProjectionPlanes (viewProj, planes);
	params.SetVectorParam (kShaderVecCameraWorldClipPlanes0, (const Vector4f&)planes[0]);
	params.SetVectorParam (kShaderVecCameraWorldClipPlanes1, (const Vector4f&)planes[1]);
	params.SetVectorParam (kShaderVecCameraWorldClipPlanes2, (const Vector4f&)planes[2]);
	params.SetVectorParam (kShaderVecCameraWorldClipPlanes3, (const Vector4f&)planes[3]);
	params.SetVectorParam (kShaderVecCameraWorldClipPlanes4, (const Vector4f&)planes[4]);
	params.SetVectorParam (kShaderVecCameraWorldClipPlanes5, (const Vector4f&)planes[5]);
}


DeviceMVPMatricesState::DeviceMVPMatricesState()
{
	GfxDevice& device = GetGfxDevice();
	CopyMatrix(device.GetViewMatrix(), m_View.GetPtr());
	CopyMatrix(device.GetWorldMatrix(), m_World.GetPtr());
	CopyMatrix(device.GetProjectionMatrix(), m_Proj.GetPtr());
}

DeviceMVPMatricesState::~DeviceMVPMatricesState()
{
	GfxDevice& device = GetGfxDevice();
	device.SetViewMatrix(m_View.GetPtr());
	device.SetWorldMatrix(m_World.GetPtr());
	device.SetProjectionMatrix(m_Proj);
	SetClippingPlaneShaderProps();
}

DeviceViewProjMatricesState::DeviceViewProjMatricesState()
{
	GfxDevice& device = GetGfxDevice();
	CopyMatrix(device.GetViewMatrix(), m_View.GetPtr());
	CopyMatrix(device.GetProjectionMatrix(), m_Proj.GetPtr());
}

DeviceViewProjMatricesState::~DeviceViewProjMatricesState()
{
	GfxDevice& device = GetGfxDevice();
	device.SetViewMatrix(m_View.GetPtr());
	device.SetProjectionMatrix(m_Proj);
	SetClippingPlaneShaderProps();
}
