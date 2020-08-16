////////////////////////////////////////////////////////////////////////////////
// File    : RTC_RealTimeClock.c
// Function: Pulse Handler moduls
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define RTC_REALTIMECLOCK_C
#include <kernel.h>
#include "RTC_RealTimeClock.h"

#include "TMR_Timer.h"

#define RTC_SECS_PER_MIN      (60UL)
#define RTC_SECS_PER_HOUR     (60UL * RTC_SECS_PER_MIN)
#define RTC_SECS_PER_DAY      (24UL * RTC_SECS_PER_HOUR)

#define RTC_UTC               (0UL * RTC_SECS_PER_HOUR)
#define RTC_CET               (1UL * RTC_SECS_PER_HOUR)

#define RTC_MAX_EVENTS        (15)
#define RTC_LEAPYEAR(y)       (((y)%4==0) && (((y)%100!=0) || ((y)%400==0)))

#define RTC_LOCAL             (RTC_CET)
#define RTC_DST(t)            (((t->u8_month  >  3) && (t->u8_month < 10)                                               ) || \
                               ((t->u8_month == 10) && (t->u8_day <  25)                                                ) || \
                               ((t->u8_month ==  3) && (t->u8_day >= 25) && (t->u8_day + (7 - t->u8_dayOfWeek) >  31)   ) || \
                               ((t->u8_month == 10) && (t->u8_day >= 25) && (t->u8_day + (7 - t->u8_dayOfWeek) <= 31)   ) || \
                               ((t->u8_month ==  3) && (t->u8_day >= 25) && (t->u8_dayOfWeek == 0) && (t->u8_hour >=  2)) || \
                               ((t->u8_month == 10) && (t->u8_day >= 25) && (t->u8_dayOfWeek == 0) && (t->u8_hour <   2))    )

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  t_event_enum  t_event ;
  PID           t_procId ;
  void *        pt_instance ;
} RTC_client_struct ;

static RTC_client_struct * pt_client   = NULL;
static PID                 t_processId ;

////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static PROCESS  RTC_Process       (void) ;
static void     RTC_ReadDateTime  (RTC_DateTime_struct       * const t_curDateTime) ;
static void     RTC_WriteDateTime (RTC_DateTime_struct const * const t_curDateTime) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

RTC_status RTC_Initialize (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       RTC initialisation routing                                 //
//                 - Initializes the clock                                    //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  RTC_status result = RTC_OK ;

  if (result == RTC_OK)
  {
    // Allocate memory for this instance
    pt_client = getmem (RTC_MAX_EVENTS * sizeof(RTC_client_struct)) ;
    if (pt_client == NULL)
    {
      (void)xc_printf ("RTC_Initialize: Memory error.\n") ;
      result = RTC_ERR_MEMORY ;
    }
  }

  if (result == RTC_OK)
  {
    unsigned char u8_index ;

    // Clear all events
    for (u8_index = 0; u8_index < RTC_MAX_EVENTS; u8_index ++)
    {
      pt_client[u8_index].t_procId = NULL ;
    }

    // Switch on the RTC
    RTC_CTRL = 0 ;

    // Create the instance task
    t_processId = KE_TaskCreate ( (procptr)RTC_Process, // Function
                                  256,                  // Stack size
                                  10,                   // Priority
                                  "RTC_Process",        // Name
                                  0 ) ;                 // Number of arguments

    if (t_processId == 0)
    {
      // Clean up
      (void)freemem (pt_client, RTC_MAX_EVENTS * sizeof(RTC_client_struct)) ;
      pt_client = NULL ;

      (void)xc_printf ("RTC_Initialize: Process error (create).\n") ;
      result = RTC_ERR_PROCESS ;
    }
  }

  if (result == RTC_OK)
  {
    if ( KE_TaskResume(t_processId) == SYSERR)
    {
      // Clean up
      (void)KE_TaskDelete (t_processId) ;
      (void)freemem (pt_client, RTC_MAX_EVENTS * sizeof(RTC_client_struct)) ;
      pt_client = NULL ;

      (void)xc_printf ("RTC_Initialize: Process error (resume).\n") ;
      result = RTC_ERR_PROCESS ;
    }
  }

  return (result) ;
}
// End: RTC_Initialize


RTC_status RTC_Terminate (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       RTC termination routing                                    //
//                 - Terminates the clock                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  if (pt_client != NULL)
  {
    // Kill the task
    (void)KE_TaskDelete (t_processId) ;

    // Return the memory to the memory manager
    (void)freemem (pt_client, RTC_MAX_EVENTS * sizeof(RTC_client_struct)) ;
    pt_client = NULL ;
  }

  return (RTC_OK) ;
}
// End: RTC_Terminate


