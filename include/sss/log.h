/*
 * Created on Tue Feb 04 2025
 *
 * Author: Peter Gusev
 * Copyright (c) 2025 Peter Gusev. All rights reserved.
 */

#ifndef __LOG_H__
#define __LOG_H__

#ifdef SSS_DEBUG_LOG

#include <stdio.h>

#define LOG_DEBUG(fmt, ...)                  \
    {                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "\n");               \
    }
#else
#define LOG_DEBUG(fmt, ...)
#endif

#endif