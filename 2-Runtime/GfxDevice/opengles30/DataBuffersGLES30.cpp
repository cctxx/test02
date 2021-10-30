#include "UnityPrefix.h"
#include "DataBuffersGLES30.h"

#if GFX_SUPPORTS_OPENGLES30

#include "AssertGLES30.h"
#include "IncludesGLES30.h"
#include "Runtime/GfxDevice/GLDataBufferCommon.h"

#include <algorithm>

#if 1
	#define DBG_LOG_BUF_GLES30(...) {}
#else
	#define DBG_LOG_BUF_GLES30(...) {printf_console(__VA_ARGS__);printf_console("\n");}
#endif

static const float	kBufferAllocateWeight			= 10.0f;			//! Default weight for buffer allocation from scratch.
static const float	kBufferSizeWeightFactor			= 1.f/8096.f;		//!< Weight factor for size difference.
static const float	kBufferUsageDiffWeight			= 8.f;				//!< Weight factor for usage difference.

static const float	kBufferDeleteSizeWeightFactor	= 1.f/16384.f;		//!< Deleting scale factor.
static const float	kBufferDeleteMaxWeightFactor	= 300.0f;			//!< ...
static const float	kBufferDeleteMinWeight			= 30.0f;			//!< Required weight for deleting buffer.

static const UInt32	kBufferPruneFrequency			= 10;				//!< Unneeded buffers are pruned once per kBufferPruneFrequency frames.

bool BufferUpdateCausesStallGLES30 (const DataBufferGLES30* buffer)
{
	// \note If needed, the min age should me made ES3 specific graphics capability.
	return buffer->GetRenderAge() < kBufferUpdateMinAgeGLES30;
}

//! Computes weight for sorting buffers 
static float ComputeBufferWeight (const DataBufferGLES30* buffer, int desiredSize, UInt32 desiredUsage)
{
	const int		bufferSize	= buffer->GetSize();
	const UInt32	bufferUsage	= buffer->GetUsage();

	if (buffer->GetSize() == 0)
		return kBufferAllocateWeight;

	const int		sizeDiff	= std::abs(bufferSize-desiredSize);

	return float(sizeDiff)*kBufferSizeWeightFactor + ((bufferUsage != desiredUsage) ? kBufferUsageDiffWeight : 0.f);
}

//! Compute weight for eliminating old buffers.
static float ComputeBufferDeleteWeight (const DataBufferGLES30* buffer)
{
	// \todo [2013-05-13 pyry] Take into account current memory pressure and buffer size.
	const UInt32 renderAge = buffer->GetRenderAge();
	return float(renderAge) - std::min(kBufferDeleteMaxWeightFactor, float(buffer->GetSize())*kBufferDeleteSizeWeightFactor);
}

struct WeightedBufferGLES3
{
	int		sizeClass;
	int		bufferNdx;
	float	weight;

	WeightedBufferGLES3 (int sizeClass_, int bufferNdx_, float weight_)
		: sizeClass	(sizeClass_)
		, bufferNdx	(bufferNdx_)
		, weight	(weight_)
	{
	}
};

struct CompareWeightsLess
{
	inline bool operator() (const WeightedBufferGLES3& a, const WeightedBufferGLES3& b) const
	{
		return a.weight < b.weight;
	}
};

struct CompareWeightsGreater
{
	inline bool operator() (const WeightedBufferGLES3& a, const WeightedBufferGLES3& b) const
	{
		return a.weight > b.weight;
	}
};

// DataBufferGLES30

static const GLenum kBufferTarget = GL_COPY_WRITE_BUFFER; //!< Target used for buffer operations.

DataBufferGLES30::DataBufferGLES30 (BufferManagerGLES30& bufferManager)
	: m_manager			(bufferManager)
	, m_buffer			(0)
	, m_size			(0)
	, m_usage			(0)
	, m_lastRecreated	(0)
	, m_lastUpdated		(0)
	, m_lastRendered	(0)
{
	GLES_CHK(glGenBuffers(1, (GLuint*)&m_buffer));
}

