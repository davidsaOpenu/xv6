#ifndef _ERRNO_H
#define _ERRNO_H
/*************************************\
 * This header is a lite implementation
 * of the errno mechanism to be handled
 * with system calls.
\*************************************/
#define MAX_ERROR_STRING_LENGTH  255

// ERROR CODES HERE
// LOCKS
#define ERR_LOCK_ALREADY_AQUIRED 100
#define ERR_LOCK_ALREADY_RELEASED 101

int errno = 0;
char errorString[MAX_ERROR_STRING_LENGTH] = {0};

void setErrorCode(int newErrorCode){
    errno = newErrorCode;
}

void setErrorString(const char newErrorString[MAX_ERROR_STRING_LENGTH]){
    strncpy(errorString, newErrorString, MAX_ERROR_STRING_LENGTH);
}

void setError(int newErrorCode, const char newErrorString[MAX_ERROR_STRING_LENGTH]){
    setErrorCode(newErrorCode);
    setErrorString(newErrorString);
}

int hasError(){
    return errno != 0;
}
#endif
