////////////////////////////////////////////////////////////////////////////////
// File    : TMR_Timer.c
// Function: Timer module
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define TMR_TIMER_C
#include <kernel.h>
#include "TMR_Timer.h"

#define NR_OF_TICK_BYTES  (5)

#define Bit(n)            (1U << (n))           // Bit mask for bit 'n'
#define MaskShiftL(i,m,s) (((i) & (m)) << (s))  // Mask 'i' with 'm' and shift left 's'
#define MaskShiftR(i,m,s) (((i) & (m)) >> (s))  // Mask 'i' with 'm' and shift right 's'

#define B0_MASK           0x01
#define B1_MASK           0x02
#define B2_MASK           0x04
#define B3_MASK           0x08
#define B4_MASK           0x10
#define B5_MASK           0x20
#define B6_MASK           0x40
#define B7_MASK           0x80

#define IRQ_EOC_EN  (B0_MASK)

#define TIM_EN      (B0_MASK)
#define RLD         (B1_MASK)
#define TIM_SINGLE  (0)
#define TIM_CONT    (B2_MASK)
#define CLK_DIV_4   (0)
#define CLK_DIV_16  (B3_MASK)
#define CLK_DIV_64  (B4_MASK)
#define CLK_DIV_256 (B3_MASK | B4_MASK)
#define CLK_SEL_SYS (0)
#define CLK_SEL_RTC (B5_MASK)
#define CLK_SEL_FAL (B6_MASK)
#define CLK_SEL_RIS (B5_MASK | B6_MASK)
#define BRK_STOP    (B7_MASK)

#define TICKS_PER_10_US       (125)
#define TMR3_RELOAD_VALUE     (0x30D4)
#define T3_max_value          0xFFFF
#define nsecs_per_T3_inc      400
#define nsecs_per_tick        (TMR_TICKS_PER_MS * 1000000UL)
#define T3_incs_per_tick      (nsecs_per_tick / nsecs_per_T3_inc)
#define T3_reload_value       ((T3_max_value - T3_incs_per_tick) + 1)


////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

static       BOOL               b_Initialised    = FALSE ;
static       void*              p_OldISR         = NULL ;
static       TMR_ticks_struct   t_currentTicks ;

////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static void ISR_Timer3       (void) ;
static void TMR_CurrentTicks (TMR_ticks_struct * const currTicks_ptr) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

TMR_status TMR_Initialize (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       Timer module initialisation routine                        //
//                 - Initializes the module                                   //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  TMR_status result = TMR_OK ;
  unsigned char cntr ;

  // Check if the module has been initialised before
  if (b_Initialised == FALSE)
  {
    // Reset / Initialize the ticks counter
    for (cntr = 0; cntr < NR_OF_TICK_BYTES; cntr ++)
    {
      t_currentTicks.u8_ticksByte[cntr] = 0x00 ;
    }

//  disable( process_state );
    p_OldISR = set_evec (IV_TMR3, &ISR_Timer3); // Install interrupt handler, store the old one
//  restore( process_state );

    TMR3_RR_H = 0x30 ;                          // Set high byte of Timer Reload Value
    TMR3_RR_L = 0xD4 ;                          // Set low byte of Timer Reload Value
    TMR3_CTL  = TIM_EN    |                     // Set Timer Enable
                RLD       |                     // Enable Reload
                CLK_DIV_4 |                     // Set prescaler divider to 4
                TIM_CONT ;                      // Set continous Mode
    TMR3_IER  = IRQ_EOC_EN ;                    // Enable End of Count Interrupt

    b_Initialised = TRUE ;                      // Mark the module as initialised
  }
  else
  {
    // Report the second initialisation error.
    (void)xc_printf ("TMR_Initialize: Second init.\n") ;
    result = TMR_ERR_INIT ;
  }

  return (result) ;
}
// End: TMR_Initialize


