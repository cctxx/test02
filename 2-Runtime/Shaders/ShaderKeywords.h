#pragma once

#include <string>

typedef int ShaderKeyword;

// We use about 14 builtin keywords now, 18 more for the user should be fine now
// (shader size explosion will happen before that limit is reached anyways...)
const int kMaxShaderKeywords = 64;
struct ShaderKeywordSet {
public:
	explicit ShaderKeywordSet () : mask(0) {}

	void Enable (ShaderKeyword key) { mask |= (0x1ULL<<key); }
	void Disable (ShaderKeyword key) { mask &= ~(0x1ULL<<key); }
	bool IsEnabled (ShaderKeyword key) const { return (mask & (0x1ULL<<key)) != 0; }
	void Reset () { mask = 0; }

	UInt64 GetMask() const { return mask; }
	void SetMask (UInt64 m) { mask = m; }

	bool operator== (const ShaderKeywordSet& o) const { return mask==o.mask; }
	bool operator!= (const ShaderKeywordSet& o) const { return mask!=o.mask; }

private:
	UInt64 mask;
};

extern ShaderKeywordSet g_ShaderKeywords;


namespace keywords {
	
	void Initialize();
	void Cleanup();
	
	ShaderKeyword Create( const std::string& name );

	#if UNITY_EDITOR
	std::string GetKeywordName (ShaderKeyword k);
	#endif
	
} // namespace keywords
