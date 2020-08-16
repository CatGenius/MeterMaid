////////////////////////////////////////////////////////////////////////////////
// File    : BMM_BucketMemory.c
// Function:
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define BMM_BUCKETMEMORY_C
#include <kernel.h>
#include "BMM_BucketMemory.h"

#include "TMR_Timer.h"
#include "RTC_RealTimeClock.h"

#define BMM_SIGNATURE         ('BMM')
#define BMM_MAX_EVENTS        (5)
#define BMM_PTR_INVALID(p)    (p->u24_signature != BMM_SIGNATURE)

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  unsigned int        u24_signature ;                     // Signature to easily validate pointers
  unsigned short      u16_nrOfBuckets ;                   // Number of buckets allocated
  unsigned short      u16_firstBucket ;                   // Number of first (= currently filled) bucket
  unsigned short      u16_lastBucket ;                    // Number of the last filled bucket
  BMM_bucket*         at_pulseBucket ;                    // Pointer to buckets
  fetchFunction       func_fetchPulses ;                  // Pointer to the fuction for retrieving received pulses
  unsigned int        u24_fetchedPulses ;                 // Fetched number of pulses, stored for slave processes
  PID                 t_pulseProcessId ;                  // Process to send events to if new pulses have been received
  PID                 t_bucketProcessId ;                 // Process to send events to in order to change the bucket
  PID                 t_slaveProcessId ;                  // Send an event to this process if new pulses have been fetched
  PID                 t_clientProcessId[BMM_MAX_EVENTS] ; // Send an event to these processes if measurement data has changed
} BMM_instance_struct ;


////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static PROCESS BMM_pulseProcess  (BMM_handle const pt_instance) ;
static PROCESS BMM_bucketProcess (BMM_handle const pt_instance) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

BMM_status BMM_Create          (BMM_handle     * const ppt_instance,
                                unsigned short   const u16_nrOfBuckets)
