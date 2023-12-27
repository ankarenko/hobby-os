#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#include "kernel/util/stdio.h"
#include "kernel/cpu/hal.h"
#include "kernel/util/ansi_codes.h"

#define KERNEL_DEBUG 1

enum debug_level {
	DEBUG_TRACE = 0,
	DEBUG_INFO = 1,
	DEBUG_WARNING = 2,
	DEBUG_ERROR = 3,
	DEBUG_FATAL = 4,
};

#ifdef KERNEL_DEBUG
#define log(...) __dbg(DEBUG_INFO, false, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define err(...) __dbg(DEBUG_ERROR, false, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define warn(...) __dbg(DEBUG_WARNING, false, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define log(...) ((void)0)
#define err(...) ((void)0)
#define warn(...) ((void)0)
#endif

// NOTE: MQ 2020-11-15
// from https://stackoverflow.com/a/11320159/1349340
// calculate number of arguments __VA_ARGS__
#define PP_ARG_N(                                     \
	_1, _2, _3, _4, _5, _6, _7, _8, _9, _10,          \
	_11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
	_21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
	_31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
	_41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
	_51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
	_61, _62, _63, N, ...) N
#define PP_RSEQ_N()                             \
	62, 61, 60,                                 \
		59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
		49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
		39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
		29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
		19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
		9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_NARG(...) PP_NARG_(_, ##__VA_ARGS__, PP_RSEQ_N())

#define __with_fmt(func, default_fmt, ...) (PP_NARG(__VA_ARGS__) == 0 ? func(default_fmt) : func(__VA_ARGS__))
#define assert(expression, ...) ((expression)  \
									 ? (void)0 \
									 : (void)({ __with_fmt(err, "expression " #expression " is falsy", ##__VA_ARGS__); __asm__ __volatile("int $0x01"); }))
#define assert_not_reached(...) ({ __with_fmt(err, "should not be reached", ##__VA_ARGS__); __asm__ __volatile__("int $0x01"); })
#define assert_not_implemented(...) ({ __with_fmt(err, "is not implemented", ##__VA_ARGS__); __asm__ __volatile__("int $0x01"); })

void debug_init();

#if UNIT_TEST
#define unit_static 
#else
#define unit_static
#endif

#endif