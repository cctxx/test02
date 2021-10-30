#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/FileUtilities.h"

#include "Runtime/Allocator/MemoryManager.h"

#include <assert.h>
#include <new>

#if ENABLE_MECANIM_PROFILER
#include <fstream>
#include <stack>
#include <string.h>
#include <stdlib.h>
#include <ctime>
#endif

template <typename TYPE> class OffsetPtr;

namespace mecanim
{

namespace memory
{	
#if ENABLE_MECANIM_PROFILER
	class Profiler
	{
	public:

		struct AllocationInfo
		{
			AllocationInfo():m_Size(0),m_Allocator("Unknow"){}
			AllocationInfo(size_t size, char const* allocator):m_Size(size),m_Allocator(allocator){}
			bool operator==(AllocationInfo const& allocationInfo)const
			{
				return m_Size == allocationInfo.m_Size && strcmp(m_Allocator, allocationInfo.m_Allocator) == 0;
			}
			size_t	m_Size;
			char const* m_Allocator;
		};


		typedef std::map<void*, AllocationInfo> AllocationInfos; 
		struct ProfilerLabel
		{
			ProfilerLabel():m_Label("Unknow"), m_ExclusiveSize(0),m_InclusiveSize(0),m_Parent(0),m_Count(0){}
			ProfilerLabel(char const* labelId):m_Label(labelId), m_ExclusiveSize(0),m_InclusiveSize(0),m_Parent(0),m_Count(0){}
			~ProfilerLabel()
			{
				for(int i=0;i<m_Children.size();i++)
				{
					m_Children[i]->~ProfilerLabel();
					MemoryManager::LowLevelFree(m_Children[i]);
				}

				m_Children.clear();
			}

			char const*		m_Label;
			size_t			m_ExclusiveSize;
			size_t			m_InclusiveSize;
			size_t			m_Count;
			AllocationInfos	m_Allocations;

			ProfilerLabel*				m_Parent;
			std::vector<ProfilerLabel*>	m_Children;
		};

		Profiler():m_Root("Root"){}
		~Profiler(){}

		static void StaticInitialize()
		{
			s_Profiler = UNITY_NEW(Profiler, kMemDefault)();
			s_Profiler->m_Current = &s_Profiler->m_Root;
		}

		static void StaticDestroy()
		{
			UNITY_DELETE(s_Profiler, kMemDefault);
		}

		void PushLabel(char const * label)
		{ 
			m_LabelStack.push(label);
			LabelMap::iterator it = m_Label.find(label);
			if(it == m_Label.end())
				it = m_Label.insert( std::make_pair(label, ProfilerLabel(label) ) ).first;
			
			AssertIf(it == m_Label.end());
			it->second.m_Count++;

			s_Profiler->m_Current->m_Children.push_back( new (MemoryManager::LowLevelAllocate(sizeof(ProfilerLabel))) ProfilerLabel(label) );
			ProfilerLabel* newLabel = s_Profiler->m_Current->m_Children.back();
			newLabel->m_Parent = s_Profiler->m_Current;
			s_Profiler->m_Current = newLabel;
		}

		void PopLabel()
		{
			m_LabelStack.pop();

			ProfilerLabel* parentLabel = s_Profiler->m_Current->m_Parent;

			// If not a single allocation had been made, remove it
			if(s_Profiler->m_Current->m_Allocations.size() == 0 && s_Profiler->m_Current->m_Children.size() == 0)
			{
				parentLabel->m_Children[parentLabel->m_Children.size()-1]->~ProfilerLabel();
				MemoryManager::LowLevelFree(parentLabel->m_Children[parentLabel->m_Children.size()-1]);

				parentLabel->m_Children.resize( parentLabel->m_Children.size()-1 );
			}
			
			s_Profiler->m_Current = parentLabel;

			assert(s_Profiler->m_Current != NULL);
		}

