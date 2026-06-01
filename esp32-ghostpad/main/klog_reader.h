#pragma once
#include <stdint.h>
#include <stddef.h>

#define KLOG_PORT 3232

int klog_reader_start(const char *path, uint16_t port);
