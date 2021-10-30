#ifndef SCRIPTLANGUAGEPORTUTILITY_H
#define SCRIPTLANGUAGEPORTUTILITY_H

#if UNITY_FLASH || UNITY_WEBGL

extern "C" bool Ext_FileContainer_IsFileCreatedAt(const char* filename);
extern "C" bool Ext_LogCallstack();

#endif
#endif
