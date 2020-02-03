/*-
 * Copyright (c) 2013 Andrew Turner <andrew@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_ATOMIC_H_
#define	_MACHINE_ATOMIC_H_

#define	isb()		__asm __volatile("isb" : : : "memory")

/*
 * Options for DMB and DSB:
 *	oshld	Outer Shareable, load
 *	oshst	Outer Shareable, store
 *	osh	Outer Shareable, all
 *	nshld	Non-shareable, load
 *	nshst	Non-shareable, store
 *	nsh	Non-shareable, all
 *	ishld	Inner Shareable, load
 *	ishst	Inner Shareable, store
 *	ish	Inner Shareable, all
 *	ld	Full system, load
 *	st	Full system, store
 *	sy	Full system, all
 */
#define	dsb(opt)	__asm __volatile("dsb " __STRING(opt) : : : "memory")
#define	dmb(opt)	__asm __volatile("dmb " __STRING(opt) : : : "memory")

#define	mb()	dmb(sy)	/* Full system memory barrier all */
#define	wmb()	dmb(st)	/* Full system memory barrier store */
#define	rmb()	dmb(ld)	/* Full system memory barrier load */

#if defined(KCSAN) && !defined(KCSAN_RUNTIME)
#include <sys/_cscan_atomic.h>
#else

#include <sys/atomic_common.h>

#define	_ATOMIC_OP_IMPL(t, w, s, op, asm_op, bar, a, l)			\
static __inline void							\
atomic_##op##_##bar##t(volatile uint##t##_t *p, uint##t##_t val)	\
{									\
	uint##t##_t tmp;						\
	int res;							\
									\
	__asm __volatile(						\
	    "1: ld"#a"xr"#s"	%"#w"0, [%2]\n"				\
	    "   "#asm_op"	%"#w"0, %"#w"0, %"#w"3\n"		\
	    "   st"#l"xr"#s"	%w1, %"#w"0, [%2]\n"			\
	    "   cbnz		%w1, 1b\n"				\
	    : "=&r"(tmp), "=&r"(res)					\
	    : "r" (p), "r" (val)					\
	    : "memory"							\
	);								\
}

#define	__ATOMIC_OP(op, asm_op, bar, a, l)				\
	_ATOMIC_OP_IMPL(8,  w, b, op, asm_op, bar, a, l)		\
	_ATOMIC_OP_IMPL(16, w, h, op, asm_op, bar, a, l)		\
	_ATOMIC_OP_IMPL(32, w,  , op, asm_op, bar, a, l)		\
	_ATOMIC_OP_IMPL(64,  ,  , op, asm_op, bar, a, l)

#define	_ATOMIC_OP(op, asm_op)						\
	__ATOMIC_OP(op, asm_op,     ,  ,  )				\
	__ATOMIC_OP(op, asm_op, acq_, a,  )				\
	__ATOMIC_OP(op, asm_op, rel_,  , l)

_ATOMIC_OP(add,      add)
_ATOMIC_OP(clear,    bic)
_ATOMIC_OP(set,      orr)
_ATOMIC_OP(subtract, sub)

#define	_ATOMIC_CMPSET_IMPL(t, w, s, bar, a, l)				\
static __inline int							\
atomic_cmpset_##bar##t(volatile uint##t##_t *p, uint##t##_t cmpval,	\
    uint##t##_t newval)							\
{									\
	uint##t##_t tmp;						\
	int res;							\
									\
	__asm __volatile(						\
	    "1: mov		%w1, #1\n"				\
	    "   ld"#a"xr"#s"	%"#w"0, [%2]\n"				\
	    "   cmp		%"#w"0, %"#w"3\n"			\
	    "   b.ne		2f\n"					\
	    "   st"#l"xr"#s"	%w1, %"#w"4, [%2]\n"			\
	    "   cbnz		%w1, 1b\n"				\
	    "2:"							\
	    : "=&r"(tmp), "=&r"(res)					\
	    : "r" (p), "r" (cmpval), "r" (newval)			\
	    : "cc", "memory"						\
	);								\
									\
	return (!res);							\
}									\
									\
