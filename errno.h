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
char errorstr[MAX_ERROR_STRING_LENGTH] = {0};
void seterrorcode(int newErrorCode){
    errno = newErrorCode;
}
void seterrorstring(const char newErrorString[MAX_ERROR_STRING_LENGTH]){
    strncpy(errorstr, newErrorString, MAX_ERROR_STRING_LENGTH);
}
void seterror(int newErrorCode, const char newErrorString[MAX_ERROR_STRING_LENGTH]){
    seterrorcode(newErrorCode);
    seterrorstring(newErrorString);
}
int haserror(){
    return errno != 0;
}
#endif
