using UnityEngine;

// See Style Guide in wiki for more information on editor styles.

namespace UnityEditor
{
// Common GUIStyles used for EditorGUI controls.
public sealed partial class EditorStyles
{
	// Style used for the labelled on all EditorGUI overloads that take a prefix label
	public static GUIStyle label { get { return s_Current.m_Label;} }
	internal GUIStyle m_Label;

	// Style for label with small font.
	public static GUIStyle miniLabel { get { return s_Current.m_MiniLabel;} }
	private GUIStyle m_MiniLabel;

	// Style for label with large font.
	public static GUIStyle largeLabel { get { return s_Current.m_LargeLabel;} }
	private GUIStyle m_LargeLabel;

	// Style for bold label.
	public static GUIStyle boldLabel { get { return s_Current.m_BoldLabel;} }
	private GUIStyle m_BoldLabel;
	
	// Style for mini bold label.
	public static GUIStyle miniBoldLabel { get { return s_Current.m_MiniBoldLabel;} }
	private GUIStyle m_MiniBoldLabel;
	
	// Style for word wrapped label.
	public static GUIStyle wordWrappedLabel { get { return s_Current.m_WordWrappedLabel;} }
	private GUIStyle m_WordWrappedLabel;
	
	// Style for word wrapped mini label.
	public static GUIStyle wordWrappedMiniLabel { get { return s_Current.m_WordWrappedMiniLabel;} }
	private GUIStyle m_WordWrappedMiniLabel;

	// Style for white label. 
	public static GUIStyle whiteLabel { get { return s_Current.m_WhiteLabel;} }
	private GUIStyle m_WhiteLabel;

	// Style for white mini label.
	public static GUIStyle whiteMiniLabel { get { return s_Current.m_WhiteMiniLabel;} }
	private GUIStyle m_WhiteMiniLabel;

	// Style for white large label.
	public static GUIStyle whiteLargeLabel { get { return s_Current.m_WhiteLargeLabel;} }
	private GUIStyle m_WhiteLargeLabel;

	// Style for white bold label.
	public static GUIStyle whiteBoldLabel { get { return s_Current.m_WhiteBoldLabel;} }
	private GUIStyle m_WhiteBoldLabel;

	// Style used for a radio button
	public static GUIStyle radioButton { get { return s_Current.m_RadioButton;} }
	private GUIStyle m_RadioButton;	
		
	// Style used for a standalone small button.
	public static GUIStyle miniButton { get { return s_Current.m_MiniButton;} }
	private GUIStyle m_MiniButton;

	// Style used for the leftmost button in a horizontal button group.
	public static GUIStyle miniButtonLeft { get { return s_Current.m_MiniButtonLeft;} }
	private GUIStyle m_MiniButtonLeft;

	// Style used for the middle buttons in a horizontal group.
	public static GUIStyle miniButtonMid { get { return s_Current.m_MiniButtonMid;} }
	private GUIStyle m_MiniButtonMid;

	// Style used for the rightmost button in a horizontal group.
	public static GUIStyle miniButtonRight { get { return s_Current.m_MiniButtonRight;} }
	private GUIStyle m_MiniButtonRight;

	// Style used for EditorGUI::ref::TextField
	public static GUIStyle textField  { get { return s_Current.m_TextField;} }
	internal GUIStyle m_TextField;

	// Smaller text field
	public static GUIStyle miniTextField  { get { return s_Current.m_MiniTextField;} }
	private GUIStyle m_MiniTextField;

	// Style used for field editors for numbers
	public static GUIStyle numberField { get { return s_Current.m_NumberField; }  }
	private GUIStyle m_NumberField;
	
	// Style used for EditorGUI::ref::Popup, EditorGUI::ref::EnumPopup, 
	public static GUIStyle popup  { get { return s_Current.m_Popup; }  }
	private GUIStyle m_Popup;
	
	// Style used for headings for structures (Vector3, Rect, etc)
	[System.Obsolete ("structHeadingLabel is deprecated, use EditorStyles.label instead.")]
	public static GUIStyle structHeadingLabel  { get { return s_Current.m_Label; }  }
	
	// Style used for headings for object fields.
	public static GUIStyle objectField  { get { return s_Current.m_ObjectField; }  }
	private GUIStyle m_ObjectField;

	// Style used for headings for the Select button in object fields.
	public static GUIStyle objectFieldThumb { get { return s_Current.m_ObjectFieldThumb; }  }
	private GUIStyle m_ObjectFieldThumb;