RTC_status RTC_GetTime (unsigned long * const u32_seconds)
{
         RTC_DateTime_struct  t_currentTime ;
         unsigned long        u32_currSeconds ;
  static unsigned long        u32_lastSeconds = 0 ;

  RTC_ReadDateTime (&t_currentTime) ;

  RTC_Date2Seconds (&t_currentTime, &u32_currSeconds) ;

  if (u32_currSeconds >= u32_lastSeconds)
  {
    u32_lastSeconds = u32_currSeconds ;
  }

  *u32_seconds = u32_lastSeconds ;

  return ;
}
// End: RTC_GetTime


RTC_status RTC_AddClient (t_event_enum         const t_eventType,
                          PID                  const t_clientProcId,
                          void         const * const pt_clientInstance)
{
  RTC_status    result   = RTC_OK ;
  unsigned char u8_index = 0 ;

  if (result == RTC_OK)
  {
    if (pt_client == NULL)
    {
      (void)xc_printf ("RTC_AddClient: Not initialized.\n") ;
      result = RTC_ERR_NOTINIT ;
    }
  }

  if (result == RTC_OK)
  {
    while ( (pt_client[u8_index].t_procId != NULL) &&
            (u8_index < RTC_MAX_EVENTS           )    )
    {
      u8_index ++ ;
    }

    if (u8_index >= RTC_MAX_EVENTS)
    {
      (void)xc_printf ("RTC_AddClient: No free slot.\n") ;
      result = RTC_ERR_NOFREESLOT ;
    }
  }

  if (result == RTC_OK)
  {
    pt_client[u8_index].t_event     = t_eventType ;
    pt_client[u8_index].t_procId    = t_clientProcId ;
    pt_client[u8_index].pt_instance = pt_clientInstance ;
  }

  return (result) ;
}


RTC_status  RTC_RemoveClient (PID                  const t_clientProcId,
                              void         const * const pt_clientInstance)
{
  RTC_status    result   = RTC_OK ;
  unsigned char u8_index = 0 ;

  if (result == RTC_OK)
  {
    if (pt_client == NULL)
    {
      (void)xc_printf ("RTC_RemoveClient: Not initialized.\n") ;
      result = RTC_ERR_NOTINIT ;
    }
  }

  if (result == RTC_OK)
  {
    while ( ( (pt_client[u8_index].t_procId    != t_clientProcId   ) ||
              (pt_client[u8_index].pt_instance != pt_clientInstance)    )&&
            (u8_index < RTC_MAX_EVENTS                                  )    )
    {
      u8_index ++ ;
    }

    if (u8_index >= RTC_MAX_EVENTS)
    {
      (void)xc_printf ("RTC_RemoveClient: Client Pid/Instance not found.\n") ;
      result = RTC_ERR_NOTFOUND ;
    }
  }

  if (result == RTC_OK)
  {
    pt_client[u8_index].t_procId = NULL ;
  }

  return (result) ;
}

void RTC_Seconds2Date (unsigned long         const u32_seconds,
                       RTC_DateTime_struct * const pt_dateTime)
{
  RTC_Seconds2UTC (u32_seconds + RTC_LOCAL, pt_dateTime) ;
  if (RTC_DST(pt_dateTime) != FALSE)
  {
    RTC_Seconds2UTC (u32_seconds + RTC_LOCAL + RTC_SECS_PER_HOUR, pt_dateTime) ;
    pt_dateTime->b_daylightSavingTime = TRUE ;
  }
  else
  {
    pt_dateTime->b_daylightSavingTime = FALSE ;
  }

  return ;
}