		void RegisterAllocation(void* ptr, char const* alloc, size_t size = 0)
		{
			AssertIf(m_LabelStack.empty());
			char const* label = m_LabelStack.empty() ? "Unknow" : m_LabelStack.top();

			std::map<char const*, ProfilerLabel>::iterator it = m_Label.find(label);
			if(it == m_Label.end())
				m_Label.insert( std::make_pair(label, ProfilerLabel(label) ) );

			m_Label[label].m_Allocations.insert( std::make_pair(ptr, AllocationInfo(size, alloc) ) );
			m_Label[label].m_ExclusiveSize += size;

			s_Profiler->m_Current->m_Allocations.insert( std::make_pair(ptr, AllocationInfo(size, alloc) ) );
			s_Profiler->m_Current->m_ExclusiveSize += size;
			s_Profiler->m_Current->m_InclusiveSize += size;

			ProfilerLabel* profilerLabel = s_Profiler->m_Current->m_Parent;
			while(profilerLabel!=NULL)
			{
				profilerLabel->m_InclusiveSize += size;
				profilerLabel = profilerLabel->m_Parent;
			}

			m_PointerRegister.insert( std::make_pair(ptr, std::make_pair( &m_Label[label], s_Profiler->m_Current)));
		}

		void UnregisterAllocation(void* ptr)
		{
			if(ptr == NULL)
				return;

			PointerRegister::iterator it = m_PointerRegister.find(ptr);

			assert(it!=m_PointerRegister.end());

			if(it!=m_PointerRegister.end())
			{
				ProfilerLabel* label = it->second.first;
				AllocationInfos::iterator itAlloc = label->m_Allocations.find(ptr);
				if(itAlloc!=label->m_Allocations.end())
				{
					label->m_ExclusiveSize -= itAlloc->second.m_Size;
					label->m_Allocations.erase(itAlloc);
					if(label->m_Allocations.size() == 0 )
						m_Label.erase( m_Label.find(label->m_Label) );
				}

				label = it->second.second;
				itAlloc = label->m_Allocations.find(ptr);
				if(itAlloc != label->m_Allocations.end())
				{
					size_t size = itAlloc->second.m_Size;
					label->m_ExclusiveSize -= size;
					label->m_InclusiveSize -= size;
					label->m_Allocations.erase(itAlloc);

					ProfilerLabel* parentLabel = label->m_Parent;
					
					if(parentLabel != NULL && label->m_Allocations.size() == 0 && label->m_Children.size() == 0)
					{
						parentLabel->m_Children.erase( std::find(parentLabel->m_Children.begin(), parentLabel->m_Children.end(), label ));
						label->~ProfilerLabel();
						MemoryManager::LowLevelFree(label);
					}
					
					while(parentLabel!=NULL)
					{
						label = parentLabel;
						parentLabel = label->m_Parent;

						label->m_InclusiveSize -= size;
						if(parentLabel != NULL && label->m_Allocations.size() == 0 && label->m_Children.size() == 0)
						{
							parentLabel->m_Children.erase( std::find(parentLabel->m_Children.begin(), parentLabel->m_Children.end(), label ));
							label->~ProfilerLabel();
							MemoryManager::LowLevelFree(label);
						}
					}
				}

				m_PointerRegister.erase(it);
			}
		}

		static Profiler* GetProfiler()
		{
			return s_Profiler;
		}

		void DumpObjectTypeMemoryInfo()
		{
			time_t rawtime;
			struct tm * timeinfo;

			time (&rawtime);
			timeinfo = localtime (&rawtime);

			string tempDir = Format("%s/ObjectTypeMemoryInfo_%.2d_%.2d_%.2d_%.2dh%.2dm%.2ds.csv", 
									GetApplicationFolder().c_str(), 
									1900 + timeinfo->tm_year, 
									timeinfo->tm_mon, 
									timeinfo->tm_mday,
									timeinfo->tm_hour,
									timeinfo->tm_min,
									timeinfo->tm_sec);
			std::ofstream file(tempDir);

			file << "Object, Size, Count, Allocation Count" << std::endl;

			LabelMap::iterator it;
			for(it=m_Label.begin();it!=m_Label.end();it++)
				file << it->second.m_Label << "," << it->second.m_ExclusiveSize << "," << it->second.m_Count << "," << it->second.m_Allocations.size() << std::endl;

			file.close();
		}

