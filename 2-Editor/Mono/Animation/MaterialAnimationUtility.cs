using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	static internal class MaterialAnimationUtility
	{
		static UndoPropertyModification[] CreateUndoPropertyModifications (int count, Object target)
		{
			UndoPropertyModification[] modifications = new UndoPropertyModification[count];
			for (int i=0;i<modifications.Length;i++)
			{
				modifications[i].propertyModification = new PropertyModification();
				modifications[i].propertyModification.target = target;
			}
			return modifications;
		}
		
		const string kMaterialPrefix = "material.";
		static void SetupPropertyModification (string name, float value, UndoPropertyModification prop)
		{
			prop.propertyModification.propertyPath = kMaterialPrefix + name;
			prop.propertyModification.value = value.ToString();
		}

		static void ApplyMaterialModificationToAnimationRecording (MaterialProperty materialProp, Object target, float value)
		{
			UndoPropertyModification[] modifications = CreateUndoPropertyModifications (1, target);
			SetupPropertyModification(materialProp.name, value, modifications[0]);
			
			Undo.postprocessModifications (modifications);
		}
		
		static void ApplyMaterialModificationToAnimationRecording (MaterialProperty materialProp, Object target, Color color)
		{
			UndoPropertyModification[] modifications = CreateUndoPropertyModifications (4, target);
			SetupPropertyModification(materialProp.name + ".r", color.r, modifications[0]);
			SetupPropertyModification(materialProp.name + ".g", color.g, modifications[1]);
			SetupPropertyModification(materialProp.name + ".b", color.b, modifications[2]);
			SetupPropertyModification(materialProp.name + ".a", color.a, modifications[3]);

			Undo.postprocessModifications (modifications);
		}

		static void ApplyMaterialModificationToAnimationRecording (string name, Object target, Vector4 vec)
		{
			UndoPropertyModification[] modifications = CreateUndoPropertyModifications (4, target);
			SetupPropertyModification(name + ".x", vec.x, modifications[0]);
			SetupPropertyModification(name + ".y", vec.y, modifications[1]);
			SetupPropertyModification(name + ".z", vec.z, modifications[2]);
			SetupPropertyModification(name + ".w", vec.w, modifications[3]);
			
			Undo.postprocessModifications (modifications);
		}
		
		static public bool IsAnimated (MaterialProperty materialProp, Renderer target)
		{
			if (materialProp.type == MaterialProperty.PropType.Texture)
				return AnimationMode.IsPropertyAnimated(target, kMaterialPrefix + materialProp.name + "_ST");
			else
				return AnimationMode.IsPropertyAnimated(target, kMaterialPrefix + materialProp.name);
		}
				
		static public bool ApplyMaterialModificationToAnimationRecording (MaterialProperty materialProp, int changedMask, Renderer target, object oldValue)
		{
			// Apply new value to material property block
			MaterialPropertyBlock block = new MaterialPropertyBlock ();
			target.GetPropertyBlock(block);
			materialProp.WriteToMaterialPropertyBlock(block, changedMask);
			target.SetPropertyBlock(block);
		
			switch (materialProp.type)
			{
				case MaterialProperty.PropType.Color:
					ApplyMaterialModificationToAnimationRecording (materialProp, target, (Color)oldValue);
					return true;
			
				case MaterialProperty.PropType.Vector:
					ApplyMaterialModificationToAnimationRecording (materialProp, target, (Vector4)oldValue);
					return true;
					
				case MaterialProperty.PropType.Float:
				case MaterialProperty.PropType.Range:
					ApplyMaterialModificationToAnimationRecording (materialProp, target, (float)oldValue);
					return true;
				
				case MaterialProperty.PropType.Texture:
				{
					if (MaterialProperty.IsTextureOffsetAndScaleChangedMask (changedMask))
					{
						string name = materialProp.name + "_ST";
						ApplyMaterialModificationToAnimationRecording (name, target, (Vector4)oldValue);
						return true;
					}
					else
						return false;
				}
			}
			
			return false;
		}
	}
}
