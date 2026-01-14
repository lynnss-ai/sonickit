/**
 * @file wasm_threading.h
 * @brief WebAssembly 线程和同步原语适配
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * WebAssembly 可以运行在两种模式：
 * 1. 单线程模式（默认）- 不需要真正的锁
 * 2. 多线程模式（使用 SharedArrayBuffer + pthread）- 使用 Emscripten 的 pthread 实现
 */

#ifndef WASM_THREADING_H
#define WASM_THREADING_H

#ifdef __EMSCRIPTEN__

#include <stdbool.h>

/* ============================================
 * 线程支持检测
 * ============================================ */

/* 如果启用了 pthread，使用真实的线程实现 */
#ifdef __EMSCRIPTEN_PTHREADS__
    #include <pthread.h>
    #include <emscripten/threading.h>
    
    #define WASM_HAS_THREADS 1
    
    /* 线程句柄 */
    typedef pthread_t wasm_thread_t;
    
    /* 互斥锁 */
    typedef pthread_mutex_t wasm_mutex_t;
    
    /* 条件变量 */
    typedef pthread_cond_t wasm_cond_t;
    
    /* 线程函数 */
    #define WASM_THREAD_FUNC void*
    #define WASM_THREAD_RETURN return NULL
    
    /* 互斥锁操作 */
    #define wasm_mutex_init(m) pthread_mutex_init(m, NULL)
    #define wasm_mutex_destroy(m) pthread_mutex_destroy(m)
    #define wasm_mutex_lock(m) pthread_mutex_lock(m)
    #define wasm_mutex_unlock(m) pthread_mutex_unlock(m)
    #define wasm_mutex_trylock(m) (pthread_mutex_trylock(m) == 0)
    
    /* 条件变量操作 */
    #define wasm_cond_init(c) pthread_cond_init(c, NULL)
    #define wasm_cond_destroy(c) pthread_cond_destroy(c)
    #define wasm_cond_wait(c, m) pthread_cond_wait(c, m)
    #define wasm_cond_signal(c) pthread_cond_signal(c)
    #define wasm_cond_broadcast(c) pthread_cond_broadcast(c)
    
    /* 线程操作 */
    static inline int wasm_thread_create(wasm_thread_t *thread, void *(*func)(void*), void *arg) {
        return pthread_create(thread, NULL, func, arg);
    }
    
    static inline int wasm_thread_join(wasm_thread_t thread) {
        return pthread_join(thread, NULL);
    }
    
    static inline int wasm_thread_detach(wasm_thread_t thread) {
        return pthread_detach(thread);
    }

#else
    /* 单线程模式 - 空操作实现 */
    #define WASM_HAS_THREADS 0
    
    /* 线程句柄（虚拟） */
    typedef int wasm_thread_t;
    
    /* 互斥锁（虚拟） */
    typedef int wasm_mutex_t;
    
    /* 条件变量（虚拟） */
    typedef int wasm_cond_t;
    
    /* 线程函数 */
    #define WASM_THREAD_FUNC void
    #define WASM_THREAD_RETURN return
    
    /* 互斥锁操作（空操作） */
    static inline void wasm_mutex_init(wasm_mutex_t *m) { (void)m; }
    static inline void wasm_mutex_destroy(wasm_mutex_t *m) { (void)m; }
    static inline void wasm_mutex_lock(wasm_mutex_t *m) { (void)m; }
    static inline void wasm_mutex_unlock(wasm_mutex_t *m) { (void)m; }
    static inline bool wasm_mutex_trylock(wasm_mutex_t *m) { (void)m; return true; }
    
    /* 条件变量操作（空操作） */
    static inline void wasm_cond_init(wasm_cond_t *c) { (void)c; }
    static inline void wasm_cond_destroy(wasm_cond_t *c) { (void)c; }
    static inline void wasm_cond_wait(wasm_cond_t *c, wasm_mutex_t *m) { (void)c; (void)m; }
    static inline void wasm_cond_signal(wasm_cond_t *c) { (void)c; }
    static inline void wasm_cond_broadcast(wasm_cond_t *c) { (void)c; }
    
    /* 线程操作（不支持） */
    static inline int wasm_thread_create(wasm_thread_t *thread, void *(*func)(void*), void *arg) {
        (void)thread; (void)func; (void)arg;
        return -1;  /* 不支持 */
    }
    
    static inline int wasm_thread_join(wasm_thread_t thread) {
        (void)thread;
        return -1;
    }
    
    static inline int wasm_thread_detach(wasm_thread_t thread) {
        (void)thread;
        return -1;
    }
#endif

/* ============================================
 * 原子操作（C11 原子操作在 Emscripten 中可用）
 * ============================================ */

#include <stdatomic.h>

typedef atomic_int wasm_atomic_int_t;
typedef atomic_bool wasm_atomic_bool_t;

#define wasm_atomic_load(ptr) atomic_load(ptr)
#define wasm_atomic_store(ptr, val) atomic_store(ptr, val)
#define wasm_atomic_fetch_add(ptr, val) atomic_fetch_add(ptr, val)
#define wasm_atomic_fetch_sub(ptr, val) atomic_fetch_sub(ptr, val)
#define wasm_atomic_compare_exchange(ptr, expected, desired) \
    atomic_compare_exchange_strong(ptr, expected, desired)

/* ============================================
 * 线程本地存储
 * ============================================ */

#ifdef __EMSCRIPTEN_PTHREADS__
    #define WASM_THREAD_LOCAL _Thread_local
#else
    #define WASM_THREAD_LOCAL static
#endif

/* ============================================
 * 工具函数
 * ============================================ */

/**
 * 检查是否支持多线程
 */
static inline bool wasm_is_threading_supported(void) {
#ifdef __EMSCRIPTEN_PTHREADS__
    return emscripten_has_threading_support();
#else
    return false;
#endif
}

/**
 * 获取可用的线程数
 */
static inline int wasm_get_thread_count(void) {
#ifdef __EMSCRIPTEN_PTHREADS__
    return emscripten_num_logical_cores();
#else
    return 1;
#endif
}

/**
 * 当前线程 ID
 */
static inline int wasm_get_thread_id(void) {
#ifdef __EMSCRIPTEN_PTHREADS__
    return (int)pthread_self();
#else
    return 0;
#endif
}

/**
 * 线程屈服（让出 CPU）
 */
static inline void wasm_thread_yield(void) {
#ifdef __EMSCRIPTEN_PTHREADS__
    pthread_yield();
#else
    /* 单线程模式下无操作 */
#endif
}

#endif /* __EMSCRIPTEN__ */

#endif /* WASM_THREADING_H */
