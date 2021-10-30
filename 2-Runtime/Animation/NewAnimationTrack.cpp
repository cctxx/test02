#include "UnityPrefix.h"
#if UNITY_EDITOR
#include "NewAnimationTrack.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Graphics/Transform.h"

using namespace std;

enum { kModifiesRotation = 1 << 0, kModifiesScale = 1 << 1 };

vector<string> NewAnimationTrack::GetCurves ()
{
	vector<string> curves;
	curves.reserve (m_Curves.size ());
	for (Curves::iterator i=m_Curves.begin ();i!=m_Curves.end ();i++)
		curves.push_back (i->attributeName);
	return curves;
}

NewAnimationTrack::NewAnimationTrack (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_ClassID = 0;
}

NewAnimationTrack::~NewAnimationTrack ()
{}

/*
template<class T> inline
float GetValue (Object& o, int offset)
{
	return *reinterpret_cast<T*> (reinterpret_cast<char*> (&o) + offset);
}

bool NewAnimationTrack::ExtractFloatValue (Object* src, const TypeTree* value, float* f)
{
	if (value == NULL)
		return false;
	if (value->m_ByteOffset == -1)
		return false;

	if (value->m_Type == "float")
	{
		if (src && f)
			*f = GetValue<float> (*src, value->m_ByteOffset);
		return true;
	}
	else if (value->m_Type == "bool")
	{
		if (src && f)
			*f = GetValue<bool> (*src, value->m_ByteOffset);
		return true;
	}
	else
		return false;
}
*/
template<class TransferFunction>
void NewAnimationTrack::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_Curves);
	TRANSFER (m_ClassID);
}

template<class TransferFunction>
void NewAnimationTrack::Channel::Transfer (TransferFunction& transfer)
{
	TRANSFER (byteOffset);
	TRANSFER (curve);
	TRANSFER (attributeName);
}

IMPLEMENT_CLASS (NewAnimationTrack)
IMPLEMENT_OBJECT_SERIALIZE (NewAnimationTrack)
#endif
