using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;
using System.IO;

namespace UnityEditor
{
    internal class HeapshotWindow : EditorWindow
    {
        private delegate void OnSelect(HeapshotUIObject o);

        public class HeapshotUIObject
        {
            private string name;
            private HeapshotReader.ObjectInfo obj;
            private List<HeapshotUIObject> children = new List<HeapshotUIObject>();
            private bool inverseReference;
            private bool isDummyObject = false;

            public HeapshotUIObject(string name, HeapshotReader.ObjectInfo refObject, bool inverseReference)
            {
                this.name = name;
                this.obj = refObject;
                this.inverseReference = inverseReference;
            }
            public bool HasChildren
            {
                get
                {
                    return inverseReference ? obj.inverseReferences.Count > 0 : obj.references.Count > 0;
                }
            }
            public bool IsExpanded
            {
                get
                {
                    return HasChildren && children.Count > 0;
                }
            }
            public string Name
            {
                get
                {
                    return name;
                }
            }
            public uint Code
            {
                get
                {
                    return obj.code;
                }
            }
            public uint Size
            {
                get
                {
                    return obj.size;
                }
            }
            public int ReferenceCount
            {
                get
                {
                    return inverseReference ? obj.inverseReferences.Count : obj.references.Count;
                }
            }
            public int InverseReferenceCount
            {
                get
                {
                    return inverseReference ? obj.references.Count : obj.inverseReferences.Count;
                }
            }
            public bool IsDummyObject
            {
                get
                {
                    return isDummyObject;
                }
                set
                {
                    isDummyObject = value;
                }
            }
            public string TypeName
            {
                get
                {
                    return obj.typeInfo.name;
                }
            }
            public HeapshotReader.ObjectInfo ObjectInfo
            {
                get
                {
                    return obj;
                }
            }
            public void Expand()
            {
                if (IsExpanded) return;
                if (HasChildren)
                {
                    if (inverseReference)
                    {
                        foreach (HeapshotReader.BackReferenceInfo backReference in obj.inverseReferences)
                        {
                            children.Add(new HeapshotUIObject(backReference.fieldInfo.name, backReference.parentObject, true));
                        }
                    }
                    else
                    {
                        foreach (HeapshotReader.ReferenceInfo reference in obj.references)
                        {
                            children.Add(new HeapshotUIObject(reference.fieldInfo.name, reference.referencedObject, false));
                        }
                    }
                }
            }
            public void Collapse()
            {
                if (!IsExpanded) return;
                children.Clear();
            }
            public List<HeapshotUIObject> Children
            {
                get
                {
                    if (HasChildren && IsExpanded) return children;
                    else return null;
                }
            }
        }
        private delegate void DelegateReceivedHeapshot(string fileName);
        private HeapshotReader heapshotReader = null;
        private const string heapshotExtension = ".heapshot";
        private List<string> heapshotFiles = new List<string>();

        private int itemIndex = -1;
        private Rect guiRect;
        private int selectedItem = -1;
        private int currentTab = 0;
        private string lastOpenedHeapshotFile = "";
        private string lastOpenedProfiler = "";
        private static DelegateReceivedHeapshot onReceivedHeapshot = null;

        internal class UIStyles
        {
            public GUIStyle background = "OL Box";
            public GUIStyle header = "OL title";
            public GUIStyle rightHeader = "OL title TextRight";
            public GUIStyle entryEven = "OL EntryBackEven";
            public GUIStyle entryOdd = "OL EntryBackOdd";
            public GUIStyle numberLabel = "OL Label";
            public GUIStyle foldout = "IN foldout";
        }
        internal class UIOptions
        {
            public const float height = 16.0f;
            public const float foldoutWidth = 14.0f;
            public const float tabWidth = 50.0f;
        }

        private List<HeapshotUIObject> hsRoots = new List<HeapshotUIObject>();
        private List<HeapshotUIObject> hsAllObjects = new List<HeapshotUIObject>();
        private List<HeapshotUIObject> hsBackTraceObjects = new List<HeapshotUIObject>();
        private Vector2 leftViewScrollPosition = Vector2.zero;
        private Vector2 rightViewScrollPosition = Vector2.zero;
        private static UIStyles ms_Styles = null;
        private SplitterState viewSplit = new SplitterState(new[] {50.0f, 50.0f}, null, null);
        private string[]titleNames = new string[]{"Field Name", "Type", "Pointer", "Size", "References/Referenced"};
        private SplitterState titleSplit1 = new SplitterState(new[] { 30.0f, 25.0f, 15.0f, 15.0f, 15.0f }, new[] { 200, 200, 50, 50, 50 }, null);
        private SplitterState titleSplit2 = new SplitterState(new[] { 30.0f, 25.0f, 15.0f, 15.0f, 15.0f }, new[] { 200, 200, 50, 50, 50 }, null);
        private int selectedHeapshot = -1;
        private int[] connectionGuids;


