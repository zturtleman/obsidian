/*  Obsidian
    Copyright (C) 2011 by tm512

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
    ---
    o_common : Obsidian Client/Server common code
*/

#ifndef O_COMMON_H
#define O_COMMON_H

#include "enet/enet.h"

#define MAXPLAYERNAME 10
#define OBS_PROTO 1

extern boolean server;
extern boolean client;

// void WriteInt32(int32_t** const Dest, int32_t Value);

// Protocol

enum
{
	MSG_NULL,

	MSG_WELCOME,

	MSG_POS,

	MSG_JOIN,

	MSG_USE,

	MSG_TIC
};

#endif
