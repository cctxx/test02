/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/object.h"

#include "Runtime/mecanim/graph/plug.h"
#include "Runtime/mecanim/graph/node.h"

namespace mecanim
{

namespace graph
{
	// This class can be subclassed if you need to create custom node and want to add them to the list of
	// instanciable node
	class GraphFactory
	{
	public:
		virtual Node* Create(eNodeType aNodeId, memory::Allocator& arAlloc)const;
		virtual GraphPlug* Create(ePlugType aPlugType, memory::Allocator& arAlloc)const;
	};

	template <typename TYPE> TYPE* Create(GraphFactory const& arFactory, memory::Allocator& arAlloc)
	{
		return static_cast<TYPE*>(arFactory.Create(TYPE::mId, arAlloc));
	}
}

}