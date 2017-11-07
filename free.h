#ifndef SP_MALLOC_FREE_H
#define SP_MALLOC_FREE_H

#include "shared.h"

namespace shared {

FreeCode
free(local::PoolsRAII &, void *) noexcept;
FreeCode
free(local::Pools &, void *) noexcept;

util::maybe<sp::bucket_size>
usable_size(local::PoolsRAII &, void *) noexcept;
util::maybe<sp::bucket_size>
usable_size(local::Pools &, void *) noexcept;

/*
 * Realloc @ptr to comply to the @length requirement
 * If necessary allocated a new bucket of memory copying over @ptr to a new
 * memory location then freeing it. Or if there is already enough memory
 * available in @ptr then it returns the old address. The function returns a ptr
 * if @pool owns @ptr otherwise None. If new memory is required to be allocated
 * it will be done from @tl.
 *
 * @param[in] tl      The thread local pool
 * @param[in] pool    The pool scanned for @ptr
 * @param[in] ptr     The memory to realloc
 * @param[in] length  The size required
 * @param[out]code    The state of the free operation
 * @return            Maybe a pointer to a memory location if sufficient size
 */
util::maybe<void *>
realloc(local::PoolsRAII &, local::PoolsRAII &, void *, std::size_t,
        /*OUT*/ shared::FreeCode &) noexcept;

util::maybe<void *>
realloc(local::Pools &, local::Pools &, void *, std::size_t,
        /*OUT*/ shared::FreeCode &) noexcept;

} // namespace shared

#endif
