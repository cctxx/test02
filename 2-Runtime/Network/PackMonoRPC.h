#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK

#include "Runtime/Mono/MonoIncludes.h"
#include "External/RakNet/builds/include/BitStream.h"
#include "NetworkEnums.h"

typedef void NetworkViewRpcFunc (RPCParameters *rpcParameters);

bool UnpackAndInvokeRPCMethod (GameObject& target, const char* name, RakNet::BitStream& parameters, SystemAddress sender, NetworkViewID viewID, RakNetTime timestamp, Object* netview);
bool PackRPCParameters (GameObject& target, const char* name, RakNet::BitStream& inStream, MonoArray* data, Object* netview);

bool UnpackAndInvokeRPCMethod (MonoBehaviour& targetBehaviour, MonoMethod* method, RakNet::BitStream& parameters, SystemAddress sender, NetworkViewID viewID, RakNetTime timestamp, Object* netview);
bool PackRPCParameters (MonoBehaviour& targetBehaviour, MonoMethod* method, RakNet::BitStream& inStream, MonoArray* data, Object* netview, bool doPack);

#endif