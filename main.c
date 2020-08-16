////////////////////////////////////////////////////////////////////////////////
// File    : main.c
// Function: MeterMaid
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////
#include <kernel.h>
#include <http.h>
#include <shell.h>
#include <network.h>
#include <netapp.h>
#include <snmp.h>

#include "TMR_Timer.h"
#include "LCD_Driver.h"
#include "PHD_PulseHandler.h"
#include "BMM_BucketMemory.h"
#include "RTC_RealTimeClock.h"
#include "WEB_Site.h"
#include "KEY_KeyHandler.h"

#define NOF_DAYS          (365)
#define NOF_HOURS         (24)
#define NOF_MINUTES       (60)

#define B0_MASK           0x01
#define B1_MASK           0x02
#define B2_MASK           0x04
#define B3_MASK           0x08
#define B4_MASK           0x10
#define B5_MASK           0x20
#define B6_MASK           0x40
#define B7_MASK           0x80

#define ELEC_MAX_PPM      (70)
#define ELEC_MAX_PPU      (1667)
#define GAS_MAX_PPM       (100)
#define GAS_MAX_PPU       (1000)
#define WATER_MAX_PPM     (10)
#define WATER_MAX_PPU     (1000)

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

struct BootInfo Bootrecord =
{
  "192.168.72.101",   // Default IP address
  "192.168.72.1",     // Default Gateway
  "128.118.25.3",     // Default Timer Server
  "192.168.72.2",     // Default File Server
  "",
  "217.77.130.146",   // Default Name Server
  "",
  0xFFFFFF00UL        // Default Subnet Mask
} ;

// Pulse handler instances for the meter models
static PHD_handle   pt_PHDelectInst     = NULL ;
static PHD_handle   pt_PHDgasInst       = NULL ;
static PHD_handle   pt_PHDwaterInst     = NULL ;
// Bucket memory instances for the meter models
static BMM_handle   pt_BMMelectMinInst  = NULL ;
static BMM_handle   pt_BMMelectHourInst = NULL ;
static BMM_handle   pt_BMMelectDayInst  = NULL ;
static BMM_handle   pt_BMMgasMinInst    = NULL ;
static BMM_handle   pt_BMMgasHourInst   = NULL ;
static BMM_handle   pt_BMMgasDayInst    = NULL ;
static BMM_handle   pt_BMMwaterMinInst  = NULL ;
static BMM_handle   pt_BMMwaterHourInst = NULL ;
static BMM_handle   pt_BMMwaterDayInst  = NULL ;
// LCD handler instance for the display
static LCD_handle   pt_LCDinstance      = NULL ;

////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static void     ISR_ElectPulse  (void) ;

static void     ISR_GasPulse    (void) ;

static void     ISR_WaterPulse  (void) ;

static PROCESS  buttonProcess   (void) ;

static PROCESS  viewLoadProcess (char           const * const ps8_displayTemplate,
                                 unsigned int           const u24_unitsPerKPulses) ;

static PROCESS  viewLogProcess  (char           const * const ps8_displayTemplate,
                                 unsigned short       * const u16_logEntry,
                                 unsigned int           const u24_unitsPerKPulses) ;

static PROCESS  clockProcess    (void) ;

static void     initMeter       (PHD_handle           * const ppt_PhdInstance,
                                 BMM_handle           * const ppt_BmmMinuteInstance,
                                 BMM_handle           * const ppt_BmmHourInstance,
                                 BMM_handle           * const ppt_BmmDayInstance,
                                 unsigned short         const u16_maxPulsesPerMinute) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

