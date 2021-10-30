#ifndef CSTRINGHASH_H
#define CSTRINGHASH_H
#include <functional>

struct hash_cstring : std::unary_function<const char*, std::size_t>
{
	unsigned operator ()(const char* key) const
	{
		unsigned h = 0;
		const unsigned sr = 8 * sizeof (unsigned) - 8;
		const unsigned mask = 0xF << (sr + 4);
		while (*key != '\0')
		{
			h = (h << 4) + *key;
			std::size_t g = h & mask;
			h ^= g | (g >> sr);
			key++;
		}
		return h;
	}
};

struct equal_cstring : std::binary_function<char*, char*, std::size_t>
{
	bool operator () (char* lhs, char* rhs) const
	{
		while (*lhs != '\0')
		{
			if (*lhs != *rhs)
				return false;
			lhs++; rhs++;
		}
		return *lhs == *rhs;
	}
};

struct smaller_cstring : std::binary_function<const char*, const char*, std::size_t>
{
	bool operator () (const char* lhs, const char* rhs) const { return strcmp (lhs, rhs) < 0; }
};

struct compare_cstring : public std::binary_function<const char*, const char*, bool>
{
	bool operator ()(const char* lhs, const char* rhs) const { return strcmp (lhs, rhs) < 0; } 
};

struct compare_string_insensitive : public std::binary_function<const std::string, const std::string, bool>
{
	bool operator ()(const std::string& lhs, const std::string& rhs) const { return StrICmp (lhs, rhs) < 0; } 
};

#endif
