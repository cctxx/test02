#include "UnityPrefix.h"
#include "GizmoDrawers.h"
#include "GizmoUtil.h"
#include "GizmoRenderer.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Projector.h"
#include "Runtime/Camera/OcclusionArea.h"
#include "Runtime/Camera/OcclusionPortal.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Dynamics/CharacterController.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Dynamics/BoxCollider.h"
#include "Runtime/Dynamics/CapsuleCollider.h"
#include "Runtime/Dynamics/SphereCollider.h"
#include "Runtime/Filters/Particles/ParticleRenderer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Dynamics/HingeJoint.h"
#include "Runtime/Dynamics/RaycastCollider.h"
#include "Runtime/Dynamics/WheelCollider.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Audio/AudioReverbZone.h"
#include "Runtime/Audio/AudioSource.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Terrain/Wind.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"
#include "Runtime/Camera/OcclusionArea.h"
#include "Runtime/NavMesh/NavMeshAgent.h"
#include "Runtime/NavMesh/NavMeshObstacle.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxConvexMesh.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxConvexMeshDesc.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxTriangleMesh.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxTriangleMeshDesc.h"
#include "Editor/Src/OcclusionCullingVisualizationState.h"
#include "Editor/Src/ParticleSystem/ParticleSystemEditor.h"
#include "Editor/Src/PrefKeys.h"
#include "Editor/Src/PrefKeys.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Scripting/Scripting.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/BoxCollider2D.h"
#include "Runtime/Physics2D/CircleCollider2D.h"
#include "Runtime/Physics2D/EdgeCollider2D.h"
#include "Runtime/Physics2D/PolygonColliderBase2D.h"
#endif

using namespace std;
using namespace Unity;

// Should match same colors in LightEditor.cs!
PREFCOLOR (kGizmoLight, 0xFEFD8880);
PREFCOLOR (kGizmoDisabledLight, 0x87743280);
// Should match same colors in AudioReverbZoneEditor.cs!
PREFCOLOR (kGizmoAudio, 0x80B3FF80);
PREFCOLOR (kGizmoDisabledAudio, 0x4D669980);
// Should match s_BoundingBoxHandleColor in EditorHandles.txt 
PREFCOLOR (kGizmoBoundingBox, 0xFFFFFF80);
PREFCOLOR (kGizmoAnimation3D, 0x00D42380);
PREFCOLOR (kGizmoCollider, 0x91F48BC0);
PREFCOLOR (kGizmoColliderDisabled, 0x54C84A64);
PREFCOLOR (kGizmoJointAxes, 0xEA9837FF);
PREFCOLOR (kGizmoJointAxes2, 0x23572AFF);
// Should match same colors in CameraEditor.cs
PREFCOLOR (kGizmoCamera, 0xE9E9E980);
PREFCOLOR (kGizmoWind, 0x3588FFFF);
// Should match colors in OcclusionAreaEditor.cs
PREFCOLOR (kGizmoOcclusionArea, 0x91F48BC0);
PREFCOLOR (kGizmoOcclusionAreaSolid, 0x91F48B20);

PREFCOLOR (kGizmoRenderLocalBoundingBox, 0xA1F48BC0);
PREFCOLOR (kGizmoRenderWorldBoundingBox, 0xFEFD8880);
PREFCOLOR (kGizmoRenderBonesBounds, 0x62F2FF80);
PREFCOLOR (kGizmoRenderBones, 0xFF0000FF);


ColorRGBAf GetPrefColor_kGizmoCollider() { return kGizmoCollider; }
ColorRGBAf GetPrefColor_kGizmoColliderDisabled() { return kGizmoColliderDisabled; }
ColorRGBAf GetPrefColor_kGizmoJointAxes() { return kGizmoJointAxes; }
ColorRGBAf GetPrefColor_kGizmoJointAxes2() { return kGizmoJointAxes2; }

static void DrawAnimationCurve3D (const AnimationCurveVec3& pos, const pair<float, float>& range);