SYSCALL main (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       main                                                       //
//                 - Starts the applications processes                        //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  // Web meter instances
  WEB_handle    pt_METelectLoadInst = NULL ;
  WEB_handle    pt_METgasLoadInst   = NULL ;
  WEB_handle    pt_METwaterLoadInst = NULL ;
  // Web table instances
  WEB_handle    pt_TABelectMinInst  = NULL ;
  WEB_handle    pt_TABelectHourInst = NULL ;
  WEB_handle    pt_TABelectDayInst  = NULL ;
  WEB_handle    pt_TABgasMinInst    = NULL ;
  WEB_handle    pt_TABgasHourInst   = NULL ;
  WEB_handle    pt_TABgasDayInst    = NULL ;
  WEB_handle    pt_TABwaterMinInst  = NULL ;
  WEB_handle    pt_TABwaterHourInst = NULL ;
  WEB_handle    pt_TABwaterDayInst  = NULL ;
  // Storage for the web page pointer
  Webpage *     pt_webSite ;
  // Device driver instance for serial port for the shell
  DID           TTYDevID;

  PID           t_buttonProcess     = NULL ;

  PID           t_clockProcess = NULL;

  PID           t_tempProcId = NULL ;

  // Set the interrupt vectors for the meter inputs
  set_evec (IV_PB0, &ISR_ElectPulse) ;
  set_evec (IV_PB1, &ISR_GasPulse) ;
  set_evec (IV_PB2, &ISR_WaterPulse) ;

  // Initialize the network and start the TCP/IP engines
  netstart();

  // Initialize the website and fetch its pointer
  (void)WEB_Initialize (&pt_webSite) ;

  // Create the instance of the electricity meter
  (void)WEB_CreateMeter (&pt_METelectLoadInst,
                         ELEC_MAX_PPU,
                         ELEC_MAX_PPM,
                         0x0001,
                         "Electricity - Load Meter",
                         "Actual load",
                         "kW") ;
  // Create the instances of the electricity tables
  (void)WEB_CreateTable (&pt_TABelectMinInst,
                         NOF_MINUTES,
                         ELEC_MAX_PPU,
                         0x0001,
                         "Electricity - Minute Table",
                         "Electricity consumption per minute",
                         "kW/h") ;
  (void)WEB_CreateTable (&pt_TABelectHourInst,
                         NOF_HOURS,
                         ELEC_MAX_PPU,
                         0x0002,
                         "Electricity - Hour Table",
                         "Electricity consumption per hour",
                         "kW/h") ;
  (void)WEB_CreateTable (&pt_TABelectDayInst,
                         NOF_DAYS,
                         ELEC_MAX_PPU,
                         0x0003,
                         "Electricity - Day Table",
                         "Electricity consumption per day",
                         "kW/h") ;

  // Create the instance of the gas meter
  (void)WEB_CreateMeter (&pt_METgasLoadInst,
                         GAS_MAX_PPU,
                         GAS_MAX_PPM,
                         0x0002,
                         "Gas - Flow Meter",
                         "Actual flow",
                         "m<sup>3</sup>/h") ;
  // Create the instances of the gas tables
  (void)WEB_CreateTable (&pt_TABgasMinInst,
                         NOF_MINUTES,
                         GAS_MAX_PPU,
                         0x0011,
                         "Gas - Minute Table",
                         "Gas consumption per minute",
                         "m<sup>3</sup>") ;
  (void)WEB_CreateTable (&pt_TABgasHourInst,
                         NOF_HOURS,
                         GAS_MAX_PPU,
                         0x0012,
                         "Gas - Hour Table",
                         "Gas consumption per hour",
                         "m<sup>3</sup>") ;
  (void)WEB_CreateTable (&pt_TABgasDayInst,
                         NOF_DAYS,
                         GAS_MAX_PPU,
                         0x0013,
                         "Gas - Day Table",
                         "Gas consumption per day",
                         "m<sup>3</sup>") ;

  // Create the instance of the gas meter
  (void)WEB_CreateMeter (&pt_METwaterLoadInst,
                         WATER_MAX_PPU,
                         WATER_MAX_PPM,
                         0x0003,
                         "Water - Flow Meter",
                         "Actual flow",
                         "m<sup>3</sup>/h") ;
  // Create the instances of the water tables
  (void)WEB_CreateTable (&pt_TABwaterMinInst,
                         NOF_MINUTES,
                         WATER_MAX_PPU,
                         0x0021,
                         "Water - Minute Table",
                         "Water consumption per minute",
                         "m<sup>3</sup>") ;
  (void)WEB_CreateTable (&pt_TABwaterHourInst,
                         NOF_HOURS,
                         WATER_MAX_PPU,
                         0x0022,
                         "Water - Hour Table",
                         "Water consumption per hour",
                         "m<sup>3</sup>") ;
  (void)WEB_CreateTable (&pt_TABwaterDayInst,
                         NOF_DAYS,
                         WATER_MAX_PPU,
                         0x0023,
                         "Water - Day Table",
                         "Water consumption per day",
                         "m<sup>3</sup>") ;

  // Initialize the webserver using default headers and method handlers, our page on port 80
  http_init (http_defmethods, httpdefheaders, pt_webSite, 80) ;

  // Initialize a shell on telnet
  telnet_init () ;

  // Initialize the timed 738 network time client
  timed_738_init () ;

  // Initialize the timer
  (void)TMR_Initialize () ;

  // Initialize the real time clock
  (void)RTC_Initialize () ;

  // Create an LC-Display instance
  (void)LCD_Create (&pt_LCDinstance,    // Storage for instance pointer
                    &PA_DR,             // Control lines on Port A
                    0x03,               // RW on Bit 2
                    0x02,               // RS on Bit 3
                    0x00,               // E  on Bit 0
                    &PA_DR,             // Data lines on Port A
                    0x04,               // Data bits on Bit 4..7
                    FALSE,              // 4-bit bus mode
                    TRUE,               // 2 Lines
                    FALSE) ;            // 5x7 Font
  // Turn on the display
  (void)LCD_SetDisplayOn (pt_LCDinstance, TRUE) ;

  // Open serial port 0 for the shell
  open (SERIAL0, 0, 0) ;
  // Open a TTY device driver and bind it to the previously opened port
  TTYDevID = open (TTY, (char*)SERIAL0, 0) ;
  if (TTYDevID != NULLPTR)
  {
    kprintf ("Starting up a shell on device %p.\n", TTYDevID) ;
    // Initialize the shell and bind it to the TTY device driver
    shell_init (TTYDevID) ;
  }

  // Set up the electricity meter
  (void)initMeter (&pt_PHDelectInst, &pt_BMMelectMinInst, &pt_BMMelectHourInst, &pt_BMMelectDayInst, ELEC_MAX_PPM) ;
  // Subscribe the electricity meter to the load change event
  (void)WEB_GetProcessId (pt_METelectLoadInst, &t_tempProcId) ;
  (void)PHD_AddClient    (pt_PHDelectInst,      t_tempProcId) ;
  // Subscribe the electricity meter minute-web-table to the minute bucket event
  (void)WEB_GetProcessId (pt_TABelectMinInst,  &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMelectMinInst,   t_tempProcId) ;
  // Subscribe the electricity meter hour-web-table to the hour bucket event
  (void)WEB_GetProcessId (pt_TABelectHourInst, &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMelectHourInst,  t_tempProcId) ;
  // Subscribe the electricity meter day-web-table to the day bucket event
  (void)WEB_GetProcessId (pt_TABelectDayInst,  &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMelectDayInst,   t_tempProcId) ;

  // Set up the gas meter
  (void)initMeter (&pt_PHDgasInst, &pt_BMMgasMinInst, &pt_BMMgasHourInst, &pt_BMMgasDayInst, GAS_MAX_PPM) ;
  // Subscribe the gas meter to the load change event
  (void)WEB_GetProcessId (pt_METgasLoadInst,   &t_tempProcId) ;
  (void)PHD_AddClient    (pt_PHDgasInst,        t_tempProcId) ;
  // Subscribe the gas meter minute-web-table to the minute bucket event
  (void)WEB_GetProcessId (pt_TABgasMinInst,    &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMgasMinInst,     t_tempProcId) ;
  // Subscribe the gas meter hour-web-table to the hour bucket event
  (void)WEB_GetProcessId (pt_TABgasHourInst,   &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMgasHourInst,    t_tempProcId) ;
  // Subscribe the gas meter day-web-table to the day bucket event
  (void)WEB_GetProcessId (pt_TABgasDayInst,    &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMgasDayInst,     t_tempProcId) ;

  // Set up the water meter
  (void)initMeter (&pt_PHDwaterInst, &pt_BMMwaterMinInst, &pt_BMMwaterHourInst, &pt_BMMwaterDayInst, WATER_MAX_PPM) ;
  // Subscribe the water meter to the load change event
  (void)WEB_GetProcessId (pt_METwaterLoadInst, &t_tempProcId) ;
  (void)PHD_AddClient    (pt_PHDwaterInst,      t_tempProcId) ;
  // Subscribe the water meter minute-web-table to the minute bucket event
  (void)WEB_GetProcessId (pt_TABwaterMinInst,  &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMwaterMinInst,   t_tempProcId) ;
  // Subscribe the water meter hour-web-table to the hour bucket event
  (void)WEB_GetProcessId (pt_TABwaterHourInst, &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMwaterHourInst,  t_tempProcId) ;
  // Subscribe the water meter day-web-table to the day bucket event
  (void)WEB_GetProcessId (pt_TABwaterDayInst,  &t_tempProcId) ;
  (void)BMM_AddClient    (pt_BMMwaterDayInst,   t_tempProcId) ;

  // Create a process for the clock on the display
  t_clockProcess = KE_TaskCreate ( (procptr)clockProcess,
                                   1024,
                                   10,
                                   "Clock Process",
                                   0) ;
  // Start the clock process
  if (t_clockProcess != 0)
  {
    KE_TaskResume (t_clockProcess) ;
  }
  // Subscribe the clock process the one-second-event of the real time clock
  (void)RTC_AddClient (e_secondEvent, t_clockProcess, NULL) ;
  // Fake a one-second-event to display initial time
  (void)KE_MBoxSend   (t_clockProcess, NULL) ;

  // Initialize the keyboard driver
  KEY_Initialize () ;

  // Create a process which interprets the keys
  t_buttonProcess = KE_TaskCreate ( (procptr)buttonProcess,
                                    256,
                                    10,
                                    "key Process",
                                    0) ;
  // Start the process
  if (t_buttonProcess != 0)
  {
    KE_TaskResume (t_buttonProcess) ;
  }
  // Subscribe the process to the up and down key
  (void)KEY_SetClient (e_Key_Left,  t_buttonProcess) ;
  (void)KEY_SetClient (e_Key_Right, t_buttonProcess) ;

  return(OK);
}
// End: main


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////

