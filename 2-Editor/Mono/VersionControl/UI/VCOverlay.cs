using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

using UnityEditor.VersionControl;

namespace UnityEditorInternal.VersionControl
{
public class Overlay
{
	// Temp state
	static Asset m_Asset;
	static Rect m_ItemRect;
	static string m_IconPrefix;
	
	public static Rect GetOverlayRect (Rect itemRect)
	{
		if (itemRect.width > itemRect.height)
		{
			itemRect.x += 16;
			itemRect.width = 20;
		}
		else
		{
			itemRect.width = 12;
		}
		itemRect.height = itemRect.width;
		return itemRect;
	}
	
	public static void DrawOverlay (Asset asset, Rect itemRect)
	{
		if (asset == null)
			return;

		if (Event.current.type != EventType.Repaint)
			return;
		
		m_Asset = asset;
		m_ItemRect = itemRect;

		m_IconPrefix = EditorSettings.externalVersionControl;
		if (m_IconPrefix == ExternalVersionControl.Disabled ||
			m_IconPrefix == ExternalVersionControl.AutoDetect ||
			m_IconPrefix == ExternalVersionControl.Generic ||
			m_IconPrefix == ExternalVersionControl.AssetServer)
			return; // no icons for these version control systems

		DrawOverlays();
	}
	
	static void DrawOverlay (Asset.States state, Rect iconRect)
	{
		Rect atlasUV = Provider.GetAtlasRectForState((int)state);
		if (atlasUV.width == 0f)
			return; // no overlay
		
		Texture2D atlas = Provider.overlayAtlas;
		if (atlas == null)
			return;

		GUI.DrawTextureWithTexCoords (iconRect, atlas, atlasUV);
	}
	
	static void DrawOverlays ()
	{
		float iconWidth = 16;
		float offsetX = 1f; // offset to compensate that icons are 16x16 with 8x8 content
		float offsetY = 4f;
		float syncOffsetX = -4f; // offset to compensate that icons are 16x16 with 8x8 content and sync overlay pos

		Rect topLeft = new Rect(m_ItemRect.x - offsetX, m_ItemRect.y - offsetY, iconWidth, iconWidth);
		Rect topRight = new Rect(m_ItemRect.xMax - iconWidth + offsetX, m_ItemRect.y - offsetY, iconWidth, iconWidth);
		Rect bottomLeft = new Rect(m_ItemRect.x - offsetX, m_ItemRect.yMax - iconWidth + offsetY, iconWidth, iconWidth);
		Rect syncRect = new Rect(m_ItemRect.xMax - iconWidth + syncOffsetX, m_ItemRect.yMax - iconWidth + offsetY, iconWidth, iconWidth);

		if (IsState (Asset.States.AddedLocal))
			DrawOverlay (Asset.States.AddedLocal, topLeft);
			
		if (IsState (Asset.States.AddedRemote))
			DrawOverlay (Asset.States.AddedRemote, topRight);
		
		if (IsState (Asset.States.CheckedOutLocal) && !IsState (Asset.States.LockedLocal) && !IsState (Asset.States.AddedLocal))
			DrawOverlay (Asset.States.CheckedOutLocal, topLeft);
			
		if (IsState (Asset.States.CheckedOutRemote) && !IsState (Asset.States.LockedRemote) && !IsState (Asset.States.AddedRemote))
			DrawOverlay (Asset.States.CheckedOutRemote, topRight);
			
		if (IsState (Asset.States.DeletedLocal))
			DrawOverlay (Asset.States.DeletedLocal, topLeft);
			
		if (IsState (Asset.States.DeletedRemote))
			DrawOverlay (Asset.States.DeletedRemote, topRight);
			
		if (IsState (Asset.States.Local) && !(IsState(Asset.States.OutOfSync) || IsState(Asset.States.Synced) || IsState(Asset.States.AddedLocal)))
			DrawOverlay (Asset.States.Local, bottomLeft);
						
		if (IsState (Asset.States.LockedLocal))
			DrawOverlay (Asset.States.LockedLocal, topLeft);
			
		if (IsState (Asset.States.LockedRemote))
			DrawOverlay (Asset.States.LockedRemote, topRight);
			
		if (IsState (Asset.States.OutOfSync))
			DrawOverlay (Asset.States.OutOfSync, syncRect);
	
		if (IsState (Asset.States.Conflicted))
			DrawOverlay (Asset.States.Conflicted, bottomLeft);		
	}
	
	static bool IsState (Asset.States state)
	{
		return m_Asset.IsState (state);
	}
}
}
