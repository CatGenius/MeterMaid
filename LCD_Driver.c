////////////////////////////////////////////////////////////////////////////////
// File    : LCD_Driver.c
// Function: LC Display driver
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
// ToDo    : - Make 'pt_ctrlPort' and 'pt_dataPort' work. (Port A is hardcoded now)
//           - Make 8-bit mode work.
////////////////////////////////////////////////////////////////////////////////

#define LCD_DRIVER_C
#include <kernel.h>
#include "LCD_Driver.h"

#include "TMR_Timer.h"

#define LCD_SIGNATURE     ('LCD')

#define B0_MASK           0x01
#define B1_MASK           0x02
#define B2_MASK           0x04
#define B3_MASK           0x08
#define B4_MASK           0x10
#define B5_MASK           0x20
#define B6_MASK           0x40
#define B7_MASK           0x80

// Define LCD commands and their bit representations
#define LCD_CMD_CLEAR     (B0_MASK)   // 'Clear display'

#define LCD_CMD_HOME      (B1_MASK)   // 'Return home'

#define LCD_CMD_ENTRY     (B2_MASK)   // 'Entry mode set'
#define LCD_CMD_ENTRY_ID  (B1_MASK)   // Set for increment, clear for decrement
#define LCD_CMD_ENTRY_S   (B0_MASK)   // Set for shift, clear for no shift

#define LCD_CMD_CTRL      (B3_MASK)   // 'Display On/Off control'
#define LCD_CMD_CRTL_D    (B2_MASK)   // Set for display on, clear for display off
#define LCD_CMD_CRTL_C    (B1_MASK)   // Set for cursor on, clear for display off
#define LCD_CMD_CRTL_B    (B0_MASK)   // Set for blink on, clear for blink off

#define LCD_CMD_SHIFT     (B4_MASK)   // 'Cursor or display shift'
#define LCD_CMD_SHIFT_SC  (B3_MASK)   // Set for display shift, clear for cursor move
#define LCD_CMD_SHIFT_RL  (B2_MASK)   // Set for right, clear for left

#define LCD_CMD_FUNC      (B5_MASK)   // 'Function set'
#define LCD_CMD_FUNC_DL   (B4_MASK)   // Set for 8 bits datalength, clear for 4 bits data length
#define LCD_CMD_FUNC_N    (B3_MASK)   // Set for 2 lines, clear for 1 line
#define LCD_CMD_FUNC_F    (B2_MASK)   // Set for 5x10 font, clear for 5x7 font

#define LCD_CMD_SETCGADR  (B6_MASK)   // 'Set the CG RAM address'
#define LCD_CMD_CGMASK    (B0_MASK|B1_MASK|B2_MASK|B3_MASK|B4_MASK|B5_MASK) // Mask for CG address

#define LCD_CMD_SETDDADR  (B7_MASK)   // 'Set the DD RAM address'
#define LCD_CMD_DDMASK    (B0_MASK|B1_MASK|B2_MASK|B3_MASK|B4_MASK|B5_MASK|B6_MASK) // Mask for DD address

#define LCD_STAT_BFMASK   (B7_MASK)   // Status register busy flag
#define LCD_STAT_ACMASK   (B0_MASK|B1_MASK|B2_MASK|B3_MASK|B4_MASK|B5_MASK|B6_MASK)   // Mask for status register address counter

#define LCD_PTR_INVALID(p) (p->u24_signature != LCD_SIGNATURE)


////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  unsigned int    u24_signature ;
  unsigned char   u8_ctrlPortAdr ;
  unsigned char   u8_rsBitMask ;
  unsigned char   u8_rwBitMask ;
  unsigned char   u8_eBitMask ;
  unsigned char   u8_dataPortAdr ;
  unsigned char   u8_1stDataBit ;
  BOOL            b_incrementMode ;
  BOOL            b_shiftMode ;
  BOOL            b_displayOn ;
  BOOL            b_cursorOn ;
  BOOL            b_cursorBlink ;
  BOOL            b_cgAddressMode ;
  unsigned char   u8_lastDdAddress ;
  unsigned char   u8_lastCgAddress ;
} LCD_instance_struct ;

