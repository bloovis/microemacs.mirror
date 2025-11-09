/*
    Copyright (C) 2008-2025 Mark Alexander

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

#include	"def.h"

/*
 * A video structure always holds
 * an array of characters whose length is equal to
 * the longest line possible. Only some of this is
 * used if "ncol" isn't the same as "NCOL".
 */
typedef struct
{
  short v_flag;			/* Flag word.                   */
  short v_color;		/* Color of the line.           */
  wchar_t v_text[NCOL];		/* The actual characters.       */
}
VIDEO;

/* Bits in VIDEO.v_flag.
 */
#define	VFCHG	0x0001		/* Changed.                     */

int sgarbf = TRUE;		/* TRUE if screen is garbage.   */
int vtrow = 0;			/* Virtual cursor row.          */
int vtcol = 0;			/* Virtual cursor column.       */
wchar_t *vttext;		/* &(vscreen[vtrow]->v_text[0]) */
int ttrow = HUGE;		/* Physical cursor row.         */
int ttcol = HUGE;		/* Physical cursor column.      */
int leftcol = 0;		/* Left column of window        */

/* Left column and number of columns for the line text portion
 * of screen rows, leaving room for an optional line number display.
 */
int tleftcol;
int tncol;

/*
 * The virtual screen, representing what we want the real
 * screen to look like.
 */
VIDEO *vscreen[NROW - 1];	/* Edge vector, virtual.        */
VIDEO video[NROW - 1];		/* Actual screen data.          */
static uchar spaces[NCOL];	/* ASCII spaces.		*/

/*
 * These variables are set by ttykbd.c when a mouse button is pressed.
 */
int mouse_button;		/* 0=left, 1=middle, 2=right	*/
int mouse_row;			/* screen row (0 = top)		*/
int mouse_column;		/* screen column (0 = left)	*/

/*
 * Forward declarations.
 */
static void vtputs (const uchar *s, int n);
static void uline (int row, VIDEO *vvp);
static void modeline (EWINDOW *wp);

/*
 * If showlinenumbers is TRUE, it means the user wants MicroEMACS to display
 * line numbers.  Set the starting column and number of columns for the text
 * portion of a line in the virtual text buffer.  We leave room for six
 * characters, to be used to display the line number.
 */
static void
setcolumns (void)
{
  if (showlinenumbers)
    {
      tleftcol = 6;
      tncol = ncol - 6;
    }
  else
    {
      tleftcol = 0;
      tncol = ncol;
    }
}

/*
 * Initialize the data structures used
 * by the display code. The edge vectors used
 * to access the screens are set up. The operating
 * system's terminal I/O channel is set up. Fill the
 * "spaces" array with ASCII blanks. The rest is done
 * at compile time. The original window is marked
 * as needing full update, and the physical screen
 * is marked as garbage, so all the right stuff happens
 * on the first call to redisplay.
 */
void
vtinit (void)
{
  VIDEO *vp;
  int i;

  ttopen ();
  ttinit ();
  setcolumns ();
  vp = &video[0];
  for (i = 0; i < NROW - 1; ++i)
    {
      vscreen[i] = vp;
      ++vp;
    }
  memset (spaces, ' ', NCOL);
}

/*
 * Tidy up the virtual display system
 * in anticipation of a return back to the host
 * operating system. Right now all we do is position
 * the cursor to the last line, erase the line, and
 * close the terminal channel.
 */
void
vttidy (void)
{
  ttcolor (CTEXT);
  ttnowindow ();		/* No scroll window.    */
  ttmove (nrow - 1, 0);		/* Echo line.           */
  tteeol ();
  tttidy ();
  ttflush ();
  ttclose ();
}

/*
 * Move the virtual cursor to an origin
 * 0 spot on the virtual display screen.
 * Also store a character pointer to the start
 * of the current line, which makes "vtputc" a little bit
 * more efficient. No checking for errors.
 */
static void
vtmove (int row, int col)
{
  vtrow = row;
  vtcol = col;
  vttext = &(vscreen[vtrow]->v_text[0]);
}

/*
 * Write a character to the virtual display,
 * dealing with long lines and the display of unprintable
 * things like control characters. Also expand tabs every 8
 * columns. This code only puts printing characters into 
 * the virtual display image. Special care must be taken when
 * expanding tabs. On a screen whose width is not a multiple
 * of 8, it is possible for the virtual cursor to hit the
 * right margin before the next tab stop is reached. This
 * makes the tab code loop if you are not careful.
 * Three guesses how we found this.  The variable leftcol
 * gives the left column number of the visible part of the window;
 * this is used to implement left and right scrolling.
 */
