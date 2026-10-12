#pragma once

#include <dlfcn.h>

namespace fakeclock::detail
{
// The function identity, not just its signature, keys the cache: two libc
// functions with identical signatures still need independent symbol lookups.
// Pass __func__ from the override so adding a function needs no central registry.
template <auto Function> auto realFunction(const char *name)
{
    static const auto resolved = reinterpret_cast<decltype(Function)>(dlsym(RTLD_NEXT, name));
    return resolved;
}
} // namespace fakeclock::detail
