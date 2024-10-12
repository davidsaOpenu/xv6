#define MUTEX_PREFIX "/.mutex/"
#define MAX_INT_ASCII_DIGITS (10)
#define MUTEX_SIZE (sizeof(MUTEX_PREFIX) + MAX_INT_ASCII_DIGITS + 1)

// #define MUTEX_DEBUG
#define MUTEX_DETECT_DEADLOCK

#define MUTEX_DETECT_DEADLOCK_MAX_SPINS (1000)

#define MUTEX_RETRY_MS (10)

#ifdef MUTEX_DEBUG
#define MUTEX_LOG_DEBUG(fmt, ...)       \
  do {                                  \
    printf(stdout, fmt, ##__VA_ARGS__); \
  } while (0)
#else
#define MUTEX_LOG_DEBUG(fmt, ...)
#endif

/**
 * Mutex status codes.
 */
enum mutex_e {
  MUTEX_SUCCESS = 0,

  MUTEX_INVALID_PARAMETER,
  MUTEX_UPTIME_ERROR,
  MUTEX_DIR_INIT_FAILED,

  MUTEX_FAILURE,
};

typedef struct mutex_s {
  char buffer[MUTEX_SIZE];
} mutex_t;

/**
 * Creates a mutex with a unique name.
 * @param mutex_var Pointer to a mutex variable to be initialized.
 * @return MUTEX_SUCCESS if the mutex was created, mutex_e value of error
 * otherwise.
 */
enum mutex_e mutex_init(mutex_t *mutex_var);

/**
 * Creates a mutex with a given name.
 * @param mutex_var Pointer to a mutex variable to be initialized.
 * @param name Name of the mutex.
 * @return MUTEX_SUCCESS if the mutex was created, mutex_e value of error
 * otherwise.
 */
enum mutex_e mutex_init_named(mutex_t *const mutex_var, const char *const name);

/**
 * Locks a mutex.
 * @return MUTEX_SUCCESS if the mutex was locked, system error otherwise.
 */
enum mutex_e mutex_lock(mutex_t *mutex_var);

/**
 * Unlocks a mutex.
 * @return MUTEX_SUCCESS if the mutex was unlocked, MUTEX_FAILURE otherwise.
 */
enum mutex_e mutex_unlock(mutex_t *mutex_var);

/**
 * Wait on a mutex and unlocks it right away.
 */
enum mutex_e mutex_wait(mutex_t *mutex_var);