typedef enum
{
  space_Command = 0,   // Command space
  space_Data    = 1    // Data space
} LCD_space_enum ;

typedef unsigned volatile char* ioptr ;

////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static void LCD_PutControl    (LCD_handle     const pt_instance) ;

static void LCD_PutEntryMode  (LCD_handle     const pt_instance) ;

static void LCD_put           (LCD_handle     const pt_instance,
                               LCD_space_enum const t_memSpace,
                               unsigned char  const u8_data) ;

static unsigned char  LCD_get (LCD_handle     const pt_instance,
                               LCD_space_enum const t_memSpace) ;

////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

LCD_status LCD_Create (LCD_handle    * const ppt_instance,
                       ISFR            const pt_ctrlPort,
                       unsigned char   const u8_rsBitNr,
                       unsigned char   const u8_rwBitNr,
                       unsigned char   const u8_eBitNr,
                       ISFR            const pt_dataPort,
                       unsigned char   const u8_firstDataBitNr,
                       BOOL            const b_8BitMode,
                       BOOL            const b_2Lines,
                       BOOL            const b_5x10Font)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD module construction routine                            //
//                 - Creates an instance                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status            result      = LCD_OK ;
  LCD_instance_struct * pt_this ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (ppt_instance == NULL                            ) ||
         (pt_ctrlPort  == NULL                            ) ||
         (u8_rsBitNr   > 7                                ) ||
         (u8_rwBitNr   > 7                                ) ||
         (u8_eBitNr    > 7                                ) ||
         (pt_dataPort  == NULL                            ) ||
         ((u8_firstDataBitNr > 4) && (b_8BitMode == FALSE)) ||
         ((u8_firstDataBitNr > 0) && (b_8BitMode != FALSE))    )
    {
      (void)xc_printf ("LCD_Create: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Allocate memory for this instance
    pt_this = getmem (sizeof(LCD_instance_struct)) ;
    if (pt_this == NULL)
    {
      (void)xc_printf ("LCD_Create: Memory error (instance).\n") ;
      result = LCD_ERR_MEMORY ;
    }
  }

  if (result == LCD_OK)
  {
    // Initialize global variables of this instance
    pt_this->u24_signature    = LCD_SIGNATURE ;
    pt_this->u8_ctrlPortAdr   = (unsigned char)pt_ctrlPort ;
    pt_this->u8_rsBitMask     = (unsigned char)0x01 << u8_rsBitNr ;
    pt_this->u8_rwBitMask     = (unsigned char)0x01 << u8_rwBitNr ;
    pt_this->u8_eBitMask      = (unsigned char)0x01 << u8_eBitNr ;
    pt_this->u8_dataPortAdr   = (unsigned char)pt_dataPort ;
    pt_this->u8_1stDataBit    = u8_firstDataBitNr ;
    pt_this->b_incrementMode  = TRUE ;
    pt_this->b_shiftMode      = FALSE;
    pt_this->b_displayOn      = FALSE ;
    pt_this->b_cursorOn       = FALSE ;
    pt_this->b_cursorBlink    = FALSE ;
    pt_this->b_cgAddressMode  = FALSE ;
    pt_this->u8_lastDdAddress = 0x00 ;
    pt_this->u8_lastCgAddress = 0x00 ;

    PA_DDR  = 0x00 ;
    PA_DR   = 0x00 ;
    PA_ALT1 = 0x00 ;
    PA_ALT2 = 0x00 ;

    //////// LCD Init sequence: First Init //////
    // Clear RS and R/W
    PA_DR   = 0x00                    ;
    // Set data and E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              LCD_CMD_FUNC_DL         | // Select 8 bits mode (High nibble only; Do no shift)
              pt_this->u8_eBitMask    ;
    // Clear E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              LCD_CMD_FUNC_DL         ; // Select 8 bits mode (High nibble only; Do no shift)
    // Clear all
    PA_DR   = 0x00                    ;

    // Wait for 4.1 ms
    TMR_MicroDelay (4100) ;

    //////// LCD Init sequence: Second Init //////
    // Clear RS and R/W
    PA_DR   = 0x00                    ;
    // Set data and E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              LCD_CMD_FUNC_DL         | // Select 8 bits mode (High nibble only; Do no shift)
              pt_this->u8_eBitMask    ;
    // Clear E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              LCD_CMD_FUNC_DL         ; // Select 8 bits mode (High nibble only; Do no shift)
    // Clear all
    PA_DR   = 0x00                    ;

    // Wait for 100 us
    TMR_MicroDelay (100) ;

    //////// LCD Init sequence: Third Init //////
    // Clear RS and R/W
    PA_DR   = 0x00                    ;
    // Set data and E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              LCD_CMD_FUNC_DL         | // Select 8 bits mode (High nibble only; Do no shift)
              pt_this->u8_eBitMask ;
    // Clear E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              LCD_CMD_FUNC_DL         ; // Select 8 bits mode (High nibble only; Do no shift)
    // Clear all
    PA_DR   = 0x00                    ;

    // Wait display not busy
    while ((LCD_get(pt_this,space_Command) & LCD_STAT_BFMASK) !=0) ;

    ////// LCD Init sequence: Set interface mode //////
    // Clear RS and R/W
    PA_DR   = 0x00                    ;
    // Set data and E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              (b_8BitMode?LCD_CMD_FUNC_DL:0x00) | // Select _true_ mode (High nibble only; Do no shift)
              pt_this->u8_eBitMask ;
    // Clear E-line
    PA_DR   = LCD_CMD_FUNC            | // Function select command (High nibble only; Do no shift)
              (b_8BitMode?LCD_CMD_FUNC_DL:0x00) ; // Select _true_ mode (High nibble only; Do no shift)
    // Clear all
    PA_DR   = 0x00                    ;

    ////// LCD Init sequence: Set the selected parameters //////
    LCD_put (pt_this, space_Command, LCD_CMD_FUNC                   |     // Function select command
                                     (b_8BitMode?LCD_CMD_FUNC_DL:0) |     // Select _true_ mode
                                     (b_2Lines  ?LCD_CMD_FUNC_N :0) |     // Select the nr of lines
                                     (b_5x10Font?LCD_CMD_FUNC_F :0)   ) ; // Select the font

    ////// LCD Init sequence: Turn off the display //////
    LCD_put (pt_this, space_Command, LCD_CMD_CTRL) ;

    ////// LCD Init sequence: Clear the display //////
    LCD_put (pt_this, space_Command, LCD_CMD_CLEAR) ;

    ////// LCD Init sequence: Select the entry mode //////
    LCD_PutEntryMode (LCD_PutEntryMode) ;

    // Fill out the instance pointer
    *ppt_instance = pt_this ;
  }

  return (result) ;
}
// End: LCD_Create