static __inline int							\
atomic_fcmpset_##bar##t(volatile uint##t##_t *p, uint##t##_t *cmpval,	\
    uint##t##_t newval)		 					\
{									\
	uint##t##_t _cmpval, tmp;					\
	int res;							\
									\
	_cmpval = *cmpval;						\
	__asm __volatile(						\
	    "   mov		%w1, #1\n"				\
	    "   ld"#a"xr"#s"	%"#w"0, [%2]\n"				\
	    "   cmp		%"#w"0, %"#w"3\n"			\
	    "   b.ne		1f\n"					\
	    "   st"#l"xr"#s"	%w1, %"#w"4, [%2]\n"			\
	    "1:"							\
	    : "=&r"(tmp), "=&r"(res)					\
	    : "r" (p), "r" (_cmpval), "r" (newval)			\
	    : "cc", "memory"						\
	);								\
	*cmpval = tmp;							\
									\
	return (!res);							\
}

#define	_ATOMIC_CMPSET(bar, a, l)					\
	_ATOMIC_CMPSET_IMPL(8,  w, b, bar, a, l)			\
	_ATOMIC_CMPSET_IMPL(16, w, h, bar, a, l)			\
	_ATOMIC_CMPSET_IMPL(32, w,  , bar, a, l)			\
	_ATOMIC_CMPSET_IMPL(64,  ,  , bar, a, l)

_ATOMIC_CMPSET(    ,  , )
_ATOMIC_CMPSET(acq_, a, )
_ATOMIC_CMPSET(rel_,  ,l)

#define	_ATOMIC_FETCHADD_IMPL(t, w)					\
static __inline uint##t##_t						\
atomic_fetchadd_##t(volatile uint##t##_t *p, uint##t##_t val)		\
{									\
	uint##t##_t tmp, ret;						\
	int res;							\
									\
	__asm __volatile(						\
	    "1: ldxr	%"#w"2, [%3]\n"					\
	    "   add	%"#w"0, %"#w"2, %"#w"4\n"			\
	    "   stxr	%w1, %"#w"0, [%3]\n"				\
            "   cbnz	%w1, 1b\n"					\
	    : "=&r" (tmp), "=&r" (res), "=&r" (ret)			\
	    : "r" (p), "r" (val)					\
	    : "memory"							\
	);								\
									\
	return (ret);							\
}

_ATOMIC_FETCHADD_IMPL(32, w)
_ATOMIC_FETCHADD_IMPL(64,  )

#define	_ATOMIC_SWAP_IMPL(t, w, zreg)					\
static __inline uint##t##_t						\
atomic_swap_##t(volatile uint##t##_t *p, uint##t##_t val)		\
{									\
	uint##t##_t ret;						\
	int res;							\
									\
	__asm __volatile(						\
	    "1: ldxr	%"#w"1, [%2]\n"					\
	    "   stxr	%w0, %"#w"3, [%2]\n"				\
            "   cbnz	%w0, 1b\n"					\
	    : "=&r" (res), "=&r" (ret)					\
	    : "r" (p), "r" (val)					\
	    : "memory"							\
	);								\
									\
	return (ret);							\
}									\
									\
static __inline uint##t##_t						\
atomic_readandclear_##t(volatile uint##t##_t *p)			\
{									\
	uint##t##_t ret;						\
	int res;							\
									\
	__asm __volatile(						\
	    "1: ldxr	%"#w"1, [%2]\n"					\
	    "   stxr	%w0, "#zreg", [%2]\n"				\
	    "   cbnz	%w0, 1b\n"					\
	    : "=&r" (res), "=&r" (ret)					\
	    : "r" (p)							\
	    : "memory"							\
	);								\
									\
	return (ret);							\
}

_ATOMIC_SWAP_IMPL(32, w, wzr)
_ATOMIC_SWAP_IMPL(64,  , xzr)

