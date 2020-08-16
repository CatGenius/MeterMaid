////////////////////////////////////////////////////////////////////////////////
// File    : WEB_Site.c
// Function: Web Site Description
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define WEB_SITE_C
#include <kernel.h>
#include <http.h>
#include <httpd.h>
#include "WEB_Site.h"

#include "RTC_RealTimeClock.h"
#include "CNV_Conversions.h"
#include "BMM_BucketMemory.h" // ToDo: Remove BMM import, functions should be parsed at 'create'
#include "PHD_PulseHandler.h" // ToDo: Remove PHD import, functions should be parsed at 'create'


#define WEB_MAX_TABLES        (10)
#define WEB_MAX_METERS        (4)

#define WEB_MAX_NAME          (50)
#define WEB_MAX_UNIT          (16)

#define WEB_TABLE_SIGNATURE   ('TAB')
#define WEB_METER_SIGNATURE   ('MET')

#define WEB_TABLE_PTR_INVALID(p)  (p->u24_signature != WEB_TABLE_SIGNATURE)
#define WEB_METER_PTR_INVALID(p)  (p->u24_signature != WEB_METER_SIGNATURE)

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

// HTML pages
extern const struct staticpage index_html ;
extern const struct staticpage top_html ;
extern const struct staticpage content_html ;
extern const struct staticpage main_html ;

// Pictures: JPG, GIF
extern const struct staticpage MeterMaid_jpg ;
extern const struct staticpage anybrowser_gif ;


typedef char      WEB_tableEntry[128] ;

typedef struct
{
  unsigned int    u24_signature ;
  char            as8_frameName[WEB_MAX_NAME] ;
  char            as8_tableName[WEB_MAX_NAME] ;
  char            as8_unitName[WEB_MAX_UNIT] ;
  unsigned int    u24_unitsPerKPulses ;
  unsigned short  u16_webNumber ;
  unsigned short  u16_nrOfEntries ;
  unsigned short  u16_currEntries ;
  WEB_tableEntry* at_entry ;
  PID             t_processId ;
} WEB_tableInst_struct ;

typedef struct
{
  unsigned int    u24_signature ;
  char            as8_frameName[WEB_MAX_NAME] ;
  char            as8_meterName[WEB_MAX_NAME] ;
  char            as8_unitName[WEB_MAX_UNIT] ;
  unsigned int    u24_unitsPerKPulses ;
  unsigned int    u24_maxCapacity ;
  unsigned int    u24_currentLoad ;
  unsigned char   u8_currentPerc ;
  unsigned short  u16_webNumber ;
  PID             t_processId ;
} WEB_meterInst_struct ;

static WEB_tableInst_struct * pt_tableInstance[WEB_MAX_TABLES] ;
static WEB_meterInst_struct * pt_meterInstance[WEB_MAX_METERS] ;


////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static SYSCALL WEB_Table        (struct http_request *request) ;
static SYSCALL WEB_Meter        (struct http_request *request) ;
static SYSCALL WEB_Grahpic      (struct http_request *request) ;
static PROCESS WEB_FillProcess  (WEB_handle  const pt_instance) ;
static PROCESS WEB_MeterProcess (WEB_handle  const pt_instance) ;


////////////////////////////////////////////////////////////////////////////////
// The page                                                                   //
////////////////////////////////////////////////////////////////////////////////

Webpage at_webSite[] = {
  {HTTP_PAGE_STATIC,  "/",                    "text/html", &index_html },
  {HTTP_PAGE_STATIC,  "/index.htm",           "text/html", &index_html },
  {HTTP_PAGE_STATIC,  "/index.html",          "text/html", &index_html },
  {HTTP_PAGE_STATIC,  "/top.html",            "text/html", &top_html },
  {HTTP_PAGE_STATIC,  "/content.html",        "text/html", &content_html },
  {HTTP_PAGE_STATIC,  "/main.html",           "text/html", &main_html },
  {HTTP_PAGE_DYNAMIC, "/table.cgi",           "text/html", (struct staticpage *)WEB_Table },
  {HTTP_PAGE_DYNAMIC, "/meter.cgi",           "text/html", (struct staticpage *)WEB_Meter },
  {HTTP_PAGE_STATIC,  "/metermaid.jpg",       "image/jpg", &MeterMaid_jpg },
  {HTTP_PAGE_STATIC,  "/anybrowser.gif",      "image/gif", &anybrowser_gif },
  {0,                 NULL,                   NULL,        NULL }
};


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