		void DumpCallSiteMemoryInfo()
		{
			time_t rawtime;
			struct tm * timeinfo;

			time (&rawtime);
			timeinfo = localtime (&rawtime);

			string tempDir = Format("%s/CallSiteMemoryInfo_%.2d_%.2d_%.2d_%.2dh%.2dm%.2ds.csv", 
									GetApplicationFolder().c_str(), 
									1900 + timeinfo->tm_year, 
									timeinfo->tm_mon, 
									timeinfo->tm_mday,
									timeinfo->tm_hour,
									timeinfo->tm_min,
									timeinfo->tm_sec);
			//std::ofstream file(tempDir);
			//file.close();
		}
		
	protected:
		static Profiler* s_Profiler;

		std::stack<char const*> 		        m_LabelStack;

		typedef std::map<char const*, ProfilerLabel> LabelMap;
		LabelMap	m_Label;

		typedef std::map<void *, std::pair<ProfilerLabel*, ProfilerLabel*> > PointerRegister;
		PointerRegister	m_PointerRegister;

		ProfilerLabel							m_Root;
		ProfilerLabel*							m_Current;
	};
#else
	class Profiler
	{
	public:Profiler(){}
		~Profiler(){}

		static void StaticInitialize()
		{
			s_Profiler = UNITY_NEW(Profiler, kMemDefault)();
		}

		static void StaticDestroy()
		{
			UNITY_DELETE(s_Profiler, kMemDefault);
		}

		void PushLabel(char const * label){ }

		void PopLabel(){}

		void RegisterAllocation(void* ptr, char const* alloc, size_t size = 0){	}

		void UnregisterAllocation(void* ptr){}

		void DumpObjectTypeMemoryInfo(){}

		void DumpCallSiteMemoryInfo(){}

		static Profiler* GetProfiler()
		{
			return s_Profiler;
		}
	protected:
		static Profiler* s_Profiler;
	};
#endif

	class AutoScopeProfiler
	{
	public:
		AutoScopeProfiler(char const* label) { Profiler::GetProfiler()->PushLabel(label); }
		~AutoScopeProfiler() { Profiler::GetProfiler()->PopLabel();}
	};

	#define SETPROFILERLABEL(type) mecanim::memory::AutoScopeProfiler scopeProfiler##type(#type) 

	class Allocator
	{
	public:
		std::size_t	AlignAddress(std::size_t aAddr, std::size_t aAlign)
		{
			return aAddr + ((~aAddr + 1U) & (aAlign - 1U));
		}
		
		virtual void*		Allocate(std::size_t size, std::size_t align) = 0 ;
		virtual void		Deallocate(void * p) = 0;		

		template <typename TYPE>
		TYPE* Construct(std::size_t align = ALIGN_OF(TYPE))
		{
			
			char *ptr = reinterpret_cast<char *>(Allocate(sizeof(TYPE), align));	
			return new( (void *) ptr) TYPE;
		}

		template <typename TYPE>
		TYPE* ConstructArray(std::size_t count, std::size_t align = ALIGN_OF(TYPE))
		{
			if(count > 0)
			{
				char *ptr = reinterpret_cast<char *>(Allocate(sizeof(TYPE)*count, align));
				return new( (void *) ptr) TYPE[count];
			}
			else
			{
				return 0;
			}
		}

		template <typename TYPE>
		TYPE* ConstructArray(const TYPE* input, std::size_t count, std::size_t align = ALIGN_OF(TYPE))
		{
			if(count > 0)
			{
				TYPE *ptr = reinterpret_cast<TYPE*> (Allocate(sizeof(TYPE)*count, align));
				memcpy(ptr, input, sizeof(TYPE)*count);
				return ptr;
			}
			else
			{
				return 0;
			}
		}
		
		template <typename TYPE>
		void Deallocate(OffsetPtr<TYPE>& p)
		{
			if(!p.IsNull())
				Deallocate(p.Get());
		}
	};

