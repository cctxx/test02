#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "Runtime/Utilities/File.h"
#include <string.h>
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Allocator/FixedSizeAllocator.h"


#if !GAMERELEASE
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/GUID.h"
UnityGUID StringToGUID (const std::string& pathName);
static const char* kTempFolder = "Temp";
#endif


SUITE (UtilityTests)
{


TEST (TestMemoryPool)
{
	const int kBubble = 256;
	const int kBubbleSize = 64;

	// memory pool with 128 elements per bubble
	MemoryPool pool (true, "TestPool", kBubbleSize, kBubble * kBubbleSize);

	std::vector<void*> ptrs;

	// allocate 128, should be one bubble and 128 objects
	for (int i = 0; i < kBubble; ++i)
	{
		ptrs.push_back (pool.Allocate ());
	}
	CHECK_EQUAL (kBubble, pool.GetAllocCount ());
	CHECK_EQUAL (1, pool.GetBubbleCount ());

	// Allocate 1 more, should be 2 bubbles and kBubble+1 objects
	ptrs.push_back (pool.Allocate ());
	CHECK_EQUAL (kBubble + 1, pool.GetAllocCount ());
	CHECK_EQUAL (2, pool.GetBubbleCount ());

	// deallocate first bubble
	void* last = ptrs[kBubble];
	for (int i = 0; i < kBubble; ++i)
	{
		pool.Deallocate (ptrs[i]);
	}
	ptrs.clear ();
	ptrs.push_back (last);
	CHECK_EQUAL (1, pool.GetAllocCount());
	CHECK_EQUAL (2, pool.GetBubbleCount());

	// now we have two bubbles: 1st totally free, 2nd one with 1 allocation
	// allocate 255 more objects. Should all fit into existing bubbles!
	for (int i = 0; i < kBubble * 2 - 1; ++i)
	{
		ptrs.push_back (pool.Allocate ());
	}
	CHECK_EQUAL (kBubble * 2, pool.GetAllocCount ());
	CHECK_EQUAL (2, pool.GetBubbleCount ());

	// Allocate one more. Should cause additional bubble
	ptrs.push_back (pool.Allocate ());
	CHECK_EQUAL (kBubble * 2 + 1, pool.GetAllocCount ());
	CHECK_EQUAL (3, pool.GetBubbleCount ());

	// Deallocate all.
	for (int i = 0; i < ptrs.size (); ++i)
	{
		pool.Deallocate (ptrs[i]);
	}
	ptrs.clear ();
	CHECK_EQUAL (0, pool.GetAllocCount ());
	CHECK_EQUAL (3, pool.GetBubbleCount ());

	// now we have three totally free bubbles
	// allocate 3*128 objects. Should fit into existing bubbles!
	for (int i = 0; i < kBubble * 3; ++i)
	{
		ptrs.push_back (pool.Allocate ());
	}
	CHECK_EQUAL (kBubble * 3, pool.GetAllocCount ());
	CHECK_EQUAL (3, pool.GetBubbleCount ());

	// deallocate all from last bubble
	for (int i = kBubble * 2; i < kBubble * 3; ++i)
	{
		pool.Deallocate (ptrs[i]);
	}
	ptrs.resize (kBubble * 2);
	CHECK_EQUAL (kBubble * 2, pool.GetAllocCount ());
	CHECK_EQUAL (3, pool.GetBubbleCount ());

	// allocate one more
	ptrs.push_back (pool.Allocate ());
	CHECK_EQUAL (kBubble * 2 + 1, pool.GetAllocCount ());
	CHECK_EQUAL (3, pool.GetBubbleCount ());

	pool.DeallocateAll ();
	ptrs.clear ();
	CHECK_EQUAL (0, pool.GetAllocCount ());
	CHECK_EQUAL (0, pool.GetBubbleCount ());

	// Test pre-allocation: preallocate 1.5 worth of bubbles
	pool.PreallocateMemory (kBubble * kBubbleSize * 3/2);
	// should result in 2 bubbles
	CHECK_EQUAL (2, pool.GetBubbleCount ());
	for (int i = 0; i < kBubble * 2; ++i)
	{
		ptrs.push_back (pool.Allocate ());
	}
	CHECK_EQUAL (kBubble * 2, pool.GetAllocCount ());
	CHECK_EQUAL (2, pool.GetBubbleCount ());

	// Allocate one more, should create additional bubble
	ptrs.push_back (pool.Allocate ());
	CHECK_EQUAL (kBubble * 2 + 1, pool.GetAllocCount ());
	CHECK_EQUAL (3, pool.GetBubbleCount ());
	pool.DeallocateAll ();
}


#if !GAMERELEASE
TEST (TestPathExist)
{
	using namespace std;

	CreateDirectory (kTempFolder);

	string filePath = AppendPathName (kTempFolder, "TestIsFileCreated");
	DeleteFileOrDirectory (filePath);

	CHECK (!IsPathCreated (filePath));
	CHECK (!IsDirectoryCreated (filePath));
	CHECK (!IsFileCreated (filePath));

	CreateFile (filePath);

	CHECK (IsPathCreated (kTempFolder));
	CHECK (IsDirectoryCreated (kTempFolder));
	CHECK (!IsFileCreated (kTempFolder));

	CHECK (IsPathCreated (filePath));
	CHECK (!IsDirectoryCreated (filePath));
	CHECK (IsFileCreated (filePath));
}
#endif


TEST (DynamicArray)
{
	// no allocation for empty array
	dynamic_array<int> array;
	CHECK_EQUAL (0, array.capacity ());

	// push_back allocates
	int j = 1;
	array.push_back (j);
	CHECK_EQUAL (1, array.size ());
	CHECK (array.capacity () > 0);

	// push_back(void)
	int& i = array.push_back ();
	i = 666;
	CHECK_EQUAL(666, array.back ());

	// clear frees memory?
	array.clear ();
	CHECK_EQUAL (0, array.size ());
	CHECK_EQUAL (0, array.capacity ());

	// 3 item list
	j = 6;
	array.push_back (j);
	j = 7;
	array.push_back (j);
	j = 8;
	array.push_back (j);

	CHECK_EQUAL (3, array.size ());

	// swapping
	dynamic_array<int> ().swap (array);
	CHECK_EQUAL (0, array.capacity ());
	CHECK_EQUAL (0, array.size ());

	// reserve
	array.reserve (1024);
	CHECK_EQUAL (1024, array.capacity ());
	CHECK_EQUAL (0, array.size ());

	// copy assignment
	dynamic_array<int> array1;
	j = 888;
	array1.push_back (j);

	array = array1;

	CHECK_EQUAL (1, array.size ());
	CHECK_EQUAL (888, array.back ());
	CHECK_EQUAL (1, array1.size ());
	CHECK_EQUAL (888, array1.back ());
}

	struct Stuff
	{
		int value;
		int identifier;

		bool operator < (const Stuff& rhs) const
		{
			return value < rhs.value;
		}

		Stuff (int a, int b) { value = a; identifier = b; }
	};


TEST (Test_vector_set_assign_clear_duplicates)
{
	// Make sure that duplicates are removed,
	// but also that only the first same instance is maintained, the following ones are killed
	Stuff input[] = { Stuff (10, 0), Stuff (11, 1), Stuff (3, 2), Stuff (3, 3), Stuff (4, 4), Stuff (10, 5) };
	Stuff output[] = { Stuff (3, 2), Stuff (4, 4), Stuff (10, 0), Stuff (11, 1) };

	vector_set<Stuff> test_set;
	test_set.assign_clear_duplicates(input, input + ARRAY_SIZE(input));

	CHECK_EQUAL(test_set.size(), ARRAY_SIZE(output));
	for (int i=0;i<ARRAY_SIZE(output);i++)
	{
		CHECK_EQUAL(output[i].value, test_set[i].value);
		CHECK_EQUAL(output[i].identifier, test_set[i].identifier);
	}
}

class TestNode : public ListElement
{
};

TEST (TestList)
{
	typedef List<TestNode> ListType;

	struct
	{
		void operator () (ListType& list, TestNode* nodes[], int count)
		{
			CHECK_EQUAL (count, list.size_slow ());
			int c = 0;
			for (ListType::iterator i = list.begin (); i != list.end (); ++i)
			{
				CHECK (nodes[c] == &*i);
				++c;
			}

			CHECK_EQUAL (count, c);
		}
	} CheckNodes;

	ListType list, emptyList, emptyList2;

	CHECK_EQUAL (0, emptyList.size_slow ());
	emptyList.clear ();
	CHECK_EQUAL (0, emptyList.size_slow ());

	TestNode* nodes[] =
	{
		new TestNode (),
		new TestNode (),
		new TestNode (),
		new TestNode (),
		new TestNode (),
		new TestNode ()
	};

	emptyList.swap (emptyList2);
	CHECK_EQUAL (0, emptyList.size_slow ());
	CHECK_EQUAL (0, emptyList2.size_slow ());

	// insertion and pushback
	list.push_back (*nodes[1]);
	list.insert (nodes[1], *nodes[0]);
	list.push_back (*nodes[2]);
	list.push_back (*nodes[3]);
	list.push_back (*nodes[5]);
	list.insert (nodes[5], *nodes[4]);
	CheckNodes (list, nodes, 6);
	// insert before self
	list.insert (list.begin(), *list.begin ());
	CheckNodes (list, nodes, 6);

	list.append (emptyList);
	CHECK_EQUAL (0, emptyList.size_slow ());
	CheckNodes (list, nodes, 6);

	// append remove into something empty
	emptyList.append (list);
	CHECK_EQUAL (0, list.size_slow ());
	CheckNodes (emptyList, nodes, 6);

	// invert operation by doing a swap
	emptyList.swap (list);
	CHECK_EQUAL (0, emptyList.size_slow ());
	CheckNodes (list, nodes, 6);

	// Create another list to test copying
	TestNode* nodes2[] =
	{
		new TestNode (),
		new TestNode (),
		new TestNode ()
	};
	ListType list2;
	list2.push_back (*nodes2[1]);
	list2.push_front (*nodes2[0]);
	list2.push_back (*nodes2[2]);
	CheckNodes (list2, nodes2, 3);

	// swap back and forth
	list2.swap (list);
	CheckNodes (list, nodes2, 3);
	CheckNodes (list2, nodes, 6);
	list.swap (list2);
	CheckNodes (list, nodes, 6);
	CheckNodes (list2, nodes2, 3);

	list.append (list2); // insert before self
	int c = 0;
	for (ListType::iterator i = list.begin (); i != list.end (); ++i)
	{
		if (c >= 6)
			CHECK (nodes2[c - 6] == &*i);
		else
			CHECK (nodes[c] == &*i);
		++c;
	}
	CHECK_EQUAL (9, list.size_slow ());
	CHECK_EQUAL (0, list2.size_slow ());
	CHECK_EQUAL (9, c);

	emptyList.append (emptyList2);
	CHECK_EQUAL (0, emptyList2.size_slow ());
	CHECK_EQUAL (0, emptyList.size_slow ());
}

TEST (DynamicBitSet)
{
	dynamic_bitset set;
	UInt32 block = 1 << 0 | 1 << 3 | 1 << 5;
	set.resize (6);
	from_block_range (&block, &block + 1, set);

	CHECK_EQUAL (true, set.test (0));
	CHECK_EQUAL (false, set.test (1));
	CHECK_EQUAL (false, set.test (2));
	CHECK_EQUAL (true, set.test (3));
	CHECK_EQUAL (false, set.test (4));
	CHECK_EQUAL (true, set.test (5));

	to_block_range (set, &block);
	bool res;

	res = block & (1 << 0);
	CHECK_EQUAL (true, res);
	res = block & (1 << 1);
	CHECK_EQUAL (false, res);
	res = block & (1 << 2);
	CHECK_EQUAL (false, res);
	res = block & (1 << 3);
	CHECK_EQUAL (true, res);
	res = block & (1 << 4);
	CHECK_EQUAL (false, res);
	res = block & (1 << 5);
	CHECK_EQUAL (true, res);
}


TEST (DynamicArrayMisc)
{
	// no allocation for empty array
	dynamic_array<int> array;
	CHECK_EQUAL (0, array.capacity ());
	CHECK (array.owns_data ());
	CHECK (array.begin () == array.end ());
	CHECK (array.empty ());

	// push_back allocates
	int j = 1;
	array.push_back (j);
	CHECK_EQUAL (1, array.size ());
	CHECK (array.capacity () > 0);

	// push_back(void)
	int& i = array.push_back ();
	i = 666;
	CHECK_EQUAL(666, array.back ());

	// clear frees memory?
	array.clear ();
	CHECK_EQUAL (0, array.size ());
	CHECK_EQUAL (0, array.capacity ());

	// 3 item list
	array.push_back (6);
	array.push_back (7);
	array.push_back (8);

	CHECK_EQUAL (3, array.size ());

	// swapping
	dynamic_array<int> ().swap (array);
	CHECK_EQUAL (0, array.capacity ());
	CHECK_EQUAL (0, array.size ());

	// reserve
	array.reserve (1024);
	CHECK_EQUAL (1024, array.capacity ());
	CHECK_EQUAL (0, array.size ());

	// copy assignment
	dynamic_array<int> array1;
	j = 888;
	array1.push_back (j);

	array = array1;

	CHECK_EQUAL (1, array.size ());
	CHECK_EQUAL (888, array.back ());
	CHECK_EQUAL (1, array1.size ());
	CHECK_EQUAL (888, array1.back ());
}


TEST (DynamicArrayErase)
{
	dynamic_array<int> vs;
	vs.resize_uninitialized(5);

	vs[0] = 0;
	vs[1] = 1;
	vs[2] = 2;
	vs[3] = 3;
	vs[4] = 4;

	vs.erase(vs.begin() + 1, vs.begin() + 4);
	CHECK_EQUAL (2, vs.size());
	CHECK_EQUAL (5, vs.capacity());
	CHECK_EQUAL (0, vs[0]);
	CHECK_EQUAL (4, vs[1]);
}

static void VerifyConsecutiveIntArray (dynamic_array<int>& vs, int size, int capacity)
{
	CHECK_EQUAL (capacity, vs.capacity());
	CHECK_EQUAL (size, vs.size());
	for (int i=0;i<vs.size();i++)
		CHECK_EQUAL (i, vs[i]);
}

TEST (DynamicArrayInsertOnEmpty)
{
	dynamic_array<int> vs;
	int vals[] = { 0, 1 };

	vs.insert(vs.begin(), vals, vals + ARRAY_SIZE(vals));

	VerifyConsecutiveIntArray(vs, 2, 2);
}


TEST (DynamicArrayInsert)
{
	dynamic_array<int> vs;
	vs.resize_uninitialized(5);

	vs[0] = 0;
	vs[1] = 1;
	vs[2] = 4;
	vs[3] = 5;
	vs[4] = 6;

	int vals[] = { 2, 3 };

	// inser two values
	vs.insert(vs.begin() + 2, vals, vals + ARRAY_SIZE(vals));
	VerifyConsecutiveIntArray(vs, 7, 7);

	// empty insert
	vs.insert(vs.begin() + 2, vals, vals);

	VerifyConsecutiveIntArray(vs, 7, 7);
}

TEST (DynamicArrayResize)
{
	dynamic_array<int> vs;
	vs.resize_initialized(3, 2);
	CHECK_EQUAL (3, vs.capacity());
	CHECK_EQUAL (3, vs.size());
	CHECK_EQUAL (2, vs[0]);
	CHECK_EQUAL (2, vs[1]);
	CHECK_EQUAL (2, vs[2]);

	vs.resize_initialized(6, 3);
	CHECK_EQUAL (6, vs.capacity());
	CHECK_EQUAL (6, vs.size());
	CHECK_EQUAL (2, vs[0]);
	CHECK_EQUAL (2, vs[1]);
	CHECK_EQUAL (2, vs[2]);
	CHECK_EQUAL (3, vs[3]);
	CHECK_EQUAL (3, vs[4]);
	CHECK_EQUAL (3, vs[5]);

	vs.resize_initialized(5, 3);
	CHECK_EQUAL (6, vs.capacity());
	CHECK_EQUAL (5, vs.size());
	CHECK_EQUAL (2, vs[0]);
	CHECK_EQUAL (2, vs[1]);
	CHECK_EQUAL (2, vs[2]);
	CHECK_EQUAL (3, vs[3]);
	CHECK_EQUAL (3, vs[4]);

	vs.resize_initialized(2, 3);
	CHECK_EQUAL (6, vs.capacity());
	CHECK_EQUAL (2, vs.size());
	CHECK_EQUAL (2, vs[0]);
	CHECK_EQUAL (2, vs[1]);
}


TEST(FixedSizeAllocator)
{
	typedef FixedSizeAllocator<sizeof(int)> TestAllocator;

	TestAllocator testalloc = TestAllocator(MemLabelId(kMemDefaultId, NULL));

	CHECK(testalloc.capacity() == 0);
	CHECK(testalloc.total_free() == 0);
	CHECK(testalloc.total_allocated() == 0);

	int* ptr1 = (int*)testalloc.alloc();
	*ptr1 = 1;

	CHECK(testalloc.capacity() == 255*sizeof(int));
	CHECK(testalloc.total_free() == 254*sizeof(int));
	CHECK(testalloc.total_allocated() == sizeof(int));

	int* ptr2 = (int*)testalloc.alloc();
	*ptr2 = 2;

	CHECK(testalloc.capacity() == 255*sizeof(int));
	CHECK(testalloc.total_free() == 253*sizeof(int));
	CHECK(testalloc.total_allocated() == 2*sizeof(int));
	CHECK(*ptr1==1);
	CHECK(ptr1 + 1 == ptr2);

	testalloc.reset();

	CHECK(testalloc.capacity() == 255*sizeof(int));
	CHECK(testalloc.total_free() == 255*sizeof(int));
	CHECK(testalloc.total_allocated() == 0);

	testalloc.free_memory();

	CHECK(testalloc.capacity() == 0);
	CHECK(testalloc.total_free() == 0);
	CHECK(testalloc.total_allocated() == 0);
}



TEST(StringFormatTest)
{
	CHECK_EQUAL ("Hello world it works", Format ("Hello %s it %s", "world", "works"));
}


#if !GAMERELEASE
TEST(UnityGUIDTest)
{
	UnityGUID identifier[5];
	identifier[0].Init ();
	identifier[1].Init ();
	identifier[2].Init ();
	identifier[3].Init ();
	identifier[4].Init ();

	CHECK (identifier[0] != identifier[1]);
	CHECK (identifier[1] != identifier[2]);
	CHECK (identifier[2] != identifier[3]);
	CHECK (identifier[3] != identifier[4]);
	identifier[0] = identifier[1];
	CHECK (identifier[0] == identifier[1]);
}
#endif


TEST (TestUtility)
{
	using namespace std;

	// Make sure that our stl implementation has consistent clear behaviour.
	// If it doesn't we should probably stop using clear.
	//
	// If this test fails, it means that std::vector clear deallocates memory.
	// Some optimized code this to not be the case!

	vector<int> test;
	test.resize (10);
	test.clear ();
	CHECK (test.capacity () != 0);
}

}
#endif
