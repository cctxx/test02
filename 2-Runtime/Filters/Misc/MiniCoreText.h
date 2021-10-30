#if !defined(MINICORETEXT_H)
#define MINICORETEXT_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef float CGFloat;
typedef const struct __CTFont * CTFontRef;
typedef const struct __CTFontDescriptor * CTFontDescriptorRef;
typedef uint32_t CTFontOrientation;
typedef const struct __CTLine * CTLineRef;

enum {
	kCTFontDefaultOrientation		= 0,
};

extern const CFStringRef kCTForegroundColorAttributeName WEAK_IMPORT_ATTRIBUTE;
extern const CFStringRef kCTFontAttributeName WEAK_IMPORT_ATTRIBUTE;


CTFontRef CTFontCreateWithName(CFStringRef name, CGFloat size, const CGAffineTransform *matrix) WEAK_IMPORT_ATTRIBUTE;
CTFontRef CTFontCreateWithGraphicsFont(CGFontRef graphicsFont, CGFloat size, const CGAffineTransform *matrix, CTFontDescriptorRef attributes) WEAK_IMPORT_ATTRIBUTE;
CGFloat CTFontGetAscent(CTFontRef font) WEAK_IMPORT_ATTRIBUTE;
CGFloat CTFontGetDescent(CTFontRef font) WEAK_IMPORT_ATTRIBUTE;
Boolean CTFontGetGlyphsForCharacters(CTFontRef font, const UniChar characters[], CGGlyph glyphs[], CFIndex count) WEAK_IMPORT_ATTRIBUTE;
CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef font, CTFontOrientation orientation, const CGGlyph glyphs[], CGRect boundingRects[], CFIndex count ) WEAK_IMPORT_ATTRIBUTE;
double CTFontGetAdvancesForGlyphs(CTFontRef font, CTFontOrientation orientation, const CGGlyph glyphs[], CGSize advances[], CFIndex count) WEAK_IMPORT_ATTRIBUTE;
CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef string) WEAK_IMPORT_ATTRIBUTE;
void CTLineDraw(CTLineRef line, CGContextRef context) WEAK_IMPORT_ATTRIBUTE;
CTFontRef CTFontCreateForString(CTFontRef currentFont, CFStringRef string, CFRange range) WEAK_IMPORT_ATTRIBUTE;
CGFontRef CGFontCreateWithDataProvider(CGDataProviderRef provider) WEAK_IMPORT_ATTRIBUTE;

#if defined(__cplusplus)
}
#endif

#endif