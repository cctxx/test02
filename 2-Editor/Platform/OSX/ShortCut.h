#import <Cocoa/Cocoa.h>
#ifdef __OBJC__

@interface ShortCut : NSObject {
	unsigned int m_ModifierMask;
	NSString *m_MainString, *m_LongString;
	NSString *m_KeyEquivalent;
}
-(void)setKeyString:(NSString *)str;
-(unsigned int) modifierKeyMask;
-(NSString *) tooltipString;
-(NSString *) keyString;
-(NSString *) mainString;
-(NSString *) longString;
-(NSString *) longStringNoKeys;
-(NSString *) keyEquivalent;
-(BOOL) accepts:(NSEvent *)theEvent;
@end

#endif
