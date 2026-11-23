/* Tests of the runtime below the OSAL: tasks, priorities, mutexes,
 * semaphores, queues, and the clock. Runs as an Atmosphere process. */
#include <stdio.h>
#include <string.h>

#include "atmo_rt.h"

#define MS 1000000ull

static int failures;
static int checks;

#define CHECK(cond)                                                        \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            failures++;                                                    \
            printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

/* A record of the order in which things happened. */
static char   trace[64];
static size_t trace_len;
static void mark(char c)
{
    if (trace_len + 1 < sizeof(trace))
        trace[trace_len++] = c;
    trace[trace_len] = '\0';
}
static void trace_reset(void)
{
    trace_len = 0;
    trace[0]  = '\0';
}

/* ---- Two tasks hand a token back and forth ---- */

#define ROUNDS 10000
static art_sem_t ping, pong, done;
static int       ping_count, pong_count;

static void ping_task(void *arg)
{
    (void)arg;
    for (int i = 0; i < ROUNDS; i++) {
        art_sem_take(&ping, ART_FOREVER);
        ping_count++;
        art_sem_give(&pong);
    }
    art_sem_give(&done);
}

static void pong_task(void *arg)
{
    (void)arg;
    for (int i = 0; i < ROUNDS; i++) {
        art_sem_take(&pong, ART_FOREVER);
        pong_count++;
        art_sem_give(&ping);
    }
    art_sem_give(&done);
}

static void test_token(void)
{
    art_task_t a, b;
    uint64_t   start, elapsed;

    art_sem_init(&ping, 0, 1);
    art_sem_init(&pong, 0, 1);
    art_sem_init(&done, 0, 2);
    CHECK(art_task_create(&a, ping_task, NULL, NULL, 16384, 10) == ART_OK);
    CHECK(art_task_create(&b, pong_task, NULL, NULL, 16384, 10) == ART_OK);
    start = art_clock_ns();
    art_sem_give(&ping);
    CHECK(art_sem_take(&done, art_deadline_after(20000 * MS)) == ART_OK);
    CHECK(art_sem_take(&done, art_deadline_after(20000 * MS)) == ART_OK);
    elapsed = art_clock_ns() - start;
    CHECK(ping_count == ROUNDS);
    CHECK(pong_count == ROUNDS);
    printf("  token: %d round trips, %lu ns each\n", ROUNDS, (unsigned long)(elapsed / ROUNDS));
    art_task_sleep(1 * MS); /* the tasks are less urgent than main and end once it blocks */
    CHECK(!art_task_valid(a));
    CHECK(!art_task_valid(b));
}

/* ---- The clock ---- */

/* A process has no I/O ports, so it cannot compare the clock with the
 * real-time clock of the machine. The kernel measures the timestamp counter
 * against the interval timer at boot; this checks what the runtime made of
 * it. */
static void test_clock(void)
{
    uint64_t hz = art_clock_counter_hz();
    uint64_t t0, t1, ticks = 0;

    printf("  timestamp counter: %lu Hz%s\n", (unsigned long)hz,
           art_clock_calibrated() ? "" : " (assumed)");
    CHECK(art_clock_calibrated());
    CHECK(hz >= 100 * 1000 * 1000ull && hz <= 10 * 1000 * 1000 * 1000ull);

    /* The clock advances, never goes backward, and counts in nanoseconds:
     * a spin of 50 ms of counter ticks takes 50 ms of the clock. */
    t0 = art_clock_ns();
    {
        uint64_t start = __builtin_ia32_rdtsc(), last = t0;
        int      backward = 0;
        while ((ticks = __builtin_ia32_rdtsc() - start) < hz / 20) {
            uint64_t now = art_clock_ns();
            if (now < last)
                backward = 1;
            last = now;
        }
        CHECK(!backward);
    }
    t1 = art_clock_ns();
    printf("  %lu counter ticks measured as %lu us\n", (unsigned long)ticks,
           (unsigned long)((t1 - t0) / 1000));
    CHECK(t1 - t0 >= 50 * MS && t1 - t0 < 100 * MS);
}

static void test_sleep(void)
{
    uint64_t worst = 0;
    for (int i = 0; i < 10; i++) {
        uint64_t start = art_clock_ns();
        uint64_t took;
        art_task_sleep(100 * MS);
        took = art_clock_ns() - start;
        CHECK(took >= 100 * MS);
        if (took - 100 * MS > worst)
            worst = took - 100 * MS;
    }
    printf("  sleep 100 ms: worst overshoot %lu us over 10 runs\n", (unsigned long)(worst / 1000));
    /* The stated error: a sleeping task alone on the processor wakes within 1 ms. */
    CHECK(worst < 1 * MS);
}

/* ---- Priorities ---- */

static void mark_task(void *arg)
{
    mark((char)(uintptr_t)arg);
}

