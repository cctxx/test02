#include "UnityPrefix.h"
#include "ProceduralMaterial.h"

int ProceduralPreset_parseValues(std::vector<std::string>& values, std::string name, std::string line)
{
	values.clear();
	int start = line.find(name);
	if (start==std::string::npos)
	{
		return 0;
	}

	line = line.substr(start+name.size());
	if (line.size()<3 || line[0]!='=' || line[1]!='\"')
	{
		return 0;
	}

	line = line.substr(2);
	int p=0;
	string val;
	while (p<line.size() && line[p]!='\"')
	{
		if (line[p]==',')
		{
			values.push_back(val); val = "";
		}
		else
		{
			val += line[p];
		}
		++p;
	}
	if (val.size()>0)
	{
		values.push_back(val);
	}
	return values.size();
}

inline std::string ProceduralPreset_getLine(std::string::const_iterator& pos, const std::string& preset)
{
	std::string::const_iterator i = pos;
	std::string::const_iterator it = std::find(i, preset.end(), '\n');
	pos = it;
	if (it!=preset.end()) ++pos;
	return std::string(i, it);
}

bool ProceduralMaterial::SetPreset(const std::string& preset)
{
	std::string::const_iterator pos = preset.begin();
	std::string line;
	line = ProceduralPreset_getLine(pos, preset);
	if (line.find("formatversion=\"1.0\"")==std::string::npos)
	{
		return false;
	}

	while (pos!=preset.end() && line.find("/sbspreset")==std::string::npos)
	{
		line = ProceduralPreset_getLine(pos, preset);
		if (line.find("presetinput")!=std::string::npos)
		{
			std::vector<std::string> identifier;
			if (ProceduralPreset_parseValues(identifier, "identifier", line)!=1)
				continue;

			SubstanceInput* input = FindSubstanceInput(identifier[0]);
			if (input==NULL || identifier[0]=="$normalformat" || identifier[0]=="$outputsize")
				continue;

			std::vector<std::string> type;
			if (ProceduralPreset_parseValues(type, "type", line)!=1)
				continue;

			unsigned int internalType = atoi(type[0].c_str());
			if (internalType!=input->internalType || internalType==Substance_IType_Image)
				continue;

			std::vector<std::string> value;
			ProceduralPreset_parseValues(value, "value", line);
			float f[4];
			int count = GetRequiredInputComponentCount((SubstanceInputType)internalType);
			if (value.size()!=count)
				continue;
			for (int i=0 ; i<count ; ++i)
			{
				f[i] = (float)atof(value[i].c_str());
			}
			SetSubstanceVector(identifier[0], Vector4f(f));
		}
	}
	RebuildTextures();
	return true;
}

std::string ProceduralMaterial::GetPreset() const
{
	std::string preset = "<sbspresets formatversion=\"1.0\" count=\"1\">\n";
	preset += " <sbspreset pkgurl=\"\" description=\"\" label=\"\">\n";
	char tmp[256];
	for (SubstanceInputs::const_iterator i=m_Inputs.begin() ; i!=m_Inputs.end() ; ++i)
	{
		preset += " <presetinput identifier=\"";
		preset += i->name;
		preset += "\" uid=\"";
		snprintf(tmp, 256, "%d", i->internalIdentifier);
		preset += tmp;
		preset += "\" type=\"";
		snprintf(tmp, 256, "%d", (int)i->internalType);
		preset += tmp;
		preset += "\" value=\"";
		if (i->internalType!=Substance_IType_Image)
		{
			bool isInteger = IsSubstanceAnyIntType(i->internalType);
			int count = GetRequiredInputComponentCount(i->internalType);
			for (int j=0 ; j<count ; ++j)
			{
				if (j>0) preset += ",";
				if (isInteger) snprintf(tmp, 256, "%d", (int)i->value.scalar[j]);
				else snprintf(tmp, 256, "%f", i->value.scalar[j]);
				preset += tmp;
			}
		}
		preset += "\"/>\n";
	}
	preset += " </sbspreset>\n";
	preset += "</sbspresets>\n";
	return preset;
}

ProceduralTexture* ProceduralMaterial::GetGeneratedTexture(const std::string& textureName)
{
	for (Textures::iterator it=m_Textures.begin() ; it!=m_Textures.end() ; ++it)
	{
		if (it->IsValid() && (*it)->GetName()==textureName)
		{
			return &**it;
		}
	}
	return NULL;
}
