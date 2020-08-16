////////////////////////////////////////////////////////////////////////////////
// File    : KEY_KeyHandler.c
// Function:
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define KEY_KEYHANDLER_C
#include <kernel.h>
#include "KEY_KeyHandler.h"

#include "TMR_Timer.h"

#define KEY_DEBOUNCE_TIME (50)

#define B0_MASK           0x01
#define B1_MASK           0x02
#define B2_MASK           0x04
#define B3_MASK           0x08
#define B4_MASK           0x10
#define B5_MASK           0x20
#define B6_MASK           0x40
#define B7_MASK           0x80

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  PID               pt_clientProcId ;
  TMR_ticks_struct  t_debounceTimeout ;
  void*             pv_oldISR ;
} KEY_instance_struct ;

static PID                  pt_processId = NULL ;
static KEY_instance_struct  at_keyInstance[5] ;

////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static PROCESS  KEY_Process (void) ;
static void     KEY_Up      (void) ;
static void     KEY_Left    (void) ;
static void     KEY_Down    (void) ;
static void     KEY_Right   (void) ;
static void     KEY_Enter   (void) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

KEY_status KEY_Initialize (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       KEY_Initialize                                             //
//                 -                                                          //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  KEY_status result = KEY_OK ;
  unsigned char u8_index ;

  if (result == KEY_OK)
  {
    // Do parameter check
    if (pt_processId != NULL)
    {
      (void)xc_printf ("KEY_Initialize: Second init.\n") ;
      result = KEY_ERR_2NDINIT ;
    }
  }

  if (result == KEY_OK)
  {

    for (u8_index = e_Key_Up; u8_index <= e_Key_Enter; u8_index ++)
    {
      // Reset the key clients
      at_keyInstance[u8_index].pt_clientProcId = NULL ;

      // Reset the debounce timeout
      TMR_NeverTimeout (&(at_keyInstance[u8_index].t_debounceTimeout)) ;
    }

    // Create the instance task
    pt_processId = KE_TaskCreate ( (procptr)KEY_Process,  // Function
                                   256,                   // Stack size
                                   10,                    // Priority
                                   "KEY_Process",         // Name
                                   0) ;                   // Number of arguments
    if (pt_processId == 0)
    {
      (void)xc_printf ("KEY_Initialize: Process error (create).\n") ;
      result = KEY_ERR_PROCESS ;
    }
  }

  if (result == KEY_OK)
  {
    if ( KE_TaskResume(pt_processId) == SYSERR)
    {
      (void)xc_printf ("KEY_Initialize: Process error (resume).\n") ;
      pt_processId = (PID)0 ;
      result = KEY_ERR_PROCESS ;
    }
  }

  if (result == KEY_OK)
  {
    // Install the interrupt handlers
    at_keyInstance[e_Key_Up   ].pv_oldISR = set_evec (IV_PB3, &KEY_Up) ;      // Install interrupt handler, store the old one
    at_keyInstance[e_Key_Down ].pv_oldISR = set_evec (IV_PB5, &KEY_Down) ;    // Install interrupt handler, store the old one
    at_keyInstance[e_Key_Left ].pv_oldISR = set_evec (IV_PB4, &KEY_Left) ;    // Install interrupt handler, store the old one
    at_keyInstance[e_Key_Right].pv_oldISR = set_evec (IV_PB6, &KEY_Right) ;   // Install interrupt handler, store the old one
    at_keyInstance[e_Key_Enter].pv_oldISR = set_evec (IV_PB7, &KEY_Enter) ;   // Install interrupt handler, store the old one
  }

  return (result) ;
}
// End: KEY_Initialize