static void test_priority(void)
{
    art_task_t t;
    uint8_t    prio = 0;

    /* main runs at priority 0, so new tasks wait until main blocks. */
    trace_reset();
    art_task_create(&t, mark_task, (void *)'c', NULL, 16384, 30);
    art_task_create(&t, mark_task, (void *)'a', NULL, 16384, 10);
    art_task_create(&t, mark_task, (void *)'b', NULL, 16384, 20);
    art_task_create(&t, mark_task, (void *)'A', NULL, 16384, 10);
    mark('m');
    art_task_sleep(1 * MS);
    CHECK(strcmp(trace, "maAbc") == 0);

    /* A more urgent new task runs before art_task_create returns. */
    CHECK(art_task_set_priority(art_task_self(), 50) == ART_OK);
    CHECK(art_task_get_priority(art_task_self(), &prio) == ART_OK && prio == 50);
    trace_reset();
    art_task_create(&t, mark_task, (void *)'h', NULL, 16384, 40);
    mark('m');
    art_task_create(&t, mark_task, (void *)'l', NULL, 16384, 60);
    mark('n');
    art_task_yield(); /* a yield does not let a less urgent task run */
    mark('o');
    art_task_sleep(1 * MS);
    CHECK(strcmp(trace, "hmnol") == 0);
    CHECK(art_task_set_priority(art_task_self(), 0) == ART_OK);
}

/* ---- Priority inheritance ----
 * L (low) owns a mutex. H (high) waits for it. M (medium) is ready. Without
 * inheritance M would run before L and delay H. */

static art_mutex_t pi_mutex;
static art_sem_t   pi_go;

static void pi_low(void *arg)
{
    (void)arg;
    art_mutex_lock(&pi_mutex);
    mark('l');
    art_sem_take(&pi_go, ART_FOREVER); /* lets main start H and M */
    mark('L');
    art_mutex_unlock(&pi_mutex); /* H takes over here */
    mark('x');
}

static void pi_high(void *arg)
{
    (void)arg;
    mark('h');
    art_mutex_lock(&pi_mutex);
    mark('H');
    art_mutex_unlock(&pi_mutex);
}

static void pi_medium(void *arg)
{
    (void)arg;
    mark('M');
}

static void test_inheritance(void)
{
    art_task_t t;

    art_mutex_init(&pi_mutex);
    art_sem_init(&pi_go, 0, 1);
    trace_reset();
    art_task_set_priority(art_task_self(), 5);
    art_task_create(&t, pi_low, NULL, NULL, 16384, 30);
    art_task_sleep(1 * MS); /* L locks and blocks on pi_go */
    art_task_create(&t, pi_high, NULL, NULL, 16384, 10);
    art_task_create(&t, pi_medium, NULL, NULL, 16384, 20);
    art_task_sleep(1 * MS); /* H runs and blocks on the mutex; M runs */
    CHECK(strcmp(trace, "lhM") == 0);

    /* Now with M made ready while L holds the mutex and H waits. */
    art_task_create(&t, pi_medium, NULL, NULL, 16384, 20);
    art_sem_give(&pi_go); /* L is ready at H's priority, M at 20 */
    art_task_sleep(1 * MS);
    CHECK(strcmp(trace, "lhMLHMx") == 0);
    art_task_set_priority(art_task_self(), 0);
    art_mutex_destroy(&pi_mutex);
}

static void test_mutex(void)
{
    art_mutex_t m;
    art_mutex_init(&m);
    CHECK(art_mutex_unlock(&m) == ART_NOT_OWNER);
    CHECK(art_mutex_lock(&m) == ART_OK);
    CHECK(art_mutex_lock(&m) == ART_OK);
    CHECK(art_mutex_held(&m));
    CHECK(art_mutex_unlock(&m) == ART_OK);
    CHECK(art_mutex_held(&m));
    CHECK(art_mutex_unlock(&m) == ART_OK);
    CHECK(!art_mutex_held(&m));
    CHECK(art_mutex_unlock(&m) == ART_NOT_OWNER);
    art_mutex_destroy(&m);
}

/* ---- Semaphores ---- */

static art_sem_t flush_sem;
static int       flushed;

static void flush_waiter(void *arg)
{
    (void)arg;
    if (art_sem_take(&flush_sem, ART_FOREVER) == ART_OK)
        flushed++;
}

static void test_sem(void)
{
    art_sem_t  s;
    art_task_t t;
    uint64_t   start, took;

    art_sem_init(&s, 1, 1);
    CHECK(art_sem_give(&s) == ART_FULL);
    CHECK(art_sem_take(&s, ART_NO_WAIT) == ART_OK);
    CHECK(art_sem_take(&s, ART_NO_WAIT) == ART_TIMEOUT);
    start = art_clock_ns();
    CHECK(art_sem_take(&s, art_deadline_after(50 * MS)) == ART_TIMEOUT);
    took = art_clock_ns() - start;
    CHECK(took >= 50 * MS && took < 51 * MS);

    art_sem_init(&s, 2, 5);
    CHECK(art_sem_count(&s) == 2);
    CHECK(art_sem_give(&s) == ART_OK);
    CHECK(art_sem_count(&s) == 3);

    art_sem_init(&flush_sem, 0, 1);
    for (int i = 0; i < 3; i++)
        art_task_create(&t, flush_waiter, NULL, NULL, 16384, 10);
    art_task_sleep(1 * MS);
    CHECK(flushed == 0);
    CHECK(art_sem_flush(&flush_sem) == ART_OK);
    art_task_sleep(1 * MS);
    CHECK(flushed == 3);
    CHECK(art_sem_count(&flush_sem) == 0);
}

