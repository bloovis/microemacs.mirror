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

/* Compile this program using: gcc -o tcap tcap.c -lncursesw */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curses.h>
#include <term.h>

#define TCAPSLEN 1024

char tcapbuf[TCAPSLEN];
static char tcbuf[1024];

void
printesc (char *p)
{
  char c;
  while ((c = *p++) != '\0')
    if (c < ' ')
      printf ("^%c", c + '@');
    else
      printf ("%c", c);
}

int
main (int argc, char *argv[])
{
  char *tv_stype, *p, *t;
  int i, n;

  if ((tv_stype = getenv ("TERM")) == NULL)	/* Don't want VAX C getenv() */
    {
      printf ("Environment variable TERM not defined!");
      return 1;
    }

  if ((tgetent (tcbuf, tv_stype)) != 1)
    {
      printf ("Unknown terminal type %s\n", tv_stype);
      return 1;
    }

  p = tcapbuf;
  for (i = 1; i < argc; i++)
    {
      const char *arg = argv[i];

      /* Try string capability. */
      t = tgetstr (arg, &p);
      if (t == NULL)
	printf ("%s (string) not defined\n", arg);
      else
	{
	  printf ("%s (string): ", arg);
	  printesc (t);
	  printf ("\n");
	}

      /* Try numeric capability. */
      n = tgetnum (arg);
      if (n == -1)
	printf ("%s (numeric) not defined\n", arg);
      else
	printf ("%s (numeric): %d\n", arg, n);

      /* Try boolean capability. */
      n = tgetflag (arg);
      if (n == 0)
	printf ("%s (boolean) not defined\n", arg);
      else
	printf ("%s (boolean): %d\n", arg, n);
      
    }
  return 0;
}
