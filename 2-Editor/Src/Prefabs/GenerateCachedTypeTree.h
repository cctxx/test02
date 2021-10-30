#ifndef GENERATE_CACHED_TYPETREE_H
#define GENERATE_CACHED_TYPETREE_H

class TypeTree;
class Object;

const TypeTree& GenerateCachedTypeTree (Object& object, int flags);
void CleanupTypeTreeCache ();

#endif