DataBufferGLES30::~DataBufferGLES30 (void)
{
	if (m_buffer)
		glDeleteBuffers(1, (GLuint*)&m_buffer);
}

void DataBufferGLES30::Disown (void)
{
	m_buffer = 0;
}

void DataBufferGLES30::Release (void)
{
	m_manager.ReleaseBuffer(this);
}

void DataBufferGLES30::RecreateStorage (int size, UInt32 usage)
{
	RecreateWithData(size, usage, 0);
}

void DataBufferGLES30::RecreateWithData (int size, UInt32 usage, const void* data)
{
	glBindBuffer(kBufferTarget, m_buffer);
	glBufferData(kBufferTarget, size, data, usage);
	glBindBuffer(kBufferTarget, 0);
	GLESAssert();

	RecordRecreate(size, usage);
}

void DataBufferGLES30::Upload (int offset, int size, const void* data)
{
	glBindBuffer(kBufferTarget, m_buffer);
	glBufferSubData(kBufferTarget, offset, size, data);
	glBindBuffer(kBufferTarget, 0);
	GLESAssert();

	RecordUpdate();
}

void* DataBufferGLES30::Map (int offset, int size, UInt32 mapBits)
{
	glBindBuffer(kBufferTarget, m_buffer);
	void* ptr = glMapBufferRange(kBufferTarget, offset, size, mapBits);
	glBindBuffer(kBufferTarget, 0);
	GLESAssert();

	return ptr;
}

void DataBufferGLES30::FlushMappedRange (int offset, int size)
{
	glBindBuffer(kBufferTarget, m_buffer);
	glFlushMappedBufferRange(kBufferTarget, offset, size);
	glBindBuffer(kBufferTarget, 0);
	GLESAssert();
}

void DataBufferGLES30::Unmap (void)
{
	glBindBuffer(kBufferTarget, m_buffer);
	glUnmapBuffer(kBufferTarget);
	glBindBuffer(kBufferTarget, 0);
	GLESAssert();
}

void DataBufferGLES30::RecordRecreate (int size, UInt32 usage)
{
	if (BufferUpdateCausesStallGLES30(this))
		DBG_LOG_BUF_GLES30("DataBufferGLES30: Warning: buffer with render age %u was recreated!", GetRenderAge());

	m_size			= size;
	m_usage			= usage;
	m_lastRecreated	= m_manager.GetFrameIndex();

	// Update GFX mem allocation stats
	REGISTER_EXTERNAL_GFX_DEALLOCATION(MAKE_DATA_BUFFER_ID(m_buffer));
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(MAKE_DATA_BUFFER_ID(m_buffer), size, this);
}

void DataBufferGLES30::RecordUpdate (void)
{
	if (BufferUpdateCausesStallGLES30(this))
		DBG_LOG_BUF_GLES30("DataBufferGLES30: Warning: buffer with render age %u was updated!", GetRenderAge());

	m_lastUpdated = m_manager.GetFrameIndex();
}

void DataBufferGLES30::RecordRender (void)
{
	m_lastRendered = m_manager.GetFrameIndex();
}

// \note Overflow is perfectly ok here.

UInt32 DataBufferGLES30::GetRecreateAge (void) const
{
	return m_manager.GetFrameIndex() - m_lastRecreated;
}

UInt32 DataBufferGLES30::GetUpdateAge (void) const
{
	return m_manager.GetFrameIndex() - m_lastUpdated;
}

UInt32 DataBufferGLES30::GetRenderAge (void) const
{
	return m_manager.GetFrameIndex() - m_lastRendered;
}

// BufferManagerGLES30

BufferManagerGLES30::BufferManagerGLES30 (void)
	: m_frameIndex(0)
{
}

BufferManagerGLES30::~BufferManagerGLES30 (void)
{
	Clear();
}