static const char* kDrawGizmosSelected = "OnDrawGizmosSelected";

#define kGizmoIconName " Gizmo"


bool HasPickGizmo (Object& o, int options, void*)
{
	AssertIf (dynamic_pptr_cast<const Unity::Component*> (&o) == NULL);
	const Unity::Component& light = static_cast<const Unity::Component&> (o);
	string name = o.GetClassName ();
	Texture2D* tex = Texture2DNamed( name + kGizmoIconName );
	return light.QueryComponent (Transform) && tex;
}

static ColorRGBAf RemapLightColor (const ColorRGBAf& src)
{
	float max = std::max(src.r, src.g);
	max = std::max(max, src.b);

	ColorRGBAf color = src;
	if (max > 0)
	{
		float mult = 1.0f/max;	
		color.r *= mult;
		color.g *= mult;
		color.b *= mult;
	}
	else
	{
		color = ColorRGBAf (1.f, 1.f, 1.f);
	}
	return color;
}

void DrawPickGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const Unity::Component*> (&o) == NULL);
	const Unity::Component& component = static_cast<const Unity::Component&> (o);
	const Transform& transform = component.GetComponent (Transform);

	Vector3f pos = transform.GetPosition ();
	string name = o.GetClassName ();
	ColorRGBA32 tint(255,255,255,255);

	// Special handling for lights
	if (o.GetClassID() == ClassID (Light))
	{
		//Use this section when gizmo icons are ready
		Light* light = (Light*)&o;
 		switch (light->GetType ())
 		{
			case kLightSpot:		name = "SpotLight"; break;
			case kLightDirectional: name = "DirectionalLight"; break;
			case kLightPoint:		name = "PointLight"; break;
			case kLightArea:		name = "AreaLight"; break;
			default: ErrorString("Unhandled LightType enum value "); break;
 		}
		tint = RemapLightColor (light->GetColor());

		const Light* mainlight = GetLightManager().GetLastMainLight();
		if (mainlight && mainlight == &o)
			name = "Main Light";
	}

	DrawIcon (pos, name + kGizmoIconName, true, tint);
}






void DrawLightGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const Light*> (&o) == NULL);
	const Light& light = (const Light&)o;
	const Transform& transform = light.GetComponent (Transform);
	
	gizmos::g_GizmoColor = light.GetEnabled () ? kGizmoLight : kGizmoDisabledLight;
	if( light.GetType() == kLightDirectional )
	{
		gizmos::BeginGizmo( transform.GetPosition() );
		SetGizmoMatrix(transform.GetLocalToWorldMatrixNoScale());
		
		float size = CalcHandleSize( transform.GetPosition() );		
		float radius = size * 0.2F;
		DrawWireDisk(Vector3f(0,0,0), Vector3f(0,0,1), radius);
		Vector3f pos[] = {
			Vector3f (radius, 0, 0), Vector3f (-radius, 0, 0), Vector3f (0, radius, 0), Vector3f (0, -radius, 0),
		    Normalize(Vector3f (1, 1, 0)) * radius, Normalize(Vector3f (1, -1, 0)) * radius,
		    Normalize(Vector3f (-1, 1, 0)) * radius, Normalize(Vector3f (-1, -1, 0)) * radius, Vector3f (0, 0, 0) };
		for (int i=0;i<sizeof (pos) / sizeof(Vector3f);i++)
		{
			DrawLine(pos[i], pos[i] + Vector3f(0,0,size));	
		}
	}
	else if (light.GetType() == kLightPoint)
	{
		Matrix4x4f noScaleMatrix;
		Vector3f center = Vector3f(0,0,0);
		noScaleMatrix.SetTranslate(center);
		gizmos::BeginGizmo( center );
		SetGizmoMatrix(noScaleMatrix);
		
		DrawWireSphereTwoShaded (transform.GetPosition(), light.GetRange(), Quaternionf::identity());
	}
	else if (light.GetType() == kLightSpot)
	{
		gizmos::BeginGizmo( transform.GetPosition() );
		SetGizmoMatrix(transform.GetLocalToWorldMatrixNoScale());
		
		DrawCircleFrustum(Vector3f::zero, light.GetSpotAngle(), light.GetRange());
	}
	
}

void DrawAudioReverbZoneGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const AudioReverbZone*> (&o) == NULL);
	const AudioReverbZone& zone = (const AudioReverbZone&)o;
	const Transform& transform = zone.GetComponent (Transform);
	
	Matrix4x4f noScaleMatrix;
	Vector3f center = Vector3f(0,0,0);
	noScaleMatrix.SetTranslate(center);
	gizmos::BeginGizmo( center );
	SetGizmoMatrix(noScaleMatrix);
	
	gizmos::g_GizmoColor = zone.GetEnabled () ? kGizmoAudio : kGizmoDisabledAudio;
	DrawWireSphereTwoShaded (transform.GetPosition(), zone.GetMinDistance(), Quaternionf::identity());
	DrawWireSphereTwoShaded (transform.GetPosition(), zone.GetMaxDistance(), Quaternionf::identity());
}

void DrawAudioSourceGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const AudioSource*> (&o) == NULL);
	const AudioSource& source = (const AudioSource&)o;
	const Transform& transform = source.GetComponent (Transform);
	
	Matrix4x4f noScaleMatrix;
	Vector3f center = Vector3f(0,0,0);
	noScaleMatrix.SetTranslate(center);
	gizmos::BeginGizmo( center );
	SetGizmoMatrix(noScaleMatrix);
	
	gizmos::g_GizmoColor = source.GetEnabled () ? kGizmoAudio : kGizmoDisabledAudio;
	DrawWireSphereTwoShaded (transform.GetPosition(), source.GetMinDistance(),  Quaternionf::identity());
	DrawWireSphereTwoShaded (transform.GetPosition(), source.GetMaxDistance(),  Quaternionf::identity());
}


#if ENABLE_2D_PHYSICS
void Draw2DColliderGizmo (Object& o, int options, void* userData)
{
	// Ignore if not a 2D collider.
	const Collider2D* collider = dynamic_pptr_cast<const Collider2D*> (&o);
	if (!collider)
		return;

	// Ignore if no shapes present.
	if (collider->GetShapeCount () == 0)
		return;

	const b2Fixture* firstFixture = collider->GetShape();


	const Transform& transform = collider->GetComponent (Transform);
	Vector3f pos = transform.GetPosition();
	gizmos::BeginGizmo (pos);

	gizmos::g_GizmoColor = kGizmoCollider;
	if (!collider->GetEnabled ())
		gizmos::g_GizmoColor = kGizmoColliderDisabled;

	Matrix4x4f matrix;
	Quaternionf rot = Quaternionf::identity();
	const b2Body* body = firstFixture->GetBody();
	// Read pose from box2d, so it always represents exactly what box2d uses.
	if (body)
	{
		pos.x = body->GetPosition().x;
		pos.y = body->GetPosition().y;
		rot = EulerToQuaternion(Vector3f(0,0,body->GetAngle()));
	}
	matrix.SetTR(pos, rot);
	SetGizmoMatrix(matrix);

	// Fetch fixture.
	const Collider2D::FixtureArray& fixtures = collider->GetShapes();

	// Circle collider.
	if (dynamic_pptr_cast<CircleCollider2D*> (&o))
	{
		Assert(firstFixture->GetType() == b2Shape::e_circle);
		const b2CircleShape* shape = (b2CircleShape*)firstFixture->GetShape();
		if (shape)
			DrawWireArc (Vector3f(shape->m_p.x,shape->m_p.y,0), Vector3f(0,0,1), Vector3f(1,0,0), 360.0f, shape->m_radius);

		return;
	}

	// Edge collider.
	if (dynamic_pptr_cast<EdgeCollider2D*> (&o))
	{
		Assert(firstFixture->GetType() == b2Shape::e_chain);
		const b2ChainShape* shape = (b2ChainShape*)firstFixture->GetShape();

		const int pointCount = shape->m_count;
		for (int i = 0; i < pointCount-1; ++i)
		{
			const b2Vec2& v1 = shape->m_vertices[i];
			const b2Vec2& v2 = shape->m_vertices[i+1];
			DrawLine (Vector3f(v1.x,v1.y,0), Vector3f(v2.x,v2.y,0));
		}
		return;
	}

	// Box/Polygon/Sprite collider.
	if (dynamic_pptr_cast<PolygonColliderBase2D*> (&o) ||
		dynamic_pptr_cast<BoxCollider2D*> (&o) )
	{
		for (Collider2D::FixtureArray::const_iterator it = fixtures.begin(); it != fixtures.end(); ++it)
		{
			const b2Fixture* fixture = *it;
			Assert (fixture->GetType() == b2Shape::e_polygon);

			const b2PolygonShape* shape = static_cast<const b2PolygonShape*>(fixture->GetShape());
			const int pointCount = shape->m_count;
			for (int i = 0, j = pointCount-1; i < pointCount; j = i++) // edge enum trick http://nothings.org/computer/edgeenum.html
			{
				b2Vec2 v1 = shape->m_vertices[i];
				b2Vec2 v2 = shape->m_vertices[j];
				DrawLine (Vector3f(v1.x,v1.y,0), Vector3f(v2.x,v2.y,0));
			}
		}
		return;
	}
}
#endif // #if ENABLE_2D_PHYSICS

void DrawCameraGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const Camera*> (&o) == NULL);
	const Camera& camera = static_cast<const Camera&> (o);
	if( !camera.IsValidToRender() )
		return;

	void* params[] = { Scripting::ScriptingWrapperFor(&o) };
	CallStaticMonoMethod("CameraEditor","RenderGizmo", params);
}

void DrawProjectorGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const Projector*> (&o) == NULL);
	const Projector& projector = static_cast<const Projector&> (o);
	const Transform& transform = projector.GetComponent (Transform);
	gizmos::BeginGizmo( transform.GetPosition() );
	gizmos::g_GizmoColor = kGizmoCamera;
	
	Matrix4x4f inverse;
	Matrix4x4f::Invert_Full( projector.GetProjectorToPerspectiveMatrix(), inverse );

	float xmin = -1.0F;
	float xmax = 1.0F;
	float ymin = -1.0F;
	float ymax = 1.0F;

	Vector3f lbn, rbn, ltn, rtn, lbf, rbf, ltf, rtf;
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmin, ymax, -1.0), lbn );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmax, ymax, -1.0), rbn );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmin, ymin, -1.0), ltn );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmax, ymin, -1.0), rtn );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmin, ymax, 1), lbf );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmax, ymax, 1), rbf );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmin, ymin, 1), ltf );
	inverse.PerspectiveMultiplyPoint3( Vector3f (xmax, ymin, 1), rtf );
	
	// Front rectangle
	DrawLine (lbn, rbn);
	DrawLine (ltn, rtn);
	DrawLine (lbn, ltn);
	DrawLine (rbn, rtn);
	// back rectangle
	DrawLine (lbf, rbf);
	DrawLine (ltf, rtf);
	DrawLine (lbf, ltf);
	DrawLine (rbf, rtf);
	// near->far DrawLines
	DrawLine (lbn, lbf);
	DrawLine (ltn, ltf);
	DrawLine (rbn, rbf);
	DrawLine (rtn, rtf);
}

