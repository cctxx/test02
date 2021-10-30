#ifndef CAMERA_UTIL_H
#define CAMERA_UTIL_H

#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Modules/ExportModules.h"

class GfxDevice;
class Vector3f;
class Plane;

void LoadPixelMatrix( const Rectf& screenRect, GfxDevice& device, bool setMatrix, bool invertYTexelOffset );
void CalcPixelMatrix (const Rectf& screenRect, Matrix4x4f &out);
void ApplyTexelOffsetsToPixelMatrix( bool invertYTexelOffset, Matrix4x4f& matrix );
void LoadFullScreenOrthoMatrix( float nearPlane = -1.0f, float farPlane = 100.0f, bool forceNoHalfTexelOffset = false );
void GetHalfTexelOffsets( float& outx, float& outy );
void SetupPixelCorrectCoordinates();

void RectfToViewport( const Rectf& r, int viewPort[4] );
void FlipScreenRectIfNeeded( const GfxDevice& device, int screenviewcoord[4] );
void SetGLViewport (const Rectf& pixelRect);

// World point to screen point
// p = world point
// outP = result (x, y = in pixels inside the viewport, z = world space distance from the camera)
//
// sets outP to (0,0,0) if fails.
bool CameraProject( const Vector3f& p, const Matrix4x4f& cameraToWorld, const Matrix4x4f& worldToClip, const int viewport[4], Vector3f& outP );

// Screen point to world point
// p = screen point (x, y = in pixels inside the viewport, z = world space distance from the camera)
//
// sets outP to (0,0,0) if fails.
bool CameraUnProject( const Vector3f& p, const Matrix4x4f& cameraToWorld, const Matrix4x4f& clipToWorld, const int viewport[4], Vector3f& outP );

// Extract frustum planes from Projection matrix
void EXPORT_COREMODULE ExtractProjectionPlanes (const Matrix4x4f& projection, Plane* planes);
void EXPORT_COREMODULE ExtractProjectionNearPlane (const Matrix4x4f& projection, Plane* outPlane);

void SetClippingPlaneShaderProps();


class DeviceMVPMatricesState {
public:
	DeviceMVPMatricesState ();
	~DeviceMVPMatricesState();
	const Matrix4x4f& GetView() const { return m_View; }
	const Matrix4x4f& GetProj() const { return m_Proj; }
private:
	Matrix4x4f	m_World, m_View, m_Proj;
};

class DeviceViewProjMatricesState {
public:
	DeviceViewProjMatricesState ();
	~DeviceViewProjMatricesState();
private:
	Matrix4x4f	m_View, m_Proj;
};

#endif
