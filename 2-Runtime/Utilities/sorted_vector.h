#ifndef SORTED_VECTOR_H
#define SORTED_VECTOR_H
#include <vector>
#include <algorithm>

/// container optimization anschauen (compressed pair for valuecompare)


template<class T, class Compare, class Allocator>
class sorted_vector : private Compare
{
	public:
	
	typedef				std::vector<T, Allocator>			container;
	typedef typename	container::iterator					iterator;
	typedef typename	container::const_iterator			const_iterator;
	typedef typename	container::reverse_iterator			reverse_iterator;
	typedef typename	container::const_reverse_iterator	const_reverse_iterator;
	typedef typename	container::value_type				value_type;
	typedef typename	container::size_type				size_type;
	typedef typename	container::difference_type			difference_type;
	typedef 			Compare								value_compare;
	typedef typename	Allocator::reference				reference;
	typedef typename	Allocator::const_reference			const_reference;
	typedef				Allocator							allocator_type;

	value_compare const& get_compare () const { return *static_cast<value_compare const*> (this); }

		
	sorted_vector (const Compare& comp, const Allocator& a)
		: c (a), value_compare (comp)	 {}
	
	
	bool      			empty() const		{ return c.empty(); }
	size_type			size()  const		{ return c.size(); }
	const value_type&	front() const 		{ return c.front(); }
	const value_type&	back() const		{ return c.front(); }
		
	const_iterator begin ()const			{ return c.begin (); }
	const_iterator end ()const				{ return c.end (); }
	iterator begin ()						{ return c.begin (); }
	iterator end ()							{ return c.end (); }
	const_reverse_iterator rbegin ()const	{ return c.rbegin (); }
	const_reverse_iterator rend ()const		{ return c.rend (); }
	reverse_iterator rbegin ()				{ return c.rbegin (); }
	reverse_iterator rend ()				{ return c.rend (); }
	
	value_compare value_comp() const		{ return get_compare ();}
/*
	template <class InputIterator>
	void insert_one (InputIterator first, InputIterator last)
	{
		c.reserve (size () + std::distance (first, last));
		for (;first != last;first++)
			insert_one (*i);
	}
	
	template<typename KeyT, typename MappedT>
	MappedT& find_or_insert (const KeyT& k)
	{
		iterator i = lower_bound (k);
		if (i == end () || get_compare () (k, *i))
			return c.insert (i, std::make_pair<KeyT, MappedT> (k, MappedT ()))->second;
		else
			return i->second;
	}
*/
	
	template<typename KeyT, typename MappedT>
	void find_or_insert (MappedT*& mappedT, const KeyT& k)
	{
		iterator i = lower_bound (k);
		if (i == end () || get_compare () (k, *i))
			#if _MSC_VER >= 1700	// Microsoft Visual Studio 2012 workaround
			mappedT =  &c.insert (i, std::make_pair<KeyT, MappedT> (static_cast<KeyT&&>(const_cast<KeyT&>(k)), MappedT ()))->second;
			#else
			mappedT =  &c.insert (i, std::make_pair<KeyT, MappedT> (k, MappedT ()))->second;
			#endif
		else
			mappedT = &i->second;
	}

	std::pair<iterator, bool> insert_one (const value_type& x)
	{
		iterator i = lower_bound (x);
		// is not included in container
		if (i == end () || get_compare () (x, *i))
			return std::make_pair (c.insert (i, x), true);
		else
			return std::make_pair (i, false);
	
	}

	void verify_duplicates_and_sorted () const
	{
		#if DEBUGMODE
		// Check that there are no duplicates in the set
		if (empty())
			return;

		const_iterator previous = c.begin();
		const_iterator i = c.begin();
		i++;
		for (; i != c.end();++i)
		{
			Assert(get_compare ()(*previous, *i));
			previous = i;
		}
		#endif
	}
	
	
	template<class CompareType>
	size_type erase_one (const CompareType& x)
	{
		iterator i = lower_bound (x);
		if (i == end () || get_compare () (x, *i))
			return 0;
		else
		{	
			c.erase (i);
			return 1;
		}
	}
	
	void      			erase(iterator position)				{ c.erase (position);}
	void      			erase(iterator first, iterator last)	{ c.erase (first, last); }
	void 				swap(sorted_vector& x)					{ c.swap (x.c); }
	
	void				clear ()								{ c.clear (); }
	
	template<class T2>
	size_type			count_one (const T2& x)const			
	{	
		const_iterator i = lower_bound (x);
		if (i == end () || get_compare () (x, *i))
			return 0;
		else
			return 1;
	}							

	template<class T2>
	iterator				find (const T2& x)						
	{
		iterator i = lower_bound (x);
		if (i == end () || get_compare () (x, *i))
			return end ();
		else
			return i;
	}

	template<class T2>
	const_iterator				find (const T2& x) const						
	{
		const_iterator i = lower_bound (x);
		if (i == end () || get_compare () (x, *i))
			return end ();
		else
			return i;
	}
		
	template<class T2>
	iterator lower_bound (const T2& x)
	{
		return std::lower_bound (c.begin (), c.end (), x, get_compare ());
	}