KEY_status KEY_Terminate (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       KEY_Terminate                                              //
//                 -                                                          //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  KEY_status result = KEY_OK ;

  if (pt_processId != 0)
  {
    // Kill the task
    (void)KE_TaskDelete (pt_processId) ;

    // Remove interrupt handlers
    (void)set_evec (IV_PB3, at_keyInstance[e_Key_Up   ].pv_oldISR) ;   // Remove interrupt handler, restore the old one
    (void)set_evec (IV_PB5, at_keyInstance[e_Key_Down ].pv_oldISR) ;   // Remove interrupt handler, restore the old one
    (void)set_evec (IV_PB4, at_keyInstance[e_Key_Left ].pv_oldISR) ;   // Remove interrupt handler, restore the old one
    (void)set_evec (IV_PB6, at_keyInstance[e_Key_Right].pv_oldISR) ;   // Remove interrupt handler, restore the old one
    (void)set_evec (IV_PB7, at_keyInstance[e_Key_Enter].pv_oldISR) ;   // Remove interrupt handler, restore the old one

    pt_processId = 0 ;
  }

  return (result) ;
}
// End: KEY_Terminate


KEY_status KEY_SetClient (KEY_Key_enum const e_key,
                          PID          const t_clientProcId)
////////////////////////////////////////////////////////////////////////////////
// Function:       KEY_SetClient                                              //
//                 -                                                          //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  KEY_status result = KEY_OK ;

  if (result == KEY_OK)
  {
    if ( (e_key < e_Key_Up   ) ||
         (e_key > e_Key_Enter)    )
    {
      (void)xc_printf ("KEY_SetClient: Parameter error.\n") ;
      result = KEY_ERR_PARAM ;
    }
  }

  if (result == KEY_OK)
  {
    at_keyInstance[e_key].pt_clientProcId = t_clientProcId ;

    if (t_clientProcId == NULL)
    {
      // Turn off the LED in the button
    }
    else
    {
      // Turn on the LED in the button
    }
  }

  return (result) ;
}
// End: KEY_SetClient


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////

static PROCESS KEY_Process (void)
{
  unsigned char u8_index ;

  for (;;)
  {
    for (u8_index = e_Key_Up; u8_index <= e_Key_Enter; u8_index ++)
    {
      if (TMR_CheckTimeout(&at_keyInstance[u8_index].t_debounceTimeout))
      {
        TMR_SetTimeout (&at_keyInstance[u8_index].t_debounceTimeout, 20) ;
        TMR_NeverTimeout (&at_keyInstance[u8_index].t_debounceTimeout) ;

        if (at_keyInstance[u8_index].pt_clientProcId != NULL)
        {
          (void)KE_MBoxSend (at_keyInstance[u8_index].pt_clientProcId, (HANDLE)u8_index) ;
        }
      }
    }
    KE_TaskSleep100 (KEY_DEBOUNCE_TIME/10) ;
  }
}

#pragma interrupt
static void KEY_Up (void)
{
  PB_DR   |= B3_MASK ;

  TMR_SetTimeout (&at_keyInstance[e_Key_Up].t_debounceTimeout, KEY_DEBOUNCE_TIME) ;

  PB_DR   &= ~B3_MASK ;

  return ;
}

#pragma interrupt
static void KEY_Left (void)
{
  PB_DR   |= B4_MASK ;

  TMR_SetTimeout (&at_keyInstance[e_Key_Left].t_debounceTimeout, KEY_DEBOUNCE_TIME) ;

  PB_DR   &= ~B4_MASK ;

  return ;
}

#pragma interrupt
static void KEY_Down (void)
{
  PB_DR   |= B5_MASK ;

  TMR_SetTimeout (&at_keyInstance[e_Key_Down].t_debounceTimeout, KEY_DEBOUNCE_TIME) ;

  PB_DR   &= ~B5_MASK ;

  return ;
}

#pragma interrupt
static void KEY_Right (void)
{
  PB_DR   |= B6_MASK ;

  TMR_SetTimeout (&at_keyInstance[e_Key_Right].t_debounceTimeout, KEY_DEBOUNCE_TIME) ;

  PB_DR   &= ~B6_MASK ;

  return ;
}

#pragma interrupt
static void KEY_Enter (void)
{
  PB_DR   |= B7_MASK ;

  TMR_SetTimeout (&at_keyInstance[e_Key_Enter].t_debounceTimeout, KEY_DEBOUNCE_TIME) ;

  PB_DR   &= ~B7_MASK ;

  return ;
}