LCD_status LCD_Delete (LCD_handle const pt_instance)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD module destruction routine                             //
//                 - Destroys an instance                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("LCD_Delete: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_Delete: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Invalidate the pointer
    pt_this->u24_signature = 0x000000 ;

    // Return the memory to the memory manager
    (void)freemem (pt_this, sizeof(LCD_instance_struct)) ;
  }

  return (result) ;
}
// End: LCD_Terminate


LCD_status LCD_Write (LCD_handle            const pt_instance,
                      unsigned char const * const u8_data,
                      size_t                const u8_length)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_Write                                                  //
//                 - Writes data to the display                               //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned char u8_cntr ;

  for (u8_cntr = 0; u8_cntr < u8_length; u8_cntr ++)
  {
    LCD_put (pt_instance, space_Data, u8_data[u8_cntr]) ;
  }
}
// End: LCD_Write


LCD_status LCD_Read (LCD_handle      const pt_instance,
                     unsigned char * const u8_data,
                     size_t          const u8_length)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_Read                                                   //
//                 - Reads data from the display                              //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  unsigned char u8_cntr ;

  for (u8_cntr = 0; u8_cntr < u8_length; u8_cntr ++)
  {
    u8_data[u8_cntr] = LCD_get (pt_instance, space_Data) ;
  }
}
// End: LCD_Read


LCD_status LCD_SetDisplayOn (LCD_handle const pt_instance,
                             BOOL       const b_on)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_SetDisplayOn                                           //
