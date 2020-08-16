////////////////////////////////////////////////////////////////////////////////
// File    : PHD_PulseHandler.c
// Function: Pulse Handler module
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define PHD_PULSEHANDLER_C
#include <kernel.h>
#include "PHD_PulseHandler.h"

#include "TMR_Timer.h"

#define PHD_SIGNATURE         ('PHD')
#define PHD_MAX_EVENTS        (5)
#define PHD_TICKS_PER_MINUTE  (60000UL)
#define PHD_DEBOUNCE_FACTOR   (4UL)

#define PHD_PTR_INVALID(p)    (p->u24_signature != PHD_SIGNATURE)

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  unsigned int        u24_signature ;
  PID                 pt_storageProcId ;
  PID                 pt_displayProcId[PHD_MAX_EVENTS] ;
  unsigned long       u32_debounceTime ;
  TMR_ticks_struct    u32_debounceTimeOut ;
  unsigned int        u24_pulses ;
  unsigned int        u24_pulsesSend ;
  unsigned int        u24_pulsesPerMinute ;
  unsigned short      u16_pulseQueueSize ;
  unsigned short      u16_pulseQueueHead ;
  unsigned short      u16_pulseQueueTail ;
  TMR_ticks_struct*   at_pulseQueue ;
  PID                 t_processId ;
} PHD_instance_struct ;


////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static PROCESS PHD_Process (PHD_handle const pt_instance) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

PHD_status PHD_Create (PHD_handle          * const ppt_instance,
                       unsigned short        const u16_maxPulsesPerMinute)
////////////////////////////////////////////////////////////////////////////////
// Function:       Pulse handler construction routine                         //
//                 - Creates an instance                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status            result      = PHD_OK ;
  PHD_instance_struct * pt_this ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (ppt_instance == NULL)
    {
      (void)xc_printf ("PHD_Create: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Allocate memory for this instance
    pt_this = getmem (sizeof(PHD_instance_struct)) ;
    if (pt_this == NULL)
    {
      (void)xc_printf ("PHD_Create: Memory error (instance).\n") ;
      result = PHD_ERR_MEMORY ;
    }
  }

  if (result == PHD_OK)
  {
    unsigned char u8_index ;

    // Initialize global variables of this instance
    pt_this->u24_signature        = PHD_SIGNATURE ;
    pt_this->pt_storageProcId     = NULL ;
    pt_this->u32_debounceTime     = (PHD_TICKS_PER_MINUTE / u16_maxPulsesPerMinute) / PHD_DEBOUNCE_FACTOR ;
    pt_this->u24_pulses           = 0 ;
    pt_this->u24_pulsesSend       = 0 ;
    pt_this->u24_pulsesPerMinute  = 0 ;
    pt_this->u16_pulseQueueSize   = u16_maxPulsesPerMinute + 1 ;
    pt_this->u16_pulseQueueHead   = 0 ;
    pt_this->u16_pulseQueueTail   = 0 ;
    TMR_SetTimeout (&(pt_this->u32_debounceTimeOut), 0) ;

    // Clear all events
    for (u8_index = 0; u8_index < PHD_MAX_EVENTS; u8_index ++)
    {
      pt_this->pt_displayProcId[u8_index] = NULL ;
    }

    // Allocate memory pulse ages
    pt_this->at_pulseQueue = getmem (pt_this->u16_pulseQueueSize * sizeof(TMR_ticks_struct)) ;
    if (pt_this->at_pulseQueue == NULL)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this, sizeof(PHD_instance_struct)) ;

      (void)xc_printf ("PHD_Create: Memory error (ages).\n") ;
      result = PHD_ERR_MEMORY ;
    }
  }

  if (result == PHD_OK)
  {
    // Create the instance task
    pt_this->t_processId = KE_TaskCreate ( (procptr)PHD_Process,  // Function
                                           256,                   // Stack size
                                           10,                    // Priority
                                           "PHD_Process",         // Name
                                           1,                     // Number of arguments
                                           pt_this ) ;            // Arg...

    if (pt_this->t_processId == 0)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this->at_pulseQueue, pt_this->u16_pulseQueueSize * sizeof(TMR_ticks_struct)) ;
      (void)freemem (pt_this, sizeof(PHD_instance_struct)) ;

      (void)xc_printf ("PHD_Create: Process error (create).\n") ;
      result = PHD_ERR_PROCESS ;
    }
  }

  if (result == PHD_OK)
  {
    if ( KE_TaskResume(pt_this->t_processId) == SYSERR)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)KE_TaskDelete (pt_this->t_processId) ;
      (void)freemem (pt_this->at_pulseQueue, pt_this->u16_pulseQueueSize * sizeof(TMR_ticks_struct)) ;
      (void)freemem (pt_this, sizeof(PHD_instance_struct)) ;

      (void)xc_printf ("PHD_Create: Process error (resume).\n") ;
      result = PHD_ERR_PROCESS ;
    }
  }

  if (result == PHD_OK)
  {
    // Fill out the instance pointer
    *ppt_instance = pt_this ;
  }

  return (result) ;
}
// End: PHD_Create