WEB_status WEB_Initialize (Webpage * * const ppt_webPage)
////////////////////////////////////////////////////////////////////////////////
// Function:       Pulse handler construction routine                         //
//                 - Creates an instance                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status    result = WEB_OK ;
  unsigned char u8_index ;

  // Initialize all web table instances
  for (u8_index = 0; u8_index < WEB_MAX_TABLES; u8_index ++)
  {
    pt_tableInstance[u8_index] = NULL ;
  }

  // Initialize all web meter instances
  for (u8_index = 0; u8_index < WEB_MAX_METERS; u8_index ++)
  {
    pt_meterInstance[u8_index] = NULL ;
  }

  // Fill out the pointer to the website
  *ppt_webPage = &at_webSite[0] ;

  return (result) ;
}


WEB_status WEB_CreateTable (WEB_handle           * const ppt_instance,
                            unsigned short         const u16_nrOfEntries,
                            unsigned int           const u24_unitsPerKPulses,
                            unsigned short         const u16_webNumber,
                            char           const * const ps8_frameName,
                            char           const * const ps8_tableName,
                            char           const * const ps8_unitName)
////////////////////////////////////////////////////////////////////////////////
// Function:       Web table construction routine                             //
//                 - Creates an instance                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status             result      = WEB_OK ;
  WEB_tableInst_struct * pt_this ;
  unsigned char          u8_index     = 0 ;

  if (result == WEB_OK)
  {
    // Do parameter check
    if ( (ppt_instance    == NULL) ||
         (u16_nrOfEntries == 0   )    )
    {
      (void)xc_printf ("WEB_CreateTable: Parameter error.\n") ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Find an empty lut entry
    while ( (pt_tableInstance[u8_index] != NULL          ) &&
            (u8_index                    < WEB_MAX_TABLES)    )
    {
      u8_index ++ ;
    }

    if (u8_index >= WEB_MAX_TABLES)
    {
      (void)xc_printf ("WEB_CreateTable: No free slot.\n") ;
      result = WEB_ERR_NOFREESLOT ;
    }
  }

  if (result == WEB_OK)
  {
    // Allocate memory for this instance
    pt_this = getmem (sizeof(WEB_tableInst_struct)) ;
    if (pt_this == NULL)
    {
      (void)xc_printf ("WEB_CreateTable: Memory error (instance).\n") ;
      result = WEB_ERR_MEMORY ;
    }
  }

  if (result == WEB_OK)
  {
    // Initialize global variables of this instance
    pt_this->u24_signature        = WEB_TABLE_SIGNATURE ;
    pt_this->u24_unitsPerKPulses  = u24_unitsPerKPulses ;
    pt_this->u16_webNumber        = u16_webNumber ;
    pt_this->u16_nrOfEntries      = u16_nrOfEntries ;
    pt_this->u16_currEntries      = 0 ;
    strncpy (pt_this->as8_frameName, ps8_frameName, WEB_MAX_NAME) ;
    strncpy (pt_this->as8_tableName, ps8_tableName, WEB_MAX_NAME) ;
    strncpy (pt_this->as8_unitName,  ps8_unitName,  WEB_MAX_UNIT) ;

    // Allocate memory for buckets
    pt_this->at_entry = getmem (pt_this->u16_nrOfEntries * sizeof(WEB_tableEntry)) ;
    if (pt_this->at_entry == NULL)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this, sizeof(WEB_tableInst_struct)) ;

      (void)xc_printf ("WEB_CreateTable: Memory error (entries).\n") ;
      result = WEB_ERR_MEMORY ;
    }
  }

  if (result == WEB_OK)
  {
    // Create the pulse process task
    pt_this->t_processId = KE_TaskCreate ( (procptr)WEB_FillProcess,  // Function
                                           256,                       // Stack size
                                           10,                        // Priority
                                           "WEB_Process",             // Name
                                           1,                         // Number of arguments
                                           pt_this ) ;                // Arg...

    if (pt_this->t_processId == 0)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this->at_entry, pt_this->u16_nrOfEntries * sizeof(WEB_tableEntry)) ;
      (void)freemem (pt_this, sizeof(WEB_tableInst_struct)) ;

      (void)xc_printf ("WEB_CreateTable: Process error (create).\n") ;
      result = WEB_ERR_PROCESS ;
    }
  }

  if (result == WEB_OK)
  {
    if ( KE_TaskResume(pt_this->t_processId) == SYSERR)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)KE_TaskDelete (pt_this->t_processId) ;
      (void)freemem (pt_this->at_entry, pt_this->u16_nrOfEntries * sizeof(WEB_tableEntry)) ;
      (void)freemem (pt_this, sizeof(WEB_tableInst_struct)) ;

      (void)xc_printf ("WEB_CreateTable: Process error (resume).\n") ;
      result = WEB_ERR_PROCESS ;
    }
  }

  if (result == WEB_OK)
  {
    // Fill out lut entry
    pt_tableInstance[u8_index] = pt_this ;

    // Fill out the instance pointer
    *ppt_instance = pt_this ;
  }

  return (result) ;
}
// End: WEB_CreateTable