        //[MenuItem("Window/Heapshot")]
        // Registered from EditorWindowController.cpp
        private static void Init()
        {
            // Get existing open window or if none, make a new one:
            EditorWindow wnd = EditorWindow.GetWindow(typeof(HeapshotWindow));
            wnd.title = "Mono heapshot";
            //thisWindow = thisWindow;
        }

        private string HeapshotPath
        {
            get
            {
                return Application.dataPath + "/../Heapshots";
            }
        }
        private static void EventHeapShotReceived(string name)
        {
            Debug.Log("Received " + name);
            if (onReceivedHeapshot != null) onReceivedHeapshot(name);
        }
        private void OnReceivedHeapshot(string name)
        {
            SearchForHeapShots();
            OpenHeapshot(name);
        }
        private void SearchForHeapShots()
        {
            heapshotFiles.Clear();
            if (Directory.Exists(HeapshotPath) == false) return;
            string[] files = Directory.GetFiles(HeapshotPath, "*" + heapshotExtension); ;
            
            selectedHeapshot = -1;
            foreach (string file in files)
            {
                string trimmedFile = file.Substring(file.LastIndexOf("\\") + 1);
                trimmedFile = trimmedFile.Substring(0, trimmedFile.IndexOf(heapshotExtension));
                heapshotFiles.Add(trimmedFile);
            }
            if (heapshotFiles.Count > 0)
            {
                selectedHeapshot = heapshotFiles.Count - 1;
            }
        }
        private static UIStyles Styles
        {
            get { return ms_Styles ?? (ms_Styles = new UIStyles()); }
        }
        private void OnEnable()
        {
            onReceivedHeapshot = OnReceivedHeapshot;
        }
        private void OnDisable()
        {
            onReceivedHeapshot = null;
        }
        private void OnFocus()
        {
            SearchForHeapShots();
        }
        private void RefreshHeapshotUIObjects()
        {
            hsRoots.Clear();
            hsAllObjects.Clear();
            foreach (HeapshotReader.ReferenceInfo reference in heapshotReader.Roots)
            {
                string name = reference.fieldInfo.name;
                hsRoots.Add(new HeapshotUIObject(name, reference.referencedObject, false));
            }


            // Sort objects alphabetically 
            SortedDictionary<string, List<HeapshotReader.ObjectInfo>> objsCommonName = new SortedDictionary<string, List<HeapshotReader.ObjectInfo>>();

            foreach (HeapshotReader.ObjectInfo o in heapshotReader.Objects)
            {
                if (o.type == HeapshotReader.ObjectType.Managed)
                {
                    string name = o.typeInfo.name;
                    if (objsCommonName.ContainsKey(name) == false)
                    {
                        objsCommonName.Add(name, new List<HeapshotReader.ObjectInfo>());
                    }
                    objsCommonName[name].Add(o);
                }
            }

            foreach (KeyValuePair<string, List<HeapshotReader.ObjectInfo>> o in objsCommonName)
            {
                // Create a dummy object which holds heapshot objects with the same name
                HeapshotReader.ObjectInfo dummyObject = new HeapshotReader.ObjectInfo();
                // We don't know the field name of the object in this case
                HeapshotReader.FieldInfo fieldInfo = new HeapshotReader.FieldInfo("(Unknown)");

                foreach (HeapshotReader.ObjectInfo objInfo in o.Value)
                {
                    dummyObject.references.Add(new HeapshotReader.ReferenceInfo(objInfo, fieldInfo));
                }
                HeapshotUIObject dummyUIObject = new HeapshotUIObject(o.Key + " x " + o.Value.Count, dummyObject, false);
                dummyUIObject.IsDummyObject = true;
                hsAllObjects.Add(dummyUIObject);
            }
        }
        private int GetItemCount(List<HeapshotUIObject> objects)
        {
            int visibleItems = 0;
            foreach (HeapshotUIObject o in objects)
            {
                visibleItems++;
                if (o.IsExpanded) visibleItems += GetItemCount(o.Children);
            }
            return visibleItems;
        }
        private void OpenHeapshot(string fileName)
        {
            heapshotReader = new HeapshotReader();
            string path = HeapshotPath + "/" + fileName;
            if (heapshotReader.Open(path))
            {
                lastOpenedHeapshotFile = fileName;
                RefreshHeapshotUIObjects();
            }
            else
            {
                Debug.LogError("Failed to read " + path);
            }    
        }