//                 - Turns on the display                                     //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("LCD_SetDisplayOn: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_SetDisplayOn: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Copy the new state
    pt_this->b_displayOn = b_on ;
    // Update the flag register
    LCD_PutControl (pt_this) ;
  }

  return (result) ;
}
// End: LCD_SetDisplayOn


LCD_status LCD_GetDisplayOn (LCD_handle   const pt_instance,
                             BOOL       * const b_on)
{
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_GetDisplayOn                                           //
//                 - Retrieves the display on/off state                       //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL) ||
         (b_on        == NULL)    )
    {
      (void)xc_printf ("LCD_GetDisplayOn: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_GetDisplayOn: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Fill out the current state
    *b_on = pt_this->b_displayOn ;
  }

  return (result) ;
}
// End: LCD_GetDisplayOn


LCD_status LCD_SetCursorOn (LCD_handle  const pt_instance,
                            BOOL        const b_on)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_SetCursorOn                                            //
//                 - Turns on the cursor                                      //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("LCD_SetCursorOn: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_SetCursorOn: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Copy the new state
    pt_this->b_cursorOn = b_on ;
    // Update the flag register
    LCD_PutControl (pt_this) ;
  }

  return (result) ;
}
// End: LCD_SetCursorOn


LCD_status LCD_GetCursorOn (LCD_handle   const pt_instance,
                            BOOL       * const b_on)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_GetCursorOn                                            //
//                 - Retrieves the cursor on/off state                        //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL) ||
         (b_on        == NULL)    )
    {
      (void)xc_printf ("LCD_GetCursorOn: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_GetCursorOn: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Fill out the current state
    *b_on = pt_this->b_cursorOn ;
  }

  return (result) ;
}
// End: LCD_GetCursorOn


LCD_status LCD_SetCursorBlink (LCD_handle const pt_instance,
                               BOOL       const b_blink)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_SetCursorBlink                                         //
//                 - Turns on the cursor blink                                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("LCD_SetCursorBlink: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_SetCursorBlink: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Copy the new state
    pt_this->b_cursorBlink = b_blink ;
    // Update the flag register
    LCD_PutControl (pt_this) ;
  }

  return (result) ;
}
// End: LCD_SetCursorBlink


LCD_status LCD_GetCursorBlink (LCD_handle   const pt_instance,
                               BOOL       * const b_blink)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_GetCursorBlink                                         //
//                 - Retrieves the cursor blink state                         //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL) ||
         (b_blink     == NULL)    )
    {
      (void)xc_printf ("LCD_GetCursorBlink: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_GetCursorBlink: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Fill out the current state
    *b_blink = pt_this->b_cursorBlink ;
  }

  return (result) ;
}
// End: LCD_GetCursorBlink


LCD_status LCD_SetDdPosition (LCD_handle    const pt_instance,
                              unsigned char const u8_position)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_SetDdPosition                                          //
//                 - Sets the cursor to the specified position                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL         ) ||
         (u8_position > LCD_CMD_DDMASK)    )
    {
      (void)xc_printf ("LCD_SetDdPosition: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_SetDdPosition: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the module is still in CG Address mode
    if (pt_this->b_cgAddressMode != FALSE)
    {
      // Retrieve the current CG position before switching to DD Address mode
      pt_this->u8_lastCgAddress = LCD_get (pt_instance, space_Command) & LCD_CMD_CGMASK ;
    }

    // Set the new position
    LCD_put (pt_this, space_Command, LCD_CMD_SETDDADR | u8_position) ;

    // Update the address mode
    pt_this->b_cgAddressMode = FALSE ;
  }

  return (result) ;
}
// End: LCD_SetDdPosition


LCD_status LCD_GetDdPosition (LCD_handle      const pt_instance,
                              unsigned char * const u8_position)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_GetDdPosition                                          //
//                 - Retrieves the cursors current position                   //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL) ||
         (u8_position == NULL)    )
    {
      (void)xc_printf ("LCD_GetDdPosition: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_GetDdPosition: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the module is in DD Address mode
    if (pt_this->b_cgAddressMode == FALSE)
    {
      // Fetch the position from the status bytes
      *u8_position = LCD_get (pt_instance, space_Command) & LCD_CMD_DDMASK ;
    }
    else
    {
      *u8_position = pt_this->u8_lastDdAddress ;
    }
  }

  return (result) ;
}
// End: LCD_GetDdPosition


