//
// Created by shawn on 25-1-19.
//

#pragma once

#include <cstdio>

// Precompiler define to get only filename;
#if defined(__FILE_NAME__)
#define ULOG_ERROR(fmt, ...) fprintf(stderr, "%s:%d " fmt "\n", __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#else
#define ULOG_ERROR(fmt, ...) fprintf(stderr, "%s:%d " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
