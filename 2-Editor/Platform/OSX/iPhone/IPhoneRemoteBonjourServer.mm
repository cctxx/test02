#include "IPhoneRemoteBonjourServer.h"
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#import <SystemConfiguration/SystemConfiguration.h>
#import <Cocoa/Cocoa.h>

#ifdef MAC_OS_X_VERSION_10_6
@interface IphoneRemoteBonjourServer : NSObject <NSNetServiceDelegate>
#else
@interface IphoneRemoteBonjourServer : NSObject 
#endif
{
	unsigned mPort;
	NSString *mType;
	NSNetService *mService;
	NSString *mName;
}
- (void)start;
- (void)stop;
- (void)stopNotification: (NSNotification*)n;
- (void)setPort:(int)port;
- (void)setType:(NSString *)type;
- (void)setName:(NSString *)name;
@end


@implementation IphoneRemoteBonjourServer
- (id)init
{
	self = [super init];
    
	if (!self)
    {
		return nil;
	}
    
	mPort = 0;
    
	[self setType: @"_unityiphoneremote._tcp"];
	[self setName: @""];
	[self start];
    
  	[[NSNotificationCenter defaultCenter]addObserver:
     self selector: @selector (stopNotification:)
                                                name: NSApplicationWillTerminateNotification
                                              object: NULL];
    
	return self;
}

- (void)start
{
	if (mPort == -1)
	{
		ErrorString("Failed Socket Port Initialization");
		return;
	}
	
	if (!iPhoneRemoteInputInit(&mPort))
	{
		ErrorString("Failed iPhone Remote Socket Initialization");
		return;
	}
	
	mService = [[NSNetService alloc] initWithDomain: @""
                                               type: mType
                                               name: mName
                                               port: mPort];
    
	if (mService)
	{
		[mService setDelegate:self];
		[mService publish];
	}
	else
	{
		ErrorString("Failed NSNetService Initialization");
	}
}

- (void)stopNotification: (NSNotification*)n
{
	[self stop];
}

- (void)stop
{
	[mService stop];
	iPhoneRemoteInputShutdown();
}

- (void)setPort:(int)port
{
	mPort = port;
}

- (void)setType:(NSString *)type
{
	[mType autorelease];
	mType = [type retain];
}

- (void)setName:(NSString *)name
{
	[mName autorelease];
	mName = [name retain];
}

- (void)netServiceWillPublish:(NSNetService *)netService
{
}

- (void)netService:(NSNetService *)netService didNotPublish:(NSDictionary *)errorDict
{
	ErrorString(Format("Failed iPhone Remote Bonjour Service %s.%s.%s",
                       [[mService name]UTF8String],
                       [[mService type]UTF8String],
                       [[mService domain]UTF8String]));
}

- (void)netServiceDidStop:(NSNetService *)netService
{
    // Stop iphone input
}

@end


void IphoneRemoteBonjourServerInit()
{
    [[IphoneRemoteBonjourServer alloc] init];
}
