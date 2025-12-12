/* KallistiOS ##version##

   arch/aica/include/types.h
   (c)2000-2001 Megan Potter

*/

/** \file    arch/types.h
    \brief   Common integer types.
    \ingroup system_types

    This file contains typedefs for some common/useful integer types. These
    types include ones that tell you exactly how long they are, as well as some
    BSD-isms.

    \author Megan Potter
*/

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <stddef.h>
#include <stdint.h>

/** \addtogroup system_types
    @{
*/

/* Generic types */
typedef uint64_t uint64;            /**< \brief 64-bit unsigned integer */
typedef uint32_t uint32;            /**< \brief 32-bit unsigned integer */
typedef uint16_t uint16;            /**< \brief 16-bit unsigned integer */
typedef uint8_t uint8;              /**< \brief 8-bit unsigned integer */
typedef int64_t int64;              /**< \brief 64-bit signed integer */
typedef int32_t int32;              /**< \brief 32-bit signed integer */
typedef int16_t int16;              /**< \brief 16-bit signed integer */
typedef int8_t int8;                /**< \brief 8-bit signed integer */

/* Volatile types */
typedef volatile uint64_t vuint64;  /**< \brief 64-bit volatile unsigned type */
typedef volatile uint32_t vuint32;  /**< \brief 32-bit volatile unsigned type */
typedef volatile uint16_t vuint16;  /**< \brief 16-bit volatile unsigned type */
typedef volatile uint8_t vuint8;    /**< \brief 8-bit volatile unsigned type */
typedef volatile int64_t vint64;    /**< \brief 64-bit volatile signed type */
typedef volatile int32_t vint32;    /**< \brief 32-bit volatile signed type */
typedef volatile int16_t vint16;    /**< \brief 16-bit volatile signed type */
typedef volatile int8_t vint8;      /**< \brief 8-bit volatile signed type */

/* This type may be used for any generic handle type that is allowed
   to be negative (for errors) and has no specific bit count
   restraints. */
typedef int handle_t;               /**< \brief Generic "handle" type */

/* Thread and priority types */
typedef handle_t tid_t;             /**< \brief Thread ID type */
typedef handle_t prio_t;            /**< \brief Priority value type */

#ifndef BYTE_ORDER
/* Make sure to pull in the base endianness defines... */
#ifndef LITTLE_ENDIAN
#include <sys/_types.h>
#endif

/** \brief  Endianness definition -- Little Endian */
#define BYTE_ORDER  LITTLE_ENDIAN
#endif

/** @} */

__END_DECLS

#endif  /* __ARCH_TYPES_H */
