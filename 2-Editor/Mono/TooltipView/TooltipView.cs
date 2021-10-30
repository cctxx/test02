using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;

namespace UnityEditor {

class TooltipView : GUIView
{
	private const float MAX_WIDTH = 300.0f;
	
	private GUIContent m_tooltip = new GUIContent();
	private Vector2 m_optimalSize;
	private GUIStyle m_Style;
	private Rect m_hoverRect;
	
	ContainerWindow m_tooltipContainer;
	static TooltipView s_guiView;
	
	void OnEnable ()
	{
		s_guiView = this;
	}

	void OnDisable()
	{
		s_guiView = null;
	}

	void OnGUI() 
	{
		if (m_tooltipContainer != null)
		{
            GUI.Box(new Rect(0,0,m_optimalSize.x,m_optimalSize.y) , m_tooltip, m_Style);	
		}
	}	

	void Setup(string tooltip, Rect rect)
	{
		m_hoverRect = rect;
		m_tooltip.text = tooltip;
	
		// Calculate size and position tooltip view
		if (m_Style == null)
			m_Style = EditorGUIUtility.GetBuiltinSkin(EditorSkin.Inspector).FindStyle("Tooltip");

		m_Style.wordWrap = false;
        m_optimalSize = m_Style.CalcSize(m_tooltip);
        
        if (m_optimalSize.x > MAX_WIDTH)
        {
        	m_Style.wordWrap = true;
        	m_optimalSize.x = MAX_WIDTH;
        	m_optimalSize.y = m_Style.CalcHeight(m_tooltip, MAX_WIDTH);        	
        } 
		
		m_tooltipContainer.position = new Rect(
    		Mathf.Floor(m_hoverRect.x + (m_hoverRect.width / 2) - (m_optimalSize.x / 2)),
    	 	Mathf.Floor(m_hoverRect.y + (m_hoverRect.height) + 10.0f), 
    	 	m_optimalSize.x, m_optimalSize.y);
    	 	
    	position = new Rect(0,0,m_optimalSize.x, m_optimalSize.y);
		
		m_tooltipContainer.ShowPopup ();
		m_tooltipContainer.SetAlpha(1.0f);	
		s_guiView.mouseRayInvisible = true;				
	}
	
	public static void Show(string tooltip, Rect rect)
	{
		if (s_guiView == null)
		{
			s_guiView = ScriptableObject.CreateInstance<TooltipView>();
            s_guiView.m_tooltipContainer = ScriptableObject.CreateInstance<ContainerWindow>();
			s_guiView.m_tooltipContainer.m_DontSaveToLayout = true;
			s_guiView.m_tooltipContainer.mainView = s_guiView;
            s_guiView.m_tooltipContainer.SetMinMaxSizes(new Vector2(10.0f, 10.0f), new Vector2(2000.0f, 2000.0f));
		}

		if (s_guiView.m_tooltip.text == tooltip && rect == s_guiView.m_hoverRect)
			return;
			
		s_guiView.Setup(tooltip, rect);
	}
	
	public static void Close()
	{
		if (s_guiView != null)
		{
			s_guiView.m_tooltipContainer.Close();	
		}
	}
	
	public static void SetAlpha(float percent)
	{
		if (s_guiView != null)
			s_guiView.m_tooltipContainer.SetAlpha(percent);	
	}
}

}