/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2020 Shaun McCance  <shaunm@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __AXING_UTF8_H__
#define __AXING_UTF8_H__

#include <glib.h>

G_BEGIN_DECLS

/* (c >= 0x80 && c <= 0xBF) */
#define axing_utf8_80bf(c) (((guchar)(c) & 0x80) && !((guchar)(c) & 0x40))

#define axing_utf8_bytes_pubid(c)                                       \
  __axing_utf8_bytes_pubid__expect_uchar__((guchar)(c))
#define __axing_utf8_bytes_pubid__expect_uchar__(c)                     \
  (                                                                     \
   (c[0] == 0x20 || c[0] == 0x0D || c[0] == 0x0A ||                     \
    c[0] == '!' || c[0] == '#' || c[0] == '$' || c[0] == '%' ||         \
    c[0] == ':' || c[0] == ';' || c[0] == '=' || c[0] == '?' ||         \
    c[0] == '@' || c[0] == '_' ||                                       \
    (c[0] >= 'a' && c[0] <= 'z') || (c[0] >= 'A' && c[0] <= 'Z') ||     \
    (c[0] >= '0' && c[0] <= '9') || (c[0] >= '\'' && c[0] <= '/') )     \
   ? 1 : 0 )

#define axing_utf8_bytes_name(c)                                        \
  __axing_utf8_bytes_name__expect_uchar__((guchar)(c))
#define __axing_utf8_bytes_name__expect_uchar__(c)                      \
 (  (c[0] == '-' || c[0] == '.' || (c[0] >= '0' && c[0] <= '9'))        \
    ? 1 :                                                               \
    (c[0] == 0xC3 && c[1] == 0xB7)                                      \
    ? 2 :                                                               \
 /* 0300 (CC-80) -- 033F (CC-BF) */                                     \
    (c[0] == 0xCC && axing_utf8_80bf(c[1]))                             \
    ? 2 :                                                               \
 /* 0340 (CD-80) -- 036F (CD-AF) */                                     \
    (c[0] == 0xCD && (c[1] >= 0x80 && c[1] <= 0xAF))                    \
    ? 2 :                                                               \
 /* 203F (E2-80-BF) */                                                  \
    (c[0] == 0xE2 && c[1] == 0x80 && c[2] == 0xBF)                      \
    ? 3 :                                                               \
 /* 2040 (E2-81-80) */                                                  \
    (c[0] == 0xE2 && c[1] == 0x81 && c[2] == 0x80)                      \
    ? 3 :                                                               \
     __axing_utf8_bytes_name_start__expect_uchar__(c) )
    

#define axing_utf8_bytes_name_start(c)                                  \
  __axing_utf8_bytes_name_start__expect_uchar__((guchar)(c))
