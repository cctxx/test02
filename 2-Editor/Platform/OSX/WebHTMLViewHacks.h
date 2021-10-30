//
//  WebHTMLViewHacks.h
//  Overriding some WebKit internals to make them work inside a hidden window rendered to a texture
//

#import <Cocoa/Cocoa.h>

#include "MissingHTMLWebView.h"
@interface WebHTMLView (unityHacks) 
//- (void)setNeedsDisplayInRect:(NSRect)invalidRect;
- (void)dragImage:(NSImage *)anImage at:(NSPoint)imageLoc offset:(NSSize)mouseOffset event:(NSEvent *)theEvent pasteboard:(NSPasteboard *)pboard source:(id)sourceObject slideBack:(BOOL)slideBack;

@end



// FIXME: this should be in a separate file
@interface NSEvent (unityHacks)

+ (NSEvent *)scrollEventWithLocation:(NSPoint)location modifierFlags:(unsigned int)flags timestamp:(NSTimeInterval)time windowNumber:(int)windowNum context:(NSGraphicsContext *)context eventNumber:(int)eventNumber
 deltaX:(float)deltaX deltaY:(float)deltaY;
- (void) setWindow: (NSWindow*) window;
 
@end