TMR_status TMR_Terminate (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       Timer module termination routine                           //
//                 - Terminates the module                                    //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  TMR_status result = TMR_OK ;

  TMR3_IER  = 0x00 ;                            // Disable all TMR3 Interrupts
  TMR3_CTL  = 0x00 ;                            // Disable TMR3
  TMR3_RR_H = 0x00 ;                            // Set Timer Reload Value back to 0
  TMR3_RR_L = 0x04 ;                            // Set Timer Reload Value back to 0

  if (b_Initialised != FALSE)
  {
    //  disable( process_state );
    (void)set_evec (IV_TMR3, p_OldISR);         // Reinstall the old interrupt handler
    //  restore( process_state );

    b_Initialised = FALSE ;                     // Mark the module as uninitialised
  }

  return (result) ;
}
// End: TMR_Terminate


void TMR_SetTimeout (TMR_ticks_struct * const pt_timeout,
                     unsigned long      const timout)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_SetTimeout                                             //
//                 - Calculate a requested timeout                            //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned long       tempTicksWord ;  // Rollover detection buffer

  // Fetch the current time
  TMR_CurrentTicks (pt_timeout) ;

  // Temporarilly store the Long-part of the current time
  tempTicksWord = pt_timeout->t_ticksCalc.u32_lsLong ;

  // Add the timout to the Long-part of the current time
  pt_timeout->t_ticksCalc.u32_lsLong += timout ;

  // Check if the Long-part has rolled over and increase
  // the Short-part if so
  if (pt_timeout->t_ticksCalc.u32_lsLong < tempTicksWord)
  {
    pt_timeout->t_ticksCalc.u16_msShort ++ ;
  }

  return ;
}
// End: TMR_SetTimeout


void TMR_PostponeTimeout (TMR_ticks_struct * const pt_timeout,
                          unsigned long      const postpone)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_PosponeTimeout                                         //
//                 - Add the requested timeout to the given timeout           //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned long       tempTicksWord ;  // Rollover detection buffer

  // Temporarilly store the Long-part of the given timeout
  tempTicksWord = pt_timeout->t_ticksCalc.u32_lsLong ;

  // Add the timout to the Long-part of the given timeout
  pt_timeout->t_ticksCalc.u32_lsLong += postpone ;

  // Check if the Long-part has rolled over and increase
  // the Short-part if so
  if (pt_timeout->t_ticksCalc.u32_lsLong < tempTicksWord)
  {
    pt_timeout->t_ticksCalc.u16_msShort ++ ;
  }

  return ;
}
// End: TMR_PosponeTimeout


void TMR_ExpireTimeout (TMR_ticks_struct * const pt_timeout)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_ExpireTimeout                                          //
//                 - Make the given timeout expire unconditionally            //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned char cntr ;

  // Set timeout time to 0 by clearing all bytes
  for (cntr = 0; cntr < NR_OF_TICK_BYTES; cntr ++)
  {
    pt_timeout->u8_ticksByte[cntr] = 0x00 ;
  }

  return ;
}
// End: TMR_ExpireTimeout


void TMR_NeverTimeout (TMR_ticks_struct * const pt_timeout)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_NeverTimeout                                           //
//                 - Prevent the given timeout to expire, ever                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned char cntr ;

  // Set timeout time to 'nearly' infinite (>584942417 year)
  for (cntr = 0; cntr < NR_OF_TICK_BYTES; cntr ++)
  {
    pt_timeout->u8_ticksByte[cntr] = 0xFF ;
  }

  return ;
}
// End: TMR_NeverTimeout


BOOL TMR_CheckTimeout (TMR_ticks_struct const * const pt_timeout)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_CheckTimeout                                           //
//                 - Check if a given timeout has expired                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  BOOL             expired ;
  TMR_ticks_struct currentTime ;

  // Fetch the current time
  TMR_CurrentTicks (&currentTime) ;

  if (currentTime.t_ticksCalc.u16_msShort == pt_timeout->t_ticksCalc.u16_msShort)
  {
    expired = (currentTime.t_ticksCalc.u32_lsLong >= pt_timeout->t_ticksCalc.u32_lsLong) ?
              TRUE :
              FALSE ;
  }
  else
  {
    expired = (currentTime.t_ticksCalc.u16_msShort >= pt_timeout->t_ticksCalc.u16_msShort) ?
              TRUE :
              FALSE ;
  }

  return (expired) ;
}
// End: TMR_CheckTimeout