static void
vtputc (int c)
{
  if (vtcol >= leftcol + ncol)
    vttext[ncol - 1] = '$';
  else if (c == '\t')
    vtputs (spaces, tabsize - ((vtcol - tleftcol) % tabsize));
  else if (CISCTRL (c) != FALSE)
    {
      vtputc ('^');
      vtputc (c ^ 0x40);
    }
  else
    {
      if (vtcol >= leftcol)
	vttext[vtcol - leftcol] = c;
      vtcol++;
    }
}


/*
 * Write a string to the virtual display.  Essentially similar
 * to vtputc(), except that a string and count are the parameters
 * instead of a single character.  The variable leftcol
 * gives the left column number of the visible part of the window;
 * this is used to implement left and right scrolling.
 */
static void
vtputs (const uchar *s, int n)
{
  wchar_t c;
  int ulen;
  const uchar *end = s + n;

  while (s < end)
    {
      c = ugetc (s, 0, &ulen);		/* Get next Unicode character */
      s += ulen;
      if (vtcol >= leftcol + ncol)
	{
	  vttext[ncol - 1] = '$';
	  return;
	}
      else if (c == '\t')
	vtputs (spaces, tabsize - ((vtcol - tleftcol) % tabsize));
      else if (c < 0x80 && CISCTRL (c) != FALSE)
	{
	  vtputc ('^');
	  vtputc (c ^ 0x40);
	}
      else
	{
	  if (vtcol >= leftcol)
	    vttext[vtcol - leftcol] = c;
	  vtcol++;
	}
    }
}


/*
 * Put a null-terminated string out to the virtual screen.
 */
static void
vtstring (const char *s)
{
  vtputs ((const uchar *)s, strlen (s));
}

/*
 * Display a line number at the specified row.
 */
static void
vtputlineno (int row, int linenumber)
{
  static char buf[20];

  if (showlinenumbers)
    {
      snprintf(buf, sizeof (buf), "%5d│", linenumber + 1);
      vtmove (row, 0);
      vtstring (buf);
    }
}


/*
 * Erase from the end of the
 * software cursor to the end of the
 * line on which the software cursor is
 * located. The display routines will decide
 * if a hardware erase to end of line command
 * should be used to display this.
 */
static void
vteeol (void)
{
  int count;

  if (vtcol < leftcol)
    vtcol = leftcol;
  if ((count = ncol + leftcol - vtcol) <= 0)
    return;
  wmemset (&vttext[vtcol - leftcol], ' ', count);
  vtcol += count;
}

/*
 * Make sure that the display is
 * right. This is a three part process. First,
 * scan through all of the windows looking for dirty
 * ones. Check the framing, and refresh the screen.
 * Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the
 * virtual and physical screens the same.
 */