WEB_status WEB_DeleteTable (WEB_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       Web table destruction routine                              //
//                 - Destroys an instance                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status                   result  = WEB_OK ;
  WEB_tableInst_struct * const pt_this = pt_instance ;

  if (result == WEB_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("WEB_DeleteTable: Parameter error.\n") ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Check if the pointer is valid
    if (WEB_TABLE_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("WEB_DeleteTable: Invalid pointer.\n") ;
      result = WEB_ERR_POINTER ;
    }
  }

  if (result == WEB_OK)
  {
    // Kill the task
    (void)KE_TaskDelete (pt_this->t_processId) ;

    // Invalidate the pointer
    pt_this->u24_signature = 0x000000 ;

    // Return the memory to the memory manager
    (void)freemem (pt_this->at_entry, pt_this->u16_nrOfEntries * sizeof(WEB_tableEntry)) ;
    (void)freemem (pt_this, sizeof(WEB_tableInst_struct)) ;
  }

  return (result) ;
}
// End: WEB_DeleteTable


WEB_status WEB_CreateMeter (WEB_handle           * const ppt_instance,
                            unsigned int           const u24_unitsPerKPulses,
                            unsigned int           const u24_maxCapacity,
                            unsigned short         const u16_webNumber,
                            char           const * const ps8_frameName,
                            char           const * const ps8_meterName,
                            char           const * const ps8_unitName)