LCD_status  LCD_SetCgPosition   (LCD_handle      const pt_instance,
                                 unsigned char   const u8_position)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_SetCgPosition                                          //
//                 - Sets the cursor to the specified position                //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL          ) ||
         (u8_position >  LCD_CMD_CGMASK)    )
    {
      (void)xc_printf ("LCD_SetCgPosition: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_SetCgPosition: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the module is still in DD Address mode
    if (pt_this->b_cgAddressMode == FALSE)
    {
      // Retrieve the current DD position before switching to CG Address mode
      pt_this->u8_lastDdAddress = LCD_get (pt_instance, space_Command) & LCD_CMD_DDMASK ;
    }

    // Set the new position
    LCD_put (pt_this, space_Command, LCD_CMD_SETCGADR | u8_position) ;

    // Update the address mode
    pt_this->b_cgAddressMode = TRUE ;
  }

  return (result) ;
}
// End: LCD_SetCgPosition


LCD_status LCD_GetCgPosition (LCD_handle      const pt_instance,
                              unsigned char * const u8_position)
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_GetCgPosition                                          //
//                 - Retrieves the cursors current position                   //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
{
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL) ||
         (u8_position == NULL)    )
    {
      (void)xc_printf ("LCD_GetCgPosition: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_GetCgPosition: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the module is in CG Address mode
    if (pt_this->b_cgAddressMode != FALSE)
    {
      // Fetch the position from the status bytes
      *u8_position = LCD_get (pt_instance, space_Command) & LCD_CMD_CGMASK ;
    }
    else
    {
      *u8_position = pt_this->u8_lastCgAddress ;
    }
  }

  return (result) ;
}
// End: LCD_GetCgPosition


