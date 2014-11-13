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
    
    char** getFileParts(const char* fname);
    char* getParentNodeName(const char*fname);


#ifdef	__cplusplus
}
#endif

#endif	/* STRINGPROC_H */