	// Style used for headings for Color fields.
	public static GUIStyle colorField { get { return s_Current.m_ColorField; }  }
	private GUIStyle m_ColorField;
	
	// Style used for headings for Layer masks.
	public static GUIStyle layerMaskField { get { return s_Current.m_LayerMaskField; } }
	private GUIStyle m_LayerMaskField;
	
	// Style used for headings for EditorGUI::ref::Toggle.
	public static GUIStyle toggle {get { return s_Current.m_Toggle; } }
	private GUIStyle m_Toggle;

	// Style used for headings for EditorGUI::ref::Foldout.
	public static GUIStyle foldout { get { return s_Current.m_Foldout; } /*set { m_Foldout = value; }*/ }
	private GUIStyle m_Foldout;

	// Style used for headings for EditorGUI::ref::Foldout.
	public static GUIStyle foldoutPreDrop { get { return s_Current.m_FoldoutPreDrop; } /*set { m_FoldoutPreDrop = value; }*/ }
	private GUIStyle m_FoldoutPreDrop;
	
	// Style used for headings for EditorGUILayout::ref::BeginToggleGroup.
	public static GUIStyle toggleGroup { get { return s_Current.m_ToggleGroup; }  }
	private GUIStyle m_ToggleGroup;

	// Standard font.
	public static Font standardFont  { get { return s_Current.m_StandardFont; } }
	internal Font m_StandardFont;

	// Bold font.
	public static Font boldFont { get { return s_Current.m_BoldFont; } }
	internal Font m_BoldFont;
	
	// Mini font.
	public static Font miniFont { get { return s_Current.m_MiniFont; } }
	internal Font m_MiniFont;
	
	// Mini Bold font.
	public static Font miniBoldFont { get { return s_Current.m_MiniBoldFont; } }
	internal Font m_MiniBoldFont;
	
	// Toolbar background from top of windows.
	public static GUIStyle toolbar { get { return s_Current.m_Toolbar; } }
	private GUIStyle m_Toolbar;
	
	// Style for Button and Toggles in toolbars.
	public static GUIStyle toolbarButton { get { return s_Current.m_ToolbarButton; } }
	private GUIStyle m_ToolbarButton;
	
	// Toolbar Popup
	public static GUIStyle toolbarPopup { get { return s_Current.m_ToolbarPopup; } }
	private GUIStyle m_ToolbarPopup;
	
	// Toolbar Dropdown
	public static GUIStyle toolbarDropDown { get { return s_Current.m_ToolbarDropDown; } }
	private GUIStyle m_ToolbarDropDown;

	// Toolbar text field
	public static GUIStyle toolbarTextField { get { return s_Current.m_ToolbarTextField; } }
	private GUIStyle m_ToolbarTextField;
	
	public static GUIStyle inspectorDefaultMargins { get { return s_Current.m_InspectorDefaultMargins; } }
	private GUIStyle m_InspectorDefaultMargins;
	
	public static GUIStyle inspectorFullWidthMargins { get { return s_Current.m_InspectorFullWidthMargins; } }
	private GUIStyle m_InspectorFullWidthMargins;
	
	internal static GUIStyle toolbarSearchField { get { return s_Current.m_ToolbarSearchField; } }
	private GUIStyle m_ToolbarSearchField;

	internal static GUIStyle toolbarSearchFieldPopup { get { return s_Current.m_ToolbarSearchFieldPopup; } }
	private GUIStyle m_ToolbarSearchFieldPopup;

	internal static GUIStyle toolbarSearchFieldCancelButton { get { return s_Current.m_ToolbarSearchFieldCancelButton; } }
	private GUIStyle m_ToolbarSearchFieldCancelButton;

	internal static GUIStyle toolbarSearchFieldCancelButtonEmpty { get { return s_Current.m_ToolbarSearchFieldCancelButtonEmpty; } }
	private GUIStyle m_ToolbarSearchFieldCancelButtonEmpty;

	internal static GUIStyle colorPickerBox { get { return s_Current.m_ColorPickerBox; } }
	private GUIStyle m_ColorPickerBox;
	
	internal static GUIStyle inspectorBig { get { return s_Current.m_InspectorBig; } }
	private GUIStyle m_InspectorBig;

	internal static GUIStyle inspectorTitlebar { get { return s_Current.m_InspectorTitlebar; } }
	private GUIStyle m_InspectorTitlebar;