LCD_status LCD_SetPostionMode (LCD_handle   const pt_instance,
                               BOOL         const b_cgMode)
{
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_SetPostionMode                                         //
//                 - Retrieves the current memory mode                        //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if (pt_instance == NULL)
    {
      (void)xc_printf ("LCD_SetPostionMode: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_SetPostionMode: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the current mode is the requested mode
    if (b_cgMode != pt_this->b_cgAddressMode)
    {
      if (b_cgMode)
      {
        LCD_put (pt_this, space_Command, LCD_CMD_SETCGADR | pt_this->u8_lastCgAddress) ;
      }
      else
      {
        LCD_put (pt_this, space_Command, LCD_CMD_SETDDADR | pt_this->u8_lastDdAddress) ;
      }

      pt_this->b_cgAddressMode = b_cgMode ;
    }
  }

  return (result) ;
}
// End: LCD_SetPostionMode


LCD_status LCD_GetPostionMode (LCD_handle   const pt_instance,
                               BOOL       * const b_cgMode)
{
////////////////////////////////////////////////////////////////////////////////
// Function:       LCD_GetPostionMode                                         //
//                 - Retrieves the current memory mode                        //
// History :       24 Jul 2004 by R. Delien:                                  //
//                 - Initial revision.                                        //
////////////////////////////////////////////////////////////////////////////////
  LCD_status                  result  = LCD_OK ;
  LCD_instance_struct * const pt_this = pt_instance ;

  if (result == LCD_OK)
  {
    // Do parameter check
    if ( (pt_instance == NULL) ||
         (b_cgMode    == NULL)    )
    {
      (void)xc_printf ("LCD_GetPostionMode: Parameter error.\n") ;
      result = LCD_ERR_PARAM ;
    }
  }

  if (result == LCD_OK)
  {
    // Check if the pointer is valid
    if (LCD_PTR_INVALID(pt_this))
    {
      (void)xc_printf ("LCD_GetPostionMode: Invalid pointer.\n") ;
      result = LCD_ERR_POINTER ;
    }
  }

  if (result == LCD_OK)
  {
    // Fill out the current state
    *b_cgMode = pt_this->b_cgAddressMode ;
  }

  return (result) ;
}
// End: LCD_GetPostionMode


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////
static void LCD_PutControl (LCD_handle const pt_instance)
{
  LCD_instance_struct const * const pt_this = pt_instance ;

  LCD_put (pt_this, space_Command, LCD_CMD_CTRL                              |
                                   (pt_this->b_cursorBlink?LCD_CMD_CRTL_B:0) |
                                   (pt_this->b_cursorOn   ?LCD_CMD_CRTL_C:0) |
                                   (pt_this->b_displayOn  ?LCD_CMD_CRTL_D:0)   ) ;

  return ;
}
// End: LCD_PutControl


static void LCD_PutEntryMode (LCD_handle const pt_instance)
{
  LCD_instance_struct const * const pt_this = pt_instance ;

  LCD_put (pt_this, space_Command, LCD_CMD_ENTRY                                 |
                                   (pt_this->b_incrementMode?LCD_CMD_ENTRY_ID:0) |
                                   (pt_this->b_shiftMode    ?LCD_CMD_ENTRY_S:0 )   ) ;

  return ;
}
// End: LCD_PutEntryMode


static void LCD_put (LCD_handle     const pt_instance,
                     LCD_space_enum const t_memSpace,
                     unsigned char  const u8_data)
{
  LCD_instance_struct const * const pt_this       = pt_instance ;
  unsigned char                     u8_spaceMask  = 0 ;
  unsigned char                     u8_dataHigh   = (((u8_data & 0xF0) >> 4) << (pt_this->u8_1stDataBit)) ;
  unsigned char                     u8_dataLow    = ((u8_data & 0x0F) << (pt_this->u8_1stDataBit)) ;

  if (t_memSpace == space_Data)
  {
    u8_spaceMask = pt_this->u8_rsBitMask ;
  }

  // Wait until the controller is ready
  while ((LCD_get(pt_this,space_Command) & LCD_STAT_BFMASK) !=0) ;

  // Set RS accordingly and clear R/W
  PA_DR   = u8_spaceMask            ;
  // Set high nibble of data and rise E-line
  PA_DR   = u8_spaceMask            |
            u8_dataHigh             |
            (pt_this->u8_eBitMask)  ;
  // Lower E-line
  PA_DR   = u8_spaceMask            |
            u8_dataHigh             ;
  // Set low nibble of data and rise E-line
  PA_DR   = u8_spaceMask            |
            u8_dataLow              |
            (pt_this->u8_eBitMask)  ;
  // Lower E-line
  PA_DR   = u8_spaceMask            |
            u8_dataLow              ;
  // Lower all lines
  PA_DR   = 0x00                    ;

  return ;
}
// End: LCD_put

static unsigned char LCD_get (LCD_handle      const pt_instance,
                              LCD_space_enum  const t_memSpace)
{
  LCD_instance_struct const * const pt_this       = pt_instance ;
  unsigned char                     u8_spaceMask  = 0 ;
  unsigned char                     u8_byte ;

  if (t_memSpace != space_Command)
  {
    u8_spaceMask = pt_this->u8_rsBitMask ;
  }

  // Configure data-bits as inputs
  PA_DDR  |= 0x0F << (pt_this->u8_1stDataBit) ;

  // Set RS accordingly and set R/W
  PA_DR   = (pt_this->u8_rwBitMask) |
             u8_spaceMask           ;

  // Raise Enable signal
  PA_DR   = (pt_this->u8_rwBitMask) |
            u8_spaceMask            |
            (pt_this->u8_eBitMask)  ;

  // Read high nibble
  u8_byte = PA_DR & 0xF0            ;

  // Lower Enable signal
  PA_DR   = (pt_this->u8_rwBitMask) |
            u8_spaceMask            ;

  // Raise Enable signal
  PA_DR   = (pt_this->u8_rwBitMask) |
            u8_spaceMask            |
            (pt_this->u8_eBitMask)  ;

  // Read high nibble
  u8_byte|= (PA_DR & 0xF0) >> 4     ;

  // Lower Enable signal
  PA_DR   = (pt_this->u8_rwBitMask) |
            u8_spaceMask            ;

  // Lower all lines
  PA_DR   = 0x00                    ;

  //  Reconfigure data-bits as outputs
  PA_DDR  &= ~(0x0F << (pt_this->u8_1stDataBit)) ;

  return (u8_byte) ;
}
// End: LCD_put