#define	_ATOMIC_TEST_OP_IMPL(t, w, op, asm_op)				\
static __inline int							\
atomic_testand##op##_##t(volatile uint##t##_t *p, u_int val)		\
{									\
	uint##t##_t mask, old, tmp;					\
	int res;							\
									\
	mask = 1u << (val & 0x1f);					\
	__asm __volatile(						\
	    "1: ldxr		%"#w"2, [%3]\n"				\
	    "  "#asm_op"	%"#w"0, %"#w"2, %"#w"4\n"		\
	    "   stxr		%w1, %"#w"0, [%3]\n"			\
	    "   cbnz		%w1, 1b\n"				\
	    : "=&r" (tmp), "=&r" (res), "=&r" (old)			\
	    : "r" (p), "r" (mask)					\
	    : "memory"							\
	);								\
									\
	return ((old & mask) != 0);					\
}

#define	_ATOMIC_TEST_OP(op, asm_op)					\
	_ATOMIC_TEST_OP_IMPL(32, w, op, asm_op)				\
	_ATOMIC_TEST_OP_IMPL(64,  , op, asm_op)

_ATOMIC_TEST_OP(clear, bic)
_ATOMIC_TEST_OP(set,   orr)

#define	_ATOMIC_LOAD_ACQ_IMPL(t, w, s)					\
static __inline uint##t##_t						\
atomic_load_acq_##t(volatile uint##t##_t *p)				\
{									\
	uint##t##_t ret;						\
									\
	__asm __volatile(						\
	    "ldar"#s"	%"#w"0, [%1]\n"					\
	    : "=&r" (ret)						\
	    : "r" (p)							\
	    : "memory");						\
									\
	return (ret);							\
}

_ATOMIC_LOAD_ACQ_IMPL(8,  w, b)
_ATOMIC_LOAD_ACQ_IMPL(16, w, h)
_ATOMIC_LOAD_ACQ_IMPL(32, w,  )
_ATOMIC_LOAD_ACQ_IMPL(64,  ,  )

#define	_ATOMIC_STORE_REL_IMPL(t, w, s)					\
static __inline void							\
atomic_store_rel_##t(volatile uint##t##_t *p, uint##t##_t val)		\
{									\
	__asm __volatile(						\
	    "stlr"#s"	%"#w"0, [%1]\n"					\
	    :								\
	    : "r" (val), "r" (p)					\
	    : "memory");						\
}

_ATOMIC_STORE_REL_IMPL(8,  w, b)
_ATOMIC_STORE_REL_IMPL(16, w, h)
_ATOMIC_STORE_REL_IMPL(32, w,  )
_ATOMIC_STORE_REL_IMPL(64,  ,  )

#define	atomic_add_int			atomic_add_32
#define	atomic_fcmpset_int		atomic_fcmpset_32
#define	atomic_clear_int		atomic_clear_32
#define	atomic_cmpset_int		atomic_cmpset_32
#define	atomic_fetchadd_int		atomic_fetchadd_32
#define	atomic_readandclear_int		atomic_readandclear_32
#define	atomic_set_int			atomic_set_32
#define	atomic_swap_int			atomic_swap_32
#define	atomic_subtract_int		atomic_subtract_32
#define	atomic_testandclear_int		atomic_testandclear_32
#define	atomic_testandset_int		atomic_testandset_32

#define	atomic_add_acq_int		atomic_add_acq_32
#define	atomic_fcmpset_acq_int		atomic_fcmpset_acq_32
#define	atomic_clear_acq_int		atomic_clear_acq_32
#define	atomic_cmpset_acq_int		atomic_cmpset_acq_32
#define	atomic_load_acq_int		atomic_load_acq_32
#define	atomic_set_acq_int		atomic_set_acq_32
#define	atomic_subtract_acq_int		atomic_subtract_acq_32

