/*
    Copyright (C) 2008 Mark Alexander

    This file is part of MicroEMACS, a small text editor.

    MicroEMACS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Name:        MicroEMACS
 *              Ncurses text-mode display
 * By:      	Mark Alexander
 *              marka@pobox.com
 *
 * $Log: tty.c,v $
 * Revision 1.2  2005-10-19 02:06:27  bloovis
 * (putline): Fix off-by-one bug.
 *
 * Revision 1.1  2005/10/18 02:18:44  bloovis
 * New files to implement ncurses screen handling.
 *
 *
 */

#include "def.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include	<ncurses.h>
#else
#include <ncursesw/ncurses.h>
#endif
#include <unistd.h>

extern  int     tttop;
extern  int     ttbot;
extern  int     tthue;

/*
 * Local variables.
 */

/*
 * Initialize the terminal.  On other terminal types, we would
 * get the handles for console input and output, and peek at the video
 * buffer to see what video attributes were being used.  With Curses,
 * we only need to get the current terminal size.
 */
void
ttinit (void)
{
  ttgetsize ();
}

/*
 * Curses needs no tidy up.
 */
void
tttidy (void)
{
}

/*
 * Move the cursor to the specified
 * origin 0 row and column position. Try to
 * optimize out extra moves; redisplay may
 * have left the cursor in the right
 * location last time!
 */
void
ttmove (int row, int col)
{
  move (row, col);
  curfp->f_ttrow = row;
  curfp->f_ttcol = col;
}

/*
 * Erase to end of line.
 */
void
tteeol (void)
{
  clrtoeol ();
}

/*
 * Erase to end of page.
 */
void
tteeop (void)
{
  clrtobot ();
}

/*
 * Make a noise.
 */

void
ttbeep (void)
{
  if (write (1,"\007",1) != 1)
    {}	/* suppress warning about "ignoring return value of write" */
}

/*
 * No-op.
 */
void
ttwindow (int top, int bot)
{
}

/*
 * No-op.
 */
void
ttnowindow (void)
{
}

/*
 * Set display color.  Just convert MicroEMACS
 * color to ncurses background attribute.
 */
void
ttcolor (int color)
{
  bkgdset (' ' | (color == CMODE ? A_REVERSE : A_NORMAL));
}

/*
 * This routine is called by the
 * "refresh the screen" command to try and resize
 * the display. The new size, which must be deadstopped
 * to not exceed the NROW and NCOL limits, it stored
 * back into "nrow" and "ncol". Display can always deal
 * with a screen NROW by NCOL. Look in "window.c" to
 * see how the caller deals with a change.
 */
void
ttresize (void)
{
  ttgetsize ();		/* found in "ttyio.c",  */
  ttinit ();
  wrefresh (curscr);
}

/*
 * High speed screen update.  row and col are 0-based.
 */
void
ttputline (int row, int col, const wchar_t *buf)
{
  /* Write line text to screen.
   */
  move (row, col);
  ttputs (buf, curfp->f_ncol - col);
}
