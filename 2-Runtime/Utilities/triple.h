#pragma once

// TEMPLATE STRUCT triple
template<class T>
struct triple
{
	// store a triple of values of the same type

	triple()
		: first(T()), second(T()), third(T())
	{	// construct from defaults
	}

	triple(const T& _Val1, const T& _Val2, const T& _Val3)
		: first(_Val1), second(_Val2), third(_Val3)
	{	// construct from specified values
	}

	template<class otherT>
	triple(const triple<otherT>& _Right)
		: first(_Right.first), second(_Right.second), third(_Right.third)
	{	// construct from a compatible triple
	}

	T first;	// the first stored value
	T second;	// the second stored value
	T third;	// the third stored value
};

template<class T>
inline bool operator==(const triple<T>& _Left, const triple<T>& _Right)
{	// test for triple equality
	return (_Left.first == _Right.first && _Left.second == _Right.second && _Left.third == _Right.third);
}

template<class T>
inline bool operator!=(const triple<T>& _Left, const triple<T>& _Right)
{	// test for triple inequality
	return (!(_Left == _Right));
}

template<class T>
inline triple<T> make_triple(T _Val1, T _Val2, T _Val3)
{	// return a triple composed from arguments
	return (triple<T>(_Val1, _Val2, _Val3));
}