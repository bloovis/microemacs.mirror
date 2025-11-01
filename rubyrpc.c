/*
    Copyright (C) 2025 Mark Alexander

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

int
rubyinit (int quiet)
{
  eprintf ("[rubyinit for RPC not implemented]");
  return FALSE;
}

const char *
rubyerror (void)
{
  return "rubyerror not implemented";
}

int
rubyloadscript (const char *path)
{
  eprintf ("[rubyloadscript for RPC not implemented]");
  return FALSE;
}

int
runruby (const char * line)
{
  eprintf ("[runruby for RPC not implemented]");
  return FALSE;
}

int
rubycall (const char *name, int f, int n)
{
  eprintf ("[rubycall for RPC not implemented]");
  return FALSE;
}