#define __axing_utf8_bytes_name_start__expect_uchar__(c)                \
 (  (c[0] == ':' || (c[0] >= 'A' && c[0] <= 'Z') ||                     \
     c[0] == '_' || (c[0] >= 'a' && c[0] <= 'z') )                      \
    ? 1 :                                                               \
 /* 00C0 (C3-80) -- 00D6 (C3-96) */                                     \
 /* 00D8 (C3-98) -- 00F6 (C3-B6) */                                     \
 /* 00F8 (C3-B8) -- 00FF (C3-BF) */                                     \
 /* FIXME: use 80bf && c[1] != 0x97 && c[1] != 0xB7 */                  \
    (c[0] == 0xC3 &&                                                    \
     ((c[1] >= 0x80 && c[1] <= 0x96) ||                                 \
      (c[1] >= 0x98 && c[1] <= 0xB6) ||                                 \
      (c[1] >= 0xB8 && c[1] <= 0xBF)) )                                 \
    ? 2 :                                                               \
 /* 0100 (C4-80) -- 02FF (CB-BF) */                                     \
    (c[0] >= 0xC4 && c[0] <= 0xCB && axing_utf8_80bf(c[1]))             \
    ? 2 :                                                               \
 /* 0370 (CD-80) -- 037D (CD-BD), 037F (CD-BF) */                       \
    (c[0] == 0xCD && (c[1] >= 0xB0 && c[1] <= 0xBF) && c[1] != 0xBE)    \
    ? 2 :                                                               \
 /* 0380 (CE-80) -- 07FF (DF-BF) */                                     \
    (c[0] >= 0xCE && c[0] <= 0xDF && axing_utf8_80bf(c[1]))             \
    ? 2 :                                                               \
 /* 0800 (E0-A0-80) -- 0BFF (E0-AF-BF) */                               \
    (c[0] == 0xE0 &&                                                    \
     (c[1] >= 0xA0 && c[1] <= 0xAF) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* 0C00 (E0-B0-80) - 0FFF (E0-BF-BF) */                                \
    (c[0] == 0xE0 &&                                                    \
     (c[1] >= 0xB0 && c[1] <= 0xBF) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* 1000 - (E1-80-80) -- 13FF (E1-8F-BF) */                             \
    (c[0] == 0xE1 &&                                                    \
     (c[1] >= 0x80 && c[1] <= 0x8F) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* 1400 (E1-90-80) -- 17FF (E1-9F-BF) */                               \
    (c[0] == 0xE1 &&                                                    \
     (c[1] >= 0x90 && c[1] <= 0x9F) && axing_utf8_80bf(c[2]))           \
 /* 1800 (E1-A0-80) -- 1BFF (E1-AF-BF) */                               \
    ? 3 :                                                               \
    (c[0] == 0xE1 &&                                                    \
     (c[1] >= 0xA0 && c[1] <= 0xAF) && axing_utf8_80bf(c[2]))           \
 /* 1C00 (E1-B0-80) -- 1FFF (E1-BF-BF) */                               \
    ? 3 :                                                               \
    (c[0] == 0xE1 &&                                                    \
     (c[1] >= 0xB0 && c[1] <= 0xBF) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* 200C (E2-80-8C) -- 200D (E2-80-8D) */                               \
    (c[0] == 0xE2 && c[1] == 0x80 && (c[2] == 0x8C || c[2] == 0x8D))    \
    ? 3 :                                                               \
 /* 2070 (E2-81-B0) -- 207F (E2-81-BF) */                               \
    (c[0] == 0xE2 && c[1] == 0x81 && (c[2] >= 0xB0 && c[2] <= 0xBF))    \
    ? 3 :                                                               \
 /* 2080 (E2-82-80) -- 217F (E2-85-BF) */                               \
    (c[0] == 0xE2 &&                                                    \
     (c[1] >= 0x82 && c[1] <= 0x85) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* 2180 (E2-86-80) -- 218F (E2-86-8F) */                               \
    (c[0] == 0xE2 && c[1] == 0x86 && (c[2] >= 0x80 && c[2] <= 0x8F))    \
    ? 3 :                                                               \
 /* 2C00 (E2-B0-80) -- 2FBF (E2-BE-BF) */                               \
    (c[0] == 0xE2 &&                                                    \
     (c[1] >= 0xB0 && c[1] <= 0xBE) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* 2FC0 (E2-BF-80) -- 2FEF (E2-BF-AF) */                               \
    (c[0] == 0xE2 && c[1] == 0xBF && (c[2] >= 0x80 && c[2] <= 0xAF))    \
    ? 3 :                                                               \
 /* NOT 3000 (E3-80-80) */                                              \
    (c[0] == 0xE3 && c[1] == 0x80 && c[2] == 0x80)                      \
    ? 0 :                                                               \
 /* 3001 (E3-80-80) -- CFFF (EC-BF-BF) */                               \
    ( (c[0] >= 0xE3 && c[0] <= 0xEC) &&                                 \
      axing_utf8_80bf(c[1]) && axing_utf8_80bf(c[2]) )                  \
    ? 3 :                                                               \
 /* D000 (ED-80-80) -- D7FF (ED-9F-BF) */                               \
    (c[0] == 0xED &&                                                    \
     (c[1] >= 0x80 && c[1] <= 0x9F) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* F900 (EF-A4-80) -- FDBF (EF-B6-BF) */                               \
    (c[0] == 0xEF &&                                                    \
     (c[1] >= 0xA4 && c[1] <= 0xB6) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* FDC0 (EF-B7-80) --  FDCF (EF-B7-8F) */                              \
    (c[0] == 0xEF && c[1] == 0xB7 && (c[2] >= 0x80 && c[2] <= 0x8F))    \
    ? 3 :                                                               \
 /* FDF0 (EF-B7-B0) -- FDFF (EF-B7-BF) */                               \
    (c[0] == 0xEF && c[1] == 0xB7 && (c[2] >= 0xB0 && c[2] <= 0xBF))    \
    ? 3 :                                                               \
 /* FE00 (EF-B8-80) -- FFBF (EF-BE-BF) */                               \
    (c[0] == 0xEF &&                                                    \
     (c[1] >= 0xB8 && c[1] <= 0xBE) && axing_utf8_80bf(c[2]))           \
    ? 3 :                                                               \
 /* FFC0 (EF-BF-80) -- FFFD (EF-BF-BD) */                               \
    (c[0] == 0xEF && c[1] == 0xBF && (c[2] >= 0x80 && c[2] <= 0xBD))    \
    ? 3 :                                                               \
 /* 10000 (F0-90-80-80) -- 3FFFF (F0-BF-BF-BF) */                       \
    ( c[0] == 0xF0 && (c[1] >= 0x90 && c[1] <= 0xBF) &&                 \
      axing_utf8_80bf(c[2]) && axing_utf8_80bf(c[3]) )                  \
    ? 4 :                                                               \
 /* 40000 (F1-80-80-80) -- 7FFFF (F1-BF-BF-BF) */                       \
    ( c[0] == 0xF1 && axing_utf8_80bf(c[1]) &&                          \
      axing_utf8_80bf(c[2]) && axing_utf8_80bf(c[3]) )                  \
    ? 4 :                                                               \
 /* 80000 (F2-80-80-80) -- BFFFF (F2-BF-BF-BF) */                       \
    ( c[0] == 0xF2 && axing_utf8_80bf(c[1]) &&                          \
      axing_utf8_80bf(c[2]) && axing_utf8_80bf(c[3]) )                  \
    ? 4 :                                                               \
 /* C0000 (F3-80-80-80) -- EFFFF (F3-AF-BF-BF) */                       \
    ( c[0] == 0xF3 && (c[1] >= 0x80 && c[1] <= 0xAF) &&                 \
      axing_utf8_80bf(c[2]) && axing_utf8_80bf(c[3]) )                  \
    ? 4 :                                                               \
  0 )



G_END_DECLS

#endif /* __AXING_UTF8_H__ */
