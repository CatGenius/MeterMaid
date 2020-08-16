////////////////////////////////////////////////////////////////////////////////
// File    : CNV_Conversions.c
// Function: Conversion module
// Author  : Robert Delien
//           Copyright (C) 2004
// History : 24 Jul 2004 by R. Delien:
//           - Initial revision.
////////////////////////////////////////////////////////////////////////////////

#define CNV_CONVERSIONS_C
#include <kernel.h>
#include "CNV_Conversions.h"

#define CNV_RADIX_MAX   36

////////////////////////////////////////////////////////////////////////////////
// Global Data                                                                //
////////////////////////////////////////////////////////////////////////////////

// Sliders to easilly convert a digit value to it's character and reverses
static const char au8_sliderLw[CNV_RADIX_MAX] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e', 'f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'} ;
static const char au8_sliderUp[CNV_RADIX_MAX] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E', 'F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'} ;


////////////////////////////////////////////////////////////////////////////////
// Local Prototypes                                                           //
////////////////////////////////////////////////////////////////////////////////

static unsigned short CNV_UInt16Power (unsigned short const u16_mantissa,
                                       unsigned short const u16_exponent) ;

static unsigned long  CNV_UInt32Power (unsigned long  const u32_mantissa,
                                       unsigned long  const u32_exponent) ;


////////////////////////////////////////////////////////////////////////////////
// Global Implementations                                                     //
////////////////////////////////////////////////////////////////////////////////

CNV_status CNV_UInt16ToString (char           * const as8_string,
                               unsigned short   const u16_value,
                               unsigned char    const u8_minLength,
                               char             const s8_padChar,
                               CNV_Radix_enum   const t_radix)
{
  CNV_status      result          = CNV_OK ;
  unsigned short  u16_tempValue ;
  char            au8_tempStr[32] ;
  BOOL            b_leadingZero ;
  unsigned char   u8_tempIndex    = 0 ;
  unsigned short  u16_power ;
  unsigned short  u16_digitNr ;
  char            s8_digitCntr ;
  unsigned char   u8_maxDigits ;

  u16_tempValue = u16_value ;

  switch (t_radix)
  {
    case e_radix_binairy:
      u8_maxDigits = 16 ;
      break ;
    case e_radix_octal:
      u8_maxDigits = 8 ;
      break ;
    case e_radix_decimal:
      u8_maxDigits = 5 ;
      break ;
    case e_radix_hexadecimal:
      u8_maxDigits = 4 ;
      break ;
    default:
      u8_maxDigits = 0 ;
      break ;
  }

  b_leadingZero = TRUE ;

  for (s8_digitCntr = u8_maxDigits; s8_digitCntr-1 >= 0; s8_digitCntr --)
  {
    u16_power      = CNV_UInt16Power ((unsigned short)t_radix, (unsigned short)s8_digitCntr-1) ;
    u16_digitNr    = u16_tempValue / u16_power ;
    u16_tempValue -= u16_digitNr * u16_power ;

    // Not the least significant digit
    if ( (u16_digitNr   != 0           ) ||
         (u8_tempIndex  != 0           ) ||
         (s8_digitCntr-1 < u8_minLength)    )
    {
      // Not a leading zero
      if (u16_digitNr == 0)
      {
        if ( (b_leadingZero     ) &&
             (s8_digitCntr-1 > 0)    )
        {
          au8_tempStr[u8_tempIndex] = s8_padChar ;
        }
        else
        {
          au8_tempStr[u8_tempIndex] = au8_sliderUp[u16_digitNr] ;
        }
      }
      else
      {
        b_leadingZero = FALSE ;
        au8_tempStr[u8_tempIndex] = au8_sliderUp[u16_digitNr] ;
      }
      u8_tempIndex ++ ;
    }
  }
  au8_tempStr[u8_tempIndex] = 0 ;

  strcpy (as8_string, au8_tempStr) ;

  return (result) ;
}