	// Should be constructed with either:
	//		kMemAnimation
	//		kMemAnimationTemp
	class MecanimAllocator : public Allocator
	{
		MemLabelId m_Label;
	protected:
		MecanimAllocator(MecanimAllocator const& e):Allocator(){ m_Label = e.m_Label; m_Label.SetRootHeader(GET_CURRENT_ALLOC_ROOT_HEADER());}
		MecanimAllocator& operator=(MecanimAllocator const &){ return *this; }		
	public:
		MecanimAllocator(MemLabelId label):m_Label(label){ m_Label.SetRootHeader(GET_CURRENT_ALLOC_ROOT_HEADER());}

		virtual void* Allocate(std::size_t size, std::size_t align)
		{
			void* p = UNITY_MALLOC_ALIGNED(m_Label, size, align);

			Profiler::GetProfiler()->RegisterAllocation(p, "MecanimAllocator", size);

			return p;
		}

		virtual void Deallocate(void * p)
		{
			Profiler::GetProfiler()->UnregisterAllocation(p);
			UNITY_FREE(m_Label, p);
		}
	};

	class InPlaceAllocator : public Allocator
	{
	protected:
		char* mP;
		char* mHead;
		std::size_t mMaxSize;
		InPlaceAllocator(InPlaceAllocator const &):Allocator(){}
		InPlaceAllocator& operator=(InPlaceAllocator const &){ return *this; }
	public:
		
		
		InPlaceAllocator(void *p, std::size_t aSize):mP(reinterpret_cast<char *>(p)),mHead(reinterpret_cast<char *>(p)), mMaxSize(aSize){};

//		InPlaceAllocator(size_t size, MemoryLabelId label)
//		{
//			mHead = mP = UNITY_MALLOC_ALIGNED(label, size, 16);
//			mMaxSize = aSize;
//			
//		}
		
		~InPlaceAllocator() { }

		inline std::size_t TotalMemorySize()const  { return mMaxSize; }
		inline std::size_t UsedMemorySize()const   { return reinterpret_cast<std::size_t>(mP) - reinterpret_cast<std::size_t>(mHead); }
		inline std::size_t FreeMemorySize()const   { return TotalMemorySize() - UsedMemorySize(); }

		void* GetMemory ()
		{
			return mP;
		}
		
		virtual void* Allocate(std::size_t n, std::size_t align)
		{	
			char *p = reinterpret_cast<char *>(AlignAddress(reinterpret_cast<std::size_t>(mP), align));
			
			// The first allocation must always be aligned already
			AssertIf(mHead == mP && p != mHead);
			
			// assert(p+n <= mHead+mMaxSize);			
				
			if(p+n <= mHead+mMaxSize)
			{
				mP = p+n;
				return reinterpret_cast<void *>(p);
			}
			else
			{
				return 0;
			}
		}

		virtual void Deallocate(void * p)
		{	
			AssertString("Not supported");
		}
	};

	class ChainedAllocator : public Allocator
	{
		ProfilerAllocationHeader* rootHeader;
	protected:

		enum {
			BlockAlign = 16
		};

		struct MemoryBlock {
			MemoryBlock*	next;
			uint8_t*		headPtr;
			std::size_t		blockSize;
		};

		ChainedAllocator(ChainedAllocator const&):Allocator(){rootHeader = GET_CURRENT_ALLOC_ROOT_HEADER();}
		ChainedAllocator& operator=(ChainedAllocator const &){ return *this; }
		
		std::size_t GetMemBlockSize(std::size_t aSize, std::size_t aAlign)
		{
			return sizeof(MemoryBlock) + aSize + aAlign - 1U;
		}

		std::size_t GetAllocateSize(std::size_t aSize, std::size_t aAlign)
		{
			return aSize + aAlign - 1U;
		}

		MemoryBlock* first;
		MemoryBlock* current;
		uint8_t*	 heapPtr;
		std::size_t  blockSize;
	public:
		ChainedAllocator(std::size_t aBlockSize):first(0),current(0),blockSize(aBlockSize)
		{
			rootHeader = GET_CURRENT_ALLOC_ROOT_HEADER();
		}

		~ChainedAllocator()
		{
			Reset();			
		}

