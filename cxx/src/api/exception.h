#ifndef LOOT_API_EXCEPTION
#define LOOT_API_EXCEPTION

#include "rust/cxx.h"

namespace loot {
std::exception_ptr mapError(const ::rust::Error& error);
}

#endif
