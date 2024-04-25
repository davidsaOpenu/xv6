#define MUTEX_PREFIX "MUTEX_"
#define MUTEX_SEPERATOR "_"
#define MAX_INT_ASCII_DIGITS 10
#define MUTEX_SIZE sizeof(MUTEX_PREFIX) + MAX_INT_ASCII_DIGITS + 1

enum mutex_e {
  MUTEX_SUCCESS = 0,

  MUTEX_INVALID_PARAMETER,
  MUTEX_UPTIME_ERROR,

  MUTEX_FAILURE,
};

typedef struct mutex_s {
  char buffer[MUTEX_SIZE];
} mutex_t;

int mutex_init(mutex_t *mutex_var);
int mutex_init_named(mutex_t *const mutex_var, const char *const name);
int mutex_lock(mutex_t *mutex_var);
int mutex_unlock(mutex_t *mutex_var);
