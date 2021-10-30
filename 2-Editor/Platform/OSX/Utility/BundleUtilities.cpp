#include "UnityPrefix.h"
#include "BundleUtilities.h"
#include <Carbon/Carbon.h>
#include "Runtime/Utilities/File.h"

static UInt32 GetVersionFromBundle(CFBundleRef bundle);
static CFPropertyListRef GetPlistFromURL(CFURLRef url);

UInt32 GetBundleVersion(std::string bundleName)
{
	bundleName = PathToAbsolutePath (bundleName);
	CFURLRef urlref = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const unsigned char*)bundleName.c_str(), bundleName.size(), true); 
	CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlref);
	if (bundleRef == NULL)
		return 0;
	
	return GetVersionFromBundle (bundleRef);
}

// Apple's CFBundle routines fuck up with bundles downloaded after Safari starts
// so we roll our own version of CFBundleGetVersionNumber().
static UInt32 GetVersionFromBundle(CFBundleRef bundle)
{
	CFURLRef bundleURL = CFBundleCopyBundleURL(bundle); 	
	CFStringRef cfAppend = CFStringCreateWithCString(kCFAllocatorDefault, "Contents/Info.plist", kCFStringEncodingUTF8);
	CFURLRef plistURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, bundleURL, cfAppend, true); 
	UInt32 result=0;
	CFRelease(cfAppend);

	CFPropertyListRef plist=GetPlistFromURL(plistURL);
	CFRelease(plistURL);
	if (plist)
	{
		if (CFDictionaryGetTypeID() == CFGetTypeID(plist))
		{
			if (CFStringRef v=(CFStringRef)CFDictionaryGetValue((CFDictionaryRef)plist,CFSTR("CFBundleVersion")))
			{
				char versionCString[4096];
				CFStringGetCString(v, versionCString, 4096, kCFStringEncodingASCII);
				result = VersionStringToNumeric(versionCString);
				CFRelease(v);
			}
		}
	}
	return result;	
}

// Convert version string of form XX.X.XcXXX to UInt32 in Apple 'vers' representation
// as described in Tech Note TN1132 (http://developer.apple.com/technotes/tn/pdf/tn1132.pdf)
UInt32 VersionStringToNumeric(std::string inString)
{
	const char* versionCString = inString.c_str();
	if((*versionCString)=='\0')
		return 0;
	int mayor=0,minor=0,fix=0,type='r',release=0;
	const char *spos=versionCString;
	mayor=*(spos++)-'0';
	if(*spos>='0'&&*spos<'9')
		mayor=mayor*10+*(spos++)-'0';
	if(*spos){
		if(*spos=='.')
		{
			spos++;
			if(*spos)		
				minor=*(spos++)-'0';
		}
		if(*spos){
			if(*spos=='.')
			{
				spos++;
				if(*spos)		
					fix=*(spos++)-'0';
			}
			if(*spos)		
			{
				type=*(spos++);
				if(*spos)		
				{
					release=*(spos++)-'0';
					if(*spos)		
					{
						release=release*10+*(spos++)-'0';
						if(*spos)		
						{
							release=release*10+*(spos++)-'0';
						}				
					}				
				}				
			}
		}
	}
	UInt32 version=0;
	version|=((mayor/10)%10)<<28;
	version|=(mayor%10)<<24;
	version|=(minor%10)<<20;
	version|=(fix%10)<<16;
	switch(type)
	{
		case 'D':
		case 'd': version|=0x2<<12;  break;
		case 'A':
		case 'a': version|=0x4<<12;  break;
		case 'B':
		case 'b': version|=0x6<<12;  break;
		case 'F':
		case 'R':
		case 'f': 
		case 'r': version|=0x8<<12;  break;			
	}
	version|=((release/100)%10)<<8;
	version|=((release/10)%10)<<4;
	version|=release%10;
	return version;
}


CFPropertyListRef GetPlistFromURL(CFURLRef url)
{
	CFPropertyListRef plist = NULL;
	if(url)
	{
		CFDataRef resCFDataRef;
		if (CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,url,&resCFDataRef, nil, nil, nil))
		{
			if(resCFDataRef)
			{
				CFStringRef errorString;
				plist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, resCFDataRef, kCFPropertyListImmutable, &errorString);
				CFRelease(resCFDataRef);
			}
		}
	}
	return plist;
}
