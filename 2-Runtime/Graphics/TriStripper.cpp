#include "UnityPrefix.h"
#include "External/Tristripper/Striper.h"
#include "TriStripper.h"
#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/Utilities/vector_utility.h"
#include "Runtime/Utilities/LogAssert.h"

using namespace std;


bool Stripify (const UInt32* faces, int count, UNITY_TEMP_VECTOR(UInt32)& strip)
{
	Assert (faces != NULL || !count);
	strip.clear ();

	// Fail on empty input.
	if (!count)
		return false;

	STRIPERCREATE stripinput;
	stripinput.DFaces = const_cast<UInt32*> (faces);
	stripinput.NbFaces = count / 3;

	STRIPERRESULT stripresult;
	Striper striper;
	if (striper.Init (stripinput) && striper.Compute (stripresult) && stripresult.NbStrips == 1)
	{
		int stripSize = stripresult.StripLengths[0];
		UInt32* stripData = reinterpret_cast<UInt32*> (stripresult.StripRuns);
		reserve_trimmed (strip, stripSize);
		strip.assign (stripData, stripData + stripSize);
		return true;
	}
	return false;
}

template<typename T>
int CountTrianglesInStrip (const T* strip, int length)
{
	int count = 0;
	// de-stripify :
	int n = length;
	for(int i=0;i<(n-2);i++)
	{
		T a = strip[i];
		T b = strip[i+1];
		T c = strip[i+2];

		// skip degenerates
		if ( a == b || a == c || b == c )
			continue;
		
		count++;
	}
	return count;
}

template int CountTrianglesInStrip<UInt16> (const UInt16* strip, int length);
template int CountTrianglesInStrip<UInt32> (const UInt32* strip, int length);

// WARNING: You have to make sure you have space for all indices in trilist (you can use CountTrianglesInStrip for that)
template<typename Tin, typename Tout>
void Destripify(const Tin* strip, int length, Tout* trilist, int capacity)
{
	// de-stripify :
	int n = length;
	int triIndex = 0;
	for(int i=0;i<(n-2);i++)
	{
		Tin a = strip[i];
		Tin b = strip[i+1];
		Tin c = strip[i+2];

		// skip degenerates
		if ( a == b || a == c || b == c )
			continue;

		// do the winding flip-flop of strips :
		if ( (i&1) == 1 )
			std::swap (a,b);

		trilist[triIndex++] = a;
		trilist[triIndex++] = b;
		trilist[triIndex++] = c;
	}
}


template void Destripify<UInt32, UInt32> (const UInt32* strip, int length, UInt32* trilist, int capacity);
template void Destripify<UInt16, UInt32> (const UInt16* strip, int length, UInt32* trilist, int capacity);
template void Destripify<UInt16, UInt16> (const UInt16* strip, int length, UInt16* trilist, int capacity);

void Destripify (const UInt16* strip, int length, UNITY_TEMP_VECTOR(UInt16)& trilist)
{
	int oldSize = trilist.size();
	trilist.resize (oldSize + CountTrianglesInStrip(strip, length) * 3);
	Destripify(strip, length, &trilist[oldSize], trilist.size());
}

void Destripify (const UInt16* strip, int length, UNITY_TEMP_VECTOR(UInt32)& trilist)
{
	int oldSize = trilist.size();
	trilist.resize (oldSize + CountTrianglesInStrip(strip, length) * 3);
	Destripify(strip, length, &trilist[oldSize], trilist.size());
}

void Destripify (const UInt32* strip, int length, UNITY_TEMP_VECTOR(UInt32)& trilist)
{
	int oldSize = trilist.size();
	trilist.resize (oldSize + CountTrianglesInStrip(strip, length) * 3);
	Destripify(strip, length, &trilist[oldSize], trilist.size());
}
