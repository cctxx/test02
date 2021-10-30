#ifndef OPTIMIZATION_UTILITY_H
#define OPTIMIZATION_UTILITY_H 1


// ALIGN_LOOP_OPTIMIZATION should be placed in heavy inner loops!
// for (int i=0;i<10000;i++)
//   ALIGN_LOOP_OPTIMIZATION
//   ...

// On the Wii global function alignment is 4 for debug builds to save exe size
//  as there are a lot of non-inlined functions.
// In that case having align 16 generates warnings, so we blank the define.
#if defined(__MWERKS__) && UNITY_RELEASE && !defined(_DEBUG)
#define ALIGN_LOOP_OPTIMIZATION asm {align 16}
#else
///@TODO: optimize this for gcc too
#define ALIGN_LOOP_OPTIMIZATION 
#endif

#endif
