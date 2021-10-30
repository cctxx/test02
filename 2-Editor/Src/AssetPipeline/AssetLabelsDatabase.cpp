#include "UnityPrefix.h"
#include "AssetLabelsDatabase.h"
#include "External/sqlite/SQLite.h"

#include "AssetDatabase.h"
#include "AssetMetaData.h"
#include "AssetImporter.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Serialize/TransferFunctions/YAMLRead.h"

const string kAssetLabelDBPath		= ":memory:"; // Use in-memory db for autocompletion
static const char* kAssetLabelSchema		= "create table if not exists assetlabels (  guid blob,  label text, primary key (guid, label)  );";
static const char* kAssetLabelDropIndex	= "drop index if exists assetlabel_idx; ";
static const char* kAssetLabelIndex		= "create index if not exists assetlabel_idx on assetlabels ( label ); ";
static const char* kAssetLabelInsert		= "insert or ignore into assetlabels(guid, label) values (?1, ?2);";
static const char* kAssetLabelSelect		= "select label, count(*) as refcount from assetlabels al1 where   label like ?2 || '%' and not exists (select 1 from assetlabels al2 where al2.guid =?1 and al1.label = al2.label ) group by label order by refcount desc, label asc limit 30;";
static const char* kAssetLabelSelectLabels	= "select label, count(*) as refcount from assetlabels al1 group by label order by refcount desc, label asc;";
static const char* kAssetLabelFlush		= "delete from assetlabels;";
static const char* kAssetLabelRemove		= "delete from assetlabels where guid = ?1 ;";
static const char* kAssetLabelBegin		= "begin transaction;";
static const char* kAssetLabelCommit		= "commit ;";

#define FatalIfSQLError(st) if(! st.ResultOK() ) FatalErrorStringDontReport(st.GetErrorString())

AssetLabelsDatabase::AssetLabelsDatabase ()
{
	m_AssetLabelDB = NULL;
}

AssetLabelsDatabase::~AssetLabelsDatabase ()
{
	delete m_AssetLabelDB;
}

SQLite* AssetLabelsDatabase::GetLabelMatchIndexDB() 
{
	if (m_AssetLabelDB != NULL)
		return m_AssetLabelDB;
	
	m_AssetLabelDB = new SQLite(kAssetLabelDBPath);
	Assert(m_AssetLabelDB!= NULL);
	SQLite::StatementHandle st(m_AssetLabelDB, kAssetLabelSchema);
	FatalIfSQLError(st);
	st.ExecuteStep();
	FatalIfSQLError(st);
	
	///@TODO: WTF is the point of initializing this every time on startup???
	RefreshLabelMatchIndex(AssetDatabase::Get().begin(), AssetDatabase::Get().end());
	
	
	return m_AssetLabelDB;
}

const std::map<UnityStr, float> AssetLabelsDatabase::GetAllLabels()
{
	std::map<UnityStr,float> labels;
	
	string predefinedLabelPath = AppendPathName(GetApplicationContentsPath(),"Resources/labels.yaml");
	
	if ( IsFileCreated( predefinedLabelPath ) ) 
	{
		TEMP_STRING labelString;
		ReadStringFromFile (&labelString, predefinedLabelPath);
  
		YAMLRead read (labelString.c_str(), labelString.size(), 0);
		read.Transfer(labels, "labels");
	}

	// Top used labels from current projects	
	SQLite::StatementHandle st(GetLabelMatchIndexDB(), kAssetLabelSelectLabels);
	FatalIfSQLError(st);
	
	while ( st.ExecuteStep() ) 
	{
		string label = st.GetTextColumn(0);
		int refcount = st.GetIntColumn(1);
		labels[label] += refcount;
	}
	FatalIfSQLError(st);
	
	return labels;
}

void AssetLabelsDatabase::LabelMatchIndexAdd(const UnityGUID& guid, const string& label)
{
	AssertIf(label =="");
	
	SQLite::StatementHandle st(GetLabelMatchIndexDB(), kAssetLabelInsert);
	st.Bind(1, guid);
	st.Bind(2, label);
	st.ExecuteStep();
	FatalIfSQLError(st);
	
}

void AssetLabelsDatabase::LabelMatchIndexRemove(const UnityGUID& guid)
{
	
	SQLite::StatementHandle st(GetLabelMatchIndexDB(), kAssetLabelRemove);
	st.Bind(1, guid);
	st.ExecuteStep();
}


void AssetLabelsDatabase::PopulateLabelMatchIndex(const UnityGUID& guid, const std::vector<UnityStr>& labels)
{
	LabelMatchIndexRemove(guid);
	std::vector<UnityStr>::const_iterator i;
	for (i = labels.begin(); i!= labels.end(); i++)
	{
		LabelMatchIndexAdd(guid,*i);
	}	
}