void RTC_Seconds2UTC (unsigned long         const u32_seconds,
                      RTC_DateTime_struct * const pt_dateTime)
{
  unsigned long   u32_totalDays ;
  unsigned short  u16_daysPerYear ;
  unsigned char   u8_daysPerMonth ;

  pt_dateTime->u8_second    =  u32_seconds % RTC_SECS_PER_MIN ;
  pt_dateTime->u8_minute    = (u32_seconds % RTC_SECS_PER_HOUR) / RTC_SECS_PER_MIN ;
  pt_dateTime->u8_hour      = (u32_seconds % RTC_SECS_PER_DAY ) / RTC_SECS_PER_HOUR ;
  u32_totalDays             = (u32_seconds / RTC_SECS_PER_DAY ) + 1UL ;     // 1-1-1970 = day 0
  pt_dateTime->u8_dayOfWeek = (u32_totalDays + 4UL) % 7UL ;       // 0 = Sunday

  // Transfer days to years
  pt_dateTime->u16_year = 1970 ;
  do
  {
    if (RTC_LEAPYEAR(pt_dateTime->u16_year))
    {
      u16_daysPerYear = 366 ;
    }
    else
    {
      u16_daysPerYear = 365 ;
    }

    if (u32_totalDays > u16_daysPerYear)
    {
      u32_totalDays -= u16_daysPerYear ;
      pt_dateTime->u16_year ++ ;
    }
  } while (u32_totalDays > u16_daysPerYear) ;

  // Transfer days to months
  pt_dateTime->u8_month = 1 ;
  do
  {
    switch (pt_dateTime->u8_month)
    {
      case 1:   case 3:   case 5:   case 7:   case 8:   case 10: case 12:
        u8_daysPerMonth = 31 ;
        break ;

      case 4:   case 6:   case 9:   case 11:
        u8_daysPerMonth = 30 ;
        break ;

      default:
        if (RTC_LEAPYEAR(pt_dateTime->u16_year))
        {
          u8_daysPerMonth = 29 ;
        }
        else
        {
          u8_daysPerMonth = 28 ;
        }
        break ;
    }

    if (u32_totalDays > u8_daysPerMonth)
    {
      u32_totalDays -= u8_daysPerMonth ;
      pt_dateTime->u8_month ++ ;
    }
  } while (u32_totalDays > u8_daysPerMonth) ;

  // Fill out the remaining days
  pt_dateTime->u8_day = u32_totalDays ;

  return ;
}


void RTC_Date2Seconds (RTC_DateTime_struct const * const pt_dateTime,
                       unsigned long             * const pu32_seconds)
{
  unsigned short  u16_totalDays = pt_dateTime->u8_day - 1 ; // 1-1-1970 = day 0
  unsigned char   u8_monthCntr  = 1 ;
  unsigned short  u16_yearCntr  = 1970 ;

  while ( (u8_monthCntr != pt_dateTime->u8_month) ||
          (u16_yearCntr != pt_dateTime->u16_year)    )
  {
    switch (u8_monthCntr)
    {
      case 1:   case 3:   case 5:   case 7:   case 8:   case 10: case 12:
        u16_totalDays += 31 ;
        break ;

      case 4:   case 6:   case 9:   case 11:
        u16_totalDays += 30 ;
        break ;

      default:
        if (RTC_LEAPYEAR(u16_yearCntr))
        {
          u16_totalDays += 29 ;
        }
        else
        {
          u16_totalDays += 28 ;
        }
        break ;
    }

    if (u8_monthCntr < 12)
    {
      u8_monthCntr ++ ;
    }
    else
    {
      u8_monthCntr = 1 ;
      u16_yearCntr ++ ;
    }
  }

  *pu32_seconds = (unsigned long)u16_totalDays          * RTC_SECS_PER_DAY  +
                  (unsigned long)pt_dateTime->u8_hour   * RTC_SECS_PER_HOUR +
                  (unsigned long)pt_dateTime->u8_minute * RTC_SECS_PER_MIN  +
                  (unsigned long)pt_dateTime->u8_second ;

  return ;
}


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////
static PROCESS RTC_Process (void)
{
  enum
  {
    e_booted,
    e_synchronizing,
    e_waiting
  } t_state = e_booted ;

  TMR_ticks_struct    t_syncTimeOut ;
  RTC_DateTime_struct t_oldDateTime ;
  RTC_DateTime_struct t_curDateTime ;
  unsigned char       u8_index ;


  unsigned long       u32_curTime ;
  unsigned long       u32_oldTime ;
  unsigned long       u32_curDateTimeSecs ;
  unsigned long       u32_oldDateTimeSecs ;

  // Fill the edge-detection buffer
  RTC_GetTime (&u32_oldTime) ;
  RTC_Seconds2Date (u32_oldTime, &t_oldDateTime) ;

  for (;;)
  {
    ////// RTC event generation //////
    RTC_GetTime (&u32_curTime) ;

    // Check if the date/time has increased (this prevents double triggers if sync has set back the clock)
    if (u32_curTime > u32_oldTime)
    {
      RTC_Seconds2Date (u32_curTime, &t_curDateTime) ;

      // Send out the 'second-events'
      for (u8_index = 0; u8_index < RTC_MAX_EVENTS; u8_index ++)
      {
        if ( (pt_client[u8_index].t_event  == e_secondEvent) &&
             (pt_client[u8_index].t_procId != NULL         )    )
        {
          (void)KE_MBoxSend (pt_client[u8_index].t_procId, pt_client[u8_index].pt_instance) ;
        }
      }

      // Send out the 'minute-events'
      if (t_curDateTime.u8_minute != t_oldDateTime.u8_minute)
      {
        for (u8_index = 0; u8_index < RTC_MAX_EVENTS; u8_index ++)
        {
          if ( (pt_client[u8_index].t_event  == e_minuteEvent) &&
               (pt_client[u8_index].t_procId != NULL         )    )
          {
            (void)KE_MBoxSend (pt_client[u8_index].t_procId, pt_client[u8_index].pt_instance) ;
          }
        }
      }

      // Send out the 'hour-events'
      if (t_curDateTime.u8_hour != t_oldDateTime.u8_hour)
      {
        for (u8_index = 0; u8_index < RTC_MAX_EVENTS; u8_index ++)
        {
          if ( (pt_client[u8_index].t_event  == e_hourEvent) &&
               (pt_client[u8_index].t_procId != NULL         )    )
          {
            (void)KE_MBoxSend (pt_client[u8_index].t_procId, pt_client[u8_index].pt_instance) ;
          }
        }
      }

      // Send out the 'day-events'
      if (t_curDateTime.u8_day != t_oldDateTime.u8_day)
      {
        for (u8_index = 0; u8_index < RTC_MAX_EVENTS; u8_index ++)
        {
          if ( (pt_client[u8_index].t_event  == e_dayEvent) &&
               (pt_client[u8_index].t_procId != NULL         )    )
          {
            (void)KE_MBoxSend (pt_client[u8_index].t_procId, pt_client[u8_index].pt_instance) ;
          }
        }
      }

      // Update the edge-detection buffer
      u32_oldTime   = u32_curTime ;
      t_oldDateTime = t_curDateTime ;
    }

    ////// RTC to Xinu synchonisation //////
    switch (t_state)
    {
      case e_booted:
        // Fetch the current time and check if it could be valid
        KE_TaskGetTime (&u32_curDateTimeSecs) ;
        if (u32_curDateTimeSecs > 0x40000000UL)
        {
          // copy the value into the edge-detection buffer
          u32_oldDateTimeSecs = u32_curDateTimeSecs ;

          // Proceed to the next state
          t_state = e_synchronizing ;
        }
        break ;

      case e_synchronizing:
        // Fetch the current time and check for an edge
        KE_TaskGetTime (&u32_curDateTimeSecs) ;
        if (u32_curDateTimeSecs != u32_oldDateTimeSecs)
        {
          // Convert the fetched time to a RTC_DateTime_struct
          RTC_Seconds2UTC (u32_curDateTimeSecs, &t_curDateTime) ;

          // Write the RTC_DateTime_struct to the RTC
          RTC_WriteDateTime (&t_curDateTime) ;

          // Setup a timer for the next sync
          TMR_SetTimeout (&t_syncTimeOut, TMR_HOUR) ;

          // Proceed to the next state
          t_state = e_waiting ;
        }
        break ;

      case e_waiting:
        // Check if the timeout has expired
        if (TMR_CheckTimeout(&t_syncTimeOut))
        {
          // Fetch the current time
          KE_TaskGetTime (&u32_curDateTimeSecs) ;

          // copy the value into the edge-detection buffer
          u32_oldDateTimeSecs = u32_curDateTimeSecs ;

          // Proceed to the previous state
          t_state = e_synchronizing ;
        }
        break ;

      default:
        break ;
    }

    // Sleep for 100ms; We don't need sub-second accuracy
    KE_TaskSleep10(1);
  }
}