	internal static GUIStyle inspectorTitlebarText { get { return s_Current.m_InspectorTitlebarText; } }
	private GUIStyle m_InspectorTitlebarText;

	internal static GUIStyle foldoutSelected { get { return s_Current.m_FoldoutSelected; } }
	private GUIStyle m_FoldoutSelected;

	// Style for notification text.
	internal static GUIStyle notificationText { get { return s_Current.m_NotificationText; } }
	private GUIStyle m_NotificationText;
	// Style for notification background area.
	internal static GUIStyle notificationBackground { get { return s_Current.m_NotificationBackground; } }
	private GUIStyle m_NotificationBackground;
	
	internal static GUIStyle helpBox { get { return s_Current.m_HelpBox; } }
	private GUIStyle m_HelpBox;

	internal static GUIStyle tagTextField { get { return s_Current.m_TagTextField; } }
	private GUIStyle m_TagTextField;

	internal static GUIStyle tagTextFieldEmpty { get { return s_Current.m_TagTextFieldEmpty; } }
	private GUIStyle m_TagTextFieldEmpty;

	internal static GUIStyle tagTextFieldButton { get { return s_Current.m_TagTextFieldButton; } }
	private GUIStyle m_TagTextFieldButton;

	internal static GUIStyle searchField { get { return s_Current.m_SearchField; } }
	private GUIStyle m_SearchField;

	internal static GUIStyle searchFieldCancelButton { get { return s_Current.m_SearchFieldCancelButton; } }
	private GUIStyle m_SearchFieldCancelButton;

	internal static GUIStyle searchFieldCancelButtonEmpty { get { return s_Current.m_SearchFieldCancelButtonEmpty; } }
	private GUIStyle m_SearchFieldCancelButtonEmpty;

	internal static GUIStyle selectionRect { get { return s_Current.m_SelectionRect; } }
	private GUIStyle m_SelectionRect;

	internal static GUIStyle minMaxHorizontalSliderThumb { get { return s_Current.m_MinMaxHorizontalSliderThumb; } }
	private GUIStyle m_MinMaxHorizontalSliderThumb;

	internal static GUIStyle progressBarBack { get { return s_Current.m_ProgressBarBack; } }
	internal static GUIStyle progressBarBar { get { return s_Current.m_ProgressBarBar; } }
	internal static GUIStyle progressBarText { get { return s_Current.m_ProgressBarText; } }
	private GUIStyle m_ProgressBarBar, m_ProgressBarText, m_ProgressBarBack;
	
	// the editor styles currently in use
	internal static EditorStyles s_Current;
	
	// the list of editor styles to use
	private static EditorStyles[] s_CachedStyles = { null, null };
	
	internal static void UpdateSkinCache ()
	{
		UpdateSkinCache (EditorGUIUtility.skinIndex);
	}
	
	internal static void UpdateSkinCache (int skinIndex)
	{
		// Don't cache the Game GUISkin styles
		if (GUIUtility.s_SkinMode == 0)
			return;

		if (s_CachedStyles[skinIndex] == null)
		{
			s_CachedStyles[skinIndex] = new EditorStyles ();
			s_CachedStyles[skinIndex].InitSharedStyles ();
		}
		
		s_Current = s_CachedStyles[skinIndex];
		EditorGUIUtility.s_FontIsBold = -1;
		EditorGUIUtility.SetBoldDefaultFont (false);
	}
	