void
update (void)
{
  LINE *lp;
  EWINDOW *wp;
  VIDEO *vp;
  int i;
  int c;
  int curcol;
  int currow;
  uchar *s, *end;

  if (curmsgf != FALSE || newmsgf != FALSE)
    {
      ALLWIND (wp)		/* For all windows.     */
	wp->w_flag |= WFMODE;	/* Must do mode lines.  */
    }
  curmsgf = newmsgf;		/* Sync. up right now.  */

  /*
   * Find the column number of the cursor, taking tabs and UTF-8 into account.
   */
  curcol = 0;
  lp = curwp->w_dot.p;		/* The line containing the cursor. */
  s = lgets (lp);
  end = (uchar *) wlgetcptr (lp, curwp->w_dot.o);
  while (s < end)
    {
      int ulen;

      c = ugetc (s, 0, &ulen);
      s += ulen;
      if (c == '\t')
	curcol += (tabsize - curcol % tabsize);
      else if (c < 0x80 && CISCTRL (c) != FALSE)
	curcol += 2;
      else
	curcol += uwidth(c);
    }

  /* If the cursor column is outside what's currently visible on
   * the screen, adjust the current window's left column
   * to make it visible.  This will effectively produce a left
   * or right scroll.
   */
  if (curcol >= tncol + curwp->w_leftcol)
    {				/* need scroll right?   */
      curwp->w_leftcol = curcol - tncol / 2;
      curwp->w_flag |= WFHARD;	/* force redraw         */
    }
  else if (curcol < curwp->w_leftcol)
    {				/* need scroll left?    */
      if (curcol < tncol / 2)	/* near left end?       */
	curwp->w_leftcol = 0;	/* put left edge at 0   */
      else
	curwp->w_leftcol = curcol - tncol / 2;
      curwp->w_flag |= WFHARD;	/* force redraw         */
    }
  curcol -= curwp->w_leftcol;	/* adjust column        */

  /*
   * Examine all windows to see which ones need a screen update.
   */
  ALLWIND(wp)
    {
      if (wp->w_flag != 0)
	{			/* Need update.         */
	  BUFFER *bp = wp->w_bufp;
	  int linenumber = 0;

	  if ((wp->w_flag & WFFORCE) == 0)
	    {
	      /* If the dot is not visible, reframe this
	       * window so that it is visible.
	       */
	      lp = wp->w_linep;
	      for (i = 0; i < wp->w_ntrows; ++i)
		{
		  if (lp == wp->w_dot.p)
		    goto out;
		  if (lp == bp->b_linep)
		    break;
		  lp = lforw (lp);
		}
	    }

	  /* We must reframe the window.  First, check if
	   * the reposition-window command has set w_force to
	   * a non-zero value N, which means the dot should be
	   * shown on row N in the window. If w_force wasn't set,
	   * reposition the dot in the middle of the window.
	   */
	  i = wp->w_force;	/* Reframe this one.    */
	  if (i > 0)
	    {
	      --i;
	      if (i >= wp->w_ntrows)
		i = wp->w_ntrows - 1;
	    }
	  else if (i < 0)
	    {
	      i += wp->w_ntrows;
	      if (i < 0)
		i = 0;
	    }
	  else
	    i = wp->w_ntrows / 2;

	  /* We have set i to the row on which we want the dot
	   * to be shown in the window.  Given that, figure out
	   * which line should be shown at the top of the window.
	   */
	  lp = wp->w_dot.p;
	  while (i != 0 && lp != firstline (bp))
	    {
	      --i;
	      lp = lback (lp);
	    }
	  wp->w_linep = lp;
	  wp->w_flag |= WFHARD;	/* Force full.          */

	out:
	  /* We've reframed the window so that the dot is
	   * visible.  We can now do a reduced or full update.
	   * First, get the window's top line and calculate
	   * its line number if we're displaying line numbers.
	   * Set i to the row number of this line in the
	   * virtual screen
	   */
	  lp = wp->w_linep;
	  if (showlinenumbers)
	    linenumber = blineno (bp, lp);
	  i = wp->w_toprow;

	  /* If the window had a simple edit done to a single line,
	   * we only have to update the virtual screen for that line.
	   */
	  if ((wp->w_flag & ~WFMODE) == WFEDIT)
	    {
	      while (lp != wp->w_dot.p)
		{
		  ++i;
		  ++linenumber;
		  lp = lforw (lp);
		}
	      vscreen[i]->v_color = CTEXT;
	      vscreen[i]->v_flag |= VFCHG;
	      vtputlineno (i, linenumber);
	      leftcol = wp->w_leftcol;
	      vtmove (i, tleftcol);
	      vtputs (lgets (lp), llength (lp));
	      vteeol ();
	      leftcol = 0;
	    }

	  /* If the window requires a hard update, we must update
	   * every one of its lines in the virtual screen.
	   */
	  else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0)
	    {
	      /* Set leftcol to tell vtputc and vtputs the
	       * the left boundary column number of the 
	       * visible part of the window.
	       */
	      leftcol = wp->w_leftcol;

	      /* For each line in the visible part of the window,
	       * update the corresponding row in the virtual
	       * screen.
	       */
	      while (i < wp->w_toprow + wp->w_ntrows)
		{
		  vscreen[i]->v_color = CTEXT;
		  vscreen[i]->v_flag |= VFCHG;
		  vtmove (i, 0);
		  if (lp != bp->b_linep)
		    {
		      /* We haven't reached the end of the buffer.
		       * Write the line number (if enabled) to the virtual
		       * screen, then the text of the line itself.
		       */
		      vtputlineno (i, linenumber);
		      vtmove (i, tleftcol);
		      vtputs (lgets (lp), llength (lp));
		      lp = lforw (lp);
		      ++linenumber;
		    }
		  vteeol ();	/* Erase the rest of the row. */
		  ++i;
		}
	      leftcol = 0;
	    }

	  /* We've updated the virtual screen for this window.
	   * If the window requires a mode line change, write out
	   * an updated mode line to the virtual screen.
	   */
	  if ((wp->w_flag & WFMODE) != 0)
	    modeline (wp);
	  wp->w_flag = 0;
	  wp->w_force = 0;
	}
    }

  /* Figure out the row number of the cursor location.
   */
  lp = curwp->w_linep;
  currow = curwp->w_toprow;
  while (lp != curwp->w_dot.p)
    {
      ++currow;
      lp = lforw (lp);
    }

  if (sgarbf != FALSE)
    {
      /* The "screen is garbage" flag is set, so write out every
       * line in the virtual screen to the physical screen.
       */
      sgarbf = FALSE;		/* Erase-page clears    */
      epresf = FALSE;		/* the message area.    */
      ttcolor (CTEXT);		/* Force color change   */
      ttmove (0, 0);
      tteeop ();
      for (i = 0; i < nrow - 1; ++i)
	uline (i, vscreen[i]);
    }

  else
    {
      /* The screen is not all garbage.  Write out only
       * the changed lines to the physical screen.
       */
      for (i = 0; i < nrow - 1; ++i)
	{
	  vp = vscreen[i];
	  if ((vp->v_flag & VFCHG) != 0)
	    uline (i, vp);
	}
    }

  /* Finally move the cursor to its correct location,
   * and flush any pending output to the terminal.
   */
  ttmove (currow, curcol + tleftcol);
  ttflush ();
}