static void RTC_ReadDateTime (RTC_DateTime_struct * const pt_curDateTime)
{
  unsigned char u8_tempMins ;

  do
  {
    u8_tempMins = RTC_MIN ;
    pt_curDateTime->u8_second    = RTC_SEC ;
    pt_curDateTime->u8_minute    = RTC_MIN ;
    pt_curDateTime->u8_hour      = RTC_HRS ;
    pt_curDateTime->u8_day       = RTC_DOM ;
    pt_curDateTime->u8_dayOfWeek = RTC_DOW - 1 ;
    pt_curDateTime->u8_month     = RTC_MON ;
    pt_curDateTime->u16_year     = (unsigned short)RTC_YR + 100 * (unsigned short)RTC_CEN ;
  } while (RTC_MIN != u8_tempMins) ;

  return ;
}


static void RTC_WriteDateTime (RTC_DateTime_struct const * const pt_curDateTime)
{
  unsigned char u8_tempMins ;

  RTC_CTRL = 1 ;
  RTC_SEC  = pt_curDateTime->u8_second ;
  RTC_MIN  = pt_curDateTime->u8_minute ;
  RTC_HRS  = pt_curDateTime->u8_hour ;
  RTC_DOM  = pt_curDateTime->u8_day ;
  RTC_DOW  = pt_curDateTime->u8_dayOfWeek + 1 ;
  RTC_MON  = pt_curDateTime->u8_month ;
  RTC_YR   = pt_curDateTime->u16_year % 100 ;
  RTC_CEN  = pt_curDateTime->u16_year / 100 ;
  RTC_CTRL = 0 ;

  return ;
}
