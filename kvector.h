#ifndef KVEC_H
#define KVEC_H

#define getelement(vec, ind, typ) *(typ*)getelementpointer(vec,ind)

typedef struct kvec {
    unsigned int vectorsize;
    unsigned int typesize;
    char* head;
    char* tail;
    char* lastaccessed;
    unsigned int lastindexaccessed;
    int valid;
    int virtualvec;
} vector;

#endif