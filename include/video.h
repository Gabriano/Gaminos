// file: "video.h"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 22 Sep 01  initial version (Marc Feeley)

#ifndef VIDEO_H
#define VIDEO_H

//-----------------------------------------------------------------------------

#include "general.h"

//-----------------------------------------------------------------------------

// A "bitmap" is a 2D grid of pixels, having a certain width (W) and
// height (H) and depth (D).  Each pixel has a color that is encoded
// with an integer of D bits.  For space efficiency the bitmap is
// organized in D "layers"; layer number "i" contains all the "i"th
// bit of the pixels (i.e. layer 0 contains all the least significant
// bits of the pixels and layer D-1 contains all the most significant
// bits of the pixels).  Each layer is represented in memory by a
// sequence of "words" (typically 8 bit bytes) where each bit
// corresponds to a pixel.  The first word corresponds to the leftmost
// group of pixels and the most significant bit of each word
// corresponds to the leftmost pixel in that group.  For example a
// bitmap of width 32 and height 2 and depth 1 can be represented with
// 8 bytes as follows:
//
//  X =                      1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//
//  Y         byte 0          byte 1          byte 2          byte 3
//  =    7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
//      v-+-+-+-+-+-+-+-v-+-+-+-+-+-+-+-v-+-+-+-+-+-+-+-v-+-+-+-+-+-+-+-v
//  0   I | | | | | | | I | | | | | | | I | | | | | | | I | | | | | | | I
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  1   I | | | | | | | I | | | | | | | I | | | | | | | I | | | | | | | I
//      ^-+-+-+-+-+-+-+-^-+-+-+-+-+-+-+-^-+-+-+-+-+-+-+-^-+-+-+-+-+-+-+-^
//       7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
//            byte 4          byte 5          byte 6          byte 7
//
// Note that the X coordinate of the pixels increases from left to
// right (which is opposite to the bit numbers), and the Y coordinate
// increases from top to bottom.  This may seem odd, but it is a
// common low-level representation used by the display hardware and in
// particular the PC's VGA card "mode 17" which has a resolution of
// 640 by 480 and where byte 0 is at physical address 0xa0000.

#define BITMAP_WORD_SELECT(b8,b16) b8 // how many bits per word

#define LOG2_BITMAP_WORD_WIDTH BITMAP_WORD_SELECT(3,4) // log2(word width)
#define BITMAP_WORD_WIDTH BITMAP_WORD_SELECT(8,16)
typedef BITMAP_WORD_SELECT(uint8,uint16) bitmap_word;
typedef BITMAP_WORD_SELECT(uint32,uint64) bitmap_quad_word; // to be able to
                                                            // shift 4 words
                                                            // at a time

//-----------------------------------------------------------------------------

// "pattern" class declaration.

class pattern
  {
  public:

    pattern (bitmap_word* words, int height, int depth);

    bitmap_word get_word (int y, int layer);

    static pattern black;
    static pattern gray25;
    static pattern gray50;
    static pattern gray75;
    static pattern white;
    static pattern red;
    static pattern green;
    static pattern yellow;
    static pattern blue;
    static pattern magenta;
    static pattern cyan;

  protected:

    bitmap_word* _words;
    int _height;
    int _depth;
  };

//-----------------------------------------------------------------------------

// "raw_bitmap" class declaration.

class raw_bitmap
  {
  public:

    void bitblt (int x,
                 int y,
                 int x_end,
                 int y_end,
                 raw_bitmap* src,
                 int src_x,
                 int src_y,
                 pattern* foreground,
                 pattern* background);

    void fill_rect (int x,
                    int y,
                    int x_end,
                    int y_end,
                    pattern* foreground);

    void frame_rect (int x,
                     int y,
                     int x_end,
                     int y_end,
                     int border,
                     pattern* foreground);

    void invert_rect (int x, int y, int x_end, int y_end);

    virtual void hide_mouse () = 0;
    virtual void show_mouse () = 0;

  protected:

    virtual bitmap_word* select_layer (int layer) = 0;

    int _width;  // width in pixels, must be a multiple of BITMAP_WORD_WIDTH
    int _height; // height in pixels
    int _depth;  // number of bits per pixel
  };

//-----------------------------------------------------------------------------

// "raw_bitmap_in_memory" class declaration.

class raw_bitmap_in_memory : public raw_bitmap
  {
  public:

    raw_bitmap_in_memory (bitmap_word* start,
                          int width,
                          int height,
                          int depth);

    virtual void hide_mouse ();
    virtual void show_mouse ();

  protected:

    virtual bitmap_word* select_layer (int layer);

    bitmap_word* _start;
  };

//-----------------------------------------------------------------------------

// "video" class declaration.

class video : public raw_bitmap
  {
  public:

    video (int mode);

    static video screen;
    static raw_bitmap_in_memory mouse_save;

    void move_mouse (int dx, int dy);

    virtual void hide_mouse ();
    virtual void show_mouse ();

  protected:

    virtual bitmap_word* select_layer (int layer);

    void get_mouse_rect (int* width, int* height);
    void draw_mouse ();

    int _mode;
    bitmap_word* _start;

    int _mouse_x;
    int _mouse_y;
    int _mouse_hides;
  };

//-----------------------------------------------------------------------------

// "font" class declaration.

class font
  {
  public:

    font (int max_width,
          int height,
          int nb_chars,
          uint16* char_map,
          uint32* char_end,
          raw_bitmap* raw);

    int get_max_width ();

    int get_height ();

    int draw_text (raw_bitmap* dst,
                   int x,
                   int y,
                   unicode_char* text,
                   int count,
                   pattern* foreground,
                   pattern* background);

    int draw_string (raw_bitmap* dst,
                     int x,
                     int y,
                     unicode_string str,
                     pattern* foreground,
                     pattern* background);

    static font mono_5x7;
    static font mono_6x9;

  protected:

    void get_char_data (unicode_char c, int& start, int& width);

    int _max_width;
    int _height;
    int _nb_chars;
    uint16* _char_map;
    uint32* _char_end;
    raw_bitmap* _raw;
  };

//-----------------------------------------------------------------------------

#endif

// Local Variables: //
// mode: C++ //
// End: //
