#pragma once

#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"

#if GFX_SUPPORTS_OPENGLES30

#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

class BufferManagerGLES30;

enum
{
	kBufferUpdateMinAgeGLES30		= 2 //!< This many frames must be elapsed since last render before next buffer update. \todo [2013-05-31 pyry] From GfxDevice caps
};

class DataBufferGLES30
{
public:
							DataBufferGLES30		(BufferManagerGLES30& bufferManager);
							~DataBufferGLES30		(void);

	void					Release					(void); //!< Release to BufferManager.

	UInt32					GetBuffer				(void) const { return m_buffer; }
	int						GetSize					(void) const { return m_size;	}
	UInt32					GetUsage				(void) const { return m_usage;	}

	void					RecreateStorage			(int size, UInt32 usage);
	void					RecreateWithData		(int size, UInt32 usage, const void* data);

	void					Upload					(int offset, int size, const void* data);

	void*					Map						(int offset, int size, UInt32 mapBits);
	void					FlushMappedRange		(int offset, int size);
	void					Unmap					(void);

	void					RecordRecreate			(int size, UInt32 usage);	//!< Updates storage parameters and recreate time.
	void					RecordUpdate			(void);						//!< Updates last update time if buffer was updated manually.
	void					RecordRender			(void);						//!< Updates last render time.

	UInt32					GetRecreateAge			(void) const;
	UInt32					GetUpdateAge			(void) const;
	UInt32					GetRenderAge			(void) const;

	//! Disown and remove buffer handle. Used if destructor should not try to delete buffer..
	void					Disown					(void);

private:
							DataBufferGLES30		(const DataBufferGLES30& other);
	DataBufferGLES30&		operator=				(const DataBufferGLES30& other);

	BufferManagerGLES30&	m_manager;
	UInt32					m_buffer;
	int						m_size;
	UInt32					m_usage;

	// \note Always used to compute relative age and overflow is handled
	//		 in computation. Thus frame index can safely overflow.
	UInt32					m_lastRecreated;		//!< Last recreated.
	UInt32					m_lastUpdated;			//!< Frame index when last updated.
	UInt32					m_lastRendered;			//!< Frame index when last rendered.
};

// BufferManager
//
// BufferManager is responsible for allocating and maintaining list of free buffer objects that
// could be recycled later on. Buffers are either allocated or recycled based on their properties.
// Most important property for proper use of buffers is to make sure they are not recycled
// too soon after using them for rendering.
//
// BufferManager is only responsible for managing currently free buffers. So user must either
// release or destroy buffer objects manually. User is also responsible of implementing sane
// usage patterns for buffers that it owns (for example not updating data right after buffer
// has been submitted for rendering).
//
// Buffers are associated to the BufferManager that was used to create them. Thus user must either
// destroy buffer, or release it back to same BufferManager.
//
// The best usage pattern for leveraging BufferManager is to always release buffers when there
// is no longer need to preserve the data in buffer object. That way BufferManager takes care
// of recycling buffer when it is appropriate.

class BufferManagerGLES30
{
public:
							BufferManagerGLES30		(void);
							~BufferManagerGLES30	(void);

	//! Acquire a new or recycled buffer. Returns either buffer object that can fit data, or empty buffer (GetSize() == 0).
	DataBufferGLES30*		AcquireBuffer			(int size, UInt32 usage);
	void					ReleaseBuffer			(DataBufferGLES30* buffer);

	void					AdvanceFrame			(void); //!< Advance frame index. Must be called at the end of frame.
	UInt32					GetFrameIndex			(void) const { return m_frameIndex; }

	//!< Invalidate all owned buffers. Used on context loss.
	void					InvalidateAll			(void);

private:
							BufferManagerGLES30		(const BufferManagerGLES30& other);
	BufferManagerGLES30&	operator=				(const BufferManagerGLES30& other);

	void					Clear					(void);

	void					PruneFreeBuffers		(void);
	UInt32					GetTotalFreeSize		(void);

	static inline int		GetSizeClassLimit		(int classNdx) { return classNdx+1 < kSizeClassCount ? (1<<(classNdx*kSizeClassStepLog2 + kSizeClassBaseLog2)) : INT_MAX; }

	void					UpdateLiveSetFromPending(void);
	void					InsertIntoLive			(DataBufferGLES30* buffer);
	static int				GetSizeClass			(int bufSize);

	UInt32					m_frameIndex;	//!< Frame index for computing buffer ages.

	enum
	{
		kSizeClassBaseLog2	= 10,
		kSizeClassStepLog2	= 1,
		kSizeClassCount		= 7
	};

	// Buffers that can not be selected are in pendingBuffers. Live buffers contain
	// buffers organized by size into kSizeClassCount classes.
	std::vector<DataBufferGLES30*>	m_pendingBuffers;
	std::vector<DataBufferGLES30*>	m_liveBuffers[kSizeClassCount];
};

// \todo [2013-05-10 pyry] Do not use singletons...
BufferManagerGLES30*	GetBufferManagerGLES30				(void);

//! Determine if buffer update will likely cause GPU stall.
bool					BufferUpdateCausesStallGLES30		(const DataBufferGLES30* buffer);

#endif // GFX_SUPPORTS_OPENGLES30
