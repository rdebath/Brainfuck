/******************************** -*- C -*- ****************************
 *
 *	lightning main include file
 *
 ***********************************************************************/


/***********************************************************************
 *
 * Copyright 2000 Free Software Foundation, Inc.
 * Written by Paolo Bonzini.
 *
 * This file is part of GNU lightning.
 *
 * GNU lightning is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1, or (at your option)
 * any later version.
 * 
 * GNU lightning is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with GNU lightning; see the file COPYING.LESSER; if not, write to the
 * Free Software Foundation, 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 ***********************************************************************/

#ifndef __lightning_h
#define __lightning_h

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__) || defined(__i386) || defined(_M_IX86)
#include <lightning/asm-common.h>

#ifndef LIGHTNING_DEBUG
#include <lightning/i386/asm.h>
#endif

#include <lightning/i386/core.h>
#include <lightning/core-common.h>
#include <lightning/i386/funcs.h>
#include <lightning/funcs-common.h>
#include <lightning/i386/fp.h>
#include <lightning/fp-common.h>
#endif

#if (defined(__powerpc__) || defined(__PPC__)) && defined(__BIG_ENDIAN__) && !defined(__PPC64__)
#include <lightning/asm-common.h>

#ifndef LIGHTNING_DEBUG
#include <lightning/ppc/asm.h>
#endif

#include <lightning/ppc/core.h>
#include <lightning/core-common.h>
#include <lightning/ppc/funcs.h>
#include <lightning/funcs-common.h>
#include <lightning/ppc/fp.h>
#include <lightning/fp-common.h>
#endif

#if defined(__sparc__)
#include <lightning/asm-common.h>

#ifndef LIGHTNING_DEBUG
#include <lightning/sparc/asm.h>
#endif

#include <lightning/sparc/core.h>
#include <lightning/core-common.h>
#include <lightning/sparc/funcs.h>
#include <lightning/funcs-common.h>
#include <lightning/sparc/fp.h>
#include <lightning/fp-common.h>
#endif

#if !defined(JIT_R0)
#include <lightning/asm-common.h>

#ifndef LIGHTNING_DEBUG
#include <lightning/asm.h>
#endif

#include <lightning/core.h>
#include <lightning/core-common.h>
#include <lightning/funcs.h>
#include <lightning/funcs-common.h>
#include <lightning/fp.h>
#include <lightning/fp-common.h>

#ifndef JIT_R0
#error GNU lightning does not support the current target
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __lightning_h */
