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
 * Name:	MicroEMACS
 *		Version stamp.
 * Version:	30
 * Last edit:	9-Feb-88
 * By:		Mark Alexander
 *		drivax!alexande
 *
 * This file contains the string(s)
 * that get written out by the show version command.
 * The string includes the date that this file was
 * compiled, so touch it when you want the date to change.
 * The string also includes the names of the optional
 * configured features (Ruby and PCRE2).
 */
#include	"def.h"
#include	"rev.h"

const char *version[] = {
  "MicroEMACS " DATE " " REV
#if USE_RUBY
  " (ruby)"
#endif
#if USE_PCRE2
  " (pcre2)"
#endif
  ,"Released under the terms of the GNU General Public Licence Version 3",
  NULL
};