////////////////////////////////////////////////////////////////////////////////
// Function:       Pulse handler construction routine                         //
//                 - Creates an instance                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status            result      = BMM_OK ;
  BMM_instance_struct * pt_this ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if ( (ppt_instance    == NULL) ||
         (u16_nrOfBuckets == 0   )    )
    {
      (void)xc_printf ("BMM_Create: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Allocate memory for this instance
    pt_this = getmem (sizeof(BMM_instance_struct)) ;
    if (pt_this == NULL)
    {
      (void)xc_printf ("BMM_Create: Memory error (instance).\n") ;
      result = BMM_ERR_MEMORY ;
    }
  }

  if (result == BMM_OK)
  {
    unsigned char u8_index ;

    // Initialize global variables of this instance
    pt_this->u24_signature        = BMM_SIGNATURE ;
    pt_this->u16_nrOfBuckets      = u16_nrOfBuckets + 1 ;
    pt_this->u16_firstBucket      = 0 ;
    pt_this->u16_lastBucket       = 0 ;
    pt_this->u24_fetchedPulses    = 0 ;
    pt_this->func_fetchPulses     = NULL ;
    pt_this->t_slaveProcessId     = NULL ;

    // Clear all events
    for (u8_index = 0; u8_index < BMM_MAX_EVENTS; u8_index ++)
    {
      pt_this->t_clientProcessId[u8_index] = NULL ;
    }

    // Allocate memory for buckets
    pt_this->at_pulseBucket = getmem (pt_this->u16_nrOfBuckets * sizeof(BMM_bucket)) ;
    if (pt_this->at_pulseBucket == NULL)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this, sizeof(BMM_instance_struct)) ;

      (void)xc_printf ("BMM_Create: Memory error (buckets).\n") ;
      result = BMM_ERR_MEMORY ;
    }
  }

  if (result == BMM_OK)
  {
    // Empty the current bucket
    pt_this->at_pulseBucket[pt_this->u16_firstBucket].u24_value = 0 ;
    // Timestamp the current bucket
    RTC_GetTime (&(pt_this->at_pulseBucket[pt_this->u16_firstBucket].u32_timeStamp)) ;

    // Create the pulse process task
    pt_this->t_pulseProcessId = KE_TaskCreate ( (procptr)BMM_pulseProcess,  // Function
                                                256,                        // Stack size
                                                10,                         // Priority
                                                "BMM_pulseCtr",             // Name
                                                1,                          // Number of arguments
                                                pt_this ) ;                 // Arg...

    if (pt_this->t_pulseProcessId == 0)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this->at_pulseBucket, pt_this->u16_nrOfBuckets * sizeof(BMM_bucket)) ;
      (void)freemem (pt_this, sizeof(BMM_instance_struct)) ;

      (void)xc_printf ("BMM_Create: Process error (create - pulse).\n") ;
      result = BMM_ERR_PROCESS ;
    }
  }

  if (result == BMM_OK)
  {
    // Create the bucket change task
    pt_this->t_bucketProcessId = KE_TaskCreate ( (procptr)BMM_bucketProcess,  // Function
                                                 256,                         // Stack size
                                                 10,                          // Priority
                                                 "BMM_bucketPr",              // Name
                                                 1,                           // Number of arguments
                                                 pt_this ) ;                  // Arg...

    if (pt_this->t_bucketProcessId == 0)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)KE_TaskDelete (pt_this->t_pulseProcessId) ;
      (void)freemem (pt_this->at_pulseBucket, pt_this->u16_nrOfBuckets * sizeof(BMM_bucket)) ;
      (void)freemem (pt_this, sizeof(BMM_instance_struct)) ;

      (void)xc_printf ("BMM_Create: Process error (create - pulse).\n") ;
      result = BMM_ERR_PROCESS ;
    }
  }

  if (result == BMM_OK)
  {
    if ( KE_TaskResume(pt_this->t_pulseProcessId) == SYSERR)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)KE_TaskDelete (pt_this->t_bucketProcessId) ;
      (void)KE_TaskDelete (pt_this->t_pulseProcessId) ;
      (void)freemem (pt_this->at_pulseBucket, pt_this->u16_nrOfBuckets * sizeof(BMM_bucket)) ;
      (void)freemem (pt_this, sizeof(BMM_instance_struct)) ;

      (void)xc_printf ("BMM_Create: Process error (resume - pulse).\n") ;
      result = BMM_ERR_PROCESS ;
    }
  }

  if (result == BMM_OK)
  {
    if ( KE_TaskResume(pt_this->t_bucketProcessId) == SYSERR)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)KE_TaskDelete (pt_this->t_bucketProcessId) ;
      (void)KE_TaskDelete (pt_this->t_pulseProcessId) ;
      (void)freemem (pt_this->at_pulseBucket, pt_this->u16_nrOfBuckets * sizeof(BMM_bucket)) ;
      (void)freemem (pt_this, sizeof(BMM_instance_struct)) ;

      (void)xc_printf ("BMM_Create: Process error (resume - pulse).\n") ;
      result = BMM_ERR_PROCESS ;
    }
  }

  if (result == BMM_OK)
  {
    // Fill out the instance pointer
    *ppt_instance = pt_this ;
  }

  return (result) ;
}
// End: BMM_Create


BMM_status BMM_Delete (BMM_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       Pulse handler destruction routine                          //
//                 - Destroys an instance                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("BMM_Delete: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_Delete: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Kill the tasks
    (void)KE_TaskDelete (pt_this->t_bucketProcessId) ;
    (void)KE_TaskDelete (pt_this->t_pulseProcessId) ;

    // Invalidate the pointer
    pt_this->u24_signature = 0x000000 ;

    // Return the memory to the memory manager
    (void)freemem (pt_this->at_pulseBucket, pt_this->u16_nrOfBuckets * sizeof(BMM_bucket)) ;
    (void)freemem (pt_this, sizeof(BMM_instance_struct)) ;
  }

  return (result) ;
}
// End: BMM_Delete


BMM_status BMM_SetMeteringFunc (BMM_handle    const pt_instance,
                                fetchFunction const func_GetPulses)
////////////////////////////////////////////////////////////////////////////////
// Function:       BMM_SetMeteringFunc                                        //
//                 - Sets the function to invoke for retieving metered pulses //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("BMM_SetMeteringFunc: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_SetMeteringFunc: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Set the metering-fetch function
    pt_this->func_fetchPulses = func_GetPulses ;
  }

  return (result) ;
}
// End: BMM_SetMeteringFunc


BMM_status BMM_GetMeteringProc (BMM_handle   const pt_instance,
                                PID        * const pt_processId)
////////////////////////////////////////////////////////////////////////////////
// Function:       BMM_GetMeteringProc                                        //
//                 - Retrieves the process ID of the metering process         //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if ( (pt_instance  == NULL) ||
         (pt_processId == NULL)    )
    {
      (void)xc_printf ("BMM_GetMeteringProc: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_GetMeteringProc: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Fill out the process ID
    *pt_processId = pt_this->t_pulseProcessId ;
  }

  return (result) ;
}
// End: BMM_GetMeteringProc


