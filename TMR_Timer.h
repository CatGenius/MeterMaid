////////////////////////////////////////////////////////////////////////////////
// File    : TMR_Timer.h
// Function: Include file of 'TMR_Timer.c'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef TMR_TIMER_H                                   // Include file already compiled ?
#define TMR_TIMER_H

#ifdef TMR_TIMER_C                                    // Compiled in TMR_Timer.C ?
#define TMR_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define TMR_EXTERN extern "C"
#else
#define TMR_EXTERN extern
#endif // __cplusplus
#endif // TMR_TIMER_C

#define TMR_OK            (0)                         // All Ok
#define TMR_ERR_INIT      (-1)                        // Double initilisation

#define TMR_TICKS_PER_MS  (1)                         // Number of timer ticks per millisecond
#define TMR_SECOND        (1000UL)                    // Number of timer ticks per second
#define TMR_MINUTE        (60000UL)                   // Number of timer ticks per minute
#define TMR_HOUR          (3600000UL)                 // Number of timer ticks per hour


// TMR types
typedef char                    TMR_status ;          // Status/Error return type

typedef struct
{
  unsigned long     u32_lsLong ;
  unsigned short    u16_msShort ;
} TMR_calc_struct ;

// Storage type used for time stamps
typedef union
{
  unsigned char     u8_ticksByte[6] ;
  TMR_calc_struct   t_ticksCalc ;
} TMR_ticks_struct ;


TMR_status      TMR_Initialize      (void) ;

TMR_status      TMR_Terminate       (void) ;

void            TMR_SetTimeout      (TMR_ticks_struct       * const pt_timeout,
                                     unsigned long            const timout) ;

void            TMR_PostponeTimeout (TMR_ticks_struct       * const pt_timeout,
                                     unsigned long            const postpone) ;

void            TMR_ExpireTimeout   (TMR_ticks_struct       * const pt_timeout) ;

void            TMR_NeverTimeout    (TMR_ticks_struct       * const pt_timeout) ;

BOOL            TMR_CheckTimeout    (TMR_ticks_struct const * const pt_timeout) ;

void            TMR_SetTimeStamp    (TMR_ticks_struct       * const pt_timestamp) ;

unsigned long   TMR_TimeStampAge    (TMR_ticks_struct const * const pt_timestamp) ;

void            TMR_Delay           (unsigned long            const delay) ;

void            TMR_MicroDelay      (unsigned short           const delay_us) ;

#endif //TMR_TIMER_H
