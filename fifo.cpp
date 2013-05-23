// file: "fifo.cpp"

// Copyright (c) 2001 by Marc Feeley and Université de Montréal, All
// Rights Reserved.
//
// Revision History
// 23 Oct 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "fifo.h"

//-----------------------------------------------------------------------------

// "fifo" class implementation.

fifo::fifo ()
{
  _lo = 0;
  _hi = 0;
}

fifo::~fifo ()
{
}

#ifdef SOLUTION

void fifo::put (uint8 byte)
{
  _m.lock ();

  for (;;)
    {
      int next_hi = (_hi + 1) % (max_elements+1);
      if (next_hi != _lo)
        {
          _circularq[_hi] = byte;
          _hi = next_hi;
          break;
        }
      _put_cv.wait (&_m);
    }

  _get_cv.broadcast ();

  _m.unlock ();
}

void fifo::get (uint8* byte)
{
  _m.lock ();

  while (_lo == _hi)
    _get_cv.wait (&_m);

  *byte = _circularq[_lo];

  _lo = (_lo + 1) % (max_elements+1);

  _put_cv.broadcast ();

  _m.unlock ();
}

bool fifo::get_or_timeout (uint8* byte, time timeout)
{
  _m.lock ();

  while (_lo == _hi)
    if (!_get_cv.wait_or_timeout (&_m, timeout))
      return FALSE;

  *byte = _circularq[_lo];

  _lo = (_lo + 1) % (max_elements+1);

  _put_cv.broadcast ();

  _m.unlock ();

  return TRUE;
}

#else

void fifo::put (uint8 byte)
{
  // **** il faut éliminer l'attente active ****

  _m.lock ();

  for (;;)
    {
      int next_hi = (_hi + 1) % (max_elements+1);
      if (next_hi != _lo)
        {
          _circularq[_hi] = byte;
          _hi = next_hi;
          break;
        }
      _m.unlock ();
      _m.lock ();
    }

  _m.unlock ();
}

void fifo::get (uint8* byte)
{
  // **** il faut éliminer l'attente active ****

  _m.lock ();

  while (_lo == _hi)
    {
      _m.unlock ();
      _m.lock ();
    }

  *byte = _circularq[_lo];

  _lo = (_lo + 1) % (max_elements+1);

  _m.unlock ();
}

bool fifo::get_or_timeout (uint8* byte, time timeout)
{
  // **** il faut éliminer l'attente active ****

  _m.lock ();

  while (_lo == _hi)
    {
      _m.unlock ();
      if (less_time (timeout, current_time ()))
        return FALSE;
      _m.lock ();
    }

  *byte = _circularq[_lo];

  _lo = (_lo + 1) % (max_elements+1);

  _m.unlock ();

  return TRUE;
}

#endif

//-----------------------------------------------------------------------------

// Local Variables: //
// mode: C++ //
// End: //