void TMR_SetTimeStamp (TMR_ticks_struct * const pt_timestamp)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_SetTimeStamp                                           //
//                 - Check if a given timeout has expired                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  // Fetch the current time
  TMR_CurrentTicks (pt_timestamp) ;

  return ;
}
// End: TMR_SetTimeStamp


unsigned long TMR_TimeStampAge (TMR_ticks_struct const * const pt_timestamp)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_TimeStampAge                                           //
//                 - Calculate the time now and the time the give timestamp   //
//                   was set                                                  //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned long       age         = 0 ;
  TMR_ticks_struct    currentTime ;

  // Fetch the current time
  TMR_CurrentTicks (&currentTime) ;

  // Check Most Significant Short for future time stamps
  if (currentTime.t_ticksCalc.u16_msShort >= pt_timestamp->t_ticksCalc.u16_msShort)
  {
    switch (currentTime.t_ticksCalc.u16_msShort - pt_timestamp->t_ticksCalc.u16_msShort)
    {
      case 0: // Most Significant Short is the same
        if (currentTime.t_ticksCalc.u32_lsLong > pt_timestamp->t_ticksCalc.u32_lsLong)
        {
          // Past time stamp: Just subtract the Least Significant LWords
          age = currentTime.t_ticksCalc.u32_lsLong - pt_timestamp->t_ticksCalc.u32_lsLong ;
        }
        else
        {
          // Future or current time stamp
          age = 0 ;
        }
        break ;

      case 1: // Most Significant LWord is increased just by 1
        if (currentTime.t_ticksCalc.u32_lsLong < pt_timestamp->t_ticksCalc.u32_lsLong)
        {
          // The Least Significant LWords has only wrapped around
          age = (0xFFFFFFFF - (pt_timestamp->t_ticksCalc.u32_lsLong - currentTime.t_ticksCalc.u32_lsLong)) + 1 ;
        }
        else
        {
          // The The stamp has just aged beyond the maximum
          age = 0xFFFFFFFF ;
        }
        break ;

      default: // Most Significant LWord is increased by two of more
        // The time stamp is ancient, older than 49 days!
        age = 0xFFFFFFFF ;
        break ;
    }
  }
  else
  {
    // Future time stamp
    age = 0 ;
  }

  return (age) ;
}
// End: TMR_TimeStampAge


void TMR_Delay (unsigned long const delay)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_Delay                                                  //
//                 - Hold up the processor for the specified nr of ticks      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  TMR_ticks_struct timeoutTime ;

  // Set a timout
  TMR_SetTimeout (&timeoutTime, delay) ;

  // Wait for it to expire
  while (!TMR_CheckTimeout (&timeoutTime))
  {
    // Just wait...
    ;
  }

  return ;
}
// End: TMR_Delay


