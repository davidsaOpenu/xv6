#define MUTEX_PREFIX "MUTEX_"
#define MUTEX_SEPERATOR "_"
#define MUTEX_SIZE sizeof(MUTEX_PREFIX) + sizeof(int)

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
int mutex_lock(mutex_t *mutex_var);
int mutex_unlock(mutex_t *mutex_var);