BMM_status BMM_GetBucketProc (BMM_handle   const pt_instance,
                              PID        * const pt_processId)
////////////////////////////////////////////////////////////////////////////////
// Function:       BMM_GetBucketProc                                          //
//                 - Retrieves the process ID of the bucket change process    //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if ( (pt_instance  == NULL) ||
         (pt_processId == NULL)    )
    {
      (void)xc_printf ("BMM_GetMeteringProc: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_GetMeteringProc: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Fill out the process ID
    *pt_processId = pt_this->t_bucketProcessId ;
  }

  return (result) ;
}
// End: BMM_GetBucketProc


BMM_status BMM_SetSlaveProc (BMM_handle const pt_instance,
                             PID        const pt_processId)
////////////////////////////////////////////////////////////////////////////////
// Function:       BMM_SetSlaveProc                                           //
//                 - Sets the process ID to re-send pulse-events to           //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if (pt_instance  == NULL)
    {
      (void)xc_printf ("BMM_SetSlaveProc: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_SetSlaveProc: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Set the process ID
    pt_this->t_slaveProcessId = pt_processId ;
  }

  return (result) ;
}
// End: BMM_SetSlaveProc


BMM_status BMM_GetPulses (BMM_handle     const pt_instance,
                          unsigned int * const pu24_pulses)
////////////////////////////////////////////////////////////////////////////////
// Function:       BMM_GetPulses                                              //
//                 - Function for slave processes to the retrieved nr of pulses //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("BMM_GetPulses: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_GetPulses: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    KE_CriticalBegin () ;

    *pu24_pulses = pt_this->u24_fetchedPulses ;
    pt_this->u24_fetchedPulses = 0 ;

    KE_CriticalEnd () ;
  }

  return (result) ;
}
// End: BMM_GetPulses