	template<class T2>
	const_iterator lower_bound (const T2& x) const
	{
		return std::lower_bound (c.begin (), c.end (), x, get_compare ());
	}

	template<class T2>
	iterator upper_bound (const T2& x)
	{
		return std::lower_bound (c.begin (), c.end (), x, get_compare ());
	}

	template<class T2>
	const_iterator upper_bound (const T2& x) const
	{
		return std::lower_bound (c.begin (), c.end (), x, get_compare ());
	}

	template<class T2>
	std::pair<iterator, iterator> equal_range (const T2& x)
	{
		return std::equal_range (c.begin (), c.end (), x, get_compare ());
	}	

	template<class T2>
	std::pair<const_iterator, const_iterator> equal_range (const T2& x) const
	{
		return std::equal_range (c.begin (), c.end (), x, get_compare ());
	}	
	
	void reserve (size_type n)	{ c.reserve (n); }

	value_type& operator [] (int n) { return c[n]; }
	const value_type& operator [] (int n) const { return c[n]; }

	public:

	container		c;
};

template<class T, class Compare, class Allocator>
class unsorted_vector : private Compare
{
	public:
	
	typedef         	std::vector<T, Allocator>			container;
	typedef typename	container::iterator					iterator;
	typedef typename	container::const_iterator			const_iterator;
	typedef typename	container::reverse_iterator			reverse_iterator;
	typedef typename	container::const_reverse_iterator	const_reverse_iterator;

	typedef typename	container::value_type				value_type;
	typedef typename	container::size_type				size_type;
	typedef typename	container::difference_type			difference_type;
	typedef 			Compare								value_compare;					
	typedef typename	Allocator::reference				reference;
	typedef typename	Allocator::const_reference			const_reference;
	typedef				Allocator							allocator_type;

	value_compare const& get_compare () const { return *static_cast<value_compare const*> (this); }

	unsorted_vector (const Compare& comp, const Allocator& a)
		: c (a), value_compare (comp)  {}
	
	
	bool      			empty() const		{ return c.empty(); }
	size_type			size()  const		{ return c.size(); }
	const value_type&	front() const 		{ return c.front(); }
	const value_type&	back() const		{ return c.front(); }
		
	const_iterator begin ()const			{ return c.begin (); }
	const_iterator end ()const				{ return c.end (); }
	iterator begin ()						{ return c.begin (); }
	iterator end ()							{ return c.end (); }
	const_reverse_iterator rbegin ()const	{ return c.rbegin (); }
	const_reverse_iterator rend ()const		{ return c.rend (); }
	reverse_iterator rbegin ()				{ return c.rbegin (); }
	reverse_iterator rend ()				{ return c.rend (); }
	
	value_compare value_comp() const		{ return get_compare ();}
/*
	template <class InputIterator>
	void insert_one (InputIterator first, InputIterator last)
	{
		c.reserve (size () + std::distance (first, last));
		for (;first != last;first++)
			insert_one (*i);
	}
*/
	template<typename KeyT, typename MappedT>
	void find_or_insert (MappedT*& mappedT, const KeyT& k)
	{
		iterator i = find (k);
		if (i == end ())
		{
			c.push_back (std::make_pair<KeyT, MappedT> (k, MappedT ()));
			mappedT = &(c.end () - 1)->second;
		}
		else
			mappedT = &i->second;
	}
	/*
	template<class Key, class Value>
	Value& find_or_insert (const Key& k)
	{
		iterator i = find (k);
		if (i == end ())
		{
			c.push_back (std::make_pair<Key, Value> (k, Value ()));
			return (c.end () - 1)->second;
		}
		else
			return i->second;
	}
	*/
	std::pair<iterator, bool> insert_one (const value_type& x)
	{
		iterator i = find (x);
		// is not included in container
		if (i == end ())
		{
			c.push_back (x);
			return std::make_pair (c.end () - 1, true);
		}
		else
			return std::make_pair (i, false);
	}
	
	template<class CompareType>
	size_type erase_one (const CompareType& x)
	{
		iterator i = find (x);
		if (i == end ())
			return 0;
		else
		{	
			erase (i);
			return 1;
		}
	}
	
	void      			erase(iterator position)	{ *position = c.back (); c.pop_back (); }
	void 					swap(unsorted_vector& x)	{ c.swap (x.c); }
	
	void					clear ()							{ c.clear (); }

	template<class T2>
	size_type			count_one (const T2& x)const			
	{	
		const_iterator i = find (x);
		return i != end ();
	}							

	template<class T2>
	iterator				find (const T2& x)
	{
		iterator b = c.begin ();
		iterator e = c.end ();
		for (;b != e;++b)
		{
			if (get_compare () (*b, x))
				return b;
		}
		return e;	
	}

	template<class T2>
	const_iterator			find (const T2& x) const
	{
	{
		const_iterator b = c.begin ();
		const_iterator e = c.end ();
		for (;b != e;++b)
		{
			if (get_compare () (*b, x))
				return b;
		}
		return e;	
	}
	
	}		
	void 					reserve (size_type n)				{ c.reserve (n); }
	value_type& operator [] (int n) { return c[n]; }
	const value_type& operator [] (int n) const { return c[n]; }

	public:

	container		c;
};

#endif