void BufferManagerGLES30::Clear (void)
{
	for (std::vector<DataBufferGLES30*>::iterator i = m_pendingBuffers.begin(); i != m_pendingBuffers.end(); i++)
		delete *i;
	m_pendingBuffers.clear();

	for (int ndx = 0; ndx < kSizeClassCount; ndx++)
	{
		for (std::vector<DataBufferGLES30*>::iterator i = m_liveBuffers[ndx].begin(); i != m_liveBuffers[ndx].end(); i++)
			delete *i;
		m_liveBuffers[ndx].clear();
	}
}

inline int BufferManagerGLES30::GetSizeClass (int size)
{
	for (int ndx = 0; ndx < kSizeClassCount; ndx++)
	{
		if (size < GetSizeClassLimit(ndx))
			return ndx;
	}
	Assert(false);
	return 0;
}

DataBufferGLES30* BufferManagerGLES30::AcquireBuffer (int size, UInt32 usage)
{
	const float	maxWeight		= kBufferAllocateWeight;
	const int	maxCandidates	= 5; // Number of potential candidates to consider actually.

	const int	sizeClass		= GetSizeClass(size); // Try only that size class
	int			numCandidates	= 0; // Number of potential candidates considered
	int			bestBufferNdx	= -1;
	float		bestWeight		= std::numeric_limits<float>::infinity();

	for (int bufferNdx = 0; bufferNdx < (int)m_liveBuffers[sizeClass].size(); bufferNdx++)
	{
		DataBufferGLES30*	buffer	= m_liveBuffers[sizeClass][bufferNdx];
		const float			weight	= ComputeBufferWeight(buffer, size, usage);

		if (weight <= maxWeight && weight < bestWeight)
		{
			bestBufferNdx	= bufferNdx;
			bestWeight		= weight;

			numCandidates += 1;
		}

		if (numCandidates >= maxCandidates)
			break; // Do not try other buffers, sorry.
	}

	DBG_LOG_BUF_GLES30("BufferManagerGLES30::AcquireBuffer(%d, 0x%04x): tried %d candidates", size, usage, numCandidates);

	if (bestBufferNdx >= 0)
	{
		const int			bufferNdx		= bestBufferNdx;
		DataBufferGLES30*	selectedBuffer	= m_liveBuffers[sizeClass][bufferNdx];

		if (bufferNdx+1 != m_liveBuffers[sizeClass].size())
			std::swap(m_liveBuffers[sizeClass][bufferNdx], m_liveBuffers[sizeClass].back());
		m_liveBuffers[sizeClass].pop_back();

		DBG_LOG_BUF_GLES30("  => selected buffer [%d]%d, weight = %f", sizeClass, bufferNdx, ComputeBufferWeight(selectedBuffer, size, usage));
		return selectedBuffer;
	}
	else
	{
		DBG_LOG_BUF_GLES30("  => creating new buffer");
		return new DataBufferGLES30(*this);
	}
}

void BufferManagerGLES30::ReleaseBuffer (DataBufferGLES30* buffer)
{
	if (!BufferUpdateCausesStallGLES30(buffer))
		InsertIntoLive(buffer);
	else
		m_pendingBuffers.push_back(buffer);
}

void BufferManagerGLES30::AdvanceFrame (void)
{
	m_frameIndex += 1; // \note Overflow is ok.

	UpdateLiveSetFromPending();

	// \todo [2013-05-13 pyry] Do we want to do pruning somewhere else as well?
	if ((m_frameIndex % kBufferPruneFrequency) == 0)
		PruneFreeBuffers();
}

void BufferManagerGLES30::UpdateLiveSetFromPending (void)
{
	int bufNdx = 0;
	while (bufNdx < (int)m_pendingBuffers.size())
	{
		if (!BufferUpdateCausesStallGLES30(m_pendingBuffers[bufNdx]))
		{
			DataBufferGLES30* newLiveBuffer = m_pendingBuffers[bufNdx];

			if (bufNdx+1 != m_pendingBuffers.size())
				std::swap(m_pendingBuffers[bufNdx], m_pendingBuffers.back());
			m_pendingBuffers.pop_back();

			InsertIntoLive(newLiveBuffer);
			// \note bufNdx now contains a new buffer and it must be processed as well. Thus bufNdx is not incremented.
		}
		else
			bufNdx += 1;
	}
}

