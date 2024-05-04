#pragma once

// #define __TRACE__

#define __TRACE_ALLOC__

static int trace_indent = 0;

#ifdef __TRACE__
    #define TRACE(expr) ({ \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        printf("*>call %s from %s %s:%d\n", #expr, __func__, __FILENAME__, __LINE__); \
        trace_indent++; \
        void* out = expr; \
        trace_indent--; \
        for (int i = 0;i < trace_indent;i++) { printf("| "); } \
        printf("*<exit %s from %s %s:%d\n", #expr, __func__, __FILENAME__, __LINE__); \
        out; \
    })
#else
    #define TRACE(expr) expr
#endif

#ifdef __TRACE_ALLOC__
    #include <stdlib.h>
    #include <stdio.h>
    
    #define malloc(thing) ({ \
        usize size = thing; \
        void* ptr = malloc(size); \
        FILE *memtrace = fopen("MEMTRACE.txt", "a"); \
        fprintf(memtrace, "MALLOC %s (%lld) @ %p | %s:%d\n", #thing, size, ptr, __FILENAME__, __LINE__); \
        fclose(memtrace); \
        ptr; \
    })
    #define realloc(addr, size) ({ \
        usize s = size; \
        void* addr_after = realloc(addr, s); \
        FILE *memtrace = fopen("MEMTRACE.txt", "a"); \
        fprintf(memtrace, "REALLOC %s @ %p with %s (%lld) -> @ %p | %s:%d\n", #addr, addr, #size, s, addr_after, __FILENAME__, __LINE__); \
        fclose(memtrace); \
        addr_after; \
    })
    #define free(addr) ({ \
        FILE *memtrace = fopen("MEMTRACE.txt", "a"); \
        fprintf(memtrace, "FREE %s @ %p | %s:%d\n", #addr, addr, __FILENAME__, __LINE__); \
        fclose(memtrace); \
        free(addr); \
    })
#endif

#ifndef __WIN32__
    #define __WIN32__
#endif
#ifndef __x86_64__
    #define __x86_64__
#endif
#ifndef __LITTLE_ENDIAN__
    #define __LITTLE_ENDIAN__
#endif

#ifdef __WIN32__
    #define __PATH_SEP__ '\\'
#else
    #define __PATH_SEP__ '/'
#endif

#define __FILENAME__ (strrchr(__FILE__, __PATH_SEP__) ? strrchr(__FILE__, __PATH_SEP__) + 1 : __FILE__)

#ifdef __x86_64__
    #define __TARGET_BITS__ 64
    typedef signed long long int isize;
typedef unsigned long long int usize;
#else
    #define __TARGET_BITS__ 32
    typedef signed long int isize;
typedef unsigned long int usize;
#endif

typedef signed char i8;
typedef signed short int i16;
typedef signed long int i32;
typedef signed long long int i64;

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;
typedef unsigned long long int u64;

typedef float f32;
typedef long f64;

typedef u8 bool;
#define true 1
#define false 0

typedef char* str;
typedef void* any;

// MANUAL
#ifdef __LITTLE_ENDIAN__
    #define __ENDIANESS__ "little"
#else
    #define __ENDIUANESS__ "big"
#endif