	private void InitSharedStyles () {
		m_ColorPickerBox = GetStyle ("ColorPickerBox");
		m_InspectorBig = GetStyle ("In BigTitle");
		m_MiniLabel = GetStyle ("miniLabel");
		m_LargeLabel = GetStyle ("LargeLabel");
		m_BoldLabel = GetStyle ("BoldLabel");
		m_MiniBoldLabel = GetStyle ("MiniBoldLabel");
		m_WordWrappedLabel = GetStyle ("WordWrappedLabel");
		m_WordWrappedMiniLabel = GetStyle ("WordWrappedMiniLabel");
		m_WhiteLabel = GetStyle ("WhiteLabel");
		m_WhiteMiniLabel = GetStyle ("WhiteMiniLabel");
		m_WhiteLargeLabel = GetStyle ("WhiteLargeLabel");
		m_WhiteBoldLabel = GetStyle ("WhiteBoldLabel");
		m_MiniTextField = GetStyle ("MiniTextField");
		m_RadioButton = GetStyle ("Radio");
		m_MiniButton = GetStyle ("miniButton");
		m_MiniButtonLeft = GetStyle ("miniButtonLeft");
		m_MiniButtonMid = GetStyle ("miniButtonMid");
		m_MiniButtonRight = GetStyle ("miniButtonRight");
		m_Toolbar = GetStyle ("toolbar");
		m_ToolbarButton = GetStyle ("toolbarbutton");
		m_ToolbarPopup = GetStyle ("toolbarPopup");
		m_ToolbarDropDown = GetStyle ("toolbarDropDown");
		m_ToolbarTextField = GetStyle ("toolbarTextField");
		m_ToolbarSearchField = GetStyle ("ToolbarSeachTextField");		
		m_ToolbarSearchFieldPopup = GetStyle ("ToolbarSeachTextFieldPopup");		
		m_ToolbarSearchFieldCancelButton = GetStyle ("ToolbarSeachCancelButton");		
		m_ToolbarSearchFieldCancelButtonEmpty = GetStyle ("ToolbarSeachCancelButtonEmpty");
		m_SearchField = GetStyle ("SearchTextField");
		m_SearchFieldCancelButton = GetStyle ("SearchCancelButton");		
		m_SearchFieldCancelButtonEmpty = GetStyle ("SearchCancelButtonEmpty");		
		m_HelpBox = GetStyle ("HelpBox");
		m_TagTextField = GetStyle ("Tag TextField");
		m_TagTextFieldEmpty = GetStyle ("Tag TextField Empty");
		m_TagTextFieldButton = GetStyle ("Tag TextField Button");
		m_SelectionRect = GetStyle ("selectionRect");
		m_MinMaxHorizontalSliderThumb = GetStyle ("MinMaxHorizontalSliderThumb");
		m_BoldFont = GetStyle ("BoldLabel").font;
		m_StandardFont = GetStyle ("Label").font;
		m_MiniFont = GetStyle ("MiniLabel").font;
		m_MiniBoldFont = GetStyle ("MiniBoldLabel").font;
		m_ProgressBarBack = GetStyle ("ProgressBarBack");
		m_ProgressBarBar = GetStyle ("ProgressBarBar");
		m_ProgressBarText = GetStyle ("ProgressBarText");
		m_FoldoutPreDrop = GetStyle ("FoldoutPreDrop");
		m_InspectorTitlebar = GetStyle ("IN Title");
		m_InspectorTitlebarText = GetStyle ("IN TitleText");
		m_ToggleGroup = GetStyle ("BoldToggle");
		m_NotificationText = GetStyle ("NotificationText");
		m_NotificationBackground = GetStyle ("NotificationBackground");
		
		// Former LookLikeControls styles
		m_Popup = m_LayerMaskField = GetStyle ("MiniPopup");
		m_TextField = m_NumberField = GetStyle ("textField");
		m_Label = GetStyle ("ControlLabel");
		m_ObjectField = GetStyle ("ObjectField");
		m_ObjectFieldThumb = GetStyle ("ObjectFieldThumb");
		m_Toggle = GetStyle ("Toggle");
		m_ColorField = GetStyle ("ColorField");
		m_Foldout = GetStyle ("Foldout");
		m_FoldoutSelected = GUIStyle.none;
		
		m_InspectorDefaultMargins = new GUIStyle ();
		m_InspectorDefaultMargins.padding = new RectOffset (
			InspectorWindow.kInspectorPaddingLeft,
			InspectorWindow.kInspectorPaddingRight, 0, 0);
		
		// For the full width margins, use padding from right side in both sides,
		// though adjust for overdraw by adding one in left side to get even margins.
		m_InspectorFullWidthMargins = new GUIStyle ();
		m_InspectorFullWidthMargins.padding = new RectOffset (
			InspectorWindow.kInspectorPaddingRight+1,
			InspectorWindow.kInspectorPaddingRight, 0, 0);
	}
	
	private GUIStyle GetStyle (string styleName) {
		GUIStyle s = GUI.skin.FindStyle (styleName);
		if (s == null)
			s = EditorGUIUtility.GetBuiltinSkin (EditorSkin.Inspector).FindStyle (styleName);
		if (s == null)
		{
			Debug.LogError ("Missing built-in guistyle " + styleName);
			s = GUISkin.error;
		}
		return s;
	}
}
}
