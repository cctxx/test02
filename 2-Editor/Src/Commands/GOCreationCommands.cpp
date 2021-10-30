#include "UnityPrefix.h"
#include "Configuration/UnityConfigureOther.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Undo/Undo.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Camera/Light.h"
#include "Runtime/Camera/RenderLayers/GUIText.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Dynamics/Cloth.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Math/Quaternion.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Filters/Misc/TextMesh.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Misc/GOCreation.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/GameCode/CloneObject.h"
#include "Editor/Src/EditorResources.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Dynamics/PrimitiveCollider.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Editor/Src/AssetPipeline/SpriteImporterUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Graphs/GraphUtils.h"
#include "Runtime/Scripting/Scripting.h"

static GameObject *CreateGameObjectCheckTarget(const std::string& name,
											   const char* componentName, ...)
{
	va_list ap;
	const char *currentArg = componentName;
	
	va_start (ap, componentName);
	
	do
	{
		if (!IsClassSupportedOnSelectedBuildTarget(Object::StringToClassID(currentArg)))
		{
			string warning = Format ("The component %s can't be added to Game Object because it is not supported by selected platform.", currentArg);
			DisplayDialog ("Game object cannot be created!", warning, "Cancel");
			return NULL;
		}
		
		currentArg = va_arg (ap, const char*);
	} while (currentArg != NULL);

	va_end (ap);

	va_start (ap, componentName);
	GameObject &go = CreateGameObjectWithVAList(name, componentName, ap);
	va_end (ap);
	
	return &go;
}

struct GOCreation : public MenuInterface {
	virtual bool Validate (const MenuItem &menuItem) {
		return true;
	}

	void Place (GameObject* go)
	{
		if (go)
		{
			MonoObject* m = Scripting::ScriptingWrapperFor(go);
			void* obs[] = { m };
			CallStaticMonoMethod("SceneView", "PlaceGameObjectInFrontOfSceneView", obs);		

			MonoClass* klass = GetMonoManager().GetBuiltinEditorMonoClass("HierarchyWindow");
			void* type[] = { mono_class_get_object(klass)};
			CallStaticMonoMethod("EditorWindow", "FocusWindowIfItsOpen", type);	
		}
	}

