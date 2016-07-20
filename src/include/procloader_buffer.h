/*
No copyright is claimed in the United States under Title 17, U.S. Code.
All Other Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _PROCLOADER_BUFFER_H
#define _PROCLOADER_BUFFER_H

#include "waterslide.h"
#include "wstypes.h"
#include "datatypes/wsdt_tuple.h"
#include "cppwrap.h"

#ifdef __cplusplus
CPP_OPEN
#endif // __cplusplus

int procbuffer_option(void *, void *, int, const char *);

//read in command line options
int procbuffer_init(void *, void *);

int procbuffer_decode(void *, wsdata_t *, wsdata_t *, uint8_t * buf, int len);

#ifdef __cplusplus
CPP_CLOSE
#endif // __cplusplus

#endif // _PROCLOADER_BUFFER_H
