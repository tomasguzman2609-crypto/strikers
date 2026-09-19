#ifndef PORT_DISC_READER_H
#define PORT_DISC_READER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runs jobs in order on one reader thread. Blocks if the queue is full.
void PortDiscQueue(void (*work)(void*), void* ctx);

// Starts a separate worker. PortDiscJoin waits for completion and deletes the thread object.
void* PortDiscSpawn(void (*work)(void*), void* ctx);
void PortDiscJoin(void* thread);

#ifdef __cplusplus
}
#endif

// Release/acquire publication for reader state and completed data.
#if defined(_MSC_VER) && !defined(__clang__)
// PORT: real MSVC (not clang-cl) has no __atomic builtins; use its intrinsics instead.
#include <intrin.h>

static inline void port_store_release_i32(int32_t* p, int32_t v)
{
    _InterlockedExchange((long*)p, (long)v);
}

static inline int32_t port_load_acquire_i32(const int32_t* p)
{
    return (int32_t)_InterlockedOr((long*)p, 0);
}

static inline void port_store_release_u64(unsigned long long* p, unsigned long long v)
{
    _InterlockedExchange64((__int64*)p, (__int64)v);
}

static inline unsigned long long port_load_acquire_u64(const unsigned long long* p)
{
    return (unsigned long long)_InterlockedOr64((__int64*)p, 0);
}
#else
static inline void port_store_release_i32(int32_t* p, int32_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

static inline int32_t port_load_acquire_i32(const int32_t* p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

static inline void port_store_release_u64(unsigned long long* p, unsigned long long v)
{
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

static inline unsigned long long port_load_acquire_u64(const unsigned long long* p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}
#endif

#endif
