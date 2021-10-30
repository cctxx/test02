#ifndef UNITY_IPHONE_REMOTE_IMPL_
#define UNITY_IPHONE_REMOTE_IMPL_

class Image;

bool iPhoneRemoteInputInit(unsigned* outPort);
void iPhoneRemoteUpdate();
void iPhoneRemoteInputShutdown();
Image& iPhoneGetRemoteScreenShotBuffer();
void iPhoneDidModifyScreenshotBuffer(bool value);
bool iPhoneHasRemoteConnected();
void iPhoneRemoteSetConnected(bool value);
int CompressImageBlock (unsigned char* outptr, size_t outBuferSize,
                        size_t w, size_t h, int compression,
                        unsigned char *inptr);

#endif
