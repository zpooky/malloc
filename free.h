// $Id$
/**
 * Contains functions shared between thread local memory allocation and the
 * interface to manage non-thread local memory. It does not manage any internal
 * state.
 *
 *
 */

#ifndef SP_MALLOC_FREE_H
#define SP_MALLOC_FREE_H

#include "shared.h"

namespace shared {

FreeCode
free(local::PoolsRAII &, local::PoolsRAII &, void *) noexcept;
FreeCode
free(local::Pools &, local::Pools &, void *) noexcept;

/* Tries to lookup the size of the underlying bucket referenced by @ptr. Will
 * return the size of the underlying bucket if @ptr is owned by @pool or None.
 *
 * @param[in] pool    The pool scanned for @ptr
 * @param[in] ptr     The pointer to find its size
 * @return            Maybe the size of the bucket referred to by @ptr
 */
util::maybe<sp::bucket_size>
usable_size(local::PoolsRAII &, void *) noexcept;
util::maybe<sp::bucket_size>
usable_size(local::Pools &, void *) noexcept;

/*
 * Allocates and free @ptr to comply to the @length requirement.
 * If necessary allocated a new bucket of memory copying over @ptr to a new
 * memory location then freeing it, or if there is already enough memory
 * available in @ptr then the old address is returned, or when the @ptr is not
 * owned by @pool nothing is returned. The function returns a pointer
 * if @pool owns @ptr otherwise None. New memory if required is allocated from
 * @tl.
 *
 * @param[in] tl      The thread local pool
 * @param[in] pool    The pool scanned for @ptr
 * @param[in] ptr     The memory to be re-allocated
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