void AssetLabelsDatabase::RefreshLabelMatchIndex(Assets::const_iterator begin, Assets::const_iterator end)
{
	Assert(m_AssetLabelDB != NULL);
	
	SQLite::StatementHandle st(m_AssetLabelDB, kAssetLabelDropIndex);
	FatalIfSQLError(st);
	st.ExecuteStep();
	FatalIfSQLError(st);
	
	SQLite::StatementHandle st2(m_AssetLabelDB, kAssetLabelFlush);
	FatalIfSQLError(st2);
	st2.ExecuteStep();
	FatalIfSQLError(st2);
	
	SQLite::StatementHandle st3(m_AssetLabelDB, kAssetLabelBegin);
	FatalIfSQLError(st3);
	st3.ExecuteStep();
	FatalIfSQLError(st3);
	
	for (Assets::const_iterator i = begin; i != end; i++)
	{
		const Asset& a = (*i).second;
		const std::vector<UnityStr>& labels = a.labels.GetLabels();
		PopulateLabelMatchIndex((*i).first, labels);
	}
	
	SQLite::StatementHandle st4(m_AssetLabelDB, kAssetLabelCommit);
	FatalIfSQLError(st4);
	st4.ExecuteStep();
	FatalIfSQLError(st4);
	
	SQLite::StatementHandle st5(m_AssetLabelDB, kAssetLabelIndex);
	FatalIfSQLError(st5);
	st5.ExecuteStep();
	FatalIfSQLError(st5);
}

std::vector<std::string> AssetLabelsDatabase::MatchLabelsPartial(const UnityGUID& guid, const string& partial) 
{
	std::vector<std::string> result;
	// Don't pass too long strings to the sql index
	if ( partial.length() > 255 )
		return result;
	
	SQLite::StatementHandle st(GetLabelMatchIndexDB(), kAssetLabelSelect);
	FatalIfSQLError(st);
	st.Bind(1, guid);
	st.Bind(2, partial);
	
	while ( st.ExecuteStep() ) 
	{
		string label = st.GetTextColumn(0);
		string refcount = st.GetTextColumn(1);
		//printf_console("Partial match for %s,%s -> '%s'. Refcount %s\n",GUIDToString(guid).c_str(), partial.c_str(), label.c_str(), refcount.c_str()); 
		result.push_back(label);
	}
	FatalIfSQLError(st);
	
	return result;
}

const std::vector<UnityStr>& AssetLabelsDatabase::GetLabels(const UnityGUID& guid) const
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	
	static std::vector<UnityStr> emptyResult;
	if (asset != NULL)
		return asset->labels.GetLabels();
	else
		return emptyResult;
}

void AssetLabelsDatabase::ClearLabels(const UnityGUID& guid) 
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset != NULL)
	{
		LabelMatchIndexRemove(guid);
		
		const_cast<Asset*> (asset)->labels.ClearLabels();
		AssetDatabase::Get().SetDirty();
		
		CopyLabelsToMetaData(guid, asset->labels);
	}
}

void AssetLabelsDatabase::CopyLabelsToMetaData(const UnityGUID& guid, const AssetLabels& labels) 
{
	AssetMetaData* metaData = FindAssetMetaDataAtPath (GetMetaDataPathFromGUID (guid));
	
	if (metaData == NULL)
	{
		FatalErrorString (Format("The file %s has no meta data. Most likely the meta data file was deleted. To fix this reimport the asset first.",GetAssetPathFromGUID(guid).c_str()));
	}
	ANALYSIS_ASSUME(metaData);
	
	metaData->labels = labels.GetLabels();
	metaData->SetDirty();

	AssetImporter* importer = FindAssetImporterAtPath (GetMetaDataPathFromGUID (guid));
	if (importer)
		importer->SetDirty ();
	
	PopulateLabelMatchIndex(guid, metaData->labels);

	AssetDatabase::Get().WriteImportSettingsIfDirty (guid);

	// case 571697
	// We want to prevent a refresh from happening from the .meta file being touched
	// Reason being that a refresh caused by tabbing out and back in to the editor can result in loss of data
	// because refresh detects that a reimport is needed and unloads what ever is in memory (even if it is dirty)
	// So after having touched the .meta file we have to update the time stamp in the asset database to prevent a reimport.
	AssetDatabase::Get().ForceTextMetaFileTimeStampUpdate(guid);
}

void AssetLabelsDatabase::SetLabels(const UnityGUID& guid, const vector<UnityStr>& labels)
{
	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset != NULL)
	{

		if (labels == asset->labels.GetLabels())
			return;

		const_cast<Asset*> (asset)->labels.SetLabels(labels.begin(), labels.end());
		
		CopyLabelsToMetaData(guid, asset->labels);

		AssetDatabase::Get().SetDirty();
	}
}
