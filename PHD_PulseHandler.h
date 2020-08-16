////////////////////////////////////////////////////////////////////////////////
// File    : PHD_PulseHandler.h
// Function: Include file of 'PHD_PulseHandler.h'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef PHD_PULSEHANDLER_H                            // Include file already compiled ?
#define PHD_PULSEHANDLER_H

#ifdef PHD_PULSEHANDLER_C                             // Compiled in PHD_PulseHandler.h ?
#define PHD_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define PHD_EXTERN extern "C"
#else
#define PHD_EXTERN extern
#endif // __cplusplus
#endif // PHD_PULSEHANDLER_C


#define PHD_OK                  (0)                   // All Ok
#define PHD_ERR_PARAM           (-1)                  // Parameter error
#define PHD_ERR_MEMORY          (-2)                  // Memory allocation error
#define PHD_ERR_POINTER         (-3)                  // Invalid pointer supplied
#define PHD_ERR_PROCESS         (-4)                  // Process allocation errord
#define PHD_ERR_NOFREESLOT      (-5)                  // No free client slot was found
#define PHD_ERR_NOTFOUND        (-6)                  // ProcessId not found


// PHD types
typedef void*                   PHD_handle ;
typedef char                    PHD_status ;          // Status/Error return type


////// Main functions //////
PHD_status  PHD_Create              (PHD_handle          * const ppt_instance,
                                     unsigned short        const u16_maxPulsesPerMinute) ;

PHD_status  PHD_Delete              (PHD_handle            const pt_instance) ;

PHD_status  PHD_HandlePulse         (PHD_handle            const pt_instance) ;

////// Metering functions //////
PHD_status  PHD_SetStorageClient    (PHD_handle            const pt_instance,
                                     PID                   const t_clientProcId) ;

PHD_status  PHD_GetPulses           (PHD_handle            const pt_instance,
                                     unsigned int        * const pu24_pulses) ;

////// Current load functions //////
PHD_status  PHD_AddClient           (PHD_handle            const pt_instance,
                                     PID                   const t_clientProcId) ;

PHD_status  PHD_RemoveClient        (PHD_handle            const pt_instance,
                                     PID                   const t_clientProcId) ;

PHD_status  PHD_GetPulsesPerMinute  (PHD_handle            const pt_instance,
                                     unsigned int        * const pu24_pulsesPerMinute) ;

#endif //PHD_PULSEHANDLER_H
