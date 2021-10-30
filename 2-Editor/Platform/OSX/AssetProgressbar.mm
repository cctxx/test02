#include "UnityPrefix.h"
#include "Editor/Platform/Interface/AssetProgressbar.h"
#include "Editor/Platform/OSX/Utility/CocoaEditorUtility.h"
#include "Runtime/Utilities/Argv.h"

using namespace std;

@interface ProgressbarController : NSObject
{
	@public
	IBOutlet id m_Progress;
	IBOutlet id m_Text;
	IBOutlet NSButton* m_CancelButton;
	int m_LastPercentValue;
	NSModalSession m_ModalSession;
	std::string* m_LastName;
	bool m_CancelPressed;
}

@end

@implementation ProgressbarController

- (id)init
{
	m_LastName = new string();
	m_LastPercentValue = -1;
	m_CancelPressed = false;
	return self;
}

- (void)dealloc
{
	delete m_LastName;
	if (m_ModalSession != NULL)
		[NSApp endModalSession:m_ModalSession];
	[[m_Progress window]close];
	[super dealloc];
}

- (void)cancel:(id)sender
{
	m_CancelPressed = true;
}

- (bool)cancelPressed
{
	return m_CancelPressed;
}

- (bool)cancelButtonEnabled
{
	return ![m_CancelButton isHidden];
}

- (void)enableCancelButton:(bool)enable
{
	[m_CancelButton setHidden:!enable];
}

// taken from http://svn.oofn.net/CTProgressBadge/trunk/CTProgressBadge.m
- (NSImage *)progressBadgeOfSize:(float)size withProgress:(float)progress
{
	float scaleFactor = size/16;
	float stroke = 2*scaleFactor;	//native size is 16 with a stroke of 2
	float shadowBlurRadius = 1*scaleFactor;
	float shadowOffset = 1*scaleFactor;
	
	float shadowOpacity = .4;
	
	NSRect pieRect = NSMakeRect(shadowBlurRadius,shadowBlurRadius+shadowOffset,size,size);
	
	NSImage *progressBadge = [[NSImage alloc] initWithSize:NSMakeSize(size + 2*shadowBlurRadius, size + 2*shadowBlurRadius+1)];
		
	[progressBadge lockFocus];
	[NSGraphicsContext saveGraphicsState];
	NSShadow *theShadow = [[NSShadow alloc] init];
	[theShadow setShadowOffset: NSMakeSize(0,-shadowOffset)];
	[theShadow setShadowBlurRadius:shadowBlurRadius];
	[theShadow setShadowColor:[[NSColor blackColor] colorWithAlphaComponent:shadowOpacity]];
	[theShadow set];
	[theShadow release];
	[[NSColor colorWithDeviceRed:91./255 green:133./255 blue:182./255 alpha:1] set];
	[[NSBezierPath bezierPathWithOvalInRect:pieRect] fill];
	[NSGraphicsContext restoreGraphicsState];
	
	[[NSColor whiteColor] set];
	if(progress <= 0)
	{
		[[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMinX(pieRect)+stroke,NSMinY(pieRect)+stroke,
														   NSWidth(pieRect)-2*stroke,NSHeight(pieRect)-2*stroke)] fill];
	}
	else if(progress < 1)
	{
		NSBezierPath *slice = [NSBezierPath bezierPath];
		[slice moveToPoint:NSMakePoint(NSMidX(pieRect),NSMidY(pieRect))];
		[slice appendBezierPathWithArcWithCenter:NSMakePoint(NSMidX(pieRect),NSMidY(pieRect)) radius:NSHeight(pieRect)/2-stroke startAngle:90 endAngle:90-progress*360 clockwise:NO];
		[slice moveToPoint:NSMakePoint(NSMidX(pieRect),NSMidY(pieRect))];
		[slice fill];
	}
	[progressBadge unlockFocus];
	
	return progressBadge;
}

- (void) setProgressBar:(float)value andText:(const string&) text canCancel:(bool)canCancel
{
	
	if (m_Progress == NULL)
	{
		if (LoadNibNamed (@"Progress.nib", self) == false)
			NSLog (@"nib not loadable");
		[self enableCancelButton:canCancel];
		[[m_Progress window]orderFront: NULL];
		[m_Progress setMaxValue: 1.0F];
		m_ModalSession = [NSApp beginModalSessionForWindow:[m_Progress window]];
		[NSApp cancelUserAttentionRequest:NSCriticalRequest];
	}
	
	// Update graphics icon only if it has changed at least once percentage point.
	int percentValue = RoundfToInt(value * 100.0F);
	if (m_LastPercentValue != percentValue)
	{
		m_LastPercentValue = percentValue;
        
		NSRect imageRect       = NSMakeRect(0, 0, 128, 128);
		NSImage *overlayImage  = [[NSImage alloc] initWithSize:imageRect.size];
		NSImage *badgeImage    = [self progressBadgeOfSize:42 withProgress:value];
		NSImage *appIcon       = [NSImage imageNamed:@"NSApplicationIcon"];
		
		[overlayImage lockFocus];
		[appIcon drawInRect:imageRect fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];
		[badgeImage compositeToPoint:NSMakePoint(0,80) fromRect:NSZeroRect operation: NSCompositeSourceOver fraction:1.0];
		[overlayImage unlockFocus];
		
		[NSApp setApplicationIconImage: overlayImage];
		
		[badgeImage release];
		[overlayImage release];
		
		// Update progress
		[m_Progress setDoubleValue: value];
	}

	if (*m_LastName != text)
	{
		*m_LastName = text;
		[m_Text setStringValue: [NSString stringWithUTF8String: text.c_str()]];
	}
	
	[m_Text displayIfNeeded];
	[m_Progress displayIfNeeded];
	if ([self cancelButtonEnabled] != canCancel)
		[self enableCancelButton:canCancel];
	[NSApp runModalSession:m_ModalSession];
}

@end

static ProgressbarController* gProgressBar = NULL;

void ClearAssetProgressbar ()
{
	[gProgressBar release];
	gProgressBar = NULL;

	[NSApp setApplicationIconImage: nil];
}

void UpdateAssetProgressbar (float value, std::string const& title, std::string const& text, bool canCancel)
{
	if( IsBatchmode() )
		return;
	
	NSAutoreleasePool *subPool = [[NSAutoreleasePool alloc] init];
	
	if (gProgressBar == NULL)
		gProgressBar = [[ProgressbarController alloc]init];
	
	[gProgressBar setProgressBar: value andText: text canCancel: canCancel];
	
	[subPool drain];
}

bool IsAssetProgressBarCancelPressed ()
{
	if( IsBatchmode() )
		return false;
	
	if (gProgressBar)
		return [gProgressBar cancelPressed];
	
	return false;
}