void DrawOcclusionAreaGizmo (Object& o, int options, void* userData)
{
	if (!GetOcclusionCullingVisualization()->GetShowOcclusionCulling() || !GetOcclusionCullingVisualization()->GetShowPreVis())
		return;
	
	AssertIf (dynamic_pptr_cast<const OcclusionArea*> (&o) == NULL);
	
	const Transform& transform = static_cast<const Unity::Component&> (o).GetComponent (Transform);
	gizmos::BeginGizmo( transform.GetPosition() );
	gizmos::g_GizmoColor = kGizmoOcclusionAreaSolid;
	
	// PSV Volumes ignore rotation
	Matrix4x4f noScaleMatrix = Matrix4x4f::identity;
	
	const OcclusionArea* volume = dynamic_pptr_cast<const OcclusionArea*> (&o);
	if (volume)
	{
		SetGizmoMatrix (noScaleMatrix);
		DrawCube (noScaleMatrix.InverseMultiplyPoint3Affine (volume->GetGlobalCenter ()), volume->GetGlobalExtents () * 2.0F);
		DrawWireCube (noScaleMatrix.InverseMultiplyPoint3Affine (volume->GetGlobalCenter ()), volume->GetGlobalExtents () * 2.0F);
	}
}
void DrawOcclusionAreaGizmoSelected (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const OcclusionArea*> (&o) == NULL);
	
	const Transform& transform = static_cast<const Unity::Component&> (o).GetComponent (Transform);
	gizmos::BeginGizmo( transform.GetPosition() );
	gizmos::g_GizmoColor = kGizmoOcclusionArea;
	
	// PSV Volumes ignore rotation
	Matrix4x4f noScaleMatrix = Matrix4x4f::identity;
	
	const OcclusionArea* volume = dynamic_pptr_cast<const OcclusionArea*> (&o);
	if (volume)
	{
		SetGizmoMatrix (noScaleMatrix);
		DrawWireCube (noScaleMatrix.InverseMultiplyPoint3Affine (volume->GetGlobalCenter ()), volume->GetGlobalExtents () * 2.0F);
	}
}
void DrawAnimationGizmo (Object& o, int options, void* userData)
{
//	const Transform& transform = static_cast<const Unity::Component&> (o).GetComponent (Transform);
//	gizmos::BeginGizmo(transform.GetPosition());

	/*AssertIf (dynamic_pptr_cast<const Animation*> (&o) == NULL);
	Animation& animated = static_cast<Animation&> (o);
	AnimationClip* anim = animated.GetClip ();
	if (anim == NULL)
		return;
	
	Transform& root = animated.GetComponent(Transform);
	gizmos::g_GizmoColor = kGizmoAnimation3D;
	
	AnimationClip::Vector3Curves& positions = anim->GetPositionCurves();
	for (AnimationClip::Vector3Curves::iterator i=positions.begin();i != positions.end();i++)
	{
		Transform* transform;
		if (i->path.empty())
			transform = &root;
		else
			transform = FindRelativeTransformWithPath(root, i->path.c_str());
		
		if (transform)
		{	
			// Setup transform to parent space transform matrix
			transform = transform->GetParent();
			if (transform)
				SetGizmoMatrix(transform->GetLocalToWorldMatrix());
			else
				SetGizmoMatrix(Matrix4x4f::identity);

			DrawAnimationCurve3D(i->curve, i->curve.GetRange());
		}
	}*/
}



static void DrawAnimationCurve3D (const AnimationCurveVec3& pos, const pair<float, float>& range)
{
	if (pos.GetKeyCount () == 0)
		return;
	enum { kSubdivisionsPerFrame = 10 };
	
	// Draw keyframes as large dots
	for (int k=0;k<pos.GetKeyCount ()-1;k++)
	{
		float begin = pos.GetKey (k).time;
		float end = pos.GetKey (k+1).time;
		
		Vector3f last (pos.Evaluate (begin));

		for (int i=0;i<=kSubdivisionsPerFrame;i++)
		{
			float t = (float)i / float(kSubdivisionsPerFrame);
			t = t * (end - begin) + begin;
			Vector3f cur = pos.Evaluate (t);
			DrawLine(last, cur);
			last = cur;
		}
	}
	
	for (int i=0;i<pos.GetKeyCount ();i++)
	{
		// Draw point
		Vector3f cur = pos.GetKey (i).value;
		cur = GetGizmoMatrix().MultiplyPoint3(cur);
		DrawIcon(cur, "curvekeyframe");
	}
}