        private void OnGUI()
        {
            GUI.Label(new Rect(0, 0, position.width, 20), "Heapshots are located here: " + Path.Combine(Application.dataPath, "Heapshots"));
            GUI.Label(new Rect(0, 20, position.width, 20), "Currently opened: " + lastOpenedHeapshotFile);
            GUI.Label(new Rect(100, 40, position.width, 20), "Profiling: " + lastOpenedProfiler);
            DoActiveProfilerButton(new Rect(0, 40, 100, 30));

            if (GUI.Button(new Rect(0, 70, 200, 20), "CaptureHeapShot", EditorStyles.toolbarDropDown))
            {
               ProfilerDriver.CaptureHeapshot();
            }

            GUI.changed = false;
            selectedHeapshot = EditorGUI.Popup(new Rect(250, 70, 500, 30), "Click to open -->", selectedHeapshot, heapshotFiles.ToArray());
            if (GUI.changed && heapshotFiles[selectedHeapshot].Length > 0)
            {
                OpenHeapshot(heapshotFiles[selectedHeapshot] + heapshotExtension);
            }
            GUILayout.BeginArea(new Rect(0, 90, position.width, 60));
                SplitterGUILayout.BeginHorizontalSplit(viewSplit);
                GUILayout.BeginVertical();
                    GUILayout.BeginHorizontal();
                    string []tabNames = new string[]{"Roots", "All Objects"};
                    for (int i = 0; i < tabNames.Length; i++)
                    {
                        bool res = GUILayout.Toggle(currentTab == i, tabNames[i], EditorStyles.toolbarButton, GUILayout.MaxHeight(UIOptions.height));
                        if (res)
                        {
                            currentTab = i;
                        }
                    }
                    GUILayout.EndHorizontal();
                    DoTitles(titleSplit1);
                GUILayout.EndVertical();


                GUILayout.BeginVertical();
                    GUILayout.Label("Back trace references", EditorStyles.toolbarButton, GUILayout.MaxHeight(UIOptions.height));
                    DoTitles(titleSplit2);
                GUILayout.EndVertical();
                SplitterGUILayout.EndHorizontalSplit();
            GUILayout.EndArea();

            // Draw Roots or AllObjects depending on the selected tab
            // We don't use GUILayout here as I don't know how to calculate rect when using
            // backStyle.Draw(...) in DoHeapshotObjects
            guiRect = new Rect(0.0f, 130.0f, viewSplit.realSizes[0], UIOptions.height);
            float leftScrollViewHeight = GetItemCount(hsAllObjects) * UIOptions.height;
            Rect scrollRect = new Rect(guiRect.x, guiRect.y, guiRect.width, position.height - guiRect.y);
            leftViewScrollPosition = GUI.BeginScrollView(scrollRect, leftViewScrollPosition, new Rect(0, 0, scrollRect.width - 20.0f, leftScrollViewHeight));//, Styles.background, Styles.background);
            itemIndex = 0;
            guiRect.y = 0.0f;
            switch (currentTab)
            {
                case 0: DoHeapshotObjects(hsRoots, titleSplit1, 0, OnSelectObject); break;
                case 1: DoHeapshotObjects(hsAllObjects, titleSplit1, 0, OnSelectObject); break;
            }
            GUI.EndScrollView();


            guiRect = new Rect(viewSplit.realSizes[0], 130.0f, viewSplit.realSizes[1], UIOptions.height);
            float rightScrollViewHeight = GetItemCount(hsBackTraceObjects) * UIOptions.height;
            scrollRect = new Rect(guiRect.x, guiRect.y, guiRect.width, position.height - guiRect.y);
            rightViewScrollPosition = GUI.BeginScrollView(scrollRect, rightViewScrollPosition, new Rect(0, 0, scrollRect.width - 20.0f, rightScrollViewHeight));
            if (hsBackTraceObjects.Count > 0)
            {
                guiRect.y = 0.0f;
                itemIndex = 0;
                DoHeapshotObjects(hsBackTraceObjects, titleSplit2, 0, null);
            }
            GUI.EndScrollView();
        }
        private void OnSelectObject(HeapshotUIObject o)
        {
            //Debug.Log("OnSelectObject " + o.Name);
            hsBackTraceObjects.Clear();
            hsBackTraceObjects.Add(new HeapshotUIObject(o.Name, o.ObjectInfo, true));
        }
        private void DoActiveProfilerButton(Rect position)
        {
            if (EditorGUI.ButtonMouseDown(position, new GUIContent("Active Profler"), FocusType.Native, EditorStyles.toolbarDropDown))
            {
                int currentProfiler = ProfilerDriver.connectedProfiler;
                connectionGuids = ProfilerDriver.GetAvailableProfilers();
                int length = connectionGuids.Length;
                int[] selected = { 0 };
                var enabled = new bool[length];
                var names = new string[length];
                for (int index = 0; index < length; index++)
                {
                    int guid = connectionGuids[index];
                    bool enable = ProfilerDriver.IsIdentifierConnectable(guid);
                    enabled[index] = enable;
                    string name = ProfilerDriver.GetConnectionIdentifier(guid);
                    if (!enable)
                        name += " (Version mismatch)";
                    names[index] = name;

                    if (guid == currentProfiler)
                        selected[0] = index;
                }

                EditorUtility.DisplayCustomMenu(position, names, enabled, selected, SelectProfilerClick, null);
            }
        }
        void SelectProfilerClick(object userData, string[] options, int selected)
        {
            int guid = connectionGuids[selected];
            lastOpenedProfiler = ProfilerDriver.GetConnectionIdentifier(guid);
            ProfilerDriver.connectedProfiler = guid;
        }
        private void DoTitles(SplitterState splitter)
        {
            SplitterGUILayout.BeginHorizontalSplit(splitter);
            for (int i = 0; i < titleNames.Length; i++)
            {
                GUILayout.Toggle(false, titleNames[i], EditorStyles.toolbarButton, GUILayout.MaxHeight(UIOptions.height));
            }
            SplitterGUILayout.EndHorizontalSplit();
        }

