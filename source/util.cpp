#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "util.hpp"


void handle_error(const std::string &msg) {
    const int tmp_errno = errno;
    std::cerr << msg << std::endl;
    char buf[1024];
    memset(buf, 0, sizeof(buf));
#ifdef __SWITCH__
    strerror_r(tmp_errno, buf, sizeof(buf));
    std::cerr << "error " << tmp_errno << ": " << std::string(buf) << std::endl;
#else
    char *err_msg = strerror_r(tmp_errno, buf, sizeof(buf));
    std::cerr << "error " << tmp_errno << ": " << std::string(err_msg) << std::endl;
#endif
}

int rand_range(const int min, const int max) {
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}
