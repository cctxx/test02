
namespace UnityEditor
{
	// ParentView is a TreeView with only two hierarchy levels (parents and childs)

	///*undocumented*
	[System.Serializable]
	internal class ParentViewFile
	{
		public string guid;
		public string name;
		public ChangeFlags changeFlags;

		public ParentViewFile(string name, string guid)
		{
			this.guid = guid;
			this.name = name;
			this.changeFlags = ChangeFlags.None;
		}

		public ParentViewFile(string name, string guid, ChangeFlags flags)
		{
			this.guid = guid;
			this.name = name;
			this.changeFlags = flags;
		}
	}

	///*undocumented*
	[System.Serializable]
	internal class ParentViewFolder
	{
		public string guid;
		public string name;
		public ChangeFlags changeFlags;
		public ParentViewFile[] files;

		private const string rootDirText = "/";
		private const string assetsFolder = "Assets";
		private const string libraryFolder = "Library";

		public static string MakeNiceName(string name)
		{
			if (name.StartsWith(assetsFolder))
			{
				if (name != assetsFolder)
				{
					name = name.Substring(assetsFolder.Length + 1);
					return name == string.Empty ? rootDirText : name;
				}
				else
					return rootDirText;
			}
			else
				if (name.StartsWith(libraryFolder))
				{
					return "../" + name;
				}
				else
					return name == string.Empty ? rootDirText : name;
		}

		public ParentViewFolder(string name, string guid)
		{
			this.guid = guid;
			this.name = name;
			this.changeFlags = ChangeFlags.None;
			files = new ParentViewFile[0];
		}

		public ParentViewFolder(string name, string guid, ChangeFlags flags)
		{
			this.guid = guid;
			this.name = name;
			this.changeFlags = flags;
			files = new ParentViewFile[0];
		}

		public ParentViewFolder CloneWithoutFiles()
		{
			return new ParentViewFolder(name, guid, changeFlags);
		}
	}

	///*undocumented*
	[System.Serializable]
	internal class ParentViewState
	{
		public ListViewState lv;
		public int selectedFolder = -1;
		public int selectedFile = -1;
		public int initialSelectedItem = -1;
		public ParentViewFolder[] folders = new ParentViewFolder[0];
		public bool[] selectedItems;

		public int GetLineCount()
		{
			int count = 0;

			for (int i = 0; i < folders.Length; i++)
				count += folders[i].files.Length + 1;

			return count;
		}

		public bool HasTrue()
		{
			for (int i = 0; i < selectedItems.Length; i++)
			{
				if (selectedItems[i])
					return true;
			}

			return false;
		}

		public void SetLineCount()
		{
			lv.totalRows = GetLineCount();
		}

		public int GetFoldersCount()
		{
			return folders.Length;
		}

		public void ClearSelection()
		{
			for (int i = 0; i < selectedItems.Length; i++)
				selectedItems[i] = false;

			initialSelectedItem = -1;
		}

		static internal int IndexOf(ParentViewFolder[] foldersFrom, string lfname)
		{
			for (int i = 0; i < foldersFrom.Length; i++)
			{
				if (string.Compare(foldersFrom[i].name, lfname, true) == 0)
					return i;
			}
			return -1;
		}

		static internal int IndexOf(ParentViewFile[] filesFrom, string lfname)
		{
			for (int i = 0; i < filesFrom.Length; i++)
			{
				if (string.Compare(filesFrom[i].name, lfname, true) == 0)
					return i;
			}
			return -1;
		}

		static internal int CompareViewFolder(ParentViewFolder p1, ParentViewFolder p2) { return string.Compare(p1.name, p2.name, true); }
		static internal int CompareViewFile(ParentViewFile p1, ParentViewFile p2) { return string.Compare(p1.name, p2.name, true); }

		private void AddAssetItem(string guid, string pathName, bool isDir, ChangeFlags changeFlags, int changeset)
		{
			if (pathName == string.Empty) // TODO: how can this happen?
				return;

			if (isDir)
			{
				string dir = ParentViewFolder.MakeNiceName(pathName);

				int index = IndexOf(folders, dir);
				if (index == -1)
				{
					ParentViewFolder mfolder = new ParentViewFolder(dir, guid, changeFlags);
					ArrayUtility.Add(ref folders, mfolder);
				}
				else
				{
					folders[index].changeFlags = changeFlags;
					folders[index].guid = guid;
				}
			}
			else
			{
				string dir = ParentViewFolder.MakeNiceName(FileUtil.DeleteLastPathNameComponent(pathName));
				string fileName = pathName.Substring(pathName.LastIndexOf("/") + 1);

				int index = IndexOf(folders, dir);
				ParentViewFolder mfolder;
				if (index == -1)
				{
					mfolder = new ParentViewFolder(dir, AssetServer.GetParentGUID(guid, changeset));
					ArrayUtility.Add(ref folders, mfolder);
				}
				else
				{
					mfolder = folders[index];
				}

				// Look for cases when we have two assets with the same name
				// One deleted and another (with different guid) new...
				index = IndexOf(mfolder.files, fileName);
				if (index != -1)
				{
					if (((int)mfolder.files[index].changeFlags & (int)ChangeFlags.Deleted) == 0)
					{
						// leave the "deleted" one
						mfolder.files[index].guid = guid;
						mfolder.files[index].changeFlags = changeFlags;
					}

					return;
				}

				ArrayUtility.Add(ref mfolder.files, new ParentViewFile(fileName, guid, changeFlags));
			}
		}

		public void AddAssetItems(AssetsItem[] assets)
		{
			foreach (AssetsItem item in assets)
			{
				AddAssetItem(item.guid, item.pathName, item.assetIsDir != 0, (ChangeFlags)item.changeFlags, -1);
			}

			System.Array.Sort(folders, CompareViewFolder);
			for (int i = 0; i < folders.Length; i++)
				System.Array.Sort(folders[i].files, CompareViewFile);
		}

		public void AddAssetItems(Changeset assets)
		{
			foreach (ChangesetItem item in assets.items)
			{
				AddAssetItem(item.guid, item.fullPath, item.assetIsDir != 0, item.changeFlags, assets.changeset);
			}

			System.Array.Sort(folders, CompareViewFolder);
			for (int i = 0; i < folders.Length; i++)
				System.Array.Sort(folders[i].files, CompareViewFile);
		}

		public void AddAssetItems(DeletedAsset[] assets)
		{
			foreach (DeletedAsset item in assets)
			{
				AddAssetItem(item.guid, item.fullPath, item.assetIsDir != 0, ChangeFlags.None, -1);
			}

			System.Array.Sort(folders, CompareViewFolder);
			for (int i = 0; i < folders.Length; i++)
				System.Array.Sort(folders[i].files, CompareViewFile);
		}

		public void Clear()
		{
			folders = new ParentViewFolder[0];
			selectedFolder = -1;
			selectedFile = -1;
			initialSelectedItem = -1;
		}

		public bool NextFileFolder(ref int folder, ref int file)
		{
			if (folder >= folders.Length)
				return false;

			ParentViewFolder mfolder = folders[folder];

			if (file >= mfolder.files.Length - 1)
			{
				folder++;
				file = -1;

				if (folder >= folders.Length)
					return false;
			}
			else
				file++;

			return true;
		}

		public bool IndexToFolderAndFile(int index, ref int folder, ref int file)
		{
			folder = 0;
			file = -1;

			for (int i = 0; i < index; i++)
				if (!NextFileFolder(ref folder, ref file))
					return false;

			return true;
		}
	}
}