        private void DoHeapshotObjects(List<HeapshotUIObject> objects, SplitterState splitter, int indent, OnSelect onSelect)
        {
            if (objects == null) return;
            Event e = Event.current;
            foreach (HeapshotUIObject o in objects)
            {
                Rect foldoutRect = new Rect(UIOptions.foldoutWidth*indent, guiRect.y, UIOptions.foldoutWidth, guiRect.height);
                Rect[] itemRect = new Rect[titleNames.Length];
                float ox = UIOptions.foldoutWidth*(indent + 1);
                for (int i = 0; i < itemRect.Length; i++)
                {
                    float wd = i == 0 ? splitter.realSizes[i] - ox : splitter.realSizes[i];
                    itemRect[i] = new Rect(ox, guiRect.y, wd, guiRect.height);
                    ox += wd;
                }


                if (e.type == EventType.Repaint)
                {
                    Rect backRect = new Rect(0, UIOptions.height*itemIndex, position.width, UIOptions.height);
                    GUIStyle backStyle = (itemIndex & 1) == 0 ? Styles.entryEven : Styles.entryOdd;
                    backStyle.Draw(backRect, GUIContent.none, false, false, itemIndex == selectedItem, false);
                }

                if (o.HasChildren)
                {
                    GUI.changed = false;

                    bool state = GUI.Toggle(foldoutRect, o.IsExpanded, GUIContent.none, Styles.foldout);
                    if (GUI.changed)
                    {
                        if (state)
                        {
                            //Debug.Log(o.Name + " Expand");
                            o.Expand();
                        }
                        else
                        {
                            //Debug.Log(o.Name + " Collapse");
                            o.Collapse();
                        }
                    }
                }

                GUI.changed = false;

                bool selected = GUI.Toggle(itemRect[0], itemIndex == selectedItem, o.Name, Styles.numberLabel);
                if (o.IsDummyObject == false)
                {
                    GUI.Toggle(itemRect[1], itemIndex == selectedItem, o.TypeName, Styles.numberLabel);
                    GUI.Toggle(itemRect[2], itemIndex == selectedItem, "0x" + o.Code.ToString("X"), Styles.numberLabel);
                    GUI.Toggle(itemRect[3], itemIndex == selectedItem, o.Size.ToString(), Styles.numberLabel);
                    GUI.Toggle(itemRect[4], itemIndex == selectedItem, string.Format("{0} / {1}", o.ReferenceCount, o.InverseReferenceCount), Styles.numberLabel);
                    if (GUI.changed && selected && onSelect != null)
                    {
                        selectedItem = itemIndex;
                        onSelect(o);
                    }
                }
                itemIndex++;
                guiRect.y += UIOptions.height;
                
                DoHeapshotObjects(o.Children, splitter, indent + 1, onSelect);
            }

        }
    }


}
