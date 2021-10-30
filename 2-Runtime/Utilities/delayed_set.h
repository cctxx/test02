#ifndef DELAYED_SET_H
#define DELAYED_SET_H

#include <set>
#include <vector>
#include "MemoryPool.h"

template <class T, class SetType = std::set<T, std::less<T> , memory_pool<T> > >
class delayed_set : public SetType {
	typedef typename std::vector<std::pair<bool, T> > delay_container;
	delay_container m_Delayed;

	public:
	
	bool is_inserted (const T& object)
	{
		typename delay_container::reverse_iterator i;
		for (i = m_Delayed.rbegin ();i != m_Delayed.rend ();i++)
		{
			if (i->second == object)
				return i->first;
		}
		return this->find (object) != this->end ();
	}
	
	void remove_delayed (const T& obj) {
		m_Delayed.push_back (std::pair<bool, T> (false, obj));
	}
	void add_delayed (const T& obj) {
		m_Delayed.push_back (std::pair<bool, T> (true, obj));
	}
	int apply_delayed_size () const { return m_Delayed.size (); }
	void apply_delayed () {
		typename delay_container::iterator iter;
		for (iter = m_Delayed.begin(); iter != m_Delayed.end(); iter++) {
			if (iter->first)
				SetType::insert (iter->second);
			else
				SetType::erase (iter->second);
		}
		m_Delayed.clear ();
	}
};

#endif