CNV_status CNV_UInt32ToString (char           * const as8_string,
                               unsigned long    const u32_value,
                               unsigned char    const u8_minLength,
                               char             const s8_padChar,
                               CNV_Radix_enum   const t_radix)
{
  CNV_status      result          = CNV_OK ;
  unsigned long   u32_tempValue ;
  char            au8_tempStr[32] ;
  BOOL            b_leadingZero ;
  unsigned char   u8_tempIndex    = 0 ;
  unsigned long   u32_power ;
  unsigned long   u32_digitNr ;
  char            s8_digitCntr ;
  unsigned char   u8_maxDigits ;

  u32_tempValue = u32_value ;

  switch (t_radix)
  {
    case e_radix_binairy:
      u8_maxDigits = 32 ;
      break ;
    case e_radix_octal:
      u8_maxDigits = 16 ;
      break ;
    case e_radix_decimal:
      u8_maxDigits = 10 ;
      break ;
    case e_radix_hexadecimal:
      u8_maxDigits = 8 ;
      break ;
    default:
      u8_maxDigits = 0 ;
      break ;
  }

  b_leadingZero = TRUE ;

  for (s8_digitCntr = u8_maxDigits; s8_digitCntr-1 >= 0; s8_digitCntr --)
  {
    u32_power      = CNV_UInt32Power ((unsigned long)t_radix, (unsigned long)s8_digitCntr-1) ;
    u32_digitNr    = u32_tempValue / u32_power ;
    u32_tempValue -= u32_digitNr * u32_power ;

    // Not the least significant digit
    if (  (u32_digitNr   != 0           ) ||
          (u8_tempIndex  != 0           ) ||
          (s8_digitCntr-1 < u8_minLength)    )
    {
      // Not a leading zero
      if (u32_digitNr == 0)
      {
        if ( (b_leadingZero     ) &&
             (s8_digitCntr-1 > 0)    )
        {
          au8_tempStr[u8_tempIndex] = s8_padChar ;
        }
        else
        {
          au8_tempStr[u8_tempIndex] = au8_sliderUp[u32_digitNr] ;
        }
      }
      else
      {
        b_leadingZero = FALSE ;
        au8_tempStr[u8_tempIndex] = au8_sliderUp[u32_digitNr] ;
      }
      u8_tempIndex ++ ;
    }
  }
  au8_tempStr[u8_tempIndex] = 0 ;

  strcpy (as8_string, au8_tempStr) ;

  return (result) ;
}


CNV_status CNV_SInt16ToString (char           * const as8_string,
                               signed short     const s16_value,
                               unsigned char    const u8_minLength,
                               char             const s8_padChar,
                               CNV_Radix_enum   const t_radix)
{
  CNV_status      result = 0 ;
  unsigned short  u16_absValue ;
  unsigned char   u8_newLength ;

  u16_absValue = (unsigned short)(-1 * s16_value) ;

  if (s16_value < 0)
  {
    if (u8_minLength > 0)
    {
      u8_newLength = u8_minLength - 1 ;
    }
    else
    {
      u8_newLength = 0 ;
    }

    as8_string[0] = '-' ;
    result = CNV_UInt16ToString (&as8_string[1], u16_absValue, u8_newLength, s8_padChar, t_radix) ;
  }
  else
  {
    result = CNV_UInt16ToString (&as8_string[0], u16_absValue, u8_minLength, s8_padChar, t_radix) ;
  }

  return (result) ;
}


CNV_status CNV_StringToUInt16 (unsigned short       * const pu16_value,
                               char           const * const as8_string,
                               CNV_Radix_enum         const t_radix)
{
  CNV_status      result          = CNV_OK ;
  unsigned char   u8_strLength ;
  unsigned short  u16_charWeigth ;
  unsigned short  u16_charValue ;
  unsigned short  u16_tempValue   = 0 ;

  u8_strLength = strlen (as8_string) ;

  for (u16_charWeigth = 0;
       ((u16_charWeigth < u8_strLength) && (result == CNV_OK));
       u16_charWeigth ++)
  {
    u16_charValue = 0 ;
    while ( (u16_charValue < CNV_RADIX_MAX) &&
            (au8_sliderUp[u16_charValue] != as8_string[(u8_strLength-1)-u16_charWeigth]) &&
            (au8_sliderLw[u16_charValue] != as8_string[(u8_strLength-1)-u16_charWeigth])    )
    {
      u16_charValue ++ ;
    }

    if ( (au8_sliderUp[u16_charValue] == as8_string[(u8_strLength-1)-u16_charWeigth]) ||
         (au8_sliderLw[u16_charValue] == as8_string[(u8_strLength-1)-u16_charWeigth])    )
    {
      if (u16_charValue < t_radix)
      {
        u16_tempValue += u16_charValue * CNV_UInt16Power ((unsigned short)t_radix, u16_charWeigth) ;
      }
      else
      {
        result = -2 ;
      }
    }
    else
    {
      result = -1 ;
    }
  }

  if (result == CNV_OK)
  {
    *pu16_value = u16_tempValue ;
  }

  return (result) ;
}


