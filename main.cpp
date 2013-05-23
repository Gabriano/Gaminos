// file: "main.cpp"

// Copyright (c) 2001-2002 by Marc Feeley and Université de Montréal, All
// Rights Reserved.
//
// Revision History
// 25 Feb 02  implemented Sirtet game (Marc Feeley)
// 23 Oct 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "general.h"
#include "term.h"
#include "fifo.h"
#include "thread.h"
#include "time.h"
#include "ps2.h"

//-----------------------------------------------------------------------------

// **** possiblement des modifications à faire dans ce fichier ****
// **** pour éliminer les défauts d'affichage                  ****

#ifdef SOLUTION
static mutex* seq;
#endif

//-----------------------------------------------------------------------------

// Pseudo-random number generator.

static int seed = 1;

int random (int n)
{
  seed = (seed * 3581 + 12751) % 131072;
  return seed % n;
}

//-----------------------------------------------------------------------------

// Constants.

#define TILE_SIZE 10
#define TOP_GAP 8
#define BOARD_BORDER 20
#define BOARD_WIDTH 19
#define BOARD_HEIGHT 28
#define PYRAMID TRUE

#define LEFT_CMD  0
#define RIGHT_CMD 1
#define DROP_CMD  2
#define LOSE_CMD  3
#define WIN_CMD   4

#define PLAYER0_LEFT_EVENT     0
#define PLAYER1_LEFT_EVENT     1
#define PLAYER0_DROP_EVENT     2
#define PLAYER1_DROP_EVENT     3
#define PLAYER0_RIGHT_EVENT    4
#define PLAYER1_RIGHT_EVENT    5
#define PLAYER0_FINISHED_EVENT 6
#define PLAYER1_FINISHED_EVENT 7
#define WIN_LOSE_ACK_EVENT     8

//-----------------------------------------------------------------------------

// "player" class.  Implements the playing board for a single player.

class player : public thread
  {
  public:

    player (int id,
            int x,
            int y,
            pattern* color,
            fifo* player_commands,
            fifo* referee_events);

  protected:

    void run ();

  private:

    void draw_tile (int col, int row);
    void erase_tile (int col, int row);
    void draw_score ();
    bool move_to (int col, int row);

    int _id;
    int _x;
    int _y;
    pattern* _color;
    fifo* _player_commands;
    fifo* _referee_events;
    bool _board[BOARD_WIDTH][BOARD_HEIGHT];
    int _score;
    int _col;
    int _row;
  };

player::player (int id,
                int x,
                int y,
                pattern* color,
                fifo* player_commands,
                fifo* referee_events)
{
  _id = id;
  _x = x;
  _y = y;
  _color = color;
  _player_commands = player_commands;
  _referee_events = referee_events;
}

void player::draw_tile (int col, int row)
{
  int x = BOARD_BORDER + _x + col*TILE_SIZE;
  int y = BOARD_BORDER + _y + row*TILE_SIZE;

#ifdef SOLUTION
  seq->lock ();
#endif

  video::screen.fill_rect
    (x + 1,
     y + 1,
     x + TILE_SIZE,
     y + TILE_SIZE,
     _color);

#ifdef SOLUTION
  seq->unlock ();
#endif
}

void player::erase_tile (int col, int row)
{
  int x = BOARD_BORDER + _x + col*TILE_SIZE;
  int y = BOARD_BORDER + _y + row*TILE_SIZE;

#ifdef SOLUTION
  seq->lock ();
#endif

  video::screen.fill_rect
    (x + 1,
     y + 1,
     x + TILE_SIZE,
     y + TILE_SIZE,
     &pattern::white);

#ifdef SOLUTION
  seq->unlock ();
#endif
}

void player::draw_score ()
{
  int x = BOARD_BORDER + _x + TILE_SIZE*BOARD_WIDTH/2 - 8*6/2;
  int y = BOARD_BORDER*3/2 + _y + TILE_SIZE*BOARD_HEIGHT - 9/2;

  unicode_char txt[16];
  unicode_char* p = txt;

  *p++ = 'S'; *p++ = 'C'; *p++ = 'O'; *p++ = 'R'; *p++ = 'E'; *p++ = ':';
  *p++ = ' ';

  int d = 1;
  while (d*10 <= _score) d *= 10;

  while (d >= 1)
    {
      *p++ = '0' + (_score / d) % 10;
      d /= 10;
    }

  *p = '\0';

#ifdef SOLUTION
  seq->lock ();
#endif

  font::mono_6x9.draw_string (&video::screen,
                              x,
                              y,
                              txt,
                              &pattern::white,
                              _color);

#ifdef SOLUTION
  seq->unlock ();
#endif
}

