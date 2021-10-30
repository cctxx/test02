#pragma once

/// Constant strings use the ConstantStringManager to reuse commonly used strings.
/// Eg. GameObject names and asset names are often exactly the same.
/// When a string is not available in ConstantStringManager, it is allocated on the heap and refcounted.
/// operator = is used to assign refcounted strings. (operator = is always dirty cheap)
/// ConstantString uses only 1 ptr in the struct directly, thus the simplest case (A shared constant string) has no allocations & only 1 pointer for storage.


/// The ConstantStringManager is initialized at load time with all common strings and
/// stays completely constant during runtime, thus it is thread safe. ConstantString looks up all strings there first and if it is in it, it will use those.
/// Eg. when loading an asset bundle we can still reduce memory usage.
/// When serializing strings on targets that dont need to worry about backwards compatibility or asset bundle compatibility, we can simply store an index to the ConstantStringManager and look it up by index on load.
/// Thus also reducing size on disk too.

struct ConstantString
{
	ConstantString (const char* str, MemLabelId label)
		: m_Buffer (NULL)
	{
		assign (str, label);
	}

	ConstantString ()
	: m_Buffer (NULL)
	{
		create_empty();
	}

	ConstantString (const ConstantString& input)
	: m_Buffer (NULL)
	{
		assign(input);
	}
	
	~ConstantString ();

	void assign (const char* str, MemLabelId label);
	void assign (const ConstantString& input);

	void operator = (const ConstantString& input);

	const char* c_str() const { return get_char_ptr_fast (); }
	bool empty () const       { return m_Buffer[0] == 0; }
	
	friend bool operator == (const ConstantString& lhs, const ConstantString& rhs)
	{
		if (lhs.owns_string () || rhs.owns_string())
			return strcmp(lhs.c_str(), rhs.c_str()) == 0;
		else
			return lhs.m_Buffer == rhs.m_Buffer;
	}

	friend bool operator == (const ConstantString& lhs, const char* rhs)
	{
		return strcmp(lhs.c_str(), rhs) == 0;
	}

	friend bool operator == (const char* rhs, const ConstantString& lhs)
	{
		return strcmp(lhs.c_str(), rhs) == 0;
	}
	
	friend bool operator != (const ConstantString& lhs, const char* rhs)
	{
		return strcmp(lhs.c_str(), rhs) != 0;
	}

	friend bool operator != (const char* rhs, const ConstantString& lhs)
	{
		return strcmp(lhs.c_str(), rhs) != 0;
	}
	
	private:
	
	inline bool owns_string () const              { return reinterpret_cast<size_t> (m_Buffer) & 1; }
	inline const char* get_char_ptr_fast () const { return reinterpret_cast<const char*> (reinterpret_cast<size_t> (m_Buffer) & ~1); }
	void cleanup ();
	void create_empty ();
	
	const char* m_Buffer;
};