// Wind arrow vertices..
Vector3f kArrowPoints[] = {
	// XZ
	Vector3f(0,0,1.5f),
	Vector3f( 1.0f,0.0f, 0.5f), Vector3f( 0.5f,0.0f,0.5f), Vector3f( 0.5f,0.0f,-1.0f),
	Vector3f(-0.5f,0.0f,-1.0f), Vector3f(-0.5f,0.0f,0.5f), Vector3f(-1.0f,0.0f, 0.5f),
	Vector3f(0,0,1.5f),
	// YZ
	Vector3f(0,0,1.5f),
	Vector3f(0.0f, 1.0f, 0.5f), Vector3f(0.0f, 0.5f,0.5f), Vector3f(0.0f, 0.5f,-1.0f),
	Vector3f(0.0f,-0.5f,-1.0f), Vector3f(0.0f,-0.5f,0.5f), Vector3f(0.0f,-1.0f, 0.5f),
	Vector3f(0,0,1.5f)
};

const int kArrowPointCount = sizeof(kArrowPoints) / sizeof(Vector3f);

void DrawWindGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const WindZone*> (&o) == NULL);
	const WindZone& wind = static_cast<const WindZone&> (o);

	const Transform& transform = wind.GetComponent (Transform);
	const Vector3f position = transform.GetPosition();
	gizmos::BeginGizmo(position);

	gizmos::g_GizmoColor = kGizmoWind;

	if (wind.GetMode() == WindZone::Spherical)
	{
		// Draw a sphere
		DrawWireSphere(position, wind.GetRadius());
	}
	else
	{
		const Vector3f direction = transform.TransformDirection(Vector3f::zAxis);

		Vector3f up = Vector3f::yAxis;
		Vector3f left = Cross(direction, up);
		up = Cross(left, direction);

		left = Normalize(left);
		up = Normalize(up);

		Vector3f lastPoint;
		for (int i = 0; i < kArrowPointCount; ++i)
		{
			const Vector3f& p = kArrowPoints[i];
			const Vector3f point = position + (p.x * left + p.y * up + p.z * direction) * 2;

			if (i > 0)
				DrawLine(lastPoint, point);

			lastPoint = point;
		}
	}
}

bool CanDrawParticleSystemIcon (Object& object, int options, void* userData)
{
	const Unity::Component& component = static_cast<const Unity::Component&> (object);
	if (component.GetClassID () != ClassID (ParticleSystem))
		return false;

	const Transform& transform = component.GetGameObject().GetComponent(Transform);
	Transform* parentTransform =  transform.GetParent ();
	return (parentTransform == NULL || parentTransform->GetGameObject().QueryComponent(ParticleSystem) == NULL);
}

void DrawParticleSystemIcon (Object& object, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<const Unity::Component*> (&object) == NULL);
	const Unity::Component& component = static_cast<const Unity::Component&> (object);
	ParticleSystem* particleSystem = component.GetGameObject().QueryComponent(ParticleSystem);

	// We only show icons for systems that are stopped (not for playing and paused systems)
	if (particleSystem && particleSystem->IsStopped ())
	{
		const Transform& transform = component.GetComponent (Transform);
		Texture2D* tex = Texture2DNamed( std::string("ParticleSystem") + kGizmoIconName );
		if (tex)
			gizmos::AddIcon (transform.GetPosition(), tex);
	}
}


bool CanDrawGameObjectIcon (Object& object, int options, void* userData)
{
	GameObject& go= (GameObject&)object;
	return go.GetIcon ().IsValid();
}

void DrawGameObjectIcon (Object& object, int options, void* userData)
{
	GameObject& go= (GameObject&)object;
	if (Texture2D* texture = go.GetIcon ())
	{
		const Transform& transform = go.GetComponent(Transform);
		gizmos::AddIcon (transform.GetPosition(), texture, go.GetName ());
	}
}

