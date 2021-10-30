#ifndef PIPELINE_H
#define PIPELINE_H

#include "Runtime/BaseClasses/GameObject.h"


class Pipeline : public Unity::Component {
public:	
	REGISTER_DERIVED_CLASS (Pipeline, Component)
	DECLARE_OBJECT_SERIALIZE (Pipeline)

	Pipeline(MemLabelId label, ObjectCreationMode mode);
	
};

#endif
