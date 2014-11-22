/* 
 * File:   StringProc.h
 * Author: rbk
 *
 * Created on November 12, 2014, 8:37 PM
 */

#ifndef STRINGPROC_H
#define	STRINGPROC_H

#ifdef	__cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>    

#define MALOC_COPY_STR(str_d, str_s) (str_d) = malloc(sizeof(strlen(str_s)));\
strcpy((str_d), (str_s));
    
    char** getFileParts(const char* fname, int* len);
    char* getParentNodeName(const char*fname);
    
    void freeFileParts(char*** fParts, const int len);


#ifdef	__cplusplus
}
#endif

#endif	/* STRINGPROC_H */