////////////////////////////////////////////////////////////////////////////////
// Function:       Web meter construction routine                             //
//                 - Creates an instance                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status             result      = WEB_OK ;
  WEB_meterInst_struct * pt_this ;
  unsigned char          u8_index     = 0 ;

  if (result == WEB_OK)
  {
    // Do parameter check
    if (ppt_instance    == NULL)
    {
      (void)xc_printf ("WEB_CreateMeter: Parameter error.\n") ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Find an empty lut entry
    while ( (pt_meterInstance[u8_index] != NULL          ) &&
            (u8_index                    < WEB_MAX_METERS)    )
    {
      u8_index ++ ;
    }

    if (u8_index >= WEB_MAX_METERS)
    {
      (void)xc_printf ("WEB_CreateMeter: No free slot.\n") ;
      result = WEB_ERR_NOFREESLOT ;
    }
  }

  if (result == WEB_OK)
  {
    // Allocate memory for this instance
    pt_this = getmem (sizeof(WEB_meterInst_struct)) ;
    if (pt_this == NULL)
    {
      (void)xc_printf ("WEB_CreateMeter: Memory error.\n") ;
      result = WEB_ERR_MEMORY ;
    }
  }

  if (result == WEB_OK)
  {
    // Initialize global variables of this instance
    pt_this->u24_signature        = WEB_METER_SIGNATURE ;
    pt_this->u24_unitsPerKPulses  = u24_unitsPerKPulses ;
    pt_this->u16_webNumber        = u16_webNumber ;
    pt_this->u24_maxCapacity      = u24_maxCapacity ;
    pt_this->u24_currentLoad      = 0 ;
    pt_this->u8_currentPerc       = 0 ;
    strncpy (pt_this->as8_frameName, ps8_frameName, WEB_MAX_NAME) ;
    strncpy (pt_this->as8_meterName, ps8_meterName, WEB_MAX_NAME) ;
    strncpy (pt_this->as8_unitName,  ps8_unitName,  WEB_MAX_UNIT) ;
  }

  if (result == WEB_OK)
  {
    // Create the pulse process task
    pt_this->t_processId = KE_TaskCreate ( (procptr)WEB_MeterProcess, // Function
                                           256,                       // Stack size
                                           10,                        // Priority
                                           "WEB_Process",             // Name
                                           1,                         // Number of arguments
                                           pt_this ) ;                // Arg...

    if (pt_this->t_processId == 0)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)freemem (pt_this, sizeof(WEB_meterInst_struct)) ;

      (void)xc_printf ("WEB_CreateMeter: Process error (create).\n") ;
      result = WEB_ERR_PROCESS ;
    }
  }

  if (result == WEB_OK)
  {
    if ( KE_TaskResume(pt_this->t_processId) == SYSERR)
    {
      // Clean up
      pt_this->u24_signature = 0x000000 ;
      (void)KE_TaskDelete (pt_this->t_processId) ;
      (void)freemem (pt_this, sizeof(WEB_meterInst_struct)) ;

      (void)xc_printf ("WEB_CreateMeter: Process error (resume).\n") ;
      result = WEB_ERR_PROCESS ;
    }
  }

  if (result == WEB_OK)
  {
    // Fill out lut entry
    pt_meterInstance[u8_index] = pt_this ;

    // Fill out the instance pointer
    *ppt_instance = pt_this ;
  }

  return (result) ;
}
// End: WEB_CreateMeter


WEB_status WEB_DeleteMeter (WEB_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       Web meter destruction routine                              //
//                 - Destroys an instance                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status                   result  = WEB_OK ;
  WEB_meterInst_struct * const pt_this = pt_instance ;

  if (result == WEB_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("WEB_DeleteMeter: Parameter error.\n") ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Check if the pointer is valid
    if (WEB_METER_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("WEB_DeleteMeter: Invalid pointer.\n") ;
      result = WEB_ERR_POINTER ;
    }
  }

  if (result == WEB_OK)
  {
    // Kill the task
    (void)KE_TaskDelete (pt_this->t_processId) ;

    // Invalidate the pointer
    pt_this->u24_signature = 0x000000 ;

    // Return the memory to the memory manager
    (void)freemem (pt_this, sizeof(WEB_meterInst_struct)) ;
  }

  return (result) ;
}
// End: WEB_DeleteMeter


WEB_status WEB_GetProcessId (WEB_handle   const pt_instance,
                             PID        * const pt_processId)
////////////////////////////////////////////////////////////////////////////////
// Function:       WEB_GetProcessId                                           //
//                 - Retrieves the process ID of the meter process            //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status                   result       = WEB_OK ;
  WEB_tableInst_struct * const pt_thisTable = pt_instance ;
  WEB_meterInst_struct * const pt_thisMeter = pt_instance ;

  if (result == WEB_OK)
  {
    // Do parameter check
    if ( (pt_instance  == NULL) ||
         (pt_processId == NULL)    )
    {
      (void)xc_printf ("WEB_GetProcessId: Parameter error.\n") ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Check if the pointer is valid
    if (!WEB_TABLE_PTR_INVALID(pt_thisTable))
    {
      // Fill out the process ID
      *pt_processId = pt_thisTable->t_processId ;
    }
    else if (!WEB_METER_PTR_INVALID(pt_thisMeter))
    {
      // Fill out the process ID
      *pt_processId = pt_thisMeter->t_processId ;
    }
    else
    {
      (void)xc_printf ("WEB_GetProcessId: Invalid pointer.\n") ;
      result = WEB_ERR_POINTER ;
    }
  }

  return (result) ;
}
// End: WEB_GetMeterProcId


