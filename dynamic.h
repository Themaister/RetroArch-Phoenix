/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DYNAMIC_H
#define __DYNAMIC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *dylib_t;
typedef void (*function_t)(void);

dylib_t dylib_load(const char *path);
void dylib_close(dylib_t lib);
function_t dylib_proc(dylib_t lib, const char *proc);

#ifdef __cplusplus
}
#endif

#endif

