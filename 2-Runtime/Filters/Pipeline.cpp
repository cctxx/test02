#include "UnityPrefix.h"
#include "Pipeline.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

IMPLEMENT_CLASS (Pipeline)
IMPLEMENT_OBJECT_SERIALIZE (Pipeline)

Pipeline::Pipeline(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

Pipeline::~Pipeline ()
{
}

template<class TransferFunction>
void Pipeline::Transfer (TransferFunction& transfer) {
	Super::Transfer (transfer);
}