/*
 * Update a single line on the physical screen. This routine only
 * uses basic functionality (no insert and delete character,
 * but erase to end of line). The "vvp" points at the VIDEO
 * structure for the line on the virtual screen.  Avoid erase to end of
 * line when updating CMODE color lines, because of the way that
 * reverse video works on most terminals.
 */
static void
uline (int row, VIDEO *vvp)
{
  vvp->v_flag &= ~VFCHG;	/* Changes done.        */
  ttcolor (vvp->v_color);
  putline (row, 0, (const wchar_t *) &vvp->v_text[0]);
}

/*
 * Redisplay the mode line for
 * the window pointed to by the "wp".
 * This is the only routine that has any idea
 * of how the modeline is formatted. You can
 * change the modeline format by hacking at
 * this routine. Called by "update" any time
 * there is a dirty window.
 */
static void
modeline (EWINDOW *wp)
{
  BUFFER *bp;
  int n;
  const char *mname;

  n = wp->w_toprow + wp->w_ntrows;	/* Location.            */
  vtmove (n, 0);		/* Seek to right line.  */
  vscreen[n]->v_flag |= VFCHG;	/* Recompute, display.  */

  vscreen[n]->v_color = CMODE;	/* Mode line color.     */
  bp = wp->w_bufp;
  if ((bp->b_flag & BFCHG) != 0)	/* "*" if changed.      */
    vtputc ('*');
  else
    vtputc (' ');
  vtstring ("MicroEMACS");
  mname = modename (bp);
  if (mname != NULL)
    {
      vtstring (" (");
      vtstring (mname);
      vtputc (')');
    }
  if (bp->b_bname[0] != 0)
    {				/* Buffer name.         */
      vtputc (' ');
      vtstring (bp->b_bname);
    }
  if (bp->b_fname[0] != 0)
    {				/* File name.           */
      vtputc (' ');
      vtstring ("File:");
      vtstring (bp->b_fname);
    }
  if (curmsgf != FALSE		/* Message alert.       */
      && wp->w_wndp == NULL)
    {
      while (vtcol < ncol - 6)
	vtputc (' ');
      vtstring ("[Msg]");
    }
  vteeol ();			/* pad out with blanks  */
}

/*
 * Handle mouse button event.
 */
int
mouseevent (int f, int n, int k)
{
  eprintf ("[Mouse button %d, row %d, column %d]",
	   mouse_button, mouse_row, mouse_column);
  return (TRUE);
}

/*
 * Set the showlinenumbers variable to TRUE if the supplied argument
 * is non-zero, FALSE otherwise.  This variable tells MicroEMACS
 * if it should display line numbers.
 * 
 */
int
displines (int f, int n, int k)
{
  EWINDOW *wp;

  showlinenumbers = (n != 0);
  setcolumns();
  sgarbf = TRUE;
  ALLWIND (wp)		/* Redraw all.          */
    wp->w_flag |= WFMODE | WFHARD;
  update ();
  return TRUE;
}
