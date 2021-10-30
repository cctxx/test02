#pragma once


#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"

#include "Runtime/Serialize/SerializeTraits.h"

#include <string.h>

namespace mecanim
{
	template<int CAPACITY = 128> class basic_string
	{
	protected:		
		char eos(){return '\0';}
		char mStr[CAPACITY];
		void terminatestr(){mStr[CAPACITY-1] = eos();}
	public:

		typedef char			value_type;
		typedef char*			iterator;
		typedef char const *	const_iterator;

		static const int npos = CAPACITY;
		static const int capacity = CAPACITY;

		basic_string(){ mStr[0] = eos(); }
		basic_string( const basic_string& str ){ strncpy(mStr, str.mStr, max_size()); terminatestr(); }
		explicit basic_string( const char * s, std::size_t n = CAPACITY){ strncpy(mStr, s, n); terminatestr(); }

		char const* c_str()const{return mStr; }

		const char& operator[] ( std::size_t pos ) const{ return mStr[pos]; }
		char& operator[] ( std::size_t pos ){ return mStr[pos]; }

		iterator begin(){return &mStr[0];}
		const_iterator begin() const{return &mStr[0];}

		iterator end(){return &mStr[size()];}
		const_iterator end() const{return &mStr[size()];}

		basic_string& operator= ( const basic_string& str ){ strncpy(mStr, str.mStr, max_size()); terminatestr(); return *this; }
		basic_string& operator= ( const char* s ){ strncpy(mStr, s, max_size()); terminatestr(); return *this; }

		basic_string& operator+= ( const basic_string& str ){ strncat(mStr, str.mStr, max_size()-size() ); terminatestr(); return *this; }
		basic_string& operator+= ( const char* s ){ strncat(mStr, s, max_size()-size()); terminatestr(); return *this; }

		int compare ( const basic_string& str ) const { return strcmp(mStr, str.mStr); }
		int compare ( const char* s ) const { return strcmp(mStr, s); }
		int compare ( size_t pos1, size_t n1, const basic_string& str ) const { return strncmp(mStr+pos1, str.mStr, n1); }
		int compare ( size_t pos1, size_t n1, const char* s) const{ return strncmp(mStr+pos1, s, n1); }

		basic_string substr ( size_t pos = 0, size_t n = npos ) const{ return basic_string( &mStr[pos], n-pos ); }

		size_t find ( const basic_string& str, size_t pos = 0 ) const 
		{ 	
			return find(str.mStr, pos);
		}		
		size_t find ( const char* s, size_t pos = 0 ) const
		{ 
			size_t i = npos;
			char const* p = strstr(&mStr[pos], s);
			if(p)
			{
				i = reinterpret_cast<size_t>(p) - reinterpret_cast<size_t>(mStr);
			}
			return i;
		}
		
		void resize(size_t size)
		{
			for(int i = 0; i < size && i < max_size();++i)
				mStr[i] = ' ';
			mStr[ size < max_size()-1 ? size : max_size()-1 ] = eos();
		}

		size_t size() const { return strlen(mStr); }
		bool empty() const { return size() == 0; }
		void clear() { mStr[0] = eos(); }
		static size_t max_size(){ return CAPACITY; }
	};

	template<int CAPACITY1, int CAPACITY2> bool operator==(basic_string<CAPACITY1> const& l, basic_string<CAPACITY2> const& r){ return l.compare(r.c_str()) == 0; }
	template<int CAPACITY> bool operator==(basic_string<CAPACITY> const& l, const char* r){ return l.compare(r) == 0; }
	
	template<int CAPACITY1, int CAPACITY2> bool operator!=(basic_string<CAPACITY1> const& l, basic_string<CAPACITY2> const& r){ return l.compare(r.c_str()) != 0; }
	template<int CAPACITY> bool operator!=(basic_string<CAPACITY> const& l, const char* r){ return l.compare(r) != 0; }

	template<int CAPACITY1, int CAPACITY2> bool operator<(basic_string<CAPACITY1> const& l, basic_string<CAPACITY2> const& r){ return l.compare(r.c_str()) < 0; }
	template<int CAPACITY> bool operator<(basic_string<CAPACITY> const& l, const char* r){ return l.compare(r) < 0; }
	

	typedef basic_string<> String;
}

