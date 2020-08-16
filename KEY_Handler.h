////////////////////////////////////////////////////////////////////////////////
// File    : KEY_KeyHandler.h
// Function: Include file of 'KEY_KeyHandler.h'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef KEY_KEYHANDLER_H                              // Include file already compiled ?
#define KEY_KEYHANDLER_H

#ifdef KEY_KEYHANDLER_C                               // Compiled in KEY_KeyHandler.c ?
#define KEY_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define KEY_EXTERN extern "C"
#else
#define KEY_EXTERN extern
#endif // __cplusplus
#endif // KEY_KEYHANDLER_C


#define KEY_OK                  (0)                   // All Ok
#define KEY_ERR_PARAM           (-1)                  // Parameter error
#define KEY_ERR_2NDINIT         (-2)
#define KEY_ERR_PROCESS         (-3)                  // Process allocation errord


// KEY types
typedef char                    KEY_status ;          // Status/Error return type
typedef enum
{
  e_Key_Up    = 0,
  e_Key_Down  = 1,
  e_Key_Left  = 2,
  e_Key_Right = 3,
  e_Key_Enter = 4
} KEY_Key_enum ;

////// Main functions //////
KEY_status  KEY_Initialize          (void) ;

KEY_status  KEY_Terminate           (void) ;

KEY_status  KEY_SetClient           (KEY_Key_enum          const e_key,
                                     PID                   const t_clientProcId) ;

#endif //KEY_KEYHANDLER_H