////////////////////////////////////////////////////////////////////////////////
// CGI Implementations                                                        //
////////////////////////////////////////////////////////////////////////////////

static const char as8_pageStart[]         = "<html>" \
                                             "<head>" ;
static const char as8_pageRefr[]          =   "<meta http-equiv=\"refresh\" content=\"%u\">" ;
static const char as8_pageRefr_c[]        =   "<meta http-equiv=\"Pragma\" content=\"no-cache\">" \
                                              "<meta http-equiv=\"cache control\" content=\"no-cache\">" \
                                              "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">" \
                                             "</head>" \
                                             "<body text=\"#000000\" bgcolor=\"#FFFFFF\" link=\"#0000EE\" vlink=\"#551A8B\" alink=\"#FF0000\">" \
                                              "<p>" \
                                              "<table border=0 cellspacing=\"6\" cellpadding=\"6\" width=\"100%\">" \
                                               "<tr bgcolor=\"#000080\">" \
                                                "<td>" \
                                                 "<b><tt>" \
                                                  "<font color=\"#FFFF00\">" ;
static const char as8_pageFrameName[]     =        "<font size=\"+1\">%s</font>" ;
static const char as8_pageFrameName_c[]   =       "</font>" \
                                                 "</tt></b>" \
                                                "</td>" \
                                               "</tr>" ;

static const char as8_pageLogTable[]      =    "<tr>" \
                                                "<td>" \
                                                 "<div align=\"center\">" \
                                                  "<table border=\"1\" width=\"55%\" bgcolor=\"#4564B4\" cellspacing=\"3\" cellpadding=\"0\" cols=\"3\" style=\"color: rgb(255,255,0)\">" \
                                                   "<tr>" \
                                                    "<td colspan=\"3\" bgcolor=\"#000080\">" \
                                                     "<p align=\"center\">" \
                                                      "<fontsize=\"3\" color=\"#FFFFFF\">" ;
static const char as8_pageTableTitle[]    =            "<b>%s</b>" ;
static const char as8_pageTableTitle_c[]  =           "</font>" \
                                                     "</p>" \
                                                    "</td>" \
                                                   "</tr>" ;
static const char as8_pageTableNumEntry[] =        "<tr>" \
                                                    "<td><b>%02u/%02u/%04u %02u:%02u.%02u</b></td>" \
                                                    "<td align=\"right\"><b>%u</b></td>" \
                                                    "<td align=\"right\"><b>%u.%03u</b></td>" \
                                                   "</tr>" ;
static const char as8_pageTableTxtEntry[] =        "<tr>" \
                                                    "<td align=\"center\"><b>%s</b></td>" \
                                                    "<td align=\"center\"><b>%s</b></td>" \
                                                    "<td align=\"center\"><b>%s</b></td>" \
                                                   "</tr>" ;
static const char as8_pageLogTable_c[]    =       "</table>" \
                                                 "</div>" \
                                                "</td>" \
                                               "</tr>" ;

static const char as8_pageMeter[]         =    "<tr>" \
                                                "<td>" \
                                                 "<div align=\"center\">" \
                                                  "<table border=\"1\" width=\"55%\" bgcolor=\"#4564B4\" cellspacing=\"3\" cellpadding=\"0\" cols=\"2\" style=\"color: rgb(255,255,0)\">" \
                                                   "<tr>" \
                                                    "<td colspan=\"2\" bgcolor=\"#000080\">" \
                                                     "<p align=\"center\">" \
                                                      "<fontsize=\"3\" color=\"#FFFFFF\">" ;
static const char as8_pageMeterTitle[]    =            "<b>%s</b>" ;
static const char as8_pageMeterTitle_c[]  =           "</font>" \
                                                     "</p>" \
                                                    "</td>" \
                                                   "</tr>" ;