static PROCESS buttonProcess (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       buttonProcess                                              //
//                 - Launches mini applications to show data on the display   //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  KEY_Key_enum    t_keyPressed ;
  unsigned char   u8_oldView    = 0 ;
  unsigned char   u8_newView    = 0 ;
  unsigned short  u16_logLine   = 0 ;
  PID             t_viewProcess = NULL;

  // Initially create and configure the view process to show the current load of electricity
  t_viewProcess = KE_TaskCreate ( (procptr)viewLoadProcess,
                                  256,
                                  10,
                                  "View Load",
                                  2,
                                  "Current electric load: %u.%03u kW",
                                  ELEC_MAX_PPU ) ;
  // Resume the view process
  if (t_viewProcess != 0)
  {
    KE_TaskResume (t_viewProcess) ;
  }
  // Subscribe the view process to the electricity-load-change event.
  (void)PHD_AddClient (pt_PHDelectInst, t_viewProcess) ;
  // Manually generate such an event to show initial data
  (void)KE_MBoxSend (t_viewProcess, pt_PHDelectInst) ;

  for (;;)
  {
    // Wait for a key pressed event
    t_keyPressed = (KEY_Key_enum)KE_MBoxReceive () ;

    // Evaluate the key
    if (t_keyPressed == e_Key_Right)
    {
      // Next view
      u8_newView = (u8_newView >= 11) ? 0 : u8_newView + 1 ;
      // Reset log line
      u16_logLine = 0 ;
    }
    else if (t_keyPressed == e_Key_Left)
    {
      // Previous view
      u8_newView = (u8_newView <= 0) ? 11 : u8_newView - 1 ;
      // Reset log line
      u16_logLine = 0 ;
    }
    else if (t_keyPressed == e_Key_Down)
    {
      // Next log line
      u16_logLine ++ ;
    }
    else if (t_keyPressed == e_Key_Up)
    {
      // Previous log line
      u16_logLine = (u16_logLine == 0) ? 0 : u16_logLine - 1 ;
    }

    // Terminate the old process
    switch (u8_oldView)
    {
      case  0:
        // Unsubscribe the view process from the electricity-load-change event.
        (void)PHD_RemoveClient (pt_PHDelectInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  1:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the electricity-minute-bucket-change event
        (void)BMM_RemoveClient (pt_BMMelectMinInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  2:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the electricity-hour-bucket-change event
        (void)BMM_RemoveClient (pt_BMMelectHourInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  3:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the electricity-day-bucket-change event
        (void)BMM_RemoveClient (pt_BMMelectDayInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  4:
        // Unsubscribe the view process from the gas-flow-change event.
        (void)PHD_RemoveClient (pt_PHDgasInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  5:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the gas-minute-bucket-change event
        (void)BMM_RemoveClient (pt_BMMgasMinInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  6:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the gas-hour-bucket-change event
        (void)BMM_RemoveClient (pt_BMMgasHourInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  7:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the gas-day-bucket-change event
        (void)BMM_RemoveClient (pt_BMMgasDayInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  8:
        // Unsubscribe the view process from the water-flow-change event.
        (void)PHD_RemoveClient (pt_PHDwaterInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case  9:
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the water-minute-bucket-change event
        (void)BMM_RemoveClient (pt_BMMwaterMinInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case 10:  // Water: hour log
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the water-hour-bucket-change event
        (void)BMM_RemoveClient (pt_BMMwaterHourInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;

      case 11:  // Water: day log
        // Unsubscribe from the up-key
        (void)KEY_SetClient (e_Key_Up,   NULL) ;
        // Unsubscribe from the down-key
        (void)KEY_SetClient (e_Key_Down, NULL) ;
        // Unsubscribe from the water-day-bucket-change event
        (void)BMM_RemoveClient (pt_BMMwaterDayInst, t_viewProcess) ;
        // Kill the view process
        KE_TaskDelete (t_viewProcess) ;
        break ;
    }

    // Launch the new process
    switch (u8_newView)
    {
      case  0:
        // Create and configure the view process to show the current load of electricity
        t_viewProcess = KE_TaskCreate ( (procptr)viewLoadProcess,
                                        256,
                                        10,
                                        "View Load",
                                        2,
                                        "Current electric load: %u.%03u kW",
                                        ELEC_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the electricity-load-change event.
        (void)PHD_AddClient (pt_PHDelectInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_PHDelectInst) ;
        break ;

      case  1:
        // Create and configure the view process to show the minute log of electricity
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%um: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u kW/h",
                                        &u16_logLine,
                                        ELEC_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the electricity-minute-bucket-change event
        (void)BMM_AddClient (pt_BMMelectMinInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMelectMinInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case  2:
        // Create and configure the view process to show the hour log of electricity
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%uh: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u kW/h",
                                        &u16_logLine,
                                        ELEC_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the electricity-hour-bucket-change event
        (void)BMM_AddClient (pt_BMMelectHourInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMelectHourInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case  3:  // Electricity: day log
        // Create and configure the view process to show the day log of electricity
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%ud: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u kW/h",
                                        &u16_logLine,
                                        ELEC_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the electricity-day-bucket-change event
        (void)BMM_AddClient (pt_BMMelectDayInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMelectDayInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case  4:
        // Create and configure the view process to show the current flow of gas
        t_viewProcess = KE_TaskCreate ( (procptr)viewLoadProcess,
                                        256,
                                        10,
                                        "View Load",
                                        2,
                                        "Current gas flow: %u.%03u m3/h",
                                        GAS_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the gas-flow-change event.
        (void)PHD_AddClient (pt_PHDgasInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_PHDgasInst) ;
        break ;

      case  5:  // Gas: minute log
        // Create and configure the view process to show the minute log of gas
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%um: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u m3",
                                        &u16_logLine,
                                        GAS_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the gas-minute-bucket-change event
        (void)BMM_AddClient (pt_BMMgasMinInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMgasMinInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case  6:  // Gas: hour log
        // Create and configure the view process to show the hour log of gas
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%uh: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u m3",
                                        &u16_logLine,
                                        GAS_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the gas-hour-bucket-change event
        (void)BMM_AddClient (pt_BMMgasHourInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMgasHourInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case  7:  // Gas: day log
        // Create and configure the view process to show the day log of gas
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%ud: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u m3",
                                        &u16_logLine,
                                        GAS_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the gas-day-bucket-change event
        (void)BMM_AddClient (pt_BMMgasDayInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMgasDayInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case  8:
        // Create and configure the view process to show the current flow of water
        t_viewProcess = KE_TaskCreate ( (procptr)viewLoadProcess,
                                        256,
                                        10,
                                        "View Load",
                                        2,
                                        "Current water flow: %u.%03u m3/h",
                                        WATER_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the water-flow-change event.
        (void)PHD_AddClient (pt_PHDwaterInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_PHDwaterInst) ;
        break ;

      case  9:  // Water: minute log
        // Create and configure the view process to show the minute log of water
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%um: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u m3",
                                        &u16_logLine,
                                        WATER_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the water-minute-bucket-change event
        (void)BMM_AddClient (pt_BMMwaterMinInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMwaterMinInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case 10:  // Water: hour log
        // Create and configure the view process to show the hour log of water
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%uh: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u m3",
                                        &u16_logLine,
                                        WATER_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the water-hour-bucket-change event
        (void)BMM_AddClient (pt_BMMwaterHourInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMwaterHourInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;

      case 11:  // Water: day log
        // Create and configure the view process to show the day log of water
        t_viewProcess = KE_TaskCreate ( (procptr)viewLogProcess,
                                        256,
                                        10,
                                        "View Log",
                                        3,
                                        "t-%ud: %02u/%02u/%04u %02u:%02u.%02u, %u.%03u m3",
                                        &u16_logLine,
                                        WATER_MAX_PPU ) ;
        // Resume the view process
        if (t_viewProcess != 0)
        {
          KE_TaskResume (t_viewProcess) ;
        }
        // Subscribe the view process to the water-day-bucket-change event
        (void)BMM_AddClient (pt_BMMwaterDayInst, t_viewProcess) ;
        // Manually generate such an event to show initial data
        (void)KE_MBoxSend (t_viewProcess, pt_BMMwaterDayInst) ;
        // Subscribe this process to the up-key
        (void)KEY_SetClient (e_Key_Up,   KE_TaskGetCurPID()) ;
        // Subscribe this process to the down-key
        (void)KEY_SetClient (e_Key_Down, KE_TaskGetCurPID()) ;
        break ;
    }

    u8_oldView = u8_newView ;
  }
}
// End: buttonProcess


static PROCESS viewLoadProcess (char         const * const ps8_displayTemplate,
                                unsigned int         const u24_unitsPerKPulses)
////////////////////////////////////////////////////////////////////////////////
// Function:       viewLoadProcess                                            //
//                 - Mini application to show the current load on the display //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  void *        pv_phdInstance ;
  unsigned int  u24_nrOfPulses      = 0 ;
  char          as_displayText[41] ;
  unsigned char u8_index ;

  for (;;)
  {
    // Wait for a measurement-change event
    pv_phdInstance = KE_MBoxReceive () ;

    // Retrieve the new measurement data
    PHD_GetPulsesPerMinute (pv_phdInstance, &u24_nrOfPulses) ;

    // Extrapolate minute to an hour
    u24_nrOfPulses *= 60 ;

    // Create the display string
    (void)xc_sprintf (as_displayText,
                      ps8_displayTemplate,
                      ((u24_nrOfPulses * u24_unitsPerKPulses + 500) / 1000) / 1000,
                      ((u24_nrOfPulses * u24_unitsPerKPulses + 500) / 1000) % 1000) ;

    // Add padding spaces to overwrite left-overs
    for (u8_index = strlen(as_displayText);  u8_index < 40; u8_index ++)
    {
      as_displayText[u8_index] = ' ' ;
    }
    as_displayText[40] = 0x00 ;

    // Set the proper display position
    (void)LCD_SetDdPosition (pt_LCDinstance, 0x40) ;

    // Write the display string to the display
    (void)LCD_Write (pt_LCDinstance, as_displayText, strlen(as_displayText)) ;
  }

  return(OK);
}
// End: viewLoadProcess


static PROCESS viewLogProcess (char           const * const ps8_displayTemplate,
                               unsigned short       * const u16_logEntry,
                               unsigned int           const u24_unitsPerKPulses)
////////////////////////////////////////////////////////////////////////////////
// Function:       viewLogProcess                                             //
//                 - Mini application to show the log on the display          //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  void*               pv_bmmInstance ;
  unsigned short      u16_nrOfBuckets ;
  BMM_bucket          t_bucket ;
  RTC_DateTime_struct t_timeStamp ;
  char                as_displayText[41] ;
  unsigned char       u8_index ;

  for (;;)
  {
    // Wait for a bucket-change event
    pv_bmmInstance = KE_MBoxReceive () ;

    // Retrieve the new number of buckets
    (void)BMM_GetNrOfBuckets (pv_bmmInstance, &u16_nrOfBuckets) ;

    // Check if there are any buckets at all
    if (u16_nrOfBuckets > 0)
    {
      // Check if the requested bucket is in range of the number of buckets available
      if (*u16_logEntry >= u16_nrOfBuckets)
      {
        // If not, set the requested bucket to the last bucket available
        *u16_logEntry = u16_nrOfBuckets - 1 ;
      }

      // Retrieve the requested bucket
      (void)BMM_GetBucketCont (pv_bmmInstance, (u16_nrOfBuckets - 1) - *u16_logEntry, &t_bucket) ;

      RTC_Seconds2Date (t_bucket.u32_timeStamp, &t_timeStamp) ;

      (void)xc_sprintf (as_displayText,
                        ps8_displayTemplate,
                        (*u16_logEntry) + 1,
                        t_timeStamp.u8_day,
                        t_timeStamp.u8_month,
                        t_timeStamp.u16_year,
                        t_timeStamp.u8_hour,
                        t_timeStamp.u8_minute,
                        t_timeStamp.u8_second,
                        ((t_bucket.u24_value * u24_unitsPerKPulses + 500) / 1000) / 1000,
                        ((t_bucket.u24_value * u24_unitsPerKPulses + 500) / 1000) % 1000) ;

    }
    else
    {
      (void)xc_sprintf (as_displayText, "- Log is empty -") ;
    }

    // Add padding spaces to overwrite left-overs
    for (u8_index = strlen(as_displayText);  u8_index < 40; u8_index ++)
    {
      as_displayText[u8_index] = ' ' ;
    }
    as_displayText[40] = 0x00 ;

    // Set the proper display position
    (void)LCD_SetDdPosition (pt_LCDinstance, 0x40) ;

    // Write the display string to the display
    (void)LCD_Write (pt_LCDinstance, as_displayText, strlen(as_displayText)) ;
  }

  return(OK);
}
// End: viewLogProcess


static PROCESS clockProcess (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       clockProcess                                               //
//                 - Mini application to show the date and time on the display//
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  static const char   as8_timeTemplate[] = "%02u/%02u/%04u - %02u:%02u.%02u          MeterMaid" ;
  unsigned long       u32_currTime ;
  RTC_DateTime_struct t_currTime ;
  char                as_displayText[41] ;

  for (;;)
  {
    (void)KE_MBoxReceive () ;

    RTC_GetTime (&u32_currTime) ;
    RTC_Seconds2Date (u32_currTime, &t_currTime) ;

    (void)xc_sprintf (as_displayText,
                      as8_timeTemplate,
                      t_currTime.u8_day,
                      t_currTime.u8_month,
                      t_currTime.u16_year,
                      t_currTime.u8_hour,
                      t_currTime.u8_minute,
                      t_currTime.u8_second) ;
    (void)LCD_SetDdPosition (pt_LCDinstance, 0x00) ;
    (void)LCD_Write (pt_LCDinstance, as_displayText, strlen(as_displayText)) ;
  }

  return(OK);
}
// End: clockProcess


static void initMeter (PHD_handle     * const ppt_PhdInstance,
                       BMM_handle     * const ppt_BmmMinuteInstance,
                       BMM_handle     * const ppt_BmmHourInstance,
                       BMM_handle     * const ppt_BmmDayInstance,
                       unsigned short   const u16_maxPulsesPerMinute)
////////////////////////////////////////////////////////////////////////////////
// Function:       initMeter                                                  //
//                 - Initialization sequence of one meter (to prevent tripple //
//                   appearance of the same sequence)                         //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PID t_tempProcId = NULL ;

  // Create the bucket memory for days
  (void)BMM_Create          (ppt_BmmDayInstance, NOF_DAYS) ;

  // Make the fill process fetch new pulses using 'BMM_GetPulses'
  (void)BMM_SetMeteringFunc (*ppt_BmmDayInstance, &BMM_GetPulses) ;

  // Retrieve the Pid of the fill process which will fill day-buckets
  (void)BMM_GetMeteringProc (*ppt_BmmDayInstance, &t_tempProcId) ;


  // Create the bucket memory for hours
  (void)BMM_Create          (ppt_BmmHourInstance, NOF_HOURS) ;

  // Notify the Pid of the fill process of day-buckets if new pulses are fetched
  (void)BMM_SetSlaveProc    (*ppt_BmmHourInstance, t_tempProcId) ;

  // Make the fill process fetch new pulses using 'BMM_GetPulses'
  (void)BMM_SetMeteringFunc (*ppt_BmmHourInstance, &BMM_GetPulses) ;

  // Retrieve the Pid of the fill process which will fill hour-buckets
  (void)BMM_GetMeteringProc (*ppt_BmmHourInstance, &t_tempProcId) ;


  // Create the bucket memory for minutes
  (void)BMM_Create          (ppt_BmmMinuteInstance, NOF_MINUTES) ;

  // Notify the Pid of the fill process of hour-buckets if new pulses are fetched
  (void)BMM_SetSlaveProc    (*ppt_BmmMinuteInstance, t_tempProcId) ;

  // Make the fill process fetch new pulses using 'PHD_GetPulses'
  (void)BMM_SetMeteringFunc (*ppt_BmmMinuteInstance, &PHD_GetPulses) ;

  // Retrieve the Pid of the fill process which will fill minute-buckets
  (void)BMM_GetMeteringProc (*ppt_BmmMinuteInstance, &t_tempProcId) ;


  // Create a pulse handler
  (void)PHD_Create          (ppt_PhdInstance, u16_maxPulsesPerMinute) ;

  // Notify the Pid of the fill process of minute-buckets if new pulses are fetched
  (void)PHD_SetStorageClient(*ppt_PhdInstance, t_tempProcId) ;


  // Retrieve the buckets bucket-change process ID
  (void)BMM_GetBucketProc   (*ppt_BmmMinuteInstance, &t_tempProcId) ;

  // Add the bucket-change process as an RTC event client
  (void)RTC_AddClient       (e_minuteEvent, t_tempProcId, NULL) ;


  // Retrieve the buckets bucket-change process ID
  (void)BMM_GetBucketProc   (*ppt_BmmHourInstance, &t_tempProcId) ;

  // Add the bucket-change process as an RTC event client
  (void)RTC_AddClient       (e_hourEvent, t_tempProcId, NULL) ;


  // Retrieve the buckets bucket-change process ID
  (void)BMM_GetBucketProc   (*ppt_BmmDayInstance, &t_tempProcId) ;

  // Add the bucket-change process as an RTC event client
  (void)RTC_AddClient       (e_dayEvent, t_tempProcId, NULL) ;

  return ;
}
// End: initMeter


#pragma interrupt
static void ISR_ElectPulse (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       ISR_ElectPulse                                             //
//                 - Just dispatching the interrupt. We didn't want to put    //
//                   this into the PHD module)                                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PB_DR   |= B0_MASK ;

  (void)PHD_HandlePulse (pt_PHDelectInst) ;

  PB_DR   &= ~B0_MASK ;

  return ;
}
// End: ISR_ElectPulse


#pragma interrupt
static void ISR_GasPulse (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       ISR_GasPulse                                               //
//                 - Just dispatching the interrupt. We didn't want to put    //
//                   this into the PHD module)                                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PB_DR   |= B1_MASK ;

  (void)PHD_HandlePulse (pt_PHDgasInst) ;

  PB_DR   &= ~B1_MASK ;

  return ;
}
// End: ISR_GasPulse


#pragma interrupt
static void ISR_WaterPulse (void)
////////////////////////////////////////////////////////////////////////////////
// Function:       ISR_WaterPulse                                             //
//                 - Just dispatching the interrupt. We didn't want to put    //
//                   this into the PHD module)                                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  PB_DR   |= B2_MASK ;

  (void)PHD_HandlePulse (pt_PHDwaterInst) ;

  PB_DR   &= ~B2_MASK ;

  return ;
}
// End: ISR_WaterPulse
