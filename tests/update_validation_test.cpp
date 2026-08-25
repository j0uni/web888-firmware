#include "update_validation.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void check_size(off_t size, bool expected)
{
    char path[] = "/tmp/web888-fpga-validation-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(ftruncate(fd, size) == 0);
    assert(close(fd) == 0);

    off_t actual = -1;
    assert(update_fpga_file_valid(path, &actual) == expected);
    assert(actual == size);
    assert(unlink(path) == 0);
}

int main()
{
    off_t size = 0;
    assert(!update_fpga_file_valid("/file/that/does/not/exist", &size));
    assert(size == -1);

    check_size(305, false);                         // observed 404 response
    check_size(UPDATE_FPGA_MIN_BYTES - 1, false);  // just below boundary
    check_size(UPDATE_FPGA_MIN_BYTES, true);       // boundary is accepted
    check_size(1250568, true);                     // observed valid HF image

    puts("update_validation_test: PASS");
    return 0;
}
