////////////////////////////////////////////////////////////////////////////////
// File    : RTC_RealTimeClock.h
// Function: Include file of 'RTC_RealTimeClock.h'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef RTC_REALTIMECLOCK_H                           // Include file already compiled ?
#define RTC_REALTIMECLOCK_H

#ifdef RTC_REALTIMECLOCK_C                            // Compiled in RTC_RealTimeClock.h ?
#define RTC_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define RTC_EXTERN extern "C"
#else
#define RTC_EXTERN extern
#endif // __cplusplus
#endif // RTC_PULSEHANDLER_C


#define RTC_OK                  (0)                   // All Ok
#define RTC_ERR_PARAM           (-1)                  // Parameter error
#define RTC_ERR_MEMORY          (-2)                  // Memory allocation error
#define RTC_ERR_PROCESS         (-3)                  // Process allocation errord
#define RTC_ERR_NOTINIT         (-4)                  // Module not initialized
#define RTC_ERR_NOFREESLOT      (-5)                  // No free client slot was found
#define RTC_ERR_NOTFOUND        (-6)                  // ProcessId and Instance not found


// PHD types
typedef char                    RTC_status ;          // Status/Error return type

typedef struct
{
  unsigned char   u8_second ;
  unsigned char   u8_minute ;
  unsigned char   u8_hour ;
  unsigned char   u8_day ;
  unsigned char   u8_dayOfWeek ;                      // Not supported (yet)
  unsigned char   u8_month ;
  unsigned short  u16_year ;
  BOOL            b_daylightSavingTime ;              // Not supported (yet)
} RTC_DateTime_struct ;

typedef enum
{
  e_secondEvent = 0,
  e_minuteEvent = 1,
  e_hourEvent   = 2,
  e_dayEvent    = 3
} t_event_enum ;


RTC_status  RTC_Initialize          (void) ;

RTC_status  RTC_Terminate           (void) ;

RTC_status  RTC_GetTime             (unsigned long             * const u32_seconds) ;

RTC_status  RTC_AddClient           (t_event_enum                const t_eventType,
                                     PID                         const t_clientProcId,
                                     void                const * const pt_clientInstance) ;

RTC_status  RTC_RemoveClient        (PID                         const t_clientProcId,
                                     void                const * const pt_clientInstance) ;

void        RTC_Seconds2Date        (unsigned long               const u32_seconds,
                                     RTC_DateTime_struct       * const pt_dateTime) ;

void        RTC_Seconds2UTC         (unsigned long               const u32_seconds,
                                     RTC_DateTime_struct       * const pt_dateTime) ;

void        RTC_Date2Seconds        (RTC_DateTime_struct const * const pt_dateTime,
                                     unsigned long             * const pu32_seconds) ;


#endif //RTC_REALTIMECLOCK_H
