#ifndef ALGORITHM_UTILITY_H
#define ALGORITHM_UTILITY_H

#include <algorithm>
#include <functional>
#include "LogAssert.h"

template<class T, class Func>
void repeat (T& type, int count, Func func)
{
	int i;
	for (i=0;i<count;i++)
		func (type);
}

template<class C, class Func>
Func for_each (C& c, Func f)
{
	return for_each (c.begin (), c.end (), f);
}

template<class C, class Func>
void erase_if (C& c, Func f)
{
	c.erase (remove_if (c.begin (), c.end (), f), c.end ());
}

template<class C, class T>
void erase (C& c, const T& t)
{
	c.erase (remove (c.begin (), c.end (), t), c.end ());
}

template<class C, class T>
typename C::iterator find (C& c, const T& value)
{
	return find (c.begin (), c.end (), value);
}

template<class C, class Pred>
typename C::iterator find_if (C& c, Pred p)
{
	return find_if (c.begin (), c.end (), p);
}

template <class T, class U>
struct EqualTo
	: std::binary_function<T, U, bool>
{
	bool operator()(const T& x, const U& y) const { return static_cast<bool>(x == y); }
};

// Returns the iterator to the last element
template<class Container>
typename Container::iterator last_iterator (Container& container)
{
	AssertIf (container.begin () == container.end ());
	typename Container::iterator i = container.end ();
	i--;
	return i;
}

template<class Container>
typename Container::const_iterator last_iterator (const Container& container)
{
	AssertIf (container.begin () == container.end ());
	typename Container::const_iterator i = container.end ();
	i--;
	return i;
}


// Efficient "add or update" for STL maps.
// For more details see item 24 on Effective STL.
// Basically it avoids constructing default value only to
// assign it later.
template<typename MAP, typename K, typename V>
bool add_or_update( MAP& m, const K& key, const V& val )
{
	typename MAP::iterator lb = m.lower_bound( key );
	if( lb != m.end() && !m.key_comp()( key, lb->first ) )
	{
		// lb points to a pair with the given key, update pair's value
		lb->second = val;
		return false;
	}
	else
	{
		// no key exists, insert new pair
		m.insert( lb, std::make_pair(key,val) );
		return true;
	}
}

template<class ForwardIterator>
bool is_sorted (ForwardIterator begin, ForwardIterator end)
{
	for (ForwardIterator next = begin; begin != end && ++next != end; ++begin)
		if (*next < *begin)
			return false;
	
	return true;
}

template<class ForwardIterator, class Predicate>
bool is_sorted (ForwardIterator begin, ForwardIterator end, Predicate pred)
{
	for (ForwardIterator next = begin; begin != end && ++next != end; ++begin)
		if (pred(*next, *begin))
			return false;
	
	return true;
}

#endif
