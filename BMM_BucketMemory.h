////////////////////////////////////////////////////////////////////////////////
// File    : BMM_BucketMemory.h
// Function: Include file of 'BMM_BucketMemory.h'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef BMM_BUCKETMEMORY_H                            // Include file already compiled ?
#define BMM_BUCKETMEMORY_H

#ifdef BMM_BUCKETMEMORY_C                             // Compiled in BMM_BucketMemory.c ?
#define BMM_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define BMM_EXTERN extern "C"
#else
#define BMM_EXTERN extern
#endif // __cplusplus
#endif // BMM_BUCKETMEMORY_C


#define BMM_OK                  (0)                   // All Ok
#define BMM_ERR_PARAM           (-1)                  // Parameter error
#define BMM_ERR_MEMORY          (-2)                  // Memory allocation error
#define BMM_ERR_POINTER         (-3)                  // Invalid pointer supplied
#define BMM_ERR_PROCESS         (-4)                  // Process allocation errord
#define BMM_ERR_NOFREESLOT      (-5)                  // No free client slot was found
#define BMM_ERR_NOTFOUND        (-6)                  // ProcessId not found


// BMM types
typedef void*                   BMM_handle ;
typedef char                    BMM_status ;          // Status/Error return type
typedef struct
{
  unsigned int    u24_value ;
  unsigned long   u32_timeStamp ;
} BMM_bucket ;


// Define the function call type required for fetching pulses
typedef char (*fetchFunction)(BMM_handle const pt_instance, unsigned int * const pu24_pulses) ;


BMM_status  BMM_Create          (BMM_handle          * const ppt_instance,
                                 unsigned short        const u16_nrOfBuckets) ;

BMM_status  BMM_Delete          (BMM_handle            const pt_instance) ;

BMM_status  BMM_SetMeteringFunc (BMM_handle            const pt_instance,
                                 fetchFunction         const func_GetPulses) ;

BMM_status  BMM_GetMeteringProc (BMM_handle            const pt_instance,
                                 PID                 * const pt_processId) ;

BMM_status  BMM_GetBucketProc   (BMM_handle            const pt_instance,
                                 PID                 * const pt_processId) ;

BMM_status  BMM_SetSlaveProc    (BMM_handle            const pt_instance,
                                 PID                   const pt_processId) ;

BMM_status  BMM_GetPulses       (BMM_handle            const pt_instance,
                                 unsigned int        * const pu24_pulses) ;

BMM_status  BMM_AddClient       (BMM_handle            const pt_instance,
                                 PID                   const t_clientProcId) ;

BMM_status  BMM_RemoveClient    (BMM_handle            const pt_instance,
                                 PID                   const t_clientProcId) ;

BMM_status  BMM_GetNrOfBuckets  (BMM_handle            const pt_instance,
                                 unsigned short      * const pu16_nrOfBuckets) ;

BMM_status  BMM_GetBucketCont   (BMM_handle            const pt_instance,
                                 unsigned short        const u16_bucketNr,
                                 BMM_bucket          * const pt_bucketContents) ;


#endif //BMM_BUCKETMEMORY_H
