#ifndef COMMON_PLATFORMDEFS_H_
#define COMMON_PLATFORMDEFS_H_

// Windows-specific definitions
#ifdef _WIN32
    #ifndef FALSE
        #define FALSE 0
    #endif
    #ifndef TRUE
        #define TRUE 1
    #endif
#endif

#endif /* COMMON_PLATFORMDEFS_H_ */
