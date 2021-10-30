/*
 *  Base64.cpp
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-12-30.
 *  Copyright 2009 Unity Technologies. All rights reserved.
 *
 */
#include "UnityPrefix.h"

#include "Base64.h"
using namespace std;

// on win IGNORE is already defined
#undef IGNORE


// Used to map from a value from 0 to 63 to the corresponding base64 character
static const char to64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define IGNORE  -1	/* not a base64 char */
#define PAD     -2	/* padding */
#define __		IGNORE /* make ignored characters look nicer in the table below */

// The other way around. Maps ascii chars to the correct value between 0 and 63
// Ignored characters return -1 and the end of data padding character, =, is -2
static const signed char from64[256] = {
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, 62,  __, __, __, 63, 
    52, 53, 54, 55,  56, 57, 58, 59,  60, 61, __, __,  __,PAD, __, __, 
    __,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14, 
    15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25, __,  __, __, __, __, 
    __, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40, 
    41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51, __,  __, __, __, __, 

    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
    __, __, __, __,  __, __, __, __,  __, __, __, __,  __, __, __, __, 
};

void Base64Encode(unsigned char * bytes, size_t len, string& result, int lineMax, const string& lf)
{
	int outputLen = (len+2)/3*4;
	if ( outputLen && lineMax )
		outputLen += ((outputLen-1) / lineMax + 1) * lf.size();
	
	result.clear();
	result.reserve(outputLen);
	
	int i=0;
	unsigned char b1,b2,b3;
	for (int chunk=0; len > 0; len -= 3, chunk++) {
	    if ( chunk == (lineMax/4) ) {
			result += lf;
			chunk = 0;
	    }
	    b1 = bytes[i++];
	    b2 = len > 1 ? bytes[i++] : '\0';
	    result += to64[b1>>2];
		result += to64[((b1 & 3) << 4 )|((b2 & 0xf0) >> 4)];
		if (len > 2) 
		{
			b3 = bytes[i++];
			result += to64[((b2 & 0xF) << 2) | ((b3 & 0xC0) >>6)];
			result += to64[b3 & 0x3F];
	    } 
		else if (len == 2)
		{
			result += to64[(b2 & 0xF) << 2];
			result += '=';
			break;
	    } 
		else /* len == 1 */
		{
			result += "==";
			break;
		}
	}
	if ( ! result.empty() )
		result += lf;
}