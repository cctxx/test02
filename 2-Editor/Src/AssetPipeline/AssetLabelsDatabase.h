#ifndef ASSETLABELS_DATABASE_H
#define ASSETLABELS_DATABASE_H

#include "AssetDatabaseStructs.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Utilities/GUID.h"

class SQLite;

class AssetLabelsDatabase
{
public:
	AssetLabelsDatabase ();
	~AssetLabelsDatabase ();
	
	void RefreshLabelMatchIndex(Assets::const_iterator begin, Assets::const_iterator end);
	void PopulateLabelMatchIndex(const UnityGUID& guid, const std::vector<UnityStr>& labels);
	void LabelMatchIndexRemove(const UnityGUID& guid);
	void LabelMatchIndexAdd(const UnityGUID& guid, const std::string& label);
	std::vector<std::string> MatchLabelsPartial(const UnityGUID& guid, const std::string& partial);
	const std::vector<UnityStr>& GetLabels(const UnityGUID& guid) const;
	void ClearLabels(const UnityGUID& guid);
	void CopyLabelsToMetaData(const UnityGUID& guid, const AssetLabels& labels);
	void SetLabels(const UnityGUID& guid, const std::vector<UnityStr>& labels);
	const std::map<UnityStr,float> GetAllLabels ();	
private:

	SQLite* GetLabelMatchIndexDB();
	SQLite*                  m_AssetLabelDB;
};


#endif
