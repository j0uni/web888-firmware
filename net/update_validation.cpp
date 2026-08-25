#include "update_validation.h"

#include <sys/stat.h>

bool update_fpga_file_valid(const char* path, off_t* size_out)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        if (size_out != nullptr) *size_out = -1;
        return false;
    }

    if (size_out != nullptr) *size_out = st.st_size;
    return st.st_size >= UPDATE_FPGA_MIN_BYTES;
}
