////////////////////////////////////////////////////////////////////////////////
// File    : WEB_Site.h
// Function: Include file of 'WEB_Site.h'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef WEB_SITE_H                                    // Include file already compiled ?
#define WEB_SITE_H

#ifdef WEB_SITE_C                                     // Compiled in WEB_Site.c ?
#define WEB_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define WEB_EXTERN extern "C"
#else
#define WEB_EXTERN extern
#endif // __cplusplus
#endif // WEB_Site_C


#define WEB_OK                  (0)                   // All Ok
#define WEB_ERR_PARAM           (-1)                  // Parameter error
#define WEB_ERR_MEMORY          (-2)                  // Memory allocation error
#define WEB_ERR_POINTER         (-3)                  // Invalid pointer supplied
#define WEB_ERR_PROCESS         (-4)                  // Process allocation errord
#define WEB_ERR_NOFREESLOT      (-5)                  // No free client slot was found

// WEB types
typedef void*                   WEB_handle ;
typedef char                    WEB_status ;          // Status/Error return type


WEB_status  WEB_Initialize        (Webpage            * * const ppt_webPage) ;


WEB_status  WEB_CreateTable       (WEB_handle           * const ppt_instance,
                                   unsigned short         const u16_nrOfEntries,
                                   unsigned int           const u24_unitsPerKPulses,
                                   unsigned short         const u16_webNumber,
                                   char           const * const ps8_frameName,
                                   char           const * const ps8_tableName,
                                   char           const * const ps8_unitName) ;

WEB_status  WEB_DeleteTable       (WEB_handle             const pt_instance) ;

WEB_status  WEB_CreateMeter       (WEB_handle           * const ppt_instance,
                                   unsigned int           const u24_unitsPerKPulses,
                                   unsigned int           const u24_maxCapacity,
                                   unsigned short         const u16_webNumber,
                                   char           const * const ps8_frameName,
                                   char           const * const ps8_meterName,
                                   char           const * const ps8_unitName) ;

WEB_status  WEB_DeleteMeter       (WEB_handle             const pt_instance) ;

WEB_status  WEB_GetProcessId      (WEB_handle             const pt_instance,
                                   PID                  * const pt_processId) ;

#endif //WEB_SITE_H