static const char as8_pageMeterEntry[]    =        "<tr>" \
                                                    "<td align=\"center\"><b>%u.%03u %s</b></td>" \
                                                    "<td align=\"center\"><b>%u %%</b></td>" \
                                                   "</tr>" ;
static const char as8_pageMeter_c[]       =       "</table>" \
                                                 "</div>" \
                                                "</td>" \
                                               "</tr>" ;

static const char as8_pageEnd[]           =   "</table>" \
                                             "</body>" \
                                            "</html>" ;

SYSCALL WEB_Table (struct http_request *request)
////////////////////////////////////////////////////////////////////////////////
// Function:       WEB_Table                                                  //
//                 - Sends a generated table to a client                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status          result           = WEB_OK ;

  static const BYTE   at_tableParam[]  = "table" ;
  unsigned short      u16_tableNumber ;
  char *              as8_buffer       = getmem (1024) ;
  unsigned long       u32_currDateTime ;
  RTC_DateTime_struct t_currDateTime ;
  unsigned char       u8_index ;
  unsigned char       u8_tableIndex ;

  if (result == WEB_OK)
  {
    // Check the number of parameters
    if (request->numparams != 1)
    {
      (void)xc_printf ("WEB_Table: Parameter error (number).\n") ;
      // Tell the client the request is erroneous
      http_output_reply (request, HTTP_404_NOT_FOUND) ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Retrieve the values of the parameters
    for (u8_index = 0; (u8_index < request->numparams) && (result == WEB_OK); u8_index ++)
    {
      // Fill out a pointer to the parameter name for easier reference
      char const * const ps8_paramName = (char *)(request->params[u8_index].key) ;

      /* Check if we're looking for this name */
      if (strcmp(at_tableParam, ps8_paramName) == 0)
      {
        // Fetch the value of the "table" parameter
        CNV_StringToUInt16 (&u16_tableNumber,
                            http_find_argument (request, at_tableParam),
                            e_radix_hexadecimal) ;
      }
      else
      {
        (void)xc_printf ("WEB_Table: Parameter error (name).\n") ;
        // Tell the client the request is erroneous
        http_output_reply (request, HTTP_404_NOT_FOUND) ;
        result = WEB_ERR_PARAM ;
      }
    }
  }

  if (result == WEB_OK)
  {
    // Lookup the table instance of the requested table number
    u8_tableIndex = 0 ;
    while ( ( (pt_tableInstance[u8_tableIndex]                == NULL           ) ||
              (pt_tableInstance[u8_tableIndex]->u16_webNumber != u16_tableNumber)    ) &&
            (u8_tableIndex < WEB_MAX_TABLES                                          )    )
    {
      u8_tableIndex ++ ;
    }

    if (u8_tableIndex >= WEB_MAX_TABLES)
    {
      (void)xc_printf ("WEB_Table: Parameter error (value).\n") ;
      // Tell the client the request is erroneous
      http_output_reply (request, HTTP_404_NOT_FOUND) ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Tell the client the request has been granted
    http_output_reply (request, HTTP_200_OK) ;

    // Send the first static part of the page
    __http_write (request, as8_pageStart, strlen(as8_pageStart)) ;

    // Set the refresh time to two seconds after the next whole minute
    RTC_GetTime (&u32_currDateTime) ;
    RTC_Seconds2UTC (u32_currDateTime, &t_currDateTime) ;
    xc_sprintf (as8_buffer, as8_pageRefr, 60 - t_currDateTime.u8_second) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    // Send the next static part of the page
    __http_write (request, as8_pageRefr_c, strlen(as8_pageRefr_c)) ;

    // Fill out the text in the banner
    xc_sprintf (as8_buffer, as8_pageFrameName, pt_tableInstance[u8_tableIndex]->as8_frameName) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    __http_write (request, as8_pageFrameName_c, strlen(as8_pageFrameName_c)) ;
    __http_write (request, as8_pageLogTable,    strlen(as8_pageLogTable)   ) ;

    // Fill out the text in the table
    xc_sprintf (as8_buffer, as8_pageTableTitle, pt_tableInstance[u8_tableIndex]->as8_tableName) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    __http_write (request, as8_pageTableTitle_c, strlen(as8_pageTableTitle_c)) ;

    for (u8_index = 0; u8_index < pt_tableInstance[u8_tableIndex]->u16_currEntries; u8_index ++)
    {
      __http_write (request,
                    pt_tableInstance[u8_tableIndex]->at_entry[u8_index],
                    strlen(pt_tableInstance[u8_tableIndex]->at_entry[u8_index])) ;
    }

    // Show meaning of the numbers at the bottom of the table
    xc_sprintf (as8_buffer,
                as8_pageTableTxtEntry,
                "Time stamp",
                "Pulses",
                pt_tableInstance[u8_tableIndex]->as8_unitName) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    __http_write (request, as8_pageLogTable_c, strlen(as8_pageLogTable_c)) ;
    __http_write (request, as8_pageEnd,        strlen(as8_pageEnd)       ) ;
  }

  freemem (as8_buffer, 1024) ;

  return (OK) ;
}

SYSCALL WEB_Meter (struct http_request *request)
////////////////////////////////////////////////////////////////////////////////
// Function:       WEB_Meter                                                  //
//                 - Sends a generated meter to a client                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  WEB_status          result           = WEB_OK ;

  static const BYTE   at_meterParam[] = "meter" ;
  unsigned short      u16_meterNumber ;
  char *              as8_buffer       = getmem (1024) ;
  unsigned char       u8_index ;
  unsigned char       u8_meterIndex ;

  if (result == WEB_OK)
  {
    // Check the number of parameters
    if (request->numparams != 1)
    {
      (void)xc_printf ("WEB_Meter: Parameter error (number).\n") ;
      // Tell the client the request is erroneous
      http_output_reply (request, HTTP_404_NOT_FOUND) ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Retrieve the values of the parameters
    for (u8_index = 0; (u8_index < request->numparams) && (result == WEB_OK); u8_index ++)
    {
      // Fill out a pointer to the parameter name for easier reference
      char const * const ps8_paramName = (char *)(request->params[u8_index].key) ;

      /* Check if we're looking for this name */
      if (strcmp(at_meterParam, ps8_paramName) == 0)
      {
        // Fetch the value of the "meter" parameter
        CNV_StringToUInt16 (&u16_meterNumber,
                            http_find_argument (request, at_meterParam),
                            e_radix_hexadecimal) ;
      }
      else
      {
        (void)xc_printf ("WEB_Meter: Parameter error (name).\n") ;
        // Tell the client the request is erroneous
        http_output_reply (request, HTTP_404_NOT_FOUND) ;
        result = WEB_ERR_PARAM ;
      }
    }
  }

  if (result == WEB_OK)
  {
    // Lookup the meter instance of the requested meter number
    u8_meterIndex = 0 ;
    while ( ( (pt_meterInstance[u8_meterIndex]                == NULL           ) ||
              (pt_meterInstance[u8_meterIndex]->u16_webNumber != u16_meterNumber)    ) &&
            (u8_meterIndex < WEB_MAX_METERS                                          )    )
    {
      u8_meterIndex ++ ;
    }

    if (u8_meterIndex >= WEB_MAX_METERS)
    {
      (void)xc_printf ("WEB_Table: Parameter error (value).\n") ;
      // Tell the client the request is erroneous
      http_output_reply (request, HTTP_404_NOT_FOUND) ;
      result = WEB_ERR_PARAM ;
    }
  }

  if (result == WEB_OK)
  {
    // Tell the client the request has been granted
    http_output_reply (request, HTTP_200_OK) ;

    // Send the first static part of the page
    __http_write (request, as8_pageStart, strlen(as8_pageStart)) ;

    // Set the refresh time to two seconds
    xc_sprintf (as8_buffer, as8_pageRefr, 2) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    // Send the next static part of the page
    __http_write (request, as8_pageRefr_c, strlen(as8_pageRefr_c)) ;

    // Fill out the text in the banner
    xc_sprintf (as8_buffer, as8_pageFrameName, pt_meterInstance[u8_meterIndex]->as8_frameName) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    __http_write (request, as8_pageFrameName_c, strlen(as8_pageFrameName_c)) ;
    __http_write (request, as8_pageMeter,       strlen(as8_pageMeter)      ) ;

    // Fill out the text in the meter
    xc_sprintf (as8_buffer, as8_pageMeterTitle, pt_meterInstance[u8_meterIndex]->as8_meterName) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    __http_write (request, as8_pageTableTitle_c, strlen(as8_pageTableTitle_c)) ;


    // Show meaning of the numbers at the bottom of the table
    xc_sprintf (as8_buffer,
                as8_pageMeterEntry,
                ((pt_meterInstance[u8_meterIndex]->u24_currentLoad * pt_meterInstance[u8_meterIndex]->u24_unitsPerKPulses + 500) / 1000) / 1000,
                ((pt_meterInstance[u8_meterIndex]->u24_currentLoad * pt_meterInstance[u8_meterIndex]->u24_unitsPerKPulses + 500) / 1000) % 1000,
                pt_meterInstance[u8_meterIndex]->as8_unitName,
                pt_meterInstance[u8_meterIndex]->u8_currentPerc) ;
    __http_write (request, as8_buffer, strlen(as8_buffer)) ;

    __http_write (request, as8_pageMeter_c, strlen(as8_pageLogTable_c)) ;
    __http_write (request, as8_pageEnd,        strlen(as8_pageEnd)       ) ;
  }

  freemem (as8_buffer, 1024) ;

  return (OK) ;
}

static PROCESS WEB_FillProcess (WEB_handle  const pt_instance)
{
  WEB_tableInst_struct * const  pt_this = pt_instance ;
  RTC_DateTime_struct           t_currTime ;
  void*                         pv_bmmInstance ;
  unsigned short                u16_entryIndex ;
  BMM_bucket                    t_bucket ;

  for (;;)
  {
    // Wait for a bucket-change event
    pv_bmmInstance = KE_MBoxReceive () ;

    // Retrieve the new number of buckets
    (void)BMM_GetNrOfBuckets (pv_bmmInstance, &(pt_this->u16_currEntries)) ;

    for (u16_entryIndex = 0; u16_entryIndex < pt_this->u16_currEntries; u16_entryIndex ++)
    {
      (void)BMM_GetBucketCont (pv_bmmInstance, (pt_this->u16_currEntries-1) - u16_entryIndex, &t_bucket) ;

      RTC_Seconds2Date (t_bucket.u32_timeStamp, &t_currTime) ;

      xc_sprintf (pt_this->at_entry[u16_entryIndex], as8_pageTableNumEntry,
                  t_currTime.u8_day, t_currTime.u8_month, t_currTime.u16_year,
                  t_currTime.u8_hour, t_currTime.u8_minute, t_currTime.u8_second,
                  t_bucket.u24_value,
                  ((t_bucket.u24_value * pt_this->u24_unitsPerKPulses + 500) / 1000) / 1000,
                  ((t_bucket.u24_value * pt_this->u24_unitsPerKPulses + 500) / 1000) % 1000) ;
    }
  }

  return (OK) ;
}


static PROCESS WEB_MeterProcess (WEB_handle  const pt_instance)
{
  WEB_meterInst_struct * const  pt_this = pt_instance ;
  RTC_DateTime_struct           t_currTime ;
  void*                         pv_phdInstance ;
  unsigned int                  u24_nrOfPulses ;

  for (;;)
  {
    // Wait for a measurement-change event
    pv_phdInstance = KE_MBoxReceive () ;

    // Retrieve the new measurement data
    PHD_GetPulsesPerMinute (pv_phdInstance, &u24_nrOfPulses) ;

    // Extrapolate minute to an hour
    pt_this->u24_currentLoad = u24_nrOfPulses * 60 ;

    // Fill out the load percentage
    pt_this->u8_currentPerc  = (100 * u24_nrOfPulses) / pt_this->u24_maxCapacity ;
  }

  return (OK) ;
}