CNV_status CNV_StringToUInt32 (unsigned long        * const pu32_value,
                               char           const * const as8_string,
                               CNV_Radix_enum         const t_radix)
{
  CNV_status      result          = CNV_OK ;
  unsigned char   u8_strLength ;
  unsigned long   u32_charWeigth ;
  unsigned long   u32_charValue ;
  unsigned long   u32_tempValue   = 0 ;

  u8_strLength = strlen (as8_string) ;

  for (u32_charWeigth = 0;
       ((u32_charWeigth < u8_strLength) && (result == CNV_OK));
       u32_charWeigth ++)
  {
    u32_charValue = 0 ;
    while ( (u32_charValue < CNV_RADIX_MAX) &&
            (au8_sliderUp[u32_charValue] != as8_string[(u8_strLength-1)-u32_charWeigth]) &&
            (au8_sliderLw[u32_charValue] != as8_string[(u8_strLength-1)-u32_charWeigth])    )
    {
      u32_charValue ++ ;
    }

    if ( (au8_sliderUp[u32_charValue] == as8_string[(u8_strLength-1)-u32_charWeigth]) ||
         (au8_sliderLw[u32_charValue] == as8_string[(u8_strLength-1)-u32_charWeigth])    )
    {
      if (u32_charValue < t_radix)
      {
        u32_tempValue += u32_charValue * CNV_UInt32Power ((unsigned long)t_radix, u32_charWeigth) ;
      }
      else
      {
        result = -2 ;
      }
    }
    else
    {
      result = -1 ;
    }
  }

  if (result == CNV_OK)
  {
    *pu32_value = u32_tempValue ;
  }

  return (result) ;
}

CNV_status CNV_GetField (unsigned char          const u8_fieldNr,
                         char                   const s8_Separator,
                         char           const * const as8_line,
                         char                 * const as8_word)
{
  CNV_status      result        = CNV_OK ;
  unsigned short  u16_wordStart = 0 ;
  unsigned short  u16_wordEnd ;
  unsigned char   u8_wordsFound = 0 ;
  unsigned short  u16_wordIndex ;

  while ((u8_wordsFound < u8_fieldNr) && (result == CNV_OK))
  {
    // find the beginning of a new word
    while (as8_line[u16_wordStart] == s8_Separator)
    {
      u16_wordStart ++ ;
    }

    // check if we've found a new word or the end of the line
    if (as8_line[u16_wordStart] != 0)
    {
      u16_wordEnd = u16_wordStart ;

      // find the end of the word
      while ((as8_line[u16_wordEnd] != s8_Separator) && (as8_line[u16_wordEnd] != 0))
      {
        u16_wordEnd ++ ;
      }

      // Check if we've really found anything
      if (u16_wordEnd > u16_wordStart)
      {
        u8_wordsFound ++ ;
        if (u8_wordsFound == u8_fieldNr)
        {
          for (u16_wordIndex = 0; u16_wordIndex < (u16_wordEnd - u16_wordStart); u16_wordIndex ++)
          {
            as8_word[u16_wordIndex] = as8_line[u16_wordIndex + u16_wordStart] ;
          }

          as8_word[u16_wordEnd - u16_wordStart] = 0 ;
        }
        else
        {
          u16_wordStart = u16_wordEnd ;
        }
      }
      else
      {
        result = -2 ;
      }
    }
    else
    {
      result = -1 ;
    }
  }

  return (result) ;
}

CNV_status CNV_EqualWord (char const * const as8_word1,
                          char const * const as8_word2)
{
  CNV_status    result  = CNV_OK ;
  unsigned char b_done   = FALSE ;
  unsigned char b_equal  = TRUE ;
  char          s8_temp1   = 0 ;
  char          s8_temp2   = 0 ;
  unsigned int  index   = 0 ;

  do
  {
    // Buffer both characters
    s8_temp1 = as8_word1[index] ;
    s8_temp2 = as8_word2[index] ;

    // Cast both characters to upper case
    if ( (s8_temp1 >= 'a') &&
         (s8_temp1 <= 'z')    )
    {
      s8_temp1 -= ('a' - 'A') ;
    }
    if ( (s8_temp2 >= 'a') &&
         (s8_temp2 <= 'z')    )
    {
      s8_temp2 -= ('a' - 'A') ;
    }

    // Compare the characters
    if (s8_temp1 == s8_temp2)
    {
      if (as8_word1[index] == 0)
      {
        b_done = TRUE ;
      }
    }
    else
    {
      b_equal = FALSE ;
    }

    index ++ ;
  } while ( (b_equal != FALSE) &&
            (b_done  == FALSE)    ) ;

  result = b_equal ;

  return (result) ;
}


////////////////////////////////////////////////////////////////////////////////
// Local Implementations                                                      //
////////////////////////////////////////////////////////////////////////////////
static unsigned short CNV_UInt16Power (unsigned short const u16_mantissa,
                                       unsigned short const u16_exponent)
{
  unsigned short u16_power   = 1 ;
  unsigned short u16_counter = 0 ;

  for (u16_counter = 0; u16_counter < u16_exponent; u16_counter ++)
  {
    u16_power *= u16_mantissa ;
  }

  return (u16_power) ;
}


static unsigned long  CNV_UInt32Power (unsigned long  const u32_mantissa,
                                       unsigned long  const u32_exponent)
{
  unsigned long u32_power   = 1 ;
  unsigned long u32_counter = 0 ;

  for (u32_counter = 0; u32_counter < u32_exponent; u32_counter ++)
  {
    u32_power *= u32_mantissa ;
  }

  return (u32_power) ;
}
