/*
 *  Base64.h
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-12-30.
 *  Copyright 2009 Unity Technologies. All rights reserved.
 *
 */

#include <string>
// Given a binary blob of size len, fills a base64 encoded version of the data into out.
// The contents of out will be cleared prior to the encoding. 
void Base64Encode(unsigned char * bytes, size_t len, std::string& result, int lineMax=76, const std::string& lf = "\n");