PHD_status PHD_Delete (PHD_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       Pulse handler destruction routine                          //
//                 - Destroys an instance                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result  = PHD_OK ;
  PHD_instance_struct * const pt_this = pt_instance ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_Delete: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Check if the pointer is valid
    if (PHD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("PHD_Delete: Invalid pointer.\n") ;
      result = PHD_ERR_POINTER ;
    }
  }

  if (result == PHD_OK)
  {
    // Kill the task
    (void)KE_TaskDelete (pt_this->t_processId) ;

    // Invalidate the pointer
    pt_this->u24_signature = 0x000000 ;

    // Return the memory to the memory manager
    (void)freemem (pt_this->at_pulseQueue, (pt_this->u16_pulseQueueSize) * sizeof(TMR_ticks_struct)) ;
    (void)freemem (pt_this, sizeof(PHD_instance_struct)) ;
  }

  return (result) ;
}
// End: PHD_Delete


PHD_status PHD_SetStorageClient (PHD_handle const pt_instance,
                                 PID        const t_clientProcId)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_SetStorageClient                                       //
//                 - Set the process which will store the pulses              //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result  = PHD_OK ;
  PHD_instance_struct * const pt_this = pt_instance ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_Delete: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Check if the pointer is valid
    if (PHD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("PHD_Delete: Invalid pointer.\n") ;
      result = PHD_ERR_POINTER ;
    }
  }

  if (result == PHD_OK)
  {
    pt_this->pt_storageProcId = t_clientProcId ;
  }

  return (result) ;
}
// End: PHD_SetStorageClient


PHD_status PHD_HandlePulse (PHD_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_HandlePulse                                            //
//                 - Process an incomming pulse (INTERRUPT CONTEXT!)          //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result  = PHD_OK ;
  PHD_instance_struct * const pt_this = pt_instance ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_HandlePulse: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Check if the pointer is valid
    if (PHD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("PHD_HandlePulse: Invalid pointer.\n") ;
      result = PHD_ERR_POINTER ;
    }
  }

  if (result == PHD_OK)
  {
    // Debounce the pulse
    if ( TMR_CheckTimeout(&(pt_this->u32_debounceTimeOut)) )
    {
      // Add a time stamp to the queue for this pulse
      TMR_SetTimeStamp (&(pt_this->at_pulseQueue[pt_this->u16_pulseQueueHead])) ;

      // Set new debounce timeout
      TMR_SetTimeout (&(pt_this->u32_debounceTimeOut), pt_this->u32_debounceTime) ;

      // Increase the pulse buffer ;
      pt_this->u24_pulses ++ ;

      // Increase the queue head
      pt_this->u16_pulseQueueHead ++ ;
      if (pt_this->u16_pulseQueueHead >= pt_this->u16_pulseQueueSize)
      {
        pt_this->u16_pulseQueueHead = 0 ;
      }

      // Check for a queue overflow
      if (pt_this->u16_pulseQueueHead == pt_this->u16_pulseQueueTail)
      {
        // Increase the queue tail
        pt_this->u16_pulseQueueTail ++ ;
        if (pt_this->u16_pulseQueueTail >= pt_this->u16_pulseQueueSize)
        {
          pt_this->u16_pulseQueueTail = 0 ;
        }
      }
    }
  }

  return (result) ;
}
// End: PHD_HandlePulse


PHD_status PHD_AddClient (PHD_handle const pt_instance,
                          PID        const t_clientProcId)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_AddClient                                              //
//                 - Subscribe a process to the event of changing nr of       //
//                   pulses per minute.                                       //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result   = PHD_OK ;
  PHD_instance_struct * const pt_this  = pt_instance ;
  unsigned char               u8_index = 0 ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_AddClient: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Find an empty slot
    while ( (pt_this->pt_displayProcId[u8_index] != NULL          ) &&
            (u8_index                             < PHD_MAX_EVENTS)    )
    {
      u8_index ++ ;
    }

    if (u8_index >= PHD_MAX_EVENTS)
    {
      (void)xc_printf ("PHD_AddClient: No free slot.\n") ;
      result = PHD_ERR_NOFREESLOT ;
    }
  }

  if (result == PHD_OK)
  {
    // Fill out the slot
    pt_this->pt_displayProcId[u8_index] = t_clientProcId ;
  }

  return (result) ;
}
// End: PHD_AddClient


PHD_status PHD_RemoveClient (PHD_handle const pt_instance,
                             PID        const t_clientProcId)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_RemoveClient                                           //