bool player::move_to (int col, int row)
{
  if (col < 0
      || col >= BOARD_WIDTH
      || row >= BOARD_HEIGHT
      || _board[col][row])
    return FALSE;

  erase_tile (_col, _row);
  _col = col;
  _row = row;
  draw_tile (col, row);

  return TRUE;
}

void player::run ()
{
  time last_step;
  int speed;
  int accel;
  uint8 cmd;
  time timeout;

#ifdef SOLUTION
  seq->lock ();
#endif

  video::screen.frame_rect
    (_x,
     _y,
     _x + BOARD_BORDER*2 + TILE_SIZE*BOARD_WIDTH + 1,
     _y + BOARD_BORDER*2 + TILE_SIZE*BOARD_HEIGHT + 1,
     BOARD_BORDER,
     _color);

  video::screen.fill_rect
    (_x + BOARD_BORDER,
     _y + BOARD_BORDER,
     _x + BOARD_BORDER + TILE_SIZE*BOARD_WIDTH + 1,
     _y + BOARD_BORDER + TILE_SIZE*BOARD_HEIGHT + 1,
     &pattern::white);

  video::screen.fill_rect
    (_x + BOARD_BORDER,
     _y + BOARD_BORDER + TILE_SIZE*TOP_GAP,
     _x + BOARD_BORDER + TILE_SIZE*BOARD_WIDTH + 1,
     _y + BOARD_BORDER + TILE_SIZE*TOP_GAP + 1,
     &pattern::cyan);

#ifdef SOLUTION
  seq->unlock ();
#endif

  _score = 0;

 fresh_board:

  draw_score ();

  for (int row = 0; row < BOARD_HEIGHT; row++)
    for (int col = 0; col < BOARD_WIDTH; col++)
      {
        if (PYRAMID
            && col + row >= BOARD_HEIGHT
            && (BOARD_WIDTH-1-col) + row >= BOARD_HEIGHT)
          {
            _board[col][row] = TRUE;
            draw_tile (col, row);
          }
        else
          {
            _board[col][row] = FALSE;
            erase_tile (col, row);
          }
      }

 new_block:

  _col = random (BOARD_WIDTH);
  _row = 0;
  speed = 8;
  accel = 0;

  last_step = current_time ();

  draw_tile (_col, _row);

  for (;;)
    {
      timeout = add_time (last_step, frequency_to_time (speed));

      if (_player_commands->get_or_timeout (&cmd, timeout))
        switch (cmd)
          {
          case LEFT_CMD:  move_to (_col-1, _row); break;
          case RIGHT_CMD: move_to (_col+1, _row); break;
          case DROP_CMD:  accel = 8;              break;
          case WIN_CMD:
          case LOSE_CMD:
            _referee_events->put (WIN_LOSE_ACK_EVENT);
            if (cmd == WIN_CMD)
              _score++;
            goto fresh_board;
          }
      else
        {
          // the "get_or_timeout" timed out so block must move down

          if (move_to (_col, _row+1)) // try to move block down
            {
              // block was moved down

              last_step = timeout;
              speed += accel;
            }
          else
            {
              // block could not be moved down

              _board[_col][_row] = TRUE;
              if (_row <= TOP_GAP)
                {
                  // tower is finished; tell the referee and hope we
                  // were the first!

                  if (_id == 0)
                    _referee_events->put (PLAYER0_FINISHED_EVENT);
                  else
                    _referee_events->put (PLAYER1_FINISHED_EVENT);

                  for (;;)
                    {
                      _player_commands->get (&cmd);
                      switch (cmd)
                        {
                        case WIN_CMD:
                        case LOSE_CMD:
                          _referee_events->put (WIN_LOSE_ACK_EVENT);
                          if (cmd == WIN_CMD)
                            _score++;
                          goto fresh_board;
                        }
                    }
                }
              else
                {
                  // drop the next block

                  goto new_block;
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------

// "input_controller" class.  Reads the keyboard and informs the referee.

class input_controller : public thread
  {
  public:

    input_controller (fifo* referee_events);

  protected:

    void run ();

  private:

    fifo* _referee_events;
  };

input_controller::input_controller (fifo* referee_events)
{
  _referee_events = referee_events;
}

void input_controller::run ()
{
  for (;;)
    {
      unicode_char c = getchar ();
      switch (c)
        {
        case 'q': _referee_events->put (PLAYER0_LEFT_EVENT);  break;
        case 'i': _referee_events->put (PLAYER1_LEFT_EVENT);  break;
        case 'w': _referee_events->put (PLAYER0_DROP_EVENT);  break;
        case 'o': _referee_events->put (PLAYER1_DROP_EVENT);  break;
        case 'e': _referee_events->put (PLAYER0_RIGHT_EVENT); break;
        case 'p': _referee_events->put (PLAYER1_RIGHT_EVENT); break;
        }
    }
}

//-----------------------------------------------------------------------------

// "referee" class.  Oversees the game.

class referee : public thread
  {
  public:

    referee ();

  protected:

    void run ();

  private:

    fifo* _referee_events;
    fifo* _to_player[2];
    player* _players[2];
  };

referee::referee ()
{
  _referee_events = new fifo;

  _to_player[0] = new fifo;
  _to_player[1] = new fifo;

  _players[0] =
    new player (0, 0, 159, &pattern::red, _to_player[0], _referee_events);

  _players[1] =
    new player (1, 409, 159, &pattern::blue, _to_player[1], _referee_events);
}

void referee::run ()
{
  uint8 event;

  (new input_controller (_referee_events))->start ();

  _players[0]->start ();
  _players[1]->start ();

  for (;;)
    {
      _referee_events->get (&event);

      switch (event)
        {
        case PLAYER0_LEFT_EVENT : _to_player[0]->put (LEFT_CMD);  break;
        case PLAYER1_LEFT_EVENT : _to_player[1]->put (LEFT_CMD);  break;
        case PLAYER0_DROP_EVENT : _to_player[0]->put (DROP_CMD);  break;
        case PLAYER1_DROP_EVENT : _to_player[1]->put (DROP_CMD);  break;
        case PLAYER0_RIGHT_EVENT: _to_player[0]->put (RIGHT_CMD); break;
        case PLAYER1_RIGHT_EVENT: _to_player[1]->put (RIGHT_CMD); break;
        case PLAYER0_FINISHED_EVENT:
        case PLAYER1_FINISHED_EVENT:
          _to_player[(event==PLAYER1_FINISHED_EVENT)]->put (WIN_CMD);
          _to_player[(event==PLAYER0_FINISHED_EVENT)]->put (LOSE_CMD);

          // wait for both players to acknowlegde

          int n = 0;
          for (;;)
            {
              _referee_events->get (&event);
              if (event == WIN_LOSE_ACK_EVENT)
                if (++n == 2)
                  break;
            }
          break;
        }
    }
}

//-----------------------------------------------------------------------------

// "cpu_load" class.  Useful to evaluate how much CPU time is unused.

class cpu_load : public thread
  {
  public:

    cpu_load ();

  protected:

    void run ();
  };

cpu_load::cpu_load ()
{
}

void cpu_load::run ()
{
  uint32 n = 0;
  time next = add_time (current_time (), seconds_to_time (1));

  for (;;)
    {
      if (less_time (next, current_time ()))
        {
#ifdef SOLUTION
          seq->lock ();
#endif
          cout << n/1000 << " ";
#ifdef SOLUTION
          seq->unlock ();
#endif
          n = 0;
          next = add_time (current_time (), seconds_to_time (1));
        }
      else
        n++;
    }
}

//-----------------------------------------------------------------------------

int main ()
{
#ifdef SOLUTION
  seq = new mutex;
#endif

  cpu_load* t1 = new cpu_load;
  referee* t2 = new referee;

  t1->start ();
  t2->start ();

  t2->join ();

  return 0;
}

//-----------------------------------------------------------------------------

// Local Variables: //
// mode: C++ //
// End: //