void BufferManagerGLES30::InsertIntoLive (DataBufferGLES30* buffer)
{
	const int	bufSize		= buffer->GetSize();
	const int	sizeClass	= GetSizeClass(bufSize);

	m_liveBuffers[sizeClass].push_back(buffer);
}

UInt32 BufferManagerGLES30::GetTotalFreeSize (void)
{
	UInt32 totalBytes = 0;

	for (std::vector<DataBufferGLES30*>::const_iterator bufIter = m_pendingBuffers.begin(); bufIter != m_pendingBuffers.end(); ++bufIter)
		totalBytes += (*bufIter)->GetSize();

	for (int ndx = 0; ndx < kSizeClassCount; ndx++)
	{
		for (std::vector<DataBufferGLES30*>::const_iterator bufIter = m_liveBuffers[ndx].begin(); bufIter != m_liveBuffers[ndx].end(); ++bufIter)
			totalBytes += (*bufIter)->GetSize();
	}

	return totalBytes;
}

void BufferManagerGLES30::PruneFreeBuffers (void)
{
	const UInt32 numBytesInFreeList = GetTotalFreeSize();
	DBG_LOG_BUF_GLES30("BufferManagerGLES30: %u B / %.2f MiB in free buffers", numBytesInFreeList, float(numBytesInFreeList) / float(1<<20));

	// \todo [2013-05-13 pyry] Do this properly - take into account allocated memory size.

	UInt32	numBytesFreed		= 0;
	int		numBuffersDeleted	= 0;

	// \note pending buffers are ignored. They will end up in live soon anyway.
	for (int sizeClass = 0; sizeClass < kSizeClassCount; sizeClass++)
	{
		int bufNdx = 0;
		while (bufNdx < m_liveBuffers[sizeClass].size())
		{
			DataBufferGLES30*	buffer	= m_liveBuffers[sizeClass][bufNdx];
			const float			weight	= ComputeBufferDeleteWeight(buffer);

			if (weight >= kBufferDeleteMinWeight)
			{
				if (bufNdx+1 != m_liveBuffers[sizeClass].size())
					std::swap(m_liveBuffers[sizeClass][bufNdx], m_liveBuffers[sizeClass].back());
				m_liveBuffers[sizeClass].pop_back();

				numBytesFreed		+= buffer->GetSize();
				numBuffersDeleted	+= 1;

				delete buffer;
			}
			else
				bufNdx += 1;
		}
	}

	DBG_LOG_BUF_GLES30("  => freed %d buffers, %u B / %.2f MiB", numBuffersDeleted, numBytesFreed, float(numBytesFreed) / float(1<<20));
}

void BufferManagerGLES30::InvalidateAll (void)
{
	for (std::vector<DataBufferGLES30*>::iterator iter = m_pendingBuffers.begin(); iter != m_pendingBuffers.end(); ++iter)
	{
		(*iter)->Disown();
		delete *iter;
	}
	m_pendingBuffers.clear();

	for (int classNdx = 0; classNdx < kSizeClassCount; classNdx++)
	{
		for (std::vector<DataBufferGLES30*>::iterator iter = m_liveBuffers[classNdx].begin(); iter != m_liveBuffers[classNdx].end(); ++iter)
		{
			(*iter)->Disown();
			delete *iter;
		}
		m_liveBuffers[classNdx].clear();
	}
}

BufferManagerGLES30* g_bufferManager = 0;

BufferManagerGLES30* GetBufferManagerGLES30 (void)
{
	if (!g_bufferManager)
		g_bufferManager = new BufferManagerGLES30();
	return g_bufferManager;
}

#endif // GFX_SUPPORTS_OPENGLES30
