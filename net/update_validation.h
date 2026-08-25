#pragma once

#include <sys/types.h>

// Real Web-888 FPGA images are about 1.2 MiB. Reject short downloads such as
// HTTP error pages before they can replace a boot-critical bitstream.
static const off_t UPDATE_FPGA_MIN_BYTES = 500 * 1024;

bool update_fpga_file_valid(const char* path, off_t* size_out = nullptr);
