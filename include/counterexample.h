#ifndef MINILINUX_COUNTEREXAMPLE_H
#define MINILINUX_COUNTEREXAMPLE_H
#include "vm.h"
void counterexample_selftest(void);
void counterexample_user_prepare(struct address_space *space, uint64_t data_page);
#endif