void DrawOcclusionPortal (Object& object, int options, void* userData)
{
	OcclusionPortal& portal = (OcclusionPortal&)object;
	Transform& transform = portal.GetComponent(Transform);

	gizmos::BeginGizmo( transform.TransformPoint(portal.GetCenter()) );
	gizmos::g_GizmoColor = kGizmoOcclusionAreaSolid;
	
	SetGizmoMatrix (transform.GetLocalToWorldMatrix());
	DrawCube (portal.GetCenter(), portal.GetSize());
	DrawWireCube (portal.GetCenter(), portal.GetSize());
}

bool CanDrawMonoScriptIcon (Object& object, int options, void* userData)
{
	MonoBehaviour& behaviour = (MonoBehaviour&)object;
	if (MonoScript* monoScript = behaviour.GetScript ())
		return monoScript->GetIcon().IsValid ();

	return false;
}
void DrawMonoScriptIcon (Object& object, int options, void* userData)
{
	MonoBehaviour& behaviour = (MonoBehaviour&)object;
	if (MonoScript* monoScript = behaviour.GetScript ())
	{
		Texture2D* texture = dynamic_pptr_cast<Texture2D*> (monoScript->GetIcon ());
		if (texture)
		{
			const char* text = 0;
			if (GameObject* go = behaviour.GetGameObjectPtr ())
				text = go->GetName();
			const Transform& transform = behaviour.GetComponent(Transform);
			gizmos::AddIcon (transform.GetPosition(), texture, text);
		}
	}
}


bool CanDrawMonoGizmo (Object& object, int options, void*)
{
	MonoBehaviour& behaviour = (MonoBehaviour&)object;
	if (behaviour.GetInstance ())
		return behaviour.GetMethod(MonoScriptCache::kDrawGizmos) != NULL;
	else
		return false;
}
void DrawMonoGizmo (Object& object, int options, void*)
{
	MonoBehaviour& behaviour = (MonoBehaviour&)object;
	if (behaviour.GetInstance ())
	{
		const Transform& transform = behaviour.GetComponent(Transform);
		gizmos::g_GizmoColor = ColorRGBAf(1.0f,1.0f,1.0f,1.0f);
		gizmos::BeginGizmo( transform.GetPosition() );
		ClearGizmoMatrix ();

		behaviour.CallMethodInactive(behaviour.GetMethod(MonoScriptCache::kDrawGizmos));
	}
}

bool CanDrawMonoGizmoSelected (Object& object, int options, void*)
{
	MonoBehaviour& behaviour = (MonoBehaviour&)object;
	if (!behaviour.GetInstance ())
		return false;

	return GetScriptingMethodRegistry().GetMethod(behaviour.GetClass(), kDrawGizmosSelected, ScriptingMethodRegistry::kWithoutArguments) != NULL;
}

void DrawMonoGizmoSelected (Object& object, int options, void*)
{
	MonoBehaviour& behaviour = (MonoBehaviour&)object;
	if (behaviour.GetInstance ())
	{
		const Transform& transform = behaviour.GetComponent(Transform);
		gizmos::g_GizmoColor = ColorRGBAf(1.0f,1.0f,1.0f,1.0f);
		gizmos::BeginGizmo( transform.GetPosition() );
		ClearGizmoMatrix ();
		behaviour.CallMethodInactive(kDrawGizmosSelected);
	}
}

