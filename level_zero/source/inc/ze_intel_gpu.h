/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
#if defined(__linux__)
#include <dlfcn.h>
#define HMODULE void *
#define MAKE_LIBRARY_NAME(NAME, VERSION) "lib" NAME ".so." VERSION
#define LOAD_DRIVER_LIBRARY(NAME) dlopen(NAME, RTLD_LAZY | RTLD_LOCAL)
#define FREE_DRIVER_LIBRARY(LIB) \
    if (LIB)                     \
    dlclose(LIB)
#define GET_FUNCTION_PTR(LIB, FUNC_NAME) dlsym(LIB, FUNC_NAME)
#elif defined(_WIN32)
#include <Windows.h>
#define MAKE_LIBRARY_NAME(NAME, VERSION) NAME VERSION ".dll"
#define LOAD_DRIVER_LIBRARY(NAME) LoadLibraryA(NAME)
#define FREE_DRIVER_LIBRARY(LIB) \
    if (LIB)                     \
    FreeLibrary(LIB)
#define GET_FUNCTION_PTR(LIB, FUNC_NAME) GetProcAddress((HMODULE)LIB, FUNC_NAME)
#else
#error "Unsupported OS"
#endif

///////////////////////////////////////////////////////////////////////////////
#if ZE_ENABLE_OCL_INTEROP
typedef struct _cl_mem *cl_mem;
typedef struct _cl_command_queue *cl_command_queue;
typedef struct _cl_context *cl_context;
typedef struct _cl_program *cl_program;
#endif

///////////////////////////////////////////////////////////////////////////////
inline bool getenv_tobool(const char *name) {
    const char *env = getenv(name);
    if ((nullptr == env) || (0 == strcmp("0", env)))
        return false;
    return (0 == strcmp("1", env));
}

#if defined(__linux__)
#define LOAD_INTEL_GPU_LIBRARY() LOAD_DRIVER_LIBRARY(MAKE_LIBRARY_NAME("ze_intel_gpu", "0.4"))
#elif defined(_WIN32)
#if _WIN64
#define LOAD_INTEL_GPU_LIBRARY() LOAD_DRIVER_LIBRARY(MAKE_LIBRARY_NAME("ze_intel_gpu", "64"))
#else
#define LOAD_INTEL_GPU_LIBRARY() LOAD_DRIVER_LIBRARY(MAKE_LIBRARY_NAME("ze_intel_gpu", "32"))
#endif
#endif