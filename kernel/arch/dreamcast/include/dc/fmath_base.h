/* KallistiOS ##version##

   dc/fmath_base.h
   Copyright (C) 2001 Andrew Kieschnick
   Copyright (C) 2014 Josh Pearson

*/

/**
    \file    dc/fmath_base.h
    \brief   Base definitions for the DC's special math instructions
    \ingroup math_intrinsics

    \author Andrew Kieschnick
    \author Josh Pearson
*/

#ifndef __DC_FMATH_BASE_H
#define __DC_FMATH_BASE_H

#include <arch/args.h>

#include <sys/cdefs.h>
__BEGIN_DECLS


/** \addtogroup math_intrinsics 
    @{
*/

/** \brief PI constant (if you don't want full math.h) */
#define F_PI 3.1415926f

/** \cond */
#define __fsin(x) \
    ({ _Complex float __value; \
       float __arg = (float)(x) * 10430.37835f; \
        __asm__("ftrc   %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "f" (__arg) \
                : "fpul"); \
        __real__ __value; })

#define __fcos(x) \
    ({ _Complex float __value; \
       float __arg = (float)(x) * 10430.37835f; \
        __asm__("ftrc   %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "f" (__arg) \
                : "fpul"); \
        __imag__ __value; })

#define __ftan(x) \
    ({ _Complex float __value; \
       float __arg = (float)(x) * 10430.37835f; \
        __asm__("ftrc   %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "f" (__arg) \
                : "fpul"); \
        __real__ __value / __imag__ __value; })

#define __fisin(x) \
    ({ _Complex float __value; \
       float __arg = (x); \
        __asm__("lds    %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "r" (__arg) \
                : "fpul"); \
        __real__ __value; })

#define __ficos(x) \
    ({ _Complex float __value; \
       float __arg = (x); \
        __asm__("lds    %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "r" (__arg) \
                : "fpul"); \
        __imag__ __value; })

#define __fitan(x) \
    ({ _Complex float __value; \
       float __arg = (x); \
        __asm__("lds    %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "r" (__arg) \
                : "fpul"); \
        __real__ __value / __imag__ __value; })

#define __fsincos(r, s, c) \
    ({ _Complex float __value; \
       float __arg = (r) * 182.04444443f; \
        __asm__("ftrc   %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "f" (__arg) \
                : "fpul"); \
        s = __real__ __value; c = __imag__ __value; })

#define __fsincosr(r, s, c) \
    ({ _Complex float __value; \
       float __arg = (r) * 10430.37835f; \
        __asm__("ftrc   %1,fpul\n\t" \
                "fsca   fpul,%0" \
                : "=f" (__value) \
                : "f" (__arg) \
                : "fpul"); \
        s = __real__ __value; c = __imag__ __value; })

#define __fsqrt(x) \
    ({ float __arg = (x); \
        __asm__("fsqrt %0\n\t" \
                : "=f" (__arg) : "0" (__arg)); \
        __arg; })

#define __frsqrt(x) \
    ({ float __arg = (x); \
        __asm__("fsrra %0\n\t" \
                : "=f" (__arg) : "0" (__arg)); \
        __arg; })

/* Floating point inner product (dot product) */
#define __fipr(x, y, z, w, a, b, c, d) ({ \
        register float __x __asm__(KOS_FPARG(0)) = (x); \
        register float __y __asm__(KOS_FPARG(1)) = (y); \
        register float __z __asm__(KOS_FPARG(2)) = (z); \
        register float __w __asm__(KOS_FPARG(3)) = (w); \
        register float __a __asm__(KOS_FPARG(4)) = (a); \
        register float __b __asm__(KOS_FPARG(5)) = (b); \
        register float __c __asm__(KOS_FPARG(6)) = (c); \
        register float __d __asm__(KOS_FPARG(7)) = (d); \
        __asm__ __volatile__( \
                              "fipr	fv8,fv4" \
                              : "+f" (KOS_SH4_SINGLE_ONLY ? __w : __z) \
                              : "f" (__x), "f" (__y), "f" (__z), "f" (__w), \
                              "f" (__a), "f" (__b), "f" (__c), "f" (__d) \
                            ); \
        KOS_SH4_SINGLE_ONLY ? __w : __z; })

/* Floating point inner product w/self (square of vector magnitude) */
#define __fipr_magnitude_sqr(x, y, z, w) ({ \
        register float __x __asm__(KOS_FPARG(0)) = (x); \
        register float __y __asm__(KOS_FPARG(1)) = (y); \
        register float __z __asm__(KOS_FPARG(2)) = (z); \
        register float __w __asm__(KOS_FPARG(3)) = (w); \
        __asm__ __volatile__( \
                              "fipr	fv4,fv4" \
                              : "+f" (KOS_SH4_SINGLE_ONLY ? __w : __z) \
                              : "f" (__x), "f" (__y), "f" (__z), "f" (__w) \
                            ); \
        KOS_SH4_SINGLE_ONLY ? __w : __z; })

/** \endcond */

/** @} */

__END_DECLS

#endif /* !__DC_FMATH_BASE_H */