void DrawNavMeshAgentGizmo (Object& o, int options, void* userData)
{
	const NavMeshAgent* agent = dynamic_pptr_cast<const NavMeshAgent*> (&o);
	if (agent)
	{
		const Transform& transform = static_cast<const Unity::Component&> (o).GetComponent (Transform);
		
		// Draw shape
		const float radius = std::max (0.0f, agent->CalculateScaledRadius());
		const float height = std::max (0.0f, agent->CalculateScaledHeight());
		const Vector3f groundPos = agent->GetGroundPositionFromTransform();
		Matrix4x4f noScaleMatrix;
		Vector3f center = Vector3f (groundPos.x, groundPos.y + 0.5f*height, groundPos.z);
		noScaleMatrix.SetTranslate (center);
		gizmos::BeginGizmo (center);
		gizmos::g_GizmoColor = (agent->GetEnabled ()) ? kGizmoCollider : kGizmoColliderDisabled;

		SetGizmoMatrix (noScaleMatrix);
		DrawWireCylinder (noScaleMatrix.InverseMultiplyPoint3Affine (center), radius, height);
		
		// Draw extra debug info
		gizmos::BeginGizmo (transform.GetPosition());
		noScaleMatrix = transform.GetLocalToWorldMatrixNoScale ();
		SetGizmoMatrix (noScaleMatrix);
		DrawNavMeshAgent (*agent);
	}
}

void DrawNavMeshObstacleGizmo (Object& o, int options, void* userData)
{
	const NavMeshObstacle* obstacle = dynamic_pptr_cast<const NavMeshObstacle*> (&o);
	if (obstacle)
	{		
		// Draw shape
		const Vector3f dimensions = obstacle->GetScaledDimensions ();
		const float radius = std::max(0.0f, dimensions.x);
		const float height = std::max(0.0f, dimensions.y);
		const Vector3f groundPos = obstacle->GetPosition();
		Matrix4x4f noScaleMatrix;
		Vector3f center = Vector3f (groundPos.x, groundPos.y + 0.5f*height, groundPos.z);
		noScaleMatrix.SetTranslate (center);
		gizmos::BeginGizmo (center);
		gizmos::g_GizmoColor = (obstacle->GetEnabled ()) ? kGizmoCollider : kGizmoColliderDisabled;
		SetGizmoMatrix (noScaleMatrix);
		DrawWireCylinder (noScaleMatrix.InverseMultiplyPoint3Affine (center), radius, height);
	}
}

void DrawDebugRendererBoundsGizmos (Object& o, int options, void* userData)
{
	Renderer& renderer = static_cast<Renderer&> (o);
	gizmos::BeginGizmo( renderer.GetTransform().GetPosition() );

	const TransformInfo& info = renderer.GetTransformInfo();

	// Local Space -> Green
	gizmos::g_GizmoColor = kGizmoRenderLocalBoundingBox;
	SetGizmoMatrix (info.worldMatrix);
	DrawWireCube (info.localAABB.GetCenter(), info.localAABB.GetExtent() * 2);

	// World Space -> Yellow
	gizmos::g_GizmoColor = kGizmoRenderWorldBoundingBox;
	SetGizmoMatrix (Matrix4x4f::identity);
	DrawWireCube (info.worldAABB.GetCenter(), info.worldAABB.GetExtent() * 2);
}

void DrawDebugRendererBonesBoundsGizmos (Object& o, int options, void* userData)
{
	SkinnedMeshRenderer& renderer = static_cast<SkinnedMeshRenderer&> (o);
	gizmos::BeginGizmo( renderer.GetTransform().GetPosition() );
    
    // Bones Bounds -> Light Blue
	gizmos::g_GizmoColor = kGizmoRenderBonesBounds;
    Mesh* skinnedMesh = renderer.GetMesh();
	if (skinnedMesh == NULL)
		return;
    
	size_t bindposeCount = renderer.GetBindposeCount();

	const Mesh::AABBContainer& bounds = skinnedMesh->GetCachedBonesBounds ();
	Assert (bindposeCount == bounds.size());

	Matrix4x4f* poses;
    ALLOC_TEMP(poses, Matrix4x4f, bindposeCount);
    if (!renderer.CalculateAnimatedPoses(poses, bindposeCount))
        return;
    
    for(int i=0;i<bindposeCount;i++)
	{
		const AABB& aabb = bounds[i];
		if (!aabb.IsValid())
			continue;
        SetGizmoMatrix (poses[i]);
        DrawWireCube (aabb.GetCenter(), aabb.GetExtent() * 2);
	}
}

