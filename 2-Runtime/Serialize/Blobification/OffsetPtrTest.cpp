#include "UnityPrefix.h"

#include "Runtime/Serialize/SerializeTraits.h"
#include "offsetptr.h"


#include <limits>


void TestOffsetPtr ()
{
	OffsetPtr<size_t>* ptrHigh = new OffsetPtr<size_t>;
	OffsetPtr<size_t>* ptrLow = new OffsetPtr<size_t>;

	size_t* ptrH = reinterpret_cast<size_t*>(std::numeric_limits<size_t>::max()-4);
	size_t* ptrL = reinterpret_cast<size_t*>(4);

	ptrHigh->reset(ptrH);
	ptrLow->reset(ptrL);

	size_t h = reinterpret_cast<size_t>(ptrHigh->Get());
	size_t l = reinterpret_cast<size_t>(ptrLow->Get());


	Assert(h == std::numeric_limits<size_t>::max()-4);
	Assert(l == 4);

	delete ptrHigh;
	delete ptrLow;

}