	virtual void Execute (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		GameObject* go = NULL;

		switch (idx) {
			case 0:
				go = CreateGameObjectCheckTarget ("GameObject", "Transform", NULL);

				if (go)
					Place (go);
				break;
			case 1:
				{
					go = CreateGameObjectCheckTarget ("Particle System", "ParticleSystem", NULL);
					if (go)
					{
						go->GetComponent (Transform).SetRotation (EulerToQuaternion ( Vector3f (Deg2Rad (-90.f), 0.f,0.f)) );;
						Place (go);
					}
				}
				break;
			case 2:
				go = CreateGameObjectCheckTarget ("Camera", "Camera", "FlareLayer", "GUILayer", "AudioListener", NULL);
				
				if (go)
					Place (go);
				break;
			case 3:
				go = CreateGameObjectCheckTarget ("GUI Text", "GUIText", NULL);

				if (go)
				{
					go->GetComponent (GUIText).SetText ("Gui Text");
					go->GetComponent (Transform).SetPosition (Vector3f (0.5F, 0.5F, 0.0F));
					
					Font* font = dynamic_pptr_cast<Font*> (GetActiveObject ());
					if (!font)
						font = GetBuiltinResource<Font> (kDefaultFontName);
					go->GetComponent(GUIText).SetFont(font);
				}
				
				break;
			case 4:
				go = CreateGameObjectCheckTarget ("New Texture", "GUITexture", NULL);

				if (go)
				{
					Texture* tex = dynamic_pptr_cast<Texture*> (GetActiveObject ());
					if (!tex)
						tex = GetBuiltinResource<Texture2D> ("UnityWaterMark-small.png");
					
					if (tex)
					{
						Rectf inset(-tex->GetDataWidth() * 0.5f, -tex->GetDataHeight() * 0.5f, tex->GetDataWidth(), tex->GetDataHeight());
						go->GetComponent (GUITexture).SetPixelInset( inset );
						go->GetComponent (GUITexture).m_Texture = tex;
						go->SetName (tex->GetName ());
					}
					go->GetComponent (GUITexture).AwakeFromLoad (kDefaultAwakeFromLoad);
					go->GetComponent (Transform).SetPosition (Vector3f (0.5F, 0.5F, 0.0F));
					go->GetComponent (Transform).SetLocalScale (Vector3f (0.0F, 0.0F, 1.0F));
				}
				
				break;
			case 5:
				go = CreateGameObjectCheckTarget ("New Text", "MeshRenderer", "TextMesh", NULL);

				if (go)
				{
					Font* font = dynamic_pptr_cast<Font*> (GetActiveObject ());
					if (!font)
						font = GetBuiltinResource<Font> (kDefaultFontName);
					
					go->GetComponent(TextMesh).SetFont(font);
					go->GetComponent (TextMesh).SetText ("Hello World");
					
					if (font)
						go->GetComponent(MeshRenderer).SetMaterial(font->GetMaterial(), 0);
					Place (go);
				}

				break;
			case 10:
				go = CreateGameObjectCheckTarget ("Point light","Light", NULL);

				if (go)
					Place (go);
				break;
			case 11:
				go = CreateGameObjectCheckTarget ("Spotlight", "Light", NULL);

				if (go)
				{
					go->GetComponent (Light).SetType (kLightSpot);
					go->GetComponent (Transform).SetLocalEulerAngles (Vector3f(90, 0, 0));
					Place (go);
				}

				break;
			case 12:
				go = CreateGameObjectCheckTarget ("Directional light", "Light", NULL);

				if (go)
				{
					go->GetComponent (Light).SetType (kLightDirectional);
					go->GetComponent (Light).SetIntensity(0.5F);
					go->GetComponent (Transform).SetLocalEulerAngles (Vector3f(50, -30, 0));
					Place (go);
				}

				break;
			case 13:
				go = CreateGameObjectCheckTarget ("Area Light", "Light", NULL);

				if (go)
				{
					go->GetComponent (Light).SetType (kLightArea);
					go->GetComponent (Transform).SetLocalEulerAngles (Vector3f(90, 0, 0));
					Place (go);
				}
				break;
			case 20:
				go = CreatePrimitive(kPrimitiveCube);
				Place (go);
				break;
			case 21:
				go = CreatePrimitive(kPrimitiveSphere);
				Place (go);
				break;
			case 22:
				go = CreatePrimitive(kPrimitiveCapsule);
				Place (go);
				break;
			case 23:
				go = CreatePrimitive(kPrimitiveCylinder);
				Place (go);
				break;
			case 24:
				go = CreatePrimitive(kPrimitivePlane);
				Place (go);
				break;
			case 25:
				go = CreatePrimitive(kPrimitiveQuad);
				Place (go);
				break;
			case 28:
				go = CreateGameObjectCheckTarget ("New Sprite", "SpriteRenderer", NULL);

				if (go)
				{
					Sprite* sprite = dynamic_pptr_cast<Sprite*>(GetActiveObject ());
					if (!sprite)
					{
						// Sprite not selected? Check if it's a Texture2D.
						Texture2D* texture = dynamic_pptr_cast<Texture2D*> (GetActiveObject ());
						if (texture)
						{
							// Check if Sprites are generated.
							sprite = GetFirstGeneratedSpriteFromTexture (texture);
							if (!sprite)
							{
								// If Sprites are not generated, validate importer settings.
								TextureImporter* ti = dynamic_pptr_cast<TextureImporter*>(FindAssetImporterForObject(texture->GetInstanceID()));
								if (ti && !ti->GetAreSpritesGenerated())
								{
									std::string warning = "Can not assign a Sprite to the new SpriteRenderer because the selected Texture is not configured to generate Sprites.";
									DisplayDialog("Sprite could not be assigned!", warning, "OK");
								}
							}
						}
					}

					if (sprite)
					{
						go->GetComponent (SpriteRenderer).SetSprite(sprite);
					}
					else
					{
						// TODO: assign a default sprite
					}

					go->GetComponent (SpriteRenderer).AwakeFromLoad (kDefaultAwakeFromLoad);
					Place (go);
				}
				
				break;
			case 30:
				go = CreateGameObjectCheckTarget ("InteractiveCloth", "InteractiveCloth", "ClothRenderer", NULL);
				
				if (go)
				{
					Place (go);
					go->GetComponent (InteractiveCloth).SetMesh (GetBuiltinResource<Mesh> ("New-Plane.fbx"));
					go->GetComponent (Renderer).SetMaterial (GetBuiltinExtraResource<Material> ("Default-Diffuse.mat"), 0);
				}

				break;
			case 31:
				ScriptingInvocation(kLogicGraphEditorNamespace, "LogicGraph", "CreateLogicGraph").Invoke();
				break;
			case 40:
				go = CreateGameObjectCheckTarget ("Reverb Zone", "AudioReverbZone", NULL);

				if (go)
					Place (go);
				break;	
		}

		if (go)
		{
			SetActiveObject (go);

			if (idx == 0)
			{
				RegisterCreatedObjectUndo (go, "Create Empty Game Object");
			}
			else
			{
				RegisterCreatedObjectUndo (go, "Create " + GetMenuNameWithoutHotkey(menuItem.m_Name));
			}
		}
	}
};


void GOCreationRegisterMenu ()
{
	static GOCreation *gGOCreation = new GOCreation;

	MenuController::AddMenuItem ("GameObject/Create Empty %#n", "0", gGOCreation,0);

	MenuController::AddMenuItem ("GameObject/Create Other/Particle System", "1", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Camera", "2", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/GUI Text", "3", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/GUI Texture", "4", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/3D Text", "5", gGOCreation,0);

	MenuController::AddSeparator ("GameObject/Create Other/",0);

	MenuController::AddMenuItem ("GameObject/Create Other/Directional Light", "12", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Point Light", "10", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Spotlight", "11", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Area Light", "13", gGOCreation, 0);

	MenuController::AddSeparator ("GameObject/Create Other/",0);

	MenuController::AddMenuItem ("GameObject/Create Other/Cube", "20", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Sphere", "21", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Capsule", "22", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Cylinder", "23", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Plane", "24", gGOCreation,0);
	MenuController::AddMenuItem ("GameObject/Create Other/Quad", "25", gGOCreation,0);

	MenuController::AddSeparator ("GameObject/Create Other/",0);

	MenuController::AddMenuItem ("GameObject/Create Other/Sprite", "28", gGOCreation,0);

	MenuController::AddSeparator ("GameObject/Create Other/",0);

	MenuController::AddMenuItem ("GameObject/Create Other/Cloth", "30", gGOCreation,0);
	
	MenuController::AddSeparator ("GameObject/Create Other/",0);
	
	MenuController::AddMenuItem ("GameObject/Create Other/Audio Reverb Zone", "40", gGOCreation,0);

	#if UNITY_LOGIC_GRAPH
		MenuController::AddSeparator ("GameObject/Create Other/",0);
		MenuController::AddMenuItem ("GameObject/Create Other/Logic Graph", "31", gGOCreation,0);
	#endif
}

STARTUP (GOCreationRegisterMenu)	// Call this on startup.
