////////////////////////////////////////////////////////////////////////////////
// File    : LCD_Driver.h
// Function: Include file of 'LCD_Driver.c'.
// Author  : Robert Delien
//           Copyright (C) 2004
////////////////////////////////////////////////////////////////////////////////

#ifndef LCD_DRIVER_H                                  // Include file already compiled ?
#define LCD_DRIVER_H

#ifdef LCD_DRIVER_C                                   // Compiled in LCD_DRIVER.C ?
#define LCD_EXTERN
#else
#ifdef __cplusplus                                    // Compiled for C++ ?
#define LCD_EXTERN extern "C"
#else
#define LCD_EXTERN extern
#endif // __cplusplus
#endif // LCD_DRIVER_C

// LCD_status values
#define LCD_OK                  ( 0)                  // All Ok
#define LCD_ERR_PARAM           (-1)                  // Parameter error
#define LCD_ERR_MEMORY          (-2)                  // Memory allocation error
#define LCD_ERR_POINTER         (-3)                  // Invalid pointer supplied

// LCD types
typedef void*                   LCD_handle ;
typedef char                    LCD_status ;          // Status/Error return type


LCD_status  LCD_Create          (LCD_handle          * const ppt_instance,
                                 ISFR                  const pt_ctrlPort,
                                 unsigned char         const u8_rsBitNr,
                                 unsigned char         const u8_rwBitNr,
                                 unsigned char         const u8_eBitNr,
                                 ISFR                  const pt_dataPort,
                                 unsigned char         const u8_firstDataBitNr,
                                 BOOL                  const b_8BitMode,
                                 BOOL                  const b_2Lines,
                                 BOOL                  const b_5x10Font) ;

LCD_status  LCD_Delete          (LCD_handle            const pt_instance) ;

LCD_status  LCD_Write           (LCD_handle            const pt_instance,
                                 unsigned char const * const u8_data,
                                 size_t                const u8_length) ;

LCD_status  LCD_Read            (LCD_handle            const pt_instance,
                                 unsigned char       * const u8_data,
                                 size_t                const u8_length) ;

LCD_status  LCD_SetDisplayOn    (LCD_handle            const pt_instance,
                                 BOOL                  const b_on) ;

LCD_status  LCD_GetDisplayOn    (LCD_handle            const pt_instance,
                                 BOOL                * const b_on) ;

LCD_status  LCD_SetCursorOn     (LCD_handle            const pt_instance,
                                 BOOL                  const b_on) ;

LCD_status  LCD_GetCursorOn     (LCD_handle            const pt_instance,
                                 BOOL                * const b_on) ;

LCD_status  LCD_SetCursorBlink  (LCD_handle            const pt_instance,
                                 BOOL                  const b_blink) ;

LCD_status  LCD_GetCursorBlink  (LCD_handle            const pt_instance,
                                 BOOL                * const b_blink) ;

LCD_status  LCD_SetDdPosition   (LCD_handle            const pt_instance,
                                 unsigned char         const u8_position) ;

LCD_status  LCD_GetDdPosition   (LCD_handle            const pt_instance,
                                 unsigned char       * const u8_position) ;

LCD_status  LCD_SetCgPosition   (LCD_handle            const pt_instance,
                                 unsigned char         const u8_position) ;

LCD_status  LCD_GetCgPosition   (LCD_handle            const pt_instance,
                                 unsigned char       * const u8_position) ;

LCD_status  LCD_SetPostionMode  (LCD_handle            const pt_instance,
                                 BOOL                  const b_cgMode) ;

LCD_status  LCD_GetPostionMode  (LCD_handle            const pt_instance,
                                 BOOL                * const b_cgMode) ;

#endif // LCD_DRIVER_H