/* ---- Queues ---- */

static art_queue_t queue;

static void queue_sender(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < 5; i++) {
        art_task_sleep(2 * MS);
        art_queue_put(&queue, &i, sizeof(i));
    }
}

static void queue_destroyer(void *arg)
{
    art_task_sleep(2 * MS);
    art_queue_destroy((art_queue_t *)arg);
}

static void test_queue(void)
{
    art_task_t t;
    uint32_t   v = 99;
    size_t     size;
    char       big[16] = "0123456789abcde";
    char       small[4];

    CHECK(art_queue_init(&queue, 4, 8) == ART_OK);
    CHECK(art_queue_get(&queue, &v, sizeof(v), &size, ART_NO_WAIT) == ART_EMPTY);
    CHECK(art_queue_put(&queue, big, 9) == ART_TOO_LARGE);
    for (uint32_t i = 0; i < 4; i++)
        CHECK(art_queue_put(&queue, &i, sizeof(i)) == ART_OK);
    CHECK(art_queue_put(&queue, &v, sizeof(v)) == ART_FULL);
    CHECK(art_queue_count(&queue) == 4);
    for (uint32_t i = 0; i < 4; i++) {
        CHECK(art_queue_get(&queue, &v, sizeof(v), &size, ART_NO_WAIT) == ART_OK);
        CHECK(v == i && size == sizeof(v));
    }
    CHECK(art_queue_put(&queue, big, 8) == ART_OK);
    CHECK(art_queue_get(&queue, small, sizeof(small), &size, ART_NO_WAIT) == ART_TOO_LARGE);
    CHECK(art_queue_get(&queue, big, sizeof(big), &size, ART_NO_WAIT) == ART_OK && size == 8);
    CHECK(art_queue_get(&queue, &v, sizeof(v), &size, art_deadline_after(5 * MS)) == ART_TIMEOUT);

    art_task_create(&t, queue_sender, NULL, NULL, 16384, 10);
    for (uint32_t i = 0; i < 5; i++) {
        CHECK(art_queue_get(&queue, &v, sizeof(v), &size, art_deadline_after(100 * MS)) == ART_OK);
        CHECK(v == i);
    }
    art_task_create(&t, queue_destroyer, &queue, NULL, 16384, 10);
    CHECK(art_queue_get(&queue, &v, sizeof(v), &size, ART_FOREVER) == ART_DESTROYED);
}

/* ---- Task creation and deletion ---- */

static int ran;
static void count_task(void *arg)
{
    (void)arg;
    ran++;
}
static void forever_task(void *arg)
{
    (void)arg;
    for (;;)
        art_task_sleep(1000 * MS);
}

static void test_tasks(void)
{
    art_task_t t[100];
    art_task_t first = 0;
    int        made  = 0;

    /* Slots and stacks are used again after a task ends. Each new task is
     * more urgent than main, so it runs and ends inside art_task_create. */
    art_task_set_priority(art_task_self(), 20);
    for (int i = 0; i < 2000; i++) {
        art_task_t h;
        CHECK(art_task_create(&h, count_task, NULL, NULL, 16384, 10) == ART_OK);
        if (i == 0)
            first = h;
        CHECK(ran == i + 1);
    }
    art_task_set_priority(art_task_self(), 0);
    CHECK(ran == 2000);
    CHECK(!art_task_valid(first));
    CHECK(art_task_delete(first) == ART_ERROR);

    /* The table fills up, and deleting tasks frees it. */
    for (int i = 0; i < 100; i++) {
        if (art_task_create(&t[i], forever_task, NULL, NULL, 16384, 10) != ART_OK)
            break;
        made++;
    }
    CHECK(made == 100);
    art_task_sleep(1 * MS);
    for (int i = 0; i < made; i++)
        CHECK(art_task_delete(t[i]) == ART_OK);
    CHECK(art_task_create(&t[0], count_task, NULL, NULL, 16384, 10) == ART_OK);
    art_task_sleep(1 * MS);
    CHECK(ran == 2001);

    CHECK(art_task_set_tag(art_task_self(), 0x1234) == ART_OK);
    CHECK(art_task_get_tag(art_task_self()) == 0x1234);
}

static void run(const char *name, void (*test)(void))
{
    int before = failures;
    printf("%s\n", name);
    test();
    printf("  %s\n", failures == before ? "ok" : "FAILED");
}

int main(void)
{
    run("token", test_token);
    run("clock", test_clock);
    run("sleep", test_sleep);
    run("priority", test_priority);
    run("inheritance", test_inheritance);
    run("mutex", test_mutex);
    run("semaphore", test_sem);
    run("queue", test_queue);
    run("tasks", test_tasks);
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
