#ifndef OBJECTHASHGENERATOR_H
#define OBJECTHASHGENERATOR_H
#include "MdFourGenerator.h"
#include "Runtime/Utilities/GUID.h"

class Object;

void FeedHashWithObject(MdFourGenerator& generator, Object& o, int flags = 0);
void FeedHashWithObjectAndAllDependencies(MdFourGenerator& generator, std::vector<Object*>& objects, int flags);
void HashInstanceID (MdFourGenerator& generator, SInt32 instanceID);


#endif