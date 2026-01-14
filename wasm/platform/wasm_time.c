/**
 * @file wasm_time.c
 * @brief WebAssembly 时间函数实现
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

/* ============================================
 * 时间戳函数
 * ============================================ */

/**
 * 获取微秒级时间戳
 * @return 微秒时间戳
 */
uint64_t voice_time_get_microseconds(void) {
    /* emscripten_get_now() 返回毫秒（double） */
    return (uint64_t)(emscripten_get_now() * 1000.0);
}

/**
 * 获取毫秒级时间戳
 * @return 毫秒时间戳
 */
uint32_t voice_time_get_milliseconds(void) {
    return (uint32_t)emscripten_get_now();
}

/**
 * 获取秒级时间戳
 * @return 秒时间戳
 */
uint32_t voice_time_get_seconds(void) {
    return (uint32_t)(emscripten_get_now() / 1000.0);
}

/**
 * 高精度睡眠（微秒）
 * 注意：WebAssembly 不支持阻塞式睡眠
 */
void voice_time_sleep_microseconds(uint32_t microseconds) {
    (void)microseconds;
    /* WASM 不支持同步睡眠，应该使用异步方式 */
    /* 如果真的需要，可以使用忙等待（不推荐） */
}

/**
 * 睡眠（毫秒）
 */
void voice_time_sleep_milliseconds(uint32_t milliseconds) {
    (void)milliseconds;
    /* WASM 不支持同步睡眠 */
}

/**
 * 获取单调时钟（用于性能测量）
 */
uint64_t voice_time_get_monotonic_ns(void) {
    /* performance.now() 提供高精度单调时钟 */
    return (uint64_t)(emscripten_get_now() * 1000000.0);
}

/**
 * 性能计时器
 */
typedef struct {
    double start_time;
    bool running;
} voice_timer_t;

static voice_timer_t g_timer = {0};

void voice_timer_start(void) {
    g_timer.start_time = emscripten_get_now();
    g_timer.running = true;
}

double voice_timer_stop(void) {
    if (!g_timer.running) {
        return 0.0;
    }
    
    double elapsed = emscripten_get_now() - g_timer.start_time;
    g_timer.running = false;
    return elapsed;
}

double voice_timer_elapsed(void) {
    if (!g_timer.running) {
        return 0.0;
    }
    
    return emscripten_get_now() - g_timer.start_time;
}

#endif /* __EMSCRIPTEN__ */
