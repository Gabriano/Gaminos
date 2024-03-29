// file: "rtlib.h"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 22 Sep 01  initial version (Marc Feeley)

#ifndef __RTLIB_H
#define __RTLIB_H

//-----------------------------------------------------------------------------

#include "general.h"

//-----------------------------------------------------------------------------

// Error handling.

void fatal_error (native_string msg);

//-----------------------------------------------------------------------------

// Math routines.

uint8 log2 (uint32 n);

//-----------------------------------------------------------------------------

// Memory management.

void* kmalloc (size_t size);
void kfree (void* ptr);

extern "C"
void* memcpy (void* dest, const void* src, size_t n);

//-----------------------------------------------------------------------------

// Execution of global constructors and destructors.

void __do_global_ctors ();
void __do_global_dtors ();

//-----------------------------------------------------------------------------

// Runtime library entry point.

extern "C"
void __rtlib_entry ();

int main ();

//-----------------------------------------------------------------------------

#endif

// Local Variables: //
// mode: C++ //
// End: //