		void Init()
		{
			if(first == 0)
			{
				size_t size = GetMemBlockSize(blockSize, BlockAlign);
				void *p = UNITY_MALLOC(MemLabelId(kMemAnimationId,rootHeader),  size);
				Profiler::GetProfiler()->RegisterAllocation(p, "ChainedAllocator", size);
				if(p)
				{
					current = first = new(p) MemoryBlock;
					current->next = 0;
					current->blockSize = blockSize;

					uint8_t* head = reinterpret_cast<uint8_t*>(p);
					heapPtr = current->headPtr = reinterpret_cast<uint8_t*>( AlignAddress( reinterpret_cast<std::size_t>(head + sizeof(MemoryBlock)), BlockAlign));				
				}
			}
		}

		void Reset()
		{
			MemoryBlock* c = first;
			while(c != 0)
			{
				void* p = reinterpret_cast<void*>(c);
				c = c->next;
				Profiler::GetProfiler()->UnregisterAllocation(p);
				UNITY_FREE(kMemAnimation, p);
			}
			current = first = 0;
			heapPtr = 0;
		}

		void Reserve(std::size_t size)
		{
			if(size > 0)
			{
				if(first == 0)
				{
					size_t size1 = GetMemBlockSize(size, BlockAlign);
					void *p = UNITY_MALLOC(MemLabelId(kMemAnimationId,rootHeader),  size1);
					Profiler::GetProfiler()->RegisterAllocation(p, "ChainedAllocator", size1);

					if(p)
					{
						current = first = new(p) MemoryBlock;
						current->next = 0;
						current->blockSize = size;

						uint8_t* head = reinterpret_cast<uint8_t*>(p);
						heapPtr = current->headPtr = reinterpret_cast<uint8_t*>( AlignAddress( reinterpret_cast<std::size_t>(head + sizeof(MemoryBlock)), BlockAlign));				
					}
				}
				else
				{
					size_t size1 = GetMemBlockSize(size, BlockAlign);
					void *p = UNITY_MALLOC(MemLabelId(kMemAnimationId,rootHeader),  size1);
					Profiler::GetProfiler()->RegisterAllocation(p, "ChainedAllocator", size1);
					if(p)
					{
						current->next = new(p) MemoryBlock;
						current = current->next;
						current->next = 0;
						current->blockSize = size > blockSize ? size : blockSize;

						uint8_t* head = reinterpret_cast<uint8_t*>(p);					
						heapPtr = current->headPtr = reinterpret_cast<uint8_t*>( AlignAddress( reinterpret_cast<std::size_t>(head + sizeof(MemoryBlock)), BlockAlign));
					}
				}
			}
		}

		// Limit case are
		//	no memory left in current block, allocate a new block
		//  no memory left in system, bail out
		virtual void* Allocate(std::size_t size, std::size_t align)
		{
			Init();

			std::size_t s = GetAllocateSize(size, align);

			// Not enough memory left in current block, allocate a new block
			if( heapPtr + s > current->headPtr + current->blockSize)
			{
				// If not enough memory left in system, bail out
				// If the requested size is bigger than the block size, allocate at least a block big enough for this request.
				size_t size1 = GetMemBlockSize(size > blockSize ? size : blockSize, BlockAlign);
				void *p = UNITY_MALLOC(MemLabelId(kMemAnimationId,rootHeader),  size1);
				Profiler::GetProfiler()->RegisterAllocation(p, "ChainedAllocator", size1);
				if(p)
				{
					current->next = new(p) MemoryBlock;
					current = current->next;
					current->next = 0;
					current->blockSize = size > blockSize ? size : blockSize;

					uint8_t* head = reinterpret_cast<uint8_t*>(p);					
					heapPtr = current->headPtr = reinterpret_cast<uint8_t*>( AlignAddress( reinterpret_cast<std::size_t>(head + sizeof(MemoryBlock)), BlockAlign));
				}
				else
					return 0;
			}

			uint8_t *p = reinterpret_cast<uint8_t *>(AlignAddress(reinterpret_cast<std::size_t>(heapPtr), align));				
			heapPtr = p+size;
			return reinterpret_cast<void *>(p);			
		}

		virtual void Deallocate(void * p)
		{
			assert(true);
		}
	};

}

}