BMM_status BMM_AddClient (BMM_handle         const pt_instance,
                          PID                const t_clientProcId)
{
  BMM_status                  result   = BMM_OK ;
  BMM_instance_struct * const pt_this  = pt_instance ;
  unsigned char               u8_index = 0 ;

  if (result == BMM_OK)
  {
    if (pt_instance == NULL)
    {
      (void)xc_printf ("BMM_AddClient: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    while ( (pt_this->t_clientProcessId[u8_index] != NULL          ) &&
            (u8_index                              < BMM_MAX_EVENTS)    )
    {
      u8_index ++ ;
    }

    if (u8_index >= BMM_MAX_EVENTS)
    {
      (void)xc_printf ("BMM_AddClient: No free slot.\n") ;
      result = BMM_ERR_NOFREESLOT ;
    }
  }

  if (result == BMM_OK)
  {
    pt_this->t_clientProcessId[u8_index] = t_clientProcId ;
  }

  return (result) ;
}
// End: BMM_AddClient


BMM_status BMM_RemoveClient (BMM_handle         const pt_instance,
                             PID                const t_clientProcId)
{
  BMM_status                  result   = BMM_OK ;
  BMM_instance_struct * const pt_this  = pt_instance ;
  unsigned char               u8_index = 0 ;

  if (result == BMM_OK)
  {
    if (pt_instance == NULL)
    {
      (void)xc_printf ("BMM_RemoveClient: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    while ( (pt_this->t_clientProcessId[u8_index] != t_clientProcId) &&
            (u8_index < BMM_MAX_EVENTS                             )    )
    {
      u8_index ++ ;
    }

    if (u8_index >= BMM_MAX_EVENTS)
    {
      (void)xc_printf ("BMM_RemoveClient: Client Pid/Instance not found.\n") ;
      result = BMM_ERR_NOTFOUND ;
    }
  }

  if (result == BMM_OK)
  {
    pt_this->t_clientProcessId[u8_index] = NULL ;
  }

  return (result) ;
}
// End: BMM_RemoveClient


BMM_status BMM_GetNrOfBuckets (BMM_handle       const pt_instance,
                               unsigned short * const pu16_nrOfBuckets)
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if ( (pt_instance      == NULL) ||
         (pu16_nrOfBuckets == NULL)    )
    {
      (void)xc_printf ("BMM_GetNrOfBuckets: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_GetNrOfBuckets: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Calculate the current number of timestamps in the queue
    if (pt_this->u16_firstBucket >= pt_this->u16_lastBucket)
    {
      *pu16_nrOfBuckets = pt_this->u16_firstBucket - pt_this->u16_lastBucket ;
    }
    else
    {
      *pu16_nrOfBuckets = (pt_this->u16_firstBucket + pt_this->u16_nrOfBuckets) - pt_this->u16_lastBucket ;
    }
  }

  return (result) ;
}
// End: BMM_GetNrOfBuckets


BMM_status BMM_GetBucketCont (BMM_handle       const pt_instance,
                              unsigned short   const u16_bucketNr,
                              BMM_bucket     * const pt_bucketContents)
{
  BMM_status                  result  = BMM_OK ;
  BMM_instance_struct * const pt_this = pt_instance ;
  BMM_bucket*                 pt_tmpBucket ;

  if (result == BMM_OK)
  {
    // Do parameter check
    if ( (pt_instance       == NULL) ||
         (pt_bucketContents == NULL)    )
    {
      (void)xc_printf ("BMM_GetBucketCont: Parameter error.\n") ;
      result = BMM_ERR_PARAM ;
    }
  }

  if (result == BMM_OK)
  {
    // Check if the pointer is valid
    if (BMM_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("BMM_GetBucketCont: Invalid pointer.\n") ;
      result = BMM_ERR_POINTER ;
    }
  }

  if (result == BMM_OK)
  {
    // Fill out a poiter to work around a compiler, eh, short coming
    pt_tmpBucket = &(pt_this->at_pulseBucket[(pt_this->u16_lastBucket + u16_bucketNr) % pt_this->u16_nrOfBuckets]) ;
    // Copy the bucket into the supplied pointer buffer
    *pt_bucketContents = *pt_tmpBucket ;
  }

  return (result) ;
}
// End: BMM_GetBucketCont


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////
static PROCESS BMM_pulseProcess (BMM_handle const pt_instance)
{
  BMM_instance_struct * const pt_this = pt_instance ;
  void*                       pt_phdInstance ;
  unsigned int                u24_nrOfPulses ;
  BMM_bucket*                 pt_tmpBucket ;

  for (;;)
  {
    // Wait for a new-pulse-event
    pt_phdInstance = KE_MBoxReceive () ;

    // Get the pulse(s)
    if (pt_this->func_fetchPulses != NULL)
    {
      // Fetch the number of metered pulses
      (void)pt_this->func_fetchPulses (pt_phdInstance, &u24_nrOfPulses) ;

      KE_CriticalBegin () ;

      // Add the pulse(s) to the buffer for the slave process
      pt_this->u24_fetchedPulses += u24_nrOfPulses ;

      // Add the pulse(e) to the current bucket
      pt_this->at_pulseBucket[pt_this->u16_firstBucket].u24_value += u24_nrOfPulses ;

      KE_CriticalEnd () ;

      // Nofify the slave process about the newly fetched pulses
      if (pt_this->t_slaveProcessId != NULL)
      {
        (void)KE_MBoxSend (pt_this->t_slaveProcessId, pt_this) ;
      }
    }
  }

  return ;
}
// End: BMM_pulseProcess


static PROCESS BMM_bucketProcess (BMM_handle const pt_instance)
{
  BMM_instance_struct * const pt_this = pt_instance ;
  BMM_bucket*                 pt_tmpBucket ;
  unsigned char               u8_index ;
  for (;;)
  {
    // Wait for a new-pulse-event
    (void)KE_MBoxReceive () ;

    KE_CriticalBegin () ;

    // Increase the queue head
    pt_this->u16_firstBucket ++ ;
    if (pt_this->u16_firstBucket >= pt_this->u16_nrOfBuckets)
    {
      pt_this->u16_firstBucket = 0 ;
    }

    // Check for a queue overflow
    if (pt_this->u16_firstBucket == pt_this->u16_lastBucket)
    {
      // Increase the queue tail
      pt_this->u16_lastBucket ++ ;
      if (pt_this->u16_lastBucket >= pt_this->u16_nrOfBuckets)
      {
        pt_this->u16_lastBucket = 0 ;
      }
    }

    // Empty the new bucket
    pt_this->at_pulseBucket[pt_this->u16_firstBucket].u24_value = 0 ;
    // Fill out the timestamp for the new bucket
    RTC_GetTime (&(pt_this->at_pulseBucket[pt_this->u16_firstBucket].u32_timeStamp)) ;

    KE_CriticalEnd () ;

    for (u8_index = 0; u8_index < BMM_MAX_EVENTS; u8_index ++)
    {
      if (pt_this->t_clientProcessId[u8_index] != NULL)
      {
        (void)KE_MBoxSend (pt_this->t_clientProcessId[u8_index], pt_this) ;
      }
    }
  }

  return ;
}
// End: BMM_NextBucket