//                 - Unsubscribe a process from the event of changing nr of   //
//                   pulses per minute.                                       //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result   = PHD_OK ;
  PHD_instance_struct * const pt_this  = pt_instance ;
  unsigned char               u8_index = 0 ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_RemoveClient: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Find the slot holding the specified process id
    while ( (pt_this->pt_displayProcId[u8_index] != t_clientProcId) &&
            (u8_index < PHD_MAX_EVENTS                            )    )
    {
      u8_index ++ ;
    }

    if (u8_index >= PHD_MAX_EVENTS)
    {
      (void)xc_printf ("PHD_RemoveClient: Client Pid/Instance not found.\n") ;
      result = PHD_ERR_NOTFOUND ;
    }
  }

  if (result == PHD_OK)
  {
    // Empty the slot
    pt_this->pt_displayProcId[u8_index] = NULL ;
  }

  return (result) ;
}
// End: PHD_RemoveClient


PHD_status PHD_GetPulsesPerMinute (PHD_handle     const pt_instance,
                                   unsigned int * const pu24_pulsesPerMinute)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_GetPulsesPerMinute                                     //
//                 - Retrieve the current nr of pulses per minute.            //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result  = PHD_OK ;
  PHD_instance_struct * const pt_this = pt_instance ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_GetPulsesPerMinute: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Check if the pointer is valid
    if (PHD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("PHD_GetPulsesPerMinute: Invalid pointer.\n") ;
      result = PHD_ERR_POINTER ;
    }
  }

  if (result == PHD_OK)
  {
    // Fill out the current number of pulses
    *pu24_pulsesPerMinute = pt_this->u24_pulsesPerMinute ;
  }

  return (result) ;
}
// End: PHD_GetPulsesPerMinute


PHD_status  PHD_GetPulses (PHD_handle            const pt_instance,
                           unsigned int        * const pu24_pulses)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_GetPulses                                              //
//                 - Retrieve the nr of metered pulses since last time        //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_status                  result  = PHD_OK ;
  PHD_instance_struct * const pt_this = pt_instance ;

  if (result == PHD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("PHD_GetPulses: Parameter error.\n") ;
      result = PHD_ERR_PARAM ;
    }
  }

  if (result == PHD_OK)
  {
    // Check if the pointer is valid
    if (PHD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("PHD_GetPulses: Invalid pointer.\n") ;
      result = PHD_ERR_POINTER ;
    }
  }

  if (result == PHD_OK)
  {
    // Begin of critical region: No interrupts, no task switches
    KE_CriticalBegin () ;

    // Fill out the number of metered pulses
    *pu24_pulses = pt_this->u24_pulses ;
    // Reset the meter counter
    pt_this->u24_pulses = 0 ;
    // Reset the meter counter change-detection-buffer
    pt_this->u24_pulsesSend = 0 ;

    // En of critical region
    KE_CriticalEnd () ;
  }

  return (result) ;
}
// End: PHD_GetPulses


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////

static PROCESS PHD_Process (PHD_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       PHD_Process                                                //
//                 - Periodically check if metered data has change and if so  //
//                   send events to clients                                   //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PHD_instance_struct * const pt_this = pt_instance ;
  unsigned char               u8_index ;
  unsigned int                u24_pulsesPerMinute ;

  for (;;)
  {
    KE_CriticalBegin () ;

    // Remove old time stamps from the queue
    while ( (pt_this->u16_pulseQueueHead != pt_this->u16_pulseQueueTail                                      ) &&
            (TMR_TimeStampAge (&(pt_this->at_pulseQueue[pt_this->u16_pulseQueueTail])) > PHD_TICKS_PER_MINUTE)    )
    {
      // Increase the queue tail
      pt_this->u16_pulseQueueTail ++ ;
      if (pt_this->u16_pulseQueueTail >= pt_this->u16_pulseQueueSize)
      {
        pt_this->u16_pulseQueueTail = 0 ;
      }
    }

    // Calculate the current number of timestamps in the queue
    if (pt_this->u16_pulseQueueHead >= pt_this->u16_pulseQueueTail)
    {
      u24_pulsesPerMinute = pt_this->u16_pulseQueueHead - pt_this->u16_pulseQueueTail ;
    }
    else
    {
      u24_pulsesPerMinute = (pt_this->u16_pulseQueueHead + pt_this->u16_pulseQueueSize) - pt_this->u16_pulseQueueTail ;
    }

    // Begin of critical region: No interrupts, no task switches
    KE_CriticalEnd () ;

    // Send a message if the number of pulses per minute has changed
    if (pt_this->u24_pulsesPerMinute != u24_pulsesPerMinute)
    {
      pt_this->u24_pulsesPerMinute = u24_pulsesPerMinute ;

      for (u8_index = 0; u8_index < PHD_MAX_EVENTS; u8_index ++)
      {
        if (pt_this->pt_displayProcId[u8_index] != NULL)
        {
          (void)KE_MBoxSend (pt_this->pt_displayProcId[u8_index], pt_this) ;
        }
      }
    }

    // Send a message if the number of pulses has changed
    if (pt_this->u24_pulsesSend != pt_this->u24_pulses)
    {
      pt_this->u24_pulsesSend = pt_this->u24_pulses ;

      if (pt_this->pt_storageProcId != NULL)
      {
        (void)KE_MBoxSend (pt_this->pt_storageProcId, pt_this) ;
      }
    }

    KE_TaskSleep(0);
  }

  return ;
}