#define	atomic_add_rel_int		atomic_add_rel_32
#define	atomic_fcmpset_rel_int		atomic_fcmpset_rel_32
#define	atomic_clear_rel_int		atomic_clear_rel_32
#define	atomic_cmpset_rel_int		atomic_cmpset_rel_32
#define	atomic_set_rel_int		atomic_set_rel_32
#define	atomic_subtract_rel_int		atomic_subtract_rel_32
#define	atomic_store_rel_int		atomic_store_rel_32

#define	atomic_add_long			atomic_add_64
#define	atomic_fcmpset_long		atomic_fcmpset_64
#define	atomic_clear_long		atomic_clear_64
#define	atomic_cmpset_long		atomic_cmpset_64
#define	atomic_fetchadd_long		atomic_fetchadd_64
#define	atomic_readandclear_long	atomic_readandclear_64
#define	atomic_set_long			atomic_set_64
#define	atomic_swap_long		atomic_swap_64
#define	atomic_subtract_long		atomic_subtract_64
#define	atomic_testandclear_long	atomic_testandclear_64
#define	atomic_testandset_long		atomic_testandset_64

#define	atomic_add_ptr			atomic_add_64
#define	atomic_fcmpset_ptr		atomic_fcmpset_64
#define	atomic_clear_ptr		atomic_clear_64
#define	atomic_cmpset_ptr		atomic_cmpset_64
#define	atomic_fetchadd_ptr		atomic_fetchadd_64
#define	atomic_readandclear_ptr		atomic_readandclear_64
#define	atomic_set_ptr			atomic_set_64
#define	atomic_swap_ptr			atomic_swap_64
#define	atomic_subtract_ptr		atomic_subtract_64

#define	atomic_add_acq_long		atomic_add_acq_64
#define	atomic_fcmpset_acq_long		atomic_fcmpset_acq_64
#define	atomic_clear_acq_long		atomic_clear_acq_64
#define	atomic_cmpset_acq_long		atomic_cmpset_acq_64
#define	atomic_load_acq_long		atomic_load_acq_64
#define	atomic_set_acq_long		atomic_set_acq_64
#define	atomic_subtract_acq_long	atomic_subtract_acq_64

#define	atomic_add_acq_ptr		atomic_add_acq_64
#define	atomic_fcmpset_acq_ptr		atomic_fcmpset_acq_64
#define	atomic_clear_acq_ptr		atomic_clear_acq_64
#define	atomic_cmpset_acq_ptr		atomic_cmpset_acq_64
#define	atomic_load_acq_ptr		atomic_load_acq_64
#define	atomic_set_acq_ptr		atomic_set_acq_64
#define	atomic_subtract_acq_ptr		atomic_subtract_acq_64

#define	atomic_add_rel_long		atomic_add_rel_64
#define	atomic_fcmpset_rel_long		atomic_fcmpset_rel_64
#define	atomic_clear_rel_long		atomic_clear_rel_64
#define	atomic_cmpset_rel_long		atomic_cmpset_rel_64
#define	atomic_set_rel_long		atomic_set_rel_64
#define	atomic_subtract_rel_long	atomic_subtract_rel_64
#define	atomic_store_rel_long		atomic_store_rel_64

#define	atomic_add_rel_ptr		atomic_add_rel_64
#define	atomic_fcmpset_rel_ptr		atomic_fcmpset_rel_64
#define	atomic_clear_rel_ptr		atomic_clear_rel_64
#define	atomic_cmpset_rel_ptr		atomic_cmpset_rel_64
#define	atomic_set_rel_ptr		atomic_set_rel_64
#define	atomic_subtract_rel_ptr		atomic_subtract_rel_64
#define	atomic_store_rel_ptr		atomic_store_rel_64

static __inline void
atomic_thread_fence_acq(void)
{

	dmb(ld);
}

static __inline void
atomic_thread_fence_rel(void)
{

	dmb(sy);
}

static __inline void
atomic_thread_fence_acq_rel(void)
{

	dmb(sy);
}

static __inline void
atomic_thread_fence_seq_cst(void)
{

	dmb(sy);
}

#endif /* KCSAN && !KCSAN_RUNTIME */

#endif /* _MACHINE_ATOMIC_H_ */
