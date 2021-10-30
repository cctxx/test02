#pragma once

#include "Configuration/UnityConfigure.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "External/Allegorithmic/builds/Engines/include/substance/inputdesc.h"
#include "Texture2D.h"

enum ProceduralPropertyType
{
	ProceduralPropertyType_Boolean = 0,
	ProceduralPropertyType_Float,
	ProceduralPropertyType_Vector2,
	ProceduralPropertyType_Vector3,
	ProceduralPropertyType_Vector4,
	ProceduralPropertyType_Color3,
	ProceduralPropertyType_Color4,
	ProceduralPropertyType_Enum,
	ProceduralPropertyType_Texture
};

inline bool IsSubstanceAnyFloatType (SubstanceInputType type) { return type >= Substance_IType_Float && type <= Substance_IType_Float4;  }
inline bool IsSubstanceAnyIntType (SubstanceInputType type)  { return (type == Substance_IType_Integer) || (type == Substance_IType_Integer2) || (type == Substance_IType_Integer3) || (type == Substance_IType_Integer4);  }
inline int GetRequiredInputComponentCount (SubstanceInputType type) 
{
	switch (type)
	{
		case Substance_IType_Float:
		case Substance_IType_Integer:
			return 1;
		case Substance_IType_Float2:
		case Substance_IType_Integer2:
			return 2;
		case Substance_IType_Float3:
		case Substance_IType_Integer3:
			return 3;
		case Substance_IType_Float4:
		case Substance_IType_Integer4:
			return 4;
		default:
			AssertString("Not supported");
			return -1;
	}
}

struct SubstanceEnumItem
{
	DECLARE_SERIALIZE(SubstanceEnumItem)
	
	int value;
	UnityStr text;
};

struct SubstanceValue
{
	DECLARE_SERIALIZE(SubstanceValue)

	float scalar[4];
	PPtr<Texture2D> texture;

	SubstanceValue() { memset(scalar, 0, sizeof(float)*4); }
};

struct SubstanceInput
{
	DECLARE_SERIALIZE(SubstanceInput)
	
	// type & names
	UnityStr name;
	UnityStr label;
	UnityStr group;
	ProceduralPropertyType type;
	SubstanceValue value;
	SubstanceInputType internalType;
	unsigned int internalIndex;
	unsigned int internalIdentifier;
	unsigned int shuffledIdentifier;

	// Ranges and step
	float minimum;
	float maximum;
	float step;

	// Enum values
	std::vector<SubstanceEnumItem> enumValues;
	std::vector<std::string> GetEnumOptions() const
	{
		std::vector<std::string> enumOptions;
		std::vector<SubstanceEnumItem>::const_iterator it;
		for (it=enumValues.begin();it!=enumValues.end();++it)
		{
			enumOptions.push_back(it->text);
		}
		return enumOptions;
	}

	// Flags
	enum Flag
	{
		Flag_SkipHint	= 1<<0,	// input is in head of the graph so the cache is not usefull
		Flag_Modified	= 1<<1, // input has been modified
		Flag_Cached		= 1<<2, // input is cached to speed up modifications
		Flag_Awake		= 1<<3, // this flag is set when AwakeFromLoad comes, this to rebuild all without having hints everywhere
		Flag_Clamp		= 1<<4	// needs to clamp value when set
	};
	unsigned int flags;
	bool IsFlagEnabled(const Flag& flag) const { return flags & (unsigned int)flag; }
	void EnableFlag(const Flag& flag, bool enabled=true) { if (enabled) flags |= (unsigned int)flag; else flags &= ~(unsigned int)flag; }

	// Altered texture links
	std::set<unsigned int> alteredTexturesUID;

	SubstanceInput ()
	{
		type = ProceduralPropertyType_Float;
		flags = SubstanceInput::Flag_Awake;
		value.scalar[0] = value.scalar[1] = value.scalar[2] = value.scalar[3] = 0.0F;
		internalType = Substance_IType_Float;
		internalIndex = 0;
		internalIdentifier = 0;

		minimum = -std::numeric_limits<float>::max();
		maximum = std::numeric_limits<float>::max();
	}
};

inline void ClampSubstanceInputValues (const SubstanceInput& input, SubstanceValue& value)
{
	// validate combo value
	if (input.type==ProceduralPropertyType_Enum && input.enumValues.size()>0)
	{
		std::vector<SubstanceEnumItem>::const_iterator it;
		for (it=input.enumValues.begin();it!=input.enumValues.end();++it)
		{
			if ((int)value.scalar[0]==it->value)
				return;
		}
		value.scalar[0] = (float)input.enumValues[0].value;
		return;
	}
	
	// clamp scalar values
	if (input.IsFlagEnabled(SubstanceInput::Flag_Clamp))
	{
		int count = GetRequiredInputComponentCount(input.internalType);
		
		if (IsSubstanceAnyIntType(input.internalType))
		{
			for (int index=0;index<count;++index)
				value.scalar[index] = (float)clamp<int>((int)(value.scalar[index]+0.5f), (int)input.minimum, (int)input.maximum);
		}
		else
		{
			for (int index=0;index<count;++index)
				value.scalar[index] = clamp<float>(value.scalar[index], input.minimum, input.maximum);
		}
	}	// step integers
	else if (IsSubstanceAnyIntType(input.internalType))
	{
		int count = GetRequiredInputComponentCount(input.internalType);
		for (int index=0;index<count;++index)
			value.scalar[index] = (float)((int)(value.scalar[index]+0.5f));
	}
}

inline bool AreSubstanceInputValuesEqual(SubstanceInputType type, SubstanceValue& first, SubstanceValue& second)
{
	if (IsSubstanceAnyFloatType(type))
	{	
		return memcmp(first.scalar, second.scalar,
			sizeof(float)*GetRequiredInputComponentCount(type))==0;
	}
	else if (IsSubstanceAnyIntType(type))
	{
		int count = GetRequiredInputComponentCount(type);
		
		for (int index=0;index<count;++index)
		{
			if ((int)first.scalar[index]!=(int)second.scalar[index])
				return false;
		}
		return true;
	}
	else if (type==Substance_IType_Image)
	{
		return first.texture==second.texture;
	}
	return false;
}

typedef std::vector<SubstanceInput> SubstanceInputs; 

template<class T>
void SubstanceEnumItem::Transfer(T& transfer)
{
	TRANSFER(value);
	TRANSFER(text);
}

template<class T>
void SubstanceValue::Transfer(T& transfer)
{
	TRANSFER(scalar[0]);
	TRANSFER(scalar[1]);
	TRANSFER(scalar[2]);
	TRANSFER(scalar[3]);
	TRANSFER(texture);
}

template<class T>
void SubstanceInput::Transfer(T& transfer)
{
	// type & names
	TRANSFER(name);
	TRANSFER(label);
	TRANSFER(group);
	transfer.Transfer(reinterpret_cast<int&> (type), "type");
	TRANSFER(value);
	transfer.Transfer(reinterpret_cast<int&> (internalType), "internalType");
	TRANSFER(internalIndex);
	TRANSFER(internalIdentifier);

	// Range and step
	TRANSFER(minimum);
	TRANSFER(maximum);
	TRANSFER(step);

	// Flags
	TRANSFER(flags);
	
	// Altered texture links
	TRANSFER(alteredTexturesUID);

	// Enum
	TRANSFER(enumValues);

	// Update status
	if (transfer.IsReading())
	{
		EnableFlag(Flag_Awake, true);
		EnableFlag(Flag_Cached, false);
	}
}
