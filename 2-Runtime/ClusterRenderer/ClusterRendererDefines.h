#pragma once

#if ENABLE_CLUSTER_SYNC
	#define DECLARE_CLUSTER_SERIALIZE(x) \
		template<class TransferFunc> void ClusterTransfer (TransferFunc& transfer);
	#define IMPLEMENT_CLUSTER_SERIALIZE(x) \
		void x##UnusedClusterTemplateInitializer_() { \
		x *a = NULL; StreamedBinaryWrite<false> *w = NULL; StreamedBinaryRead<false> *r = NULL; \
		a->ClusterTransfer(*r);	a->ClusterTransfer(*w); }
#else
	#define DECLARE_CLUSTER_SERIALIZE(x)
	#define IMPLEMENT_CLUSTER_SERIALIZE(x)
#endif