void TMR_MicroDelay (unsigned short const delay_us)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_Delay                                                  //
//                 - Hold up the processor for the specified nr of ticks      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned char            u8_old_tmr3_dr[2] ;
  unsigned short volatile *pu16_old_tmr3 = (unsigned short volatile *)&(u8_old_tmr3_dr[0]) ;

  unsigned char            u8_cur_tmr3_dr[2] ;
  unsigned short volatile *pu16_cur_tmr3 = (unsigned short volatile *)&(u8_cur_tmr3_dr[0]) ;

  unsigned int             u24_ticks_to_wait ;
  unsigned int             u24_ticks_waited = 0 ;

  // Store TMR3 value first, for highest accuracy
  u8_old_tmr3_dr[0] = TMR3_DR_L ; // Read the lower byte first, this will also latch the higher byte
  u8_old_tmr3_dr[1] = TMR3_DR_H ; // Read the latched higher byte

  // Ticks-per-us is 12.5. To keep the 0.5, we use ticks-per-10us which is 125.
  // To calculate the proper nr of ticks to wait, the delay must be multiplied
  // by 10 too.
  u24_ticks_to_wait = (delay_us * TICKS_PER_10_US) / 10 ;

  while (u24_ticks_waited < u24_ticks_to_wait)
  {
    // Fetch the current TMR3 value
    u8_cur_tmr3_dr[0] = TMR3_DR_L ; // Read the lower byte first, this will also latch the higher byte
    u8_cur_tmr3_dr[1] = TMR3_DR_H ; // Read the latched higher byte

    // Check if the timer has decreased and if it rolled under
    if (*pu16_cur_tmr3 < *pu16_old_tmr3)
    {
      // Timer has decreased, just add the difference
      u24_ticks_waited += (*pu16_old_tmr3 - *pu16_cur_tmr3) ;
    }
    else if (*pu16_cur_tmr3 > *pu16_old_tmr3)
    {
      // Timer has rolled under.
      u24_ticks_waited += *pu16_old_tmr3 + (TMR3_RELOAD_VALUE - *pu16_cur_tmr3) ;
    }

    *pu16_old_tmr3 = *pu16_cur_tmr3 ;
  }

  return ;
}


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////

static void TMR_CurrentTicks (TMR_ticks_struct * const currTicks_ptr)
////////////////////////////////////////////////////////////////////////////////
// Function:       TMR_CurrentTicks                                           //
//                 - Fetch the accurate current ticks (without stopping)      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  volatile unsigned char    tempByte ;

  // To prevent a roll-over while reading the separate  byte, we could
  // disable the interrupts, but this would reduce the ticks accuracy.
  // That's why we use a trick:
  do
  {
    // Temporarilly store the first roll-over dependent byte directly from the running clock
    tempByte = t_currentTicks.u8_ticksByte[1] ;

    // Copy the contents ro the running clock into the return buffer
    *currTicks_ptr = t_currentTicks ;

    // Check if the Least Significant Byte has rolled over during this operation,
    // having the incread a More Significant Byte and making the buffer invalid
  } while (currTicks_ptr->u8_ticksByte[1] != tempByte) ;

  return ;
}
// End: TMR_CurrentTicks


#pragma interrupt
static void ISR_Timer3 (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       Timer 3 interrupt service routine:                         //
//                 - Each interrupt will increment the 40-bit ticks counter.  //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  // Read Interrupt Identification Register to clear pending TMR3 interrupts
  const unsigned char tmr3_iir_value = TMR3_IIR ;

  // There's a neater way to increase the counter, but this is the quickst.
  // Since this occurs ever millisecond, we'll go for the quickest sollution.
  t_currentTicks.u8_ticksByte[0] ++ ; // Takes 1.8us @ 50MHz
  if (t_currentTicks.u8_ticksByte[0] == 0)
  {
    t_currentTicks.u8_ticksByte[1] ++ ;
    if (t_currentTicks.u8_ticksByte[1] == 0)
    {
      t_currentTicks.u8_ticksByte[2] ++ ;
      if (t_currentTicks.u8_ticksByte[2] == 0)
      {
        t_currentTicks.u8_ticksByte[3] ++ ;
        if (t_currentTicks.u8_ticksByte[3] == 0)
        {
          t_currentTicks.u8_ticksByte[4] ++ ;
          if (t_currentTicks.u8_ticksByte[4] == 0)
          {
            t_currentTicks.u8_ticksByte[5] ++ ;
            if (t_currentTicks.u8_ticksByte[5] == 0)
            {
              // This only happens after 2^48 ms (>34 years)
              panic ("Timer overflow!!!") ;
            }
          }
        }
      }
    }
  }

  return ;
}
// End: ISR_Timer0
