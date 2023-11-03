/*******************************************************************************
 *
 * Copyright 2023 Portable Software Company
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#define INC_STDIO
#define INC_STDLIB
#define INC_STRING
#define INC_CTYPE
#define INC_LIMITS
#include "includes.h"
#include "base.h"
#define DBCMPB
#include "dbcmp.h"

#define c250 0xFA00
#define c234 0xEA00

#define UVTABLESTART (1024 * 32)
#define UVTABLEINC (1024 * 32)
#define UVTABLEMARGIN 1024

EXTERN UCHAR charmap[256];
static UCHAR sc;
static UINT su;

/* the control code hash codes are created according to this formula: */
/* (<1st char> + <2nd char> * 2 + <length>) modulo 32 */

static CTLCODE cc_triangle = { NULL, c250 + 155, CC_PRINT + CC_SPECIAL, "TRIANGLE"};	/* code 0 */
static CTLCODE cc_click = { &cc_triangle, c250 + 18, CC_KEYIN + CC_DISPLAY, "CLICK"};	/* code 0 */
static CTLCODE cc_scrleft = { &cc_click, c250 + 10, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "SCRLEFT"};  /* code 0 */
static CTLCODE cc_utk = { &cc_scrleft, c250 + 138, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "UTK"};	/* code 0 */
static CTLCODE cc_rv = { &cc_utk, 212, CC_KEYIN, "RV"};  /* code 0 */
static CTLCODE cc_clslin = { NULL, c250 + 8, CC_KEYIN + CC_DISPLAY, "CLSLIN"};	/* code 1 */
static CTLCODE cc_scrright = { &cc_clslin, c250 + 11, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "SCRRIGHT"};  /* code 1 */
static CTLCODE cc_blinkon = { &cc_scrright, 223, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "BLINKON"};	/* code 1 */
static CTLCODE cc_revon = { &cc_blinkon, 215, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "REVON"};	/* code 1 */
static CTLCODE cc_line = { NULL, c250 + 68, CC_PRINT + CC_SPECIAL, "LINE"}; /* code 2 */
static CTLCODE cc_clickon = { &cc_line, c250 + 21, CC_KEYIN + CC_DISPLAY, "CLICKON"};	/* code 2 */
static CTLCODE cc_blinkoff = { &cc_clickon, c250 + 31, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "BLINKOFF"};	/* code 2 */
static CTLCODE cc_ovsmode = { &cc_blinkoff, c250 + 146, CC_KEYIN + CC_DISPLAY, "OVSMODE"};  /* code 2 */
static CTLCODE cc_zs = { &cc_ovsmode, c250 + 56, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_WRITE, "ZS"};  /* code 2 */
static CTLCODE cc_revoff = { &cc_zs, c250 + 33, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "REVOFF"};	/* code 2 */
static CTLCODE cc_box = { NULL, c250 + 152, CC_PRINT + CC_SPECIAL, "BOX"};	/* code 3 */
static CTLCODE cc_clickoff = { &cc_box, c250 + 22, CC_KEYIN + CC_DISPLAY, "CLICKOFF"};	/* code 3 */
static CTLCODE cc_flush = { &cc_clickoff, 249, CC_PRINT, "FLUSH"};	/* code 3 */
static CTLCODE cc_resetsw = { &cc_flush, c250 + 14, CC_KEYIN + CC_DISPLAY, "RESETSW"};	/* code 3 */
static CTLCODE cc_b = { &cc_resetsw, 217, CC_KEYIN + CC_DISPLAY, "B"};  /* code 3 */
static CTLCODE cc_hln = { &cc_b, c250 + 128, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "HLN"};	/* code 3 */
static CTLCODE cc_dna = { &cc_hln, c250 + 149, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "DNA"}; /* code 3 */
static CTLCODE cc_vdblon = { NULL, c250 + 143, CC_KEYIN + CC_DISPLAY, "VDBLON"};  /* code 4 */
static CTLCODE cc_c = { &cc_vdblon, 236, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_SPECIAL, "C"};	/* code 4 */
static CTLCODE cc_jl = { &cc_c, 221, CC_KEYIN, "JL"};	/* code 4 */
static CTLCODE cc_setswlr = { &cc_jl, c250 + 13, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "SETSWLR"};	/* code 4 */
static CTLCODE cc_setswtb = { &cc_setswlr, c250 + 12, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "SETSWTB"};	/* code 4 */
static CTLCODE cc_rectangle = { NULL, c250 + 151, CC_PRINT + CC_SPECIAL, "RECTANGLE"}; /* code 5 */
static CTLCODE cc_vdbloff = { &cc_rectangle, c250 + 144, CC_KEYIN + CC_DISPLAY, "VDBLOFF"};  /* code 5 */
static CTLCODE cc_setswall = { &cc_vdbloff, c250 + 57, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "SETSWALL"};	/* code 5 */
static CTLCODE cc_kl = { &cc_setswall, c250 + 51, CC_KEYIN + CC_SPECIAL, "KL"}; /* code 5 */
static CTLCODE cc_eon = { NULL, 220, CC_KEYIN, "EON"};  /* code 6 */
static CTLCODE cc_color = { &cc_eon, c250 + 52, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_SPECIAL, "COLOR"};	/* code 6 */
static CTLCODE cc_boldon = { &cc_color, 237, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "BOLDON"};  /* code 6 */
static CTLCODE cc_ll = { &cc_boldon, 241, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_WRITE + CC_CHANGE, "LL"};	/* code 6 */
static CTLCODE cc_textangle = { NULL, c250 + 72, CC_PRINT + CC_SPECIAL, "TEXTANGLE" }; /* code 7 */
static CTLCODE cc_linewidth = { &cc_textangle, c250 + 69, CC_PRINT + CC_SPECIAL, "LINEWIDTH" }; /* code 7 */
static CTLCODE cc_in = { &cc_linewidth, 209, CC_KEYIN + CC_DISPLAY, "IN"};  /* code 7 */
static CTLCODE cc_llc = { &cc_in, c250 + 133, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "LLC"};	/* code 7 */
static CTLCODE cc_eoff = { &cc_llc, 219, CC_KEYIN, "EOFF"};  /* code 7 */
static CTLCODE cc_boldoff = { &cc_eoff, c250 + 29, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "BOLDOFF"};	/* code 7 */
static CTLCODE cc_f = { &cc_boldoff, 238, CC_PRINT + CC_SPECIAL, "F"};  /* code 7 */
static CTLCODE cc_rj = { NULL, c250 + 70, CC_PRINT, "RJ"};  /* code 8 */
static CTLCODE cc_zf = { &cc_rj, 210, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_WRITE, "ZF"};	/* code 8 */
static CTLCODE cc_font = { &cc_zf, c250 + 67, CC_PRINT + CC_SPECIAL, "FONT"}; /* code 8 */
static CTLCODE cc_coloroff = { NULL, c250 + 32, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "COLOROFF"};  /* code 9 */
static CTLCODE cc_yellow = { &cc_coloroff, 227, CC_KEYIN + CC_DISPLAY + CC_PRINT, "YELLOW"};	/* code 9 */
static CTLCODE cc_h = { &cc_yellow, 245, CC_KEYIN + CC_DISPLAY /* + CC_PRINT*/ + CC_SPECIAL, "H"};  /* code 9 */
static CTLCODE cc_hon = { &cc_h, 215, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "HON"};	/* code 9 */
static CTLCODE cc_crs = { NULL, c250 + 130, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "CRS"};  /* code 10 */
static CTLCODE cc_pl = { &cc_crs, 242, CC_PRINT + CC_WRITE + CC_DISPLAY + CC_KEYIN + CC_SPECIAL + CC_CHANGE, "PL"};	/* code 10 */
static CTLCODE cc_hoff = { &cc_pl, 216, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "HOFF"};	/* code 10 */
static CTLCODE cc_format = { &cc_hoff, c250 + 49, CC_DISPLAY + CC_PRINT + CC_WRITE + CC_SPECIAL, "FORMAT"}; /* code 10 */
static CTLCODE cc_inschr = { NULL, c250 + 3, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "INSCHR"};	/* code 11 */
static CTLCODE cc_inslin = { &cc_inschr, c250 + 5, CC_KEYIN + CC_DISPLAY, "INSLIN"};  /* code 11 */
static CTLCODE cc_ffbin = { NULL, c250 + 156, CC_PRINT + CC_SPECIAL, "FB"};	/* code 12 */
static CTLCODE cc_insmode = { &cc_ffbin, c250 + 145, CC_KEYIN + CC_DISPLAY, "INSMODE"};  /* code 12 */
static CTLCODE cc_backgrnd = { &cc_insmode, 249, CC_KEYIN + CC_DISPLAY, "BACKGRND"};  /* code 12 */
static CTLCODE cc_white = { &cc_backgrnd, 231, CC_KEYIN + CC_DISPLAY + CC_PRINT, "WHITE"};	/* code 12 */
static CTLCODE cc_ha = { &cc_white, 246, CC_KEYIN + CC_DISPLAY /* + CC_PRINT*/ + CC_SPECIAL, "HA"};	/* code 12 */
static CTLCODE cc_plus = { &cc_ha, 232, CC_KEYIN + CC_DISPLAY + CC_PRINT, "+"};	/* code 12 */
static CTLCODE cc_dblon = { NULL, c250 + 139, CC_KEYIN + CC_DISPLAY, "DBLON"};	/* code 13 */
static CTLCODE cc_l = { &cc_dblon, 235, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_SPECIAL, "L"};  /* code 13 */
static CTLCODE cc_sl = { &cc_l, c250 + 55, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_WRITE + CC_SPECIAL + CC_CHANGE, "SL"};	/* code 13 */
static CTLCODE cc_es = { &cc_sl, 203, CC_KEYIN + CC_DISPLAY, "ES"};  /* code 13 */
static CTLCODE cc_dcon = { NULL, c250 + 151, CC_KEYIN + CC_DISPLAY, "DCON"};  /* code 14 */
static CTLCODE cc_dbloff = { &cc_dcon, c250 + 140, CC_KEYIN + CC_DISPLAY, "DBLOFF"};  /* code 14 */
static CTLCODE cc_minus = { &cc_dbloff, 211, CC_KEYIN + CC_DISPLAY + CC_PRINT, "-"};	/* code 14 */
static CTLCODE cc_invisible = { &cc_minus, c250 + 54, CC_KEYIN + CC_DISPLAY, "INVISIBLE"}; /* code 14 */
static CTLCODE cc_dcoff = { NULL, c250 + 152, CC_KEYIN + CC_DISPLAY, "DCOFF"};  /* code 15 */
static CTLCODE cc_eson = { &cc_dcoff, c250 + 15, CC_KEYIN, "ESON"};	/* code 15 */
static CTLCODE cc_mp = { &cc_eson, 243, CC_WRITE, "MP"};	/* code 15 */
static CTLCODE cc_dtk = { &cc_mp, c250 + 136, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "DTK"};	/* code 15 */
static CTLCODE cc_ed = { &cc_dtk, c250 + 41, CC_KEYIN + CC_DISPLAY, "ED"};  /* code 15 */
static CTLCODE cc_n = { &cc_ed, 233, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_SPECIAL, "N"};  /* code 15 */
static CTLCODE cc_esoff = { NULL, c250 + 16, CC_KEYIN, "ESOFF"};  /* code 16 */
static CTLCODE cc_jr = { &cc_esoff, 222, CC_KEYIN, "JR"};  /* code 16 */
static CTLCODE cc_de = { &cc_jr, 213, CC_KEYIN, "DE"};  /* code 16 */
static CTLCODE cc_green = { &cc_de, 226, CC_KEYIN + CC_DISPLAY + CC_PRINT, "GREEN"};	/* code 16 */
static CTLCODE cc_ulc = { &cc_green, c250 + 131, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "ULC"};  /* code 16 */
static CTLCODE cc_eschar = { NULL, c250 + 17, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "ESCHAR"};	/* code 17 */
static CTLCODE cc_eu = { &cc_eschar, c250 + 40, CC_KEYIN + CC_DISPLAY, "EU"};  /* code 17 */
static CTLCODE cc_ulon = { &cc_eu, 214, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "ULON"};	/* code 17 */
static CTLCODE cc_vln = { &cc_ulon, c250 + 129, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "VLN"};	/* code 17 */
static CTLCODE cc_edit = { &cc_vln, 197, CC_KEYIN + CC_DISPLAY, "EDIT"};  /* code 17 */
static CTLCODE cc_pon = { &cc_edit, c250 + 25, CC_KEYIN + CC_DISPLAY, "PON"}; /* code 17 */
static CTLCODE cc_uloff = { NULL, c250 + 30, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "ULOFF"};	/* code 18 */
static CTLCODE cc_hd = { &cc_uloff, c250 + 1, CC_KEYIN + CC_DISPLAY, "HD"};  /* code 18 */
static CTLCODE cc_dv = { &cc_hd, 196, CC_KEYIN, "DV"};  /* code 18 */
static CTLCODE cc_poff = { &cc_dv, c250 + 26, CC_KEYIN + CC_DISPLAY, "POFF"}; /* code 18 */
static CTLCODE cc_it = { NULL, 208, CC_KEYIN + CC_DISPLAY, "IT"};  /* code 19 */
static CTLCODE cc_editon = { &cc_it, c250 + 37, CC_KEYIN + CC_DISPLAY, "EDITON"};	/* code 19 */
static CTLCODE cc_curson = { &cc_editon, c250 + 19, CC_KEYIN + CC_DISPLAY, "CURSON"};  /* code 19 */
static CTLCODE cc_cursor = { &cc_curson, c250 + 20, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "CURSOR"};	/* code 19 */
static CTLCODE cc_lrc = { &cc_cursor, c250 + 134, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "LRC"};	/* code 19 */
static CTLCODE cc_r = { &cc_lrc, 206, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "R"};  /* code 19 */
static CTLCODE cc_ef = { &cc_r, 204, CC_KEYIN + CC_DISPLAY, "EF"};	/* code 19 */
static CTLCODE cc_lc = { NULL, c250 + 28, CC_KEYIN + CC_DISPLAY, "LC"};	/* code 20 */
static CTLCODE cc_delchr = { &cc_lc, c250 + 4, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "DELCHR"};	/* code 20 */
static CTLCODE cc_dellin = { &cc_delchr, c250 + 6, CC_KEYIN + CC_DISPLAY, "DELLIN"};  /* code 20 */
static CTLCODE cc_cursoff = { &cc_dellin, c250 + 20, CC_KEYIN + CC_DISPLAY, "CURSOFF"};  /* code 20 */
static CTLCODE cc_editoff = { &cc_cursoff, c250 + 38, CC_KEYIN + CC_DISPLAY, "EDITOFF"};	/* code 20 */
static CTLCODE cc_hu = { &cc_editoff, c250 + 2, CC_KEYIN + CC_DISPLAY, "HU"};  /* code 20 */
static CTLCODE cc_opnlin = { NULL, c250 + 7, CC_KEYIN + CC_DISPLAY, "OPNLIN"};	/* code 21 */
static CTLCODE cc_kcon = { &cc_opnlin, 232, CC_KEYIN, "KCON"};  /* code 21 */
static CTLCODE cc_hdblon = { NULL, c250 + 141, CC_KEYIN + CC_DISPLAY, "HDBLON"};  /* code 22 */
static CTLCODE cc_magenta = { &cc_hdblon, 229, CC_KEYIN + CC_DISPLAY + CC_PRINT, "MAGENTA"};	/* code 22 */
static CTLCODE cc_toff = { &cc_magenta, c234 + 255, CC_KEYIN, "TOFF"};	/* code 22 */
static CTLCODE cc_kcoff = { &cc_toff, 211, CC_KEYIN, "KCOFF"};  /* code 22 */
static CTLCODE cc_raw = { NULL, c250 + 23, CC_KEYIN + CC_DISPLAY, "RAW"};	/* code 23 */
static CTLCODE cc_hdbloff = { &cc_raw, c250 + 142, CC_KEYIN + CC_DISPLAY, "HDBLOFF"};  /* code 23 */
static CTLCODE cc_ltk = { &cc_hdbloff, c250 + 137, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "LTK"};	/* code 23 */
static CTLCODE cc_v = { &cc_ltk, 247, CC_KEYIN + CC_DISPLAY /* + CC_PRINT*/ + CC_SPECIAL, "V"};	/* code 23 */
static CTLCODE cc_bgcolor = { &cc_v, c250 + 53, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_SPECIAL, "BGCOLOR"}; /* code 23 */
static CTLCODE cc_ltor = { NULL, c250 + 36, CC_KEYIN + CC_DISPLAY, "LTOR"};  /* code 24 */
static CTLCODE cc_upa = { &cc_ltor, c250 + 147, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "UPA"}; /* code 24 */
static CTLCODE cc_cj = { NULL, c250 + 71, CC_PRINT + CC_SPECIAL, "CJ" }; /* code 25 */
static CTLCODE cc_rawon = { &cc_cj, c250 + 23, CC_KEYIN + CC_DISPLAY, "RAWON"};  /* code 25 */
static CTLCODE cc_dim = { &cc_rawon, 239, CC_KEYIN + CC_DISPLAY, "DIM"};  /* code 25 */
static CTLCODE cc_cyan = { &cc_dim, 230, CC_KEYIN + CC_DISPLAY + CC_PRINT, "CYAN"};	/* code 25 */
static CTLCODE cc_rptdown = { &cc_cyan, c250 + 34, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "RPTDOWN"};	/* code 25 */
static CTLCODE cc_rptchar = { &cc_rptdown, c250 + 9, CC_KEYIN + CC_DISPLAY + CC_PRINT + CC_SPECIAL, "RPTCHAR"};	/* code 25 */
static CTLCODE cc_tab = { &cc_rptchar, 252, CC_READ + CC_WRITE + CC_PRINT + CC_SPECIAL, "TAB"};	/* code 25 */
static CTLCODE cc_bigdot = { NULL, c250 + 154, CC_PRINT + CC_SPECIAL, "BIGDOT"}; /* code 26 */
static CTLCODE cc_rawoff = { &cc_bigdot, c250 + 24, CC_KEYIN + CC_DISPLAY, "RAWOFF"};  /* code 26 */
static CTLCODE cc_va = { &cc_rawoff, 248, CC_KEYIN + CC_DISPLAY /* + CC_PRINT*/ + CC_SPECIAL, "VA"};	/* code 26 */
static CTLCODE cc_dion = { &cc_va, 237, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "DION"};	/* code 26 */
static CTLCODE cc_circle = { NULL, c250 + 153, CC_PRINT + CC_SPECIAL, "CIRCLE"}; /* code 27 */
static CTLCODE cc_dioff = { &cc_circle, c250 + 29, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "DIOFF"};	/* code 27 */
static CTLCODE cc_lfa = { &cc_dioff, c250 + 150, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "LFA"}; /* code 27 */
static CTLCODE cc_fgcolor = { &cc_lfa, c250 + 52, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_SPECIAL, "FGCOLOR"};	/* code 27 */
static CTLCODE cc_urc = { NULL, c250 + 132, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "URC"};  /* code 28 */
static CTLCODE cc_rd = { &cc_urc, 207, CC_KEYIN + CC_DISPLAY + CC_SPECIAL, "RD"};	/* code 28 */
static CTLCODE cc_rtk = { NULL, c250 + 135, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "RTK"};  /* code 29 */
static CTLCODE cc_uc = { &cc_rtk, c250 + 27, CC_KEYIN + CC_DISPLAY, "UC"};  /* code 29 */
static CTLCODE cc_cl = { &cc_uc, 240, CC_KEYIN + CC_DISPLAY, "CL"};	/* code 29 */
static CTLCODE cc_rta = { &cc_cl, c250 + 148, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_GRAPHIC, "RTA"}; /* code 29 */
static CTLCODE cc_rtol = { NULL, c250 + 35, CC_KEYIN + CC_DISPLAY, "RTOL"};  /* code 30 */
static CTLCODE cc_blue = { &cc_rtol, 228, CC_KEYIN + CC_DISPLAY + CC_PRINT, "BLUE"};	/* code 30 */
static CTLCODE cc_over = { NULL, 244, CC_KEYIN + CC_DISPLAY, "OVER"};  /* code 31 */
static CTLCODE cc_red = { &cc_over, 225, CC_KEYIN + CC_DISPLAY + CC_PRINT, "RED"};	/* code 31 */
static CTLCODE cc_black = { &cc_red, 224, CC_KEYIN + CC_DISPLAY + CC_PRINT, "BLACK"};  /* code 31 */
static CTLCODE cc_v2lon = { &cc_black, 237, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "V2LON"};	/* code 31 */
static CTLCODE cc_alloff = { &cc_v2lon, 216, CC_KEYIN + CC_DISPLAY + CC_SCROLL, "ALLOFF"};  /* code 31 */
static CTLCODE cc_el = { &cc_alloff, 205, CC_KEYIN + CC_DISPLAY, "EL"};	/* code 31 */
static CTLCODE cc_scrend = { &cc_el, c250 + 42, CC_KEYIN + CC_DISPLAY + CC_SCROLL + CC_SPECIAL, "SCREND"}; /* code 31 */

CTLCODE *ctlcodeptr[32] = {
		&cc_rv,        /* code 0 */
		&cc_revon,     /* code 1 */
		&cc_revoff,    /* code 2 */
		&cc_dna,		/* code 3 */
		&cc_setswtb,   /* code 4 */
		&cc_kl,		/* code 5 */
		&cc_ll,        /* code 6 */
		&cc_f,         /* code 7 */
		&cc_font,      /* code 8 */
		&cc_hon,       /* code 9 */
		&cc_format,	/* code 10 */
		&cc_inslin,    /* code 11 */
		&cc_plus,      /* code 12 */
		&cc_es,        /* code 13 */
		&cc_invisible, /* code 14 */
		&cc_n,         /* code 15 */
		&cc_ulc,       /* code 16 */
		&cc_pon,		/* code 17 */
		&cc_poff,		/* code 18 */
		&cc_ef,		/* code 19 */
		&cc_hu,        /* code 20 */
		&cc_kcon,		/* code 21 */
		&cc_kcoff,     /* code 22 */
		&cc_bgcolor,	/* code 23 */
		&cc_upa,		/* code 24 */
		&cc_tab,       /* code 25 */
		&cc_dion,      /* code 26 */
		&cc_fgcolor,	/* code 27 */
		&cc_rd,        /* code 28 */
		&cc_rta,		/* code 29 */
		&cc_blue,      /* code 30 */
		&cc_scrend	/* code 31 */
};


/* Record definitions are stored as follows: the name	*/
/* is hashed into a code that ranges from 0-31.  That	*/
/* hash code is an index into a 32 element array of	*/
/* recordaccess structures.  The recordaccess struct	*/
/* within the 32 element array recindex points to the	*/
/* record definition for the rec name in this struct	*/
/* and a pointer to the next recordaccess struct rec	*/
/* (in the growing array recindex2) of the same hash	*/
/* code.										*/
/*											*/
/* The hash code is as follows:					*/
/* (<1st Char> + 2 * <2nd Char> + <length>) & 0x1F	*/
/*											*/
/* The format of the record definition in the recdef	*/
/* array is a follows:							*/
/* Pos. 0 (1 byte) - field name length n			*/
/* Pos. 1 (n bytes) - field name					*/
/* Pos. 1+n (m bytes) - data area code				*/
/* Pos. 1+n+m (1 byte) - end of field def, 0xF5		*/
/* Pos. 2+n+m (2 bytes) - amount to increment dataadr	*/
/* Pos. 4+n+m (1 byte) - field type				*/
/*	Note:									*/
/*	If a 0xFE is encountered in Pos. 5+n+m, continue	*/
/*	with an array INITIAL list until 0xFF is		*/
/*	encountered (this is not the 0xFF that terminates */
/*	field list - see next note).					*/
/*	Note:									*/
/*	Repeat pos 0 to 4+n+m for each field until		*/
/*	0xFF is encountered.							*/
static INT recindexsize = 0;				/* size (in # of structs) of recindexptr in use */
static INT recindexalloc = 0;				/* size (in # of structs) of recindexptr allocated */
static UCHAR **recdef;						/* record definitions, pointed to by recindex */
static UINT recdefsize = 0;					/* size (in bytes) of recdef in use */
static UINT recdefalloc = 0;					/* size (in bytes) of recdef allocated */


/* The user verb table consists of user verb prototype */
/* information blocks which are contiguous and are in	*/
/* the order encountered in the source program the	*/
/* format of each block is as follows:				*/
/* 0-3 (4 bytes): INT offset of next block with	*/
/*			   same first character (end is 0) */
/* 4   (1 byte): UCHAR, 0 for user verb, 1 for cverb,	*/
/*				    2 for make verb, 3 for method, */
/*				    4 for destroy verb,			*/
/*				    5 for automatic class 		*/
/*					instantiation (TRANSIENT)	*/
/* 5   (1 byte): UCHAR, length of user verb name n	*/
/* 6   (n bytes): UCHAR[n] user verb name (no trailing */
/*			   zero) (n is 1-31)				*/
/* n+6 (1 byte): UCHAR, length of class name m, 0 for	*/
/*			  no class name specified (used in		*/
/*			  automatic class instantiation & make	*/
/*			  verbs)							*/
/* n+7 (m bytes): UCHAR[m] class name (no trailing	*/
/*			   zero) (m is 0-31)				*/
/* m+n+7 (1 byte): UCHAR RULE_xxxx type checking of 	*/
/*			  non-positional parameter			*/
/* m+n+8 (1 byte): UCHAR number of positional params	*/
/* m+n+9 (i bytes): UCHAR[i] RULE_xxxx type checking of*/
/*			   i positional parameters (i is 0-255)	*/
/*											*/
/* the following repeats as many times as there are	*/
/* keyword parameters (beginning at position m+n+i+10)	*/
/* 0   (1 byte): UCHAR length of keyword j (zero		*/
/*			  indicates end of keyword parameters)	*/
/* 1   (1 byte): UCHAR # of operands k of keyword(0-5)	*/
/* 2-5 (4 bytes): INT data area address of keyword	*/
/*			   literal (-1 if not yet used)		*/
/* 6   (j bytes): UCHAR[j] keyword(no trailing 0,j<32) */
/* j+6 (k bytes): UCHAR[k] RULE_xxxx type checking of	*/
/*			   operands (n is 0-5)				*/
static UCHAR **uvtablepptr = NULL;	/* pointer to pointer to user verb prototype table */
static INT uvindex[27];  /* offsets to first linked list entry in uvtable, 0 is end */

#define VERBTYPE_VERB		0
#define VERBTYPE_CVERB		1
#define VERBTYPE_MAKE		2
#define VERBTYPE_METHOD		3
#define VERBTYPE_DESTROY		4
#define VERBTYPE_TRANSIENT	5

#define NUMVRBKYWRDS 4
#define VRBKYWRD_MAKE		0
#define VRBKYWRD_METHOD		1
#define VRBKYWRD_DESTROY		2
#define VRBKYWRD_TRANSIENT	3
static CHAR *vrbtypekywrd[] = { "MAKE", "METHOD", "DESTROY", "TRANSIENT" };

/**
 * Process other ctl codes:  number, equate, all *P, *Wccc, *Tccc
 * return value is:  control code parse continuation code (CCPARSE_xxx), or
 * -1 if error occurred, or 0 if nothing else to do (continue with NEXT_LIST)
 */
INT cc_other(UINT rule)
{
	UCHAR firstchar, r, linecntsave1, linecntsave2, equalflag;

	r = (UCHAR) rule;
	firstchar = (UCHAR) toupper(line[linecnt]);
	if (!isdigit(firstchar)) {
		linecntsave1 = (UCHAR)linecnt;
		if (!line[++linecnt] && firstchar == 'P') {
			error(ERROR_SYNTAX);
			return(-1);
		}
		if (line[linecnt] == '=' || (firstchar == 'P' && line[linecnt] == ':')) {
			if (!line[++linecnt]) {
				error(ERROR_SYNTAX);
				return(-1);
			}
			equalflag = TRUE;
			whitespace();
		}
		else equalflag = FALSE;
		switch (firstchar) {
			case 'P':     /* *Pn:n */
				if (r != RULE_KYCC && r != RULE_DSCC && r != RULE_PRCC) break;
				whitespace();
				linecntsave2 = (UCHAR)linecnt;
				scantoken(upcaseflg);
				if (tokentype == TOKEN_NUMBER) symtype = TYPE_INTEGER;
				switch (tokentype) {
					case TOKEN_LPAREND:
						savcodebufcnt = codebufcnt;
						putcode1(0xC7);
						linecnt = linecntsave2; /* put token back */
						return(CCPARSE_PNVAR);
					case TOKEN_WORD:
						if (getdsym((CHAR *) token) == -1) {
							if (r == RULE_DSCC || r == RULE_KYCC) {
								error(ERROR_VARNOTFOUND);
								return(-1);
							}
							else break;
						}
						if (symtype != TYPE_EQUATE) {
							if (symtype >= TYPE_NVAR && symtype <= TYPE_NVARPTRARRAY3) {
								savcodebufcnt = codebufcnt;
								putcode1(0xC7);
								linecnt = linecntsave2; /* put token back */
								return(CCPARSE_PNVAR);
							}
							break;
						}
						// fall through
						/* no break */
					case TOKEN_NUMBER:
						if (symtype != TYPE_EQUATE && checkdcon(token)) {
							error(ERROR_SYNTAX);
							return(-1);
						}
						savcodebufcnt = codebufcnt;
						if (symvalue < 256) {
							putcodehhll((USHORT) (0xC600 + (UCHAR) symvalue));
						}
						else {
							putcodehhll(0xC7FD);
							putcodehhll((USHORT) symvalue);
						}
						if (line[linecnt++] != ':') {
							error(ERROR_SYNTAX);
							return(-1);
						}
						return(CCPARSE_PCON);
					default:
						break;
				}
				break; /* parse as a tab (equate or var) */
			case 'T': 	/* *Tn */
				if (r != RULE_KYCC) break;
				if (!line[linecnt] || line[linecnt] == ',' ||
				    line[linecnt] == ':' || line[linecnt] == ';' ||
				    charmap[line[linecnt]] == 2) {
					putcodehhll(0xFA30);
					putcodehhll(0xFF02);
					return(0);
				}
				linecntsave2 = (UCHAR)linecnt;
				scantoken(upcaseflg);
				if (tokentype == TOKEN_WORD) {
					if (getdsym((CHAR *) token) == -1) break;
				}
				else if (!equalflag && !strcmp((CHAR *) token, "255")) {
					putcodehhll(0xEAFF);
					return(0);
				}
				linecnt = linecntsave2; /* put token back */
				putcodehhll(0xFA30);
				return(CCPARSE_NVARCON);
			case 'W': 	/* *Wn */
				if (r != RULE_KYCC && r != RULE_DSCC) break;
				if (!line[linecnt] || line[linecnt] == ',' ||
				    line[linecnt] == ':' || line[linecnt] == ';' ||
				    charmap[line[linecnt]] == 2) {
					putcodehhll(0xFA2F);
					putcodehhll(0xFF01);
					return(0);
				}
				linecntsave2 = (UCHAR)linecnt;
				scantoken(upcaseflg);
				if (tokentype == TOKEN_WORD) if (getdsym((CHAR *) token) == -1) break;
				linecnt = linecntsave2; /* put token back */
				putcodehhll(0xFA2F);
				return(CCPARSE_NVARCON);
		}
		linecnt = linecntsave1; /* put tokens back */
		/* it's not a control code, see if its a tab (an equate or nvar) */
		if (r != RULE_RDCC && r != RULE_PRCC && r != RULE_WTCC) {
			error(ERROR_SYNTAX);
			return(-1);
		}
		scantoken(upcaseflg);
		if (getdsym((CHAR *) token) == -1) {
			error(ERROR_VARNOTFOUND);
			return(-1);
		}
		if (symtype != TYPE_EQUATE) {
			tabflag = TRUE;
			putcode1(0xFD);
			if (symtype == TYPE_NVAR) {
				putcodeadr(symvalue);
				return(0);
			}
			linecnt = linecntsave1;
			return(CCPARSE_NVARCON);  /* let caller handle numeric array variables */
		}
	}
	else {
		scantoken(SCAN_UPCASE);
		if (tokentype == TOKEN_NUMBER) symtype = TYPE_INTEGER;
	}
	if (r != RULE_RDCC && r != RULE_PRCC && r != RULE_WTCC) {
		error(ERROR_SYNTAX);
		return(-1);
	}
	if (symtype != TYPE_EQUATE && checkdcon(token)) {
		error(ERROR_SYNTAX);
		return(-1);
	}
	if (!symvalue || symvalue > 64 * 1024 || (r == RULE_PRCC && symvalue > 400)) {
		error(ERROR_BADTABVALUE);
		return(-1);
	}
	tabflag = TRUE;
	if (symvalue < 256) putcodehhll((USHORT) (0xFC00 + (UCHAR) symvalue));
	else {
		putcodehhll(0xFA7F);
		putcodehhll((USHORT) symvalue);
	}
	return(0);
}

/**
 * special syntax control code
 * return value is:  control code parse continuation code (CCPARSE_xxx), or
 * -1 if error occurred, or 0 if nothing else to do (continue with NEXT_LIST)
 */
INT cc_special(UINT code, UINT rule)
{
	UCHAR r, linecntsave;
	UINT m;

	r = (UCHAR) rule;
	switch (code) {
		case 206:  /* *R */
			m = c250 + 65;
			goto cc_special_1;
		case 207:	 /* *RD */
			m = c250 + 66;
			goto cc_special_1;
		case 233:  /* *N */
			m = c250 + 46;
			goto cc_special_1;
		case 235:  /* *L */
			m = c250 + 64;
			goto cc_special_1;
		case 236:  /* *C */
			m = c250 + 63;
			goto cc_special_1;
		case 238:  /* *F */
			m = c250 + 62;
			goto cc_special_1;
		case 242:  /* PL */
			if (r == RULE_DSCC || r == RULE_PRCC) codebuf[codebufcnt - 1] = 211;
			return(0);
		case 245:  /* H */
		case 247:  /* V */
			if (line[linecnt] == '=' || line[linecnt] == ':') linecnt++;
			whitespace();
			return(CCPARSE_NVARCON);
		case 246:  /* HA */
		case 248:  /* VA */
			if (line[linecnt] == '=' || line[linecnt] == ':') linecnt++;
			whitespace();
			return(CCPARSE_NVARNCON);
		case 252:	/* TAB */
			if (line[linecnt] == '=') linecnt++;
			tabflag = TRUE;
			whitespace();
			linecntsave = (UCHAR)linecnt;
			scantoken(upcaseflg);
			if (tokentype == TOKEN_WORD) {
				if (getdsym((CHAR *) token) != -1) {
					if (baseclass(symtype) == TYPE_NVAR) {
						codebufcnt--;
						putcode1(253);
						linecnt = linecntsave; /* put token back */
						return(CCPARSE_NVARCON);
					}
					if (symtype != TYPE_EQUATE) goto badsyntax;
				}
				else goto badsyntax;
			}
			else if (tokentype == TOKEN_LPAREND) {
				codebufcnt--;
				putcode1(253);
				linecnt = linecntsave; /* put token back */
				return(CCPARSE_NVARCON);
			}
			else if (tokentype != TOKEN_NUMBER || checkdcon(token)) goto badsyntax;
			if (!symvalue || symvalue > 64 * 1024 || (r == RULE_PRCC && symvalue > 400)) {
				error(ERROR_BADTABVALUE);
				return(-1);
			}
			tabflag = TRUE;
			if (symvalue < 256) putcode1((UCHAR) symvalue);
			else {
				codebufcnt--;
				putcodehhll(0xFA7F);
				putcodehhll((USHORT) symvalue);
			}
			return(0);
	}
	code -= c250;
	switch (code) {
		case 20:	/* *CURSOR */
			if (line[linecnt++] != '=') goto badsyntax;
			whitespace();
			if (definecnt) scantoken((UINT) upcaseflg + SCAN_NOADVANCE);	/* expand defines */
			if (line[linecnt++] != '*') goto badsyntax;
			scantoken(SCAN_UPCASE + SCAN_NOEXPDEFINE);
			if (tokentype != TOKEN_WORD) goto badsyntax;
			if (!strcmp((CHAR *) token, "ON")) r = 58;
			else if (!strcmp((CHAR *) token, "OFF")) r = 20;
			else if (!strcmp((CHAR *) token, "NORM")) r = 19;
			else if (!strcmp((CHAR *) token, "BLOCK")) r = 59;
			else if (!strcmp((CHAR *) token, "HALF")) r = 60;
			else if (!strcmp((CHAR *) token, "ULINE")) r = 61;
			else goto badsyntax;
			codebuf[codebufcnt - 1] = r;
			return(0);
		case 42:	/* *SCREND */
			codebufcnt -= 2;
			if (!scrlparseflg) {
				error(ERROR_BADSCREND);
				return(-1);
			}
			return(CCPARSE_SCREND);
		case 47:  /* *W */
			if (line[linecnt] == '=') {
				linecnt++;
				whitespace();
				return(CCPARSE_NVARCON);
			}
			if (charmap[line[linecnt]] == 6) return(CCPARSE_NVARCON);
			putcodehhll(0xFF01);
			return(0);
		case 48:	/* *T */
			if (line[linecnt] == '=') {
				linecnt++;
				whitespace();
				return(CCPARSE_NVARCON);
			}
			if (charmap[line[linecnt]] == 6) return(CCPARSE_NVARCON);
			putcodehhll(0xFF02);
			return(0);
		case 49:	/* *FORMAT */
		case 156:	/* *FB */
			if (line[linecnt] != '=') goto badsyntax;
			linecnt++;
			whitespace();
			return(CCPARSE_CVARLIT);
		case 51:  /* *KL */
			if (line[linecnt] != '=') goto badsyntax;
			linecnt++;
			whitespace();
			return(CCPARSE_CVAR_NVARCON);
		case 52:	/* *COLOR, FGCOLOR */
		case 53:	/* *BGCOLOR */
			if (line[linecnt] != '=') goto badsyntax;
			linecnt++;
			whitespace();
			if (definecnt) scantoken((UINT) upcaseflg + SCAN_NOADVANCE);	/* expand defines */
			if (line[linecnt] == '*') {
				linecnt++;
				scantoken(SCAN_UPCASE + SCAN_NOEXPDEFINE);
				if (tokentype != TOKEN_WORD) goto badsyntax;
				if (!strcmp((CHAR *) token, "BLACK")) r = 0;
				else if (!strcmp((CHAR *) token, "RED")) r = 1;
				else if (!strcmp((CHAR *) token, "GREEN")) r = 2;
				else if (!strcmp((CHAR *) token, "YELLOW")) r = 3;
				else if (!strcmp((CHAR *) token, "BLUE")) r = 4;
				else if (!strcmp((CHAR *) token, "MAGENTA")) r = 5;
				else if (!strcmp((CHAR *) token, "CYAN")) r = 6;
				else if (!strcmp((CHAR *) token, "WHITE")) r = 7;
				else goto badsyntax;
				if (codebuf[codebufcnt-1] == 52) {  /* *COLOR= */
					codebufcnt--;
					codebuf[codebufcnt-1] = 224 + r;
				}
				else {
					codebuf[codebufcnt-2] = 249;
					codebuf[codebufcnt-1] = 224 + r;
				}
				return(0);
			}
			return(CCPARSE_NVARCON);
		case 55:  /* *SL */
			if (r == RULE_DSCC || r == RULE_PRCC) {
				codebufcnt--;
				codebuf[codebufcnt - 1] = 232;
			}
			return(0);
		case 57: /* SETSWALL */
			if (line[linecnt] == '=' || line[linecnt] == ':') linecnt++;
			whitespace();
			return(CCPARSE_4NVARCON);
		case 72: /* TEXTANGLE */
			if (line[linecnt++] != '=') goto badsyntax;
			whitespace();
			return(CCPARSE_NVARNCON);
		case 69: /* LINEWIDTH */
			/* fall through */
		case 71: /* CJ */
			/* fall through */
		case 153: /* CIRCLE */
			/* fall through */
		case 154: /* BIGDOT */
			if (line[linecnt++] != '=') goto badsyntax;
			whitespace();
			return(CCPARSE_NVARCON);
		case 155: /* TRIANGLE */
			if (line[linecnt++] != '=') goto badsyntax;
			whitespace();
			return(CCPARSE_4NVARCON);
	}
	switch (code) {
		case 3:  /* INSCHR */
		case 4:  /* DELCHR */
		case 12: /* SETSWTB */
		case 13: /* SETSWLR */
		case 68: /* LINE */
			if (line[linecnt] == '=' || line[linecnt] == ':') linecnt++;
			whitespace();
			return(CCPARSE_2NVARCON);
		case 10: /* SCRLEFT */
		case 11: /* SCRRIGHT */
			if (line[linecnt] == '=' || line[linecnt] == ':') linecnt++;
			whitespace();
			return(CCPARSE_SCRLR);
		case 151: /* RECTANGLE */
		case 152: /* BOX */
			if (line[linecnt++] != '=') goto badsyntax;
			whitespace();
			return(CCPARSE_2NVARCON);
	}
	if (code == 9 || code == 34) {	/* RPTCHAR, RPTDOWN */
		if (line[linecnt] == '=' || line[linecnt] == ':') linecnt++;
		whitespace();
		if (code != 9 || rule != RULE_PRCC) graphicparseflg = TRUE;  /* allow graphic chars like *utk, *hln, etc. */
		return(CCPARSE_CVARLIT_NVARCON);
	}
	if (code == 67 || code == 17) { /* FONT=cvarlit, ESCHAR=cvarlit */
		if (line[linecnt] == '=') linecnt++;
		whitespace();
		return(CCPARSE_CVARLIT);
	}
	goto badsyntax;

cc_special_1:  /* if next char is =, then m is ctl code with NVARCON */
	if (line[linecnt] == '=') {
		linecnt++;
		whitespace();
		codebufcnt--;
		putcodehhll(m);
		return(CCPARSE_NVARCON);
	}
	return(0);

badsyntax:
	error(ERROR_SYNTAX);
	return(-1);
}

UINT s_uverb_types(CHAR *ptr)	/* return the RULE_xxxx for type at ptr */
{
	switch(*ptr) {
		case 'A' :
			if (!memcmp(ptr, "ARRAY", 5)) return(RULE_ARRAY);
			if (!memcmp(ptr, "AFILE", 5)) return(RULE_AFILE);
			if (!memcmp(ptr, "ANYVARLIT", 9)) return(RULE_ANYSRC);
			if (!memcmp(ptr, "ANY", 3)) return(RULE_ANYVAR);
			break;
		case 'C' :
			if (!memcmp(ptr, "CVARLIT", 7)) return(RULE_CSRC);
			if (!memcmp(ptr, "CVAR", 4)) return(RULE_CVAR);
			if (!memcmp(ptr, "CNVARLIT", 8)) return(RULE_CNSRC);
			if (!memcmp(ptr, "CNVAR", 5)) return(RULE_CNVAR);
			if (!memcmp(ptr, "CARRAY1", 7)) return(RULE_CARRAY1);
			if (!memcmp(ptr, "CARRAY2", 7)) return(RULE_CARRAY2);
			if (!memcmp(ptr, "CARRAY3", 7)) return(RULE_CARRAY3);
			if (!memcmp(ptr, "CARRAY", 6)) return(RULE_CARRAY);
			if (!memcmp(ptr, "COMFILE", 7)) return(RULE_COMFILE);
			break;
		case 'D' :
			if (!memcmp(ptr, "DEVICE", 6)) return(RULE_DEVICE);
			break;
		case 'F' :
			if (!memcmp(ptr, "FILE", 4)) return(RULE_FILE);
			break;
		case 'I' :
			if (!memcmp(ptr, "IFILE", 5)) return(RULE_IFILE);
			if (!memcmp(ptr, "IMAGE", 5)) return(RULE_IMAGE);
			break;
		case 'L' :
			if (!memcmp(ptr, "LIST", 4)) return(RULE_LIST);
			if (!memcmp(ptr, "LABEL", 5)) return(RULE_LABEL);
			break;
		case 'N' :
			if (!memcmp(ptr, "NVARLIT", 7)) return(RULE_NSRCEQU);
			if (!memcmp(ptr, "NVAR", 4)) return(RULE_NVAR);
			if (!memcmp(ptr, "NARRAY1", 7)) return(RULE_NARRAY1);
			if (!memcmp(ptr, "NARRAY2", 7)) return(RULE_NARRAY2);
			if (!memcmp(ptr, "NARRAY3", 7)) return(RULE_NARRAY3);
			if (!memcmp(ptr, "NARRAY", 6)) return(RULE_NARRAY);
			break;
		case 'O':
			if (!memcmp(ptr, "OBJECT", 6)) return(RULE_OBJECT);
			break;
		case 'P' :
			if (!memcmp(ptr, "PFILE", 5)) return(RULE_PFILE);
			break;
		case 'Q' :
			if (!memcmp(ptr, "QUEUE", 5)) return(RULE_QUEUE);
			break;
		case 'R' :
			if (!memcmp(ptr, "RESOURCE", 8)) return(RULE_RESOURCE);
			break;
		case 'V' :
			if (!memcmp(ptr, "VARLIT", 6)) return(RULE_CNSRC);
			if (!memcmp(ptr, "VAR", 3)) return(RULE_CNVAR);
			break;
    }
    return(0);
}

UCHAR baseclass(UINT cursymtype)	 /* return the base class type for a given symtype */
{
	UINT curbasetype;

	curbasetype = cursymtype & ~TYPE_PTR;
	switch (curbasetype) {
		case TYPE_CARRAY1:
		case TYPE_CARRAY2:
		case TYPE_CARRAY3:
			return(TYPE_CVAR);
		case TYPE_NARRAY1:
		case TYPE_NARRAY2:
		case TYPE_NARRAY3:
			return(TYPE_NVAR);
	}
	switch (cursymtype) {
		case TYPE_CVARPTRARRAY1:
		case TYPE_CVARPTRARRAY2:
		case TYPE_CVARPTRARRAY3:
			return(TYPE_CVAR);
		case TYPE_NVARPTRARRAY1:
		case TYPE_NVARPTRARRAY2:
		case TYPE_NVARPTRARRAY3:
			return(TYPE_NVAR);
		case TYPE_VVARPTRARRAY1:
		case TYPE_VVARPTRARRAY2:
		case	TYPE_VVARPTRARRAY3:
			return(TYPE_VVARPTR);
		default:
			return((UCHAR)curbasetype);
	}
}

void makelist(INT curdefpos)
{
	INT i, dataadrinc, namelen, saveadr;
	UINT arrayshape[4];
	CHAR fldname[LABELSIZ + LABELSIZ];
	UCHAR prefixlen, *def;

	if (withnamesflg) putdataname(label);
	saveadr = savdataadr;
	symtype = TYPE_LIST;
	symvalue = dataadr;
	putdsym(label, FALSE);
/*** CODE: SETTING OF RECORDLEVEL WILL CAUSE AN INCOMPLETE RECORD DEFINITION TO BE ***/
/***       COPIED TO THE RECORD DEFINITION BUFFER WHICH IS NOT USEFUL AND WASTES ***/
/***       MEMORY ***/
	recordlevel++;
	do {
		int minimumF5LoopRepetitions;
		int f5LoopCounter;
		def = *recdef;
		namelen = def[curdefpos++];
		if (namelen) {
			strcpy(fldname, label);
			strcat(fldname, ".");
			prefixlen = (UCHAR)strlen(fldname);
			for (i = 0; i < namelen; i++) fldname[prefixlen + i] = def[curdefpos++];
			fldname[prefixlen + i] = '\0';
			if (withnamesflg) putdataname(&fldname[prefixlen]);
		}

		/*
		 * Here we need to look for an 0xE2 indicating a leading variable name that
		 * should be skipped.
		 * This will happen if the original record is not a DEFINITION and had WITH NAMES.
		 * 
		 */
		if ((*recdef)[curdefpos] == 0xE2) {
			++curdefpos;
			curdefpos += (*recdef)[curdefpos] + 1;
		}

		/**
		 * March 2015 jpr  fixed with version 16.2+
		 *
		 * Eduardo Sabatino found a freaky subtle bug in this section
		 * that must have been here for many years.
		 * We use 0xF5 as a marker for the code output loop below to stop.
		 * But, some variable declarations can result in a legit 0xF5
		 * showing up. Some of these things would cause a bogus compile
		 * error. Some would crash the compiler. Some would silently
		 * generate a bad dbc file.
		 * The one that found the problem was a CHAR 501 in a record definition.
		 * Decimal 501 when changed to LLHH is 0xF5, 0x01
		 * Looks like 245 might also trip us up in some types of variables like QUEUE.
		 */
		minimumF5LoopRepetitions = 0;
		f5LoopCounter = 0;
		if ((*recdef)[curdefpos] == 0xFC || (*recdef)[curdefpos] == 0xFF) {
			UCHAR c2 = (*recdef)[curdefpos + 1];
			if ((*recdef)[curdefpos] == 0xFC) {
				if (c2 == 0x11 /* IMAGE */) {
					minimumF5LoopRepetitions = 6; // FC, 11, (H=)LLHH, (V=)LLHH
				}
				else if (c2 == 0x12 /* QUEUE */) {
					minimumF5LoopRepetitions = 6; // FC, 12, (ENTRIES=)LLHH, (SIZE=)LLHH
				}
				else if (c2 == 0x17 /* INIT */) {
					minimumF5LoopRepetitions = 4; // FC, 17, LLHH
				}
				else if (c2 == 0x1F /* CHAR */) {
					minimumF5LoopRepetitions = 4; // FC, 1F, LLHH
				}
			}
			else {
				if (c2 == 0x01 || c2 == 0x04 /* ONE dimensional array */ ) {
					minimumF5LoopRepetitions = 4; // FF, nn, (dimension size)LLHH
				}
				else if (c2 == 0x02 || c2 == 0x05 /* TWO dimensional array */ ) {
					minimumF5LoopRepetitions = 6; // FF, nn, (dimension size)LLHH, (dimension size)LLHH
				}
				else if (c2 == 0x03 || c2 == 0x06 /* THREE dimensional array */ ) {
					minimumF5LoopRepetitions = 8; // FF, nn, (dim size)LLHH, (dim size)LLHH, (dim size)LLHH
				}
			}
		}

		while (f5LoopCounter++ < minimumF5LoopRepetitions || (*recdef)[curdefpos] != 0xF5) {
			putdata1((*recdef)[curdefpos++]);
		}

		curdefpos++;
		def = *recdef;
		dataadrinc = (((INT) def[curdefpos + 2]) << 16) + (((INT) def[curdefpos + 1]) << 8) + (INT) def[curdefpos];
		symtype = def[curdefpos + 3];
		curdefpos += 4;
		symvalue = dataadr;
		dataadr += dataadrinc;
		if (namelen) putdsym(fldname, FALSE);
		savdataadr = dataadr;
		if (((symtype >= TYPE_CARRAY1 && symtype <= TYPE_CARRAY3) || (symtype >= TYPE_NARRAY1 && symtype <= TYPE_NARRAY3)) && !(symtype & TYPE_PTR)) {
			for (def = *recdef, i = 0; i < 4; i++) {
				arrayshape[i] = (((UINT) def[curdefpos + 1]) << 8) + def[curdefpos];
				curdefpos += 2;
			}
			putarrayshape(symvalue, arrayshape);
		}
		if ((*recdef)[curdefpos] == 0xFE) {
			curdefpos++;
			do putdata1((*recdef)[curdefpos]);
			while ((*recdef)[curdefpos++] != 0xFF);
		}
	} while ((*recdef)[curdefpos] != 0xFF);
	recordlevel--;
	savdataadr = saveadr;
}

INT getrecdef(CHAR *recname)
{
	INT hashvalue;
	USHORT workclsid;
	struct recordaccess *recindex;

	if (classlevel) workclsid = curclassid;
	else workclsid = 0;

	if (recindexptr == NULL) return -1;
	recindex = *recindexptr;
	hashvalue = (recname[0] + recname[1] * 2 + strlen(recname)) & 0x1F;
	if (!recindex[hashvalue].name[0]) return -1;  /* rec not defined */

	do {
		if (!strcmp(recname, recindex[hashvalue].name) && recindex[hashvalue].rlevel <= rlevel &&
			recindex[hashvalue].classid == workclsid &&
			rlevelid[recindex[hashvalue].rlevel] == recindex[hashvalue].routineid
		) return recindex[hashvalue].defptr;
		hashvalue = recindex[hashvalue].nextrec;
	} while (hashvalue != -1);
	return -1;
}

/* 
 * recdefpos < 0 new rec def allocation, >= 0 point to another rec def
 */
INT putrecdef(CHAR *recname, INT recdefpos)
{
	INT hashvalue, reuserec, lastrec;
	USHORT workclsid;
	struct recordaccess *recindex;

	if (classlevel) workclsid = curclassid;
	else workclsid = 0;

	if (recindexptr == NULL) {
		dbcmem((UCHAR ***) &recindexptr, 64 * sizeof(struct recordaccess));
		recindexalloc = 64;
		recindex = *recindexptr;
		for (recindexsize = 0; recindexsize < 32; recindexsize++) recindex[recindexsize].name[0] = '\0';
	}
	else recindex = *recindexptr;
	if (recdefpos < 0) recdefpos = recdefsize;  /* allocate new rec def space */
	hashvalue = (recname[0] + recname[1] * 2 + strlen(recname)) & 0x1F;
	if (!recindex[hashvalue].name[0]) { /* first rec with this hash value */
		strcpy(recindex[hashvalue].name, recname);
		recindex[hashvalue].rlevel = (UCHAR)rlevel;
		recindex[hashvalue].classid = workclsid;
		recindex[hashvalue].routineid = (USHORT)rlevelid[rlevel];
		recindex[hashvalue].defptr = recdefpos;
		recindex[hashvalue].nextrec = -1;
		currechash = hashvalue;
		goto exit_putdefrec;
	}

	reuserec = -1;
	do {
		if (!strcmp(recname, recindex[hashvalue].name) && recindex[hashvalue].rlevel <= rlevel &&
			recindex[hashvalue].classid == workclsid) {
			if (rlevelid[recindex[hashvalue].rlevel] == recindex[hashvalue].routineid) {
				error(ERROR_RECORDREDEF);
				return -1;
			}
			if (recindex[hashvalue].rlevel == rlevel && reuserec == -1) reuserec = hashvalue;  /* reuse recordindex struct */
		}
		lastrec = hashvalue;
		hashvalue = recindex[hashvalue].nextrec;
	} while (hashvalue != -1);

	/* fill in structure */
	if (reuserec == -1) {
		if (recindexsize == recindexalloc) {
			dbcmem((UCHAR ***) &recindexptr, (recindexalloc << 1) * sizeof(struct recordaccess));
			recindexalloc <<= 1;
			recindex = *recindexptr;
		}
		recindex[lastrec].nextrec = reuserec = recindexsize++;
		strcpy(recindex[reuserec].name, recname);
		recindex[reuserec].nextrec = -1;
	}
	recindex[reuserec].rlevel = (UCHAR)rlevel;
	recindex[reuserec].classid = workclsid;
	recindex[reuserec].routineid = (USHORT)rlevelid[rlevel];
	recindex[reuserec].defptr = recdefpos;
	currechash = reuserec;

exit_putdefrec:
	return(recdefpos);
}

void recdefhdr(CHAR *varname)
{
	UINT labellen;

	putdef1(labellen = (UCHAR) strlen(varname));
	putdef((UCHAR *) varname, labellen);
}

/* stype       TYPE_??? of variable */
/* dataadrinc  expanded data area size of var (amt dataadr is incremented) */
void recdeftail(UINT stype, UINT dataadrinc)
{
	putdef1(0xF5);
	putdef1((UCHAR) dataadrinc);
	putdef1((UCHAR) (dataadrinc >> 8));
	putdef1((UCHAR) (dataadrinc >> 16));
	putdef1(stype);
}

void putdef1(UINT data)	/* write 1 byte to record def area */
{
	if (recdef == NULL || recdefalloc - recdefsize < 50) dbcmem(&recdef, recdefalloc += 500);
	(*recdef)[recdefsize++] = (UCHAR)data;
}

void putdefhhll(UINT data)	/* write two bytes to record def area */
{
	if (recdef == NULL || recdefalloc - recdefsize < 50) dbcmem(&recdef, recdefalloc += 500);
	(*recdef)[recdefsize] = (UCHAR) (data >> 8);
	(*recdef)[recdefsize + 1] = (UCHAR) data;
	recdefsize += 2;
}

void putdefllhh(UINT data)		/* write two bytes to record def area */
{
	if (recdef == NULL || recdefalloc - recdefsize < 50) dbcmem(&recdef, recdefalloc += 500);
	(*recdef)[recdefsize] = (UCHAR) data;
	(*recdef)[recdefsize + 1] = (UCHAR) (data >> 8);
	recdefsize += 2;
}

void putdef(UCHAR *data, UINT n)	/* write n bytes to record def area */
{
	if (recdef == NULL || recdefalloc - recdefsize - n < 50) dbcmem(&recdef, recdefalloc += 500);
	memcpy((*recdef)+recdefsize, data, n);
	recdefsize += n;
}

void putdeflmh(INT data)	/* write three bytes to record def	area */
{
	if (recdef == NULL || recdefalloc - recdefsize < 50) dbcmem(&recdef, recdefalloc += 500);
	(*recdef)[recdefsize] = (UCHAR) data;
	(*recdef)[recdefsize + 1] = (UCHAR) ((UINT) data >> 8);
	(*recdef)[recdefsize + 2] = (UCHAR) (data >> 16);
	recdefsize += 3;
}

INT putrecdsym(CHAR *label)
{
	INT labelsize;
	CHAR reclabel[LABELSIZ + LABELSIZ];
	struct recordaccess *currecptr = &(*recindexptr)[currechash];

	if (!recdefonly) {
		labelsize = (int)strlen(label);
		if (!labelsize) return(1);
		if ((INT)(strlen(currecptr->name) + labelsize + 2) >= LABELSIZ) {
			error(ERROR_BADVARNAME);
			return(-1);
		}
		strcpy(reclabel, currecptr->name);
		strcat(reclabel, ".");
		strcat(reclabel, label);
		putdsym(reclabel, FALSE);
	}
	return(1);
}

INT writeroutine()
{
	UCHAR *undefaddr, savsymbase, savsymtype, labelflg;
 	CHAR addrvar[LABELSIZ], savtoken[TOKENSIZE];
	INT savsymvalue;
	UINT i, n, k, l, j;

	savsymbase = (UCHAR)symbasetype;
	savsymtype = (UCHAR)symtype;
	savsymvalue = symvalue;
	savecode(0);
	codebufcnt = 0;
	putcode1(rtncode);
	undefaddr = (*addrfixup);
	for (i = 0, n = 0, j = 0; i < undfnexist; j += n, i++) {
		n = undefaddr[j++];
		labelflg = FALSE;
		if (undefaddr[j] == '~') {
			j++;
			n--;
			labelflg = TRUE;
		}
		for (k = 0; k < n; k++) {
			if (undefaddr[j + k] == '[' || undefaddr[j + k] == '(') break;
			addrvar[k] = undefaddr[j + k];
		}
		addrvar[k] = 0;
		if (!labelflg) {
			if (getdsym(addrvar) == -1) {
				strcpy(savtoken, (CHAR *) token);
				strcpy((CHAR *) token, addrvar);
				error(ERROR_VARNOTFOUND);
				strcpy((CHAR *) token, savtoken);
				undfnexist = 0;
				symbasetype = savsymbase;
				symtype = savsymtype;
				symvalue = savsymvalue;
				codebufcnt = 0;
				restorecode(0);
				return(0);
			}
			else {
				symbasetype = (UCHAR) (symtype & ~TYPE_PTR);
				if (k < n && (undefaddr[j + k] == '[' || undefaddr[j + k] == '(')) {
					saveline(line, linecnt);
					linecnt = 0;
					for (l = 0; k < n; k++, l++) line[l] = undefaddr[j + k];
					line[l] = 0;
					if (symtype == TYPE_CVARPTRARRAY1 || symtype == TYPE_NVARPTRARRAY1 || symtype == TYPE_VVARPTRARRAY1) {
						if (!arraysymaddr(1)) return(0);
					}
					else if (symtype == TYPE_CVARPTRARRAY2 || symtype == TYPE_NVARPTRARRAY2 || symtype == TYPE_VVARPTRARRAY2) {
						if (!arraysymaddr(2)) return(0);
					}
					else if (symtype == TYPE_CVARPTRARRAY3 || symtype == TYPE_NVARPTRARRAY3 || symtype == TYPE_VVARPTRARRAY3) {
						if (!arraysymaddr(3)) return(0);
					}
					else {
						error(ERROR_BADVARTYPE);
						return(0);
					}
					restoreline(line, &linecnt);
				}
				else if (symbasetype <= TYPE_OBJECT && (symtype & TYPE_PTR)) putcodeadr(symvalue);
				else {
					strcpy(savtoken, (CHAR *) token);
					strcpy((CHAR *) token, addrvar);
					error(ERROR_BADVARTYPE);
					strcpy((CHAR *) token, savtoken);
					undfnexist = 0;
					symbasetype = savsymbase;
					symtype = savsymtype;
					symvalue = savsymvalue;
					codebufcnt = 0;
					restorecode(0);
					savcodeadr = codeadr;
					return(0);
				}
			}
		}
		else {
			if (getpsym(addrvar) == -1) {
				strcpy(savtoken, (CHAR *) token);
				strcpy((CHAR *) token, addrvar);
				error(ERROR_VARNOTFOUND);
				strcpy((CHAR *) token, savtoken);
				undfnexist = 0;
				symbasetype = savsymbase;
				symtype = savsymtype;
				symvalue = savsymvalue;
				codebufcnt = 0;
				restorecode(0);
				return(0);
			}
			else {
				if (symtype == TYPE_PLABELVAR) putcodeadr(0x800000 + symvalue);
				else {
					strcpy(savtoken, (CHAR *) token);
					strcpy((CHAR *) token, addrvar);
					error(ERROR_BADVARTYPE);
					strcpy((CHAR *) token, savtoken);
					undfnexist = 0;
					symbasetype = savsymbase;
					symtype = savsymtype;
					symvalue = savsymvalue;
					codebufcnt = 0;
					restorecode(0);
					savcodeadr = codeadr;
					return(0);
				}
			}
		}
	}
	undfnexist = 0;
	putcode1(0xFF);
	putcodeout();
	symbasetype = savsymbase;
	symtype = savsymtype;
	symvalue = savsymvalue;
	restorecode(0);
	savcodeadr = codeadr;
	return(1);
}

/**
 * On first entry locate user verb, if found return rule, else return 0
 * On subsequent entries, return next rule
 */
UINT uverbfnc()
{
	UCHAR n;
	INT clsnamesize;
	INT k, i, rule;
	CHAR symverb[VERBSIZE];
	CHAR modname[32];
	CHAR classname[LABELSIZ];
	INT mthdsymvalue, objsymvalue;
	static INT tmpobject;
	static UCHAR opcodeexist;	/* set to TRUE if correct call opcode is in codebuf */
	static UCHAR classinst;		/* set to TRUE if a class instantiation took place to make user verb call */
	static UCHAR mtdcall;		/* set to TRUE if a method was called */
	static UINT calllabel;
	static UCHAR *uvtable;		/* pointer to user verb prototype table */
	static UCHAR uvstate;		/* 0 parsing keyword, 1 parsing delimiter, 2 parse nonpos, 3 parse keyword	*/
	static UCHAR uvppcount;		/* positional parameters remaining */
	static UCHAR uvnonposrule;	/* non-positional parameter rule */
	static UCHAR uvlinecntsave;	/* save keyword start linecnt */
	static INT uvparmpos;		/* offset of next pos parameter rule or non-pos rule or 0 */
	static INT uvkwpos;		/* offset of start of keyword table or 0 */
	static INT uvkwrulesave;	/* save of offset of keyword rules in user verb table */
	static INT numkwparm; 	/* number of parameters for the current keyword */
	static INT curkwparm; 	/* current keyword parameter being parsed */

	if (!rulecnt) {  /* initial entry */
		if (uvtablepptr == NULL) return(0);
		uvtable = *uvtablepptr;
		mtdcall = FALSE;
		classinst = FALSE;
		tmpvarflg = 3;  /* do NOT reuse temporary variables in parm expressions */
		constantfoldflg = FALSE;  /* turn constant folding off */
		n = (UCHAR) strlen(verb);
		if (verb[0] < 'A' || verb[0] > 'Z') k = uvindex[26];
		else k = uvindex[verb[0] - 'A'];
		for ( ; ; ) {
			if (!k) return(0);
			if (uvtable[k + 5] == n) {
				for (i = 0; (i < n) && (verb[i] == toupper((symverb[i] = uvtable[k + i + 6]))); i++);
				if (i == n) {
					symverb[i] = '\0';
					break;
				}
			}
			k = load4hhll(uvtable + k);
		}
		clsnamesize = uvtable[k + n + 6];
		uvnonposrule = uvtable[k + n + 7 + clsnamesize];
		uvppcount = uvtable[k + n + 8 + clsnamesize];
		uvparmpos = k + n + clsnamesize + 9;
		uvkwpos = k + n + uvppcount + clsnamesize + 9;
		codebufcnt = 0;
		switch (uvtable[k + 4]) {
			case VERBTYPE_VERB:
				if (usepsym(symverb, LABEL_REFFORCE) == RC_ERROR) return(NEXT_TERMINATE);
				calllabel = (UINT) symvalue;
				if (TYPE_PLABELMREF == symtype) {
					error(ERROR_BADMETHODCALL);	
					return(NEXT_TERMINATE);
				}
				if (uvppcount) {	/* positional parms exist */
					putcode1(111);
					putcodehhll(calllabel);
					opcodeexist = TRUE;
				}
				else opcodeexist = FALSE;
				uvstate = 0;  /* parse parameter next */
				uvlinecntsave = (UCHAR)linecnt;
				break;
			case VERBTYPE_CVERB:
				putcode1(88);
				putcode1(143);
				putcodeadr(makeclit((UCHAR *) symverb));
				opcodeexist = TRUE;
				uvstate = 0;  /* parse parameter next */
				uvlinecntsave = (UCHAR)linecnt;
				break;
			case VERBTYPE_MAKE:
				putcode1(192);				/* XPREFIX */
				putcodehhll(0x58E2);		/* MAKE <obj>,<class>,<mod> */
				scantoken(upcaseflg);
				if (tokentype != TOKEN_WORD) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				if (getdsym((CHAR *) token) == RC_ERROR) {
					error(ERROR_VARNOTFOUND);
					return(NEXT_TERMINATE);
				}
				if ((symtype & ~TYPE_PTR) != TYPE_OBJECT) {
					error(ERROR_BADVARTYPE);
					return(NEXT_TERMINATE);
				}
				objsymvalue = symvalue;
				putcodeadr(objsymvalue);
				if (clsnamesize) {			/* verb type: VERB !MAKE(class) */
					memcpy(classname, &uvtable[k + n + 7], (size_t) clsnamesize);
					classname[clsnamesize] = 0;
					if (getclass(classname, modname) == RC_ERROR) {
						error(ERROR_CLASSNOTFOUND);
						return(NEXT_TERMINATE);
					}			
				}
				else {					/* verb type: VERB !MAKE */
					scantoken(upcaseflg);
					if (TOKEN_LPAREND == tokentype) {
						whitespace();
						scantoken(upcaseflg);
						clsnamesize = (int)strlen((CHAR *) token);
						if (clsnamesize >= LABELSIZ) {
							error(ERROR_INVALCLASS);
							return(NEXT_TERMINATE);
						}
						if (getclass((CHAR *) token, modname) == RC_ERROR) {
							error(ERROR_CLASSNOTFOUND);
							return(NEXT_TERMINATE);
						}			
						strcpy(classname, (CHAR *) token);
						whitespace();
						scantoken(upcaseflg);
						if (tokentype != TOKEN_RPAREND) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
					}
				}
				putcodeadr(makeclit((UCHAR *) classname));
				putcodeadr(makeclit((UCHAR *) modname));
				putcode1(0xFF);
				if (usepsym(symverb, LABEL_METHODREFDEF) == RC_ERROR) return(NEXT_TERMINATE);
				calllabel = (UINT) symvalue;
				if (makemethodref((UINT) symvalue) == RC_ERROR) return(NEXT_TERMINATE);
				putcodeadr(objsymvalue);
				opcodeexist = TRUE;
				mtdcall = TRUE;
				uvstate = 1;	/* parse delimiter next */
				if (!uvppcount && !uvtable[uvkwpos] && !uvnonposrule &&
				    line[linecnt] && charmap[line[linecnt]] != 2) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				break;
			case VERBTYPE_METHOD:
				scantoken(upcaseflg);
				if (tokentype != TOKEN_WORD || clsnamesize) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				if (usepsym(symverb, LABEL_METHODREFDEF) == RC_ERROR) return(NEXT_TERMINATE);
				calllabel = (UINT) symvalue;
				if (makemethodref((UINT) symvalue) == RC_ERROR) return(NEXT_TERMINATE);
				if (getdsym((CHAR *) token) == RC_ERROR) {
					error(ERROR_VARNOTFOUND);
					return(NEXT_TERMINATE);
				}
				if ((symtype & ~TYPE_PTR) != TYPE_OBJECT) {
					error(ERROR_BADVARTYPE);
					return(NEXT_TERMINATE);
				}
				putcodeadr(symvalue);
				opcodeexist = TRUE;
				uvstate = 1;	/* parse delimiter next */
				mtdcall = TRUE;
				if (!uvppcount && !uvtable[uvkwpos] && !uvnonposrule &&
				    line[linecnt] && charmap[line[linecnt]] != 2) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				break;
			case VERBTYPE_DESTROY:
				scantoken(upcaseflg);
				if (tokentype != TOKEN_WORD) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				if (getdsym((CHAR *) token) == RC_ERROR) {
					error(ERROR_VARNOTFOUND);
					return(NEXT_TERMINATE);
				}
				if ((symtype & ~TYPE_PTR) != TYPE_OBJECT) {
					error(ERROR_BADVARTYPE);
					return(NEXT_TERMINATE);
				}
				objsymvalue = symvalue;
				if (!uvppcount && !uvtable[uvkwpos] && !uvnonposrule &&
				    line[linecnt] && charmap[line[linecnt]] != 2) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				if (usepsym(symverb, LABEL_METHODREFDEF) == RC_ERROR) return(NEXT_TERMINATE);
				calllabel = (UINT) symvalue;
				if (makemethodref((UINT) symvalue) == RC_ERROR) return(NEXT_TERMINATE);
				putcodeadr(objsymvalue);			
				opcodeexist = TRUE;
				mtdcall = TRUE;
				uvstate = 1;	/* parse delimiter next */
				classinst = TRUE;
				tmpobject = objsymvalue; 
				break;
			case VERBTYPE_TRANSIENT:
				/* make temp object, issue method call, destory object */
				if (usepsym(symverb, LABEL_METHODREFDEF) == RC_ERROR) return(NEXT_TERMINATE);
				mthdsymvalue = symvalue;
				memcpy(classname, &uvtable[k + n + 7], (size_t) clsnamesize);
				classname[clsnamesize] = 0;
				putcode1(192);				/* XPREFIX */
				putcodehhll(0x58E2);		/* MAKE <obj>,<class>,<mod> */
				tmpobject = xtempvar(TEMPTYPE_OBJECT);	/* this temporary cannot be reused */
				putcodeadr(tmpobject);
				getclass(classname, modname);
				putcodeadr(makeclit((UCHAR *) classname));
				putcodeadr(makeclit((UCHAR *) modname));
				putcode1(0xFF);
				if (makemethodref((UINT) mthdsymvalue) == RC_ERROR) return(NEXT_TERMINATE);
				putcodeadr(tmpobject);
				opcodeexist = TRUE;
				classinst = TRUE;
				mtdcall = TRUE;
				uvstate = 0;  /* parse parameter next */
				uvlinecntsave = (UCHAR)linecnt;
				if (!uvppcount && !uvtable[uvkwpos] && !uvnonposrule &&
				    line[linecnt] && charmap[line[linecnt]] != 2) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				whitespace();
				break;
		}
		if (!uvppcount && !uvtable[uvkwpos] && !uvnonposrule) {
			if (!opcodeexist) {
				makegoto(calllabel);
				codebuf[0] -= 4;
			}
			else {
				if (mtdcall) putcode1(0xFF);
				else if (codebuf[0] == 88 && codebuf[1] == 143) putcode1(0xFF); 
				if (classinst) {	/* a class was instantiated to make this verb call - delete it */
					putcodehhll(0x58E3);	/* destroy <obj> */
					putcodeadr(tmpobject);
				}
			}
			return(NEXT_TERMINATE);
		}
		if (!uvstate) {
			if (uvppcount) return(uvtable[uvparmpos] + NEXT_NOPARSE);
			else return(RULE_PARSEUP + NEXT_NOPARSE);
		}
		else {	/* special case, parsing delimiter before first verb parameter */
			if (uvppcount) {
				uvppcount++;
				uvparmpos--;
				return(RULE_NOPARSE + NEXT_PREPOPT);
			}
			else return(RULE_NOPARSE + NEXT_LIST);
		}
	}

	/* not initial entry */
/*** CODE CHECK: IS IT POSSIBLE THAT uvtablepptr HAS MOVED AND uvtable IS NOT VALID ***/
	/* YES IT IS! JPR 15FEB2022 */

	uvtable = *uvtablepptr;   // new on 15FEB2022
	switch(uvstate) {
		case 0:	  /* uvstate == 0, parameter was just parsed */
			uvstate = 1;	    /* parse delimiter next */
			if (uvppcount) {
				return(RULE_NOPARSE + NEXT_PREPOPT);
			}
			else {
				if (tokentype == 0) { /* keywords, non-pos optionally not there */
					if (!opcodeexist) {
						makegoto(calllabel);
						codebuf[0] -= 4;
						opcodeexist = TRUE;
					}
					return(NEXT_TERMINATE);
				}
				if (!opcodeexist) {
					putcode1(111);
					putcodehhll(calllabel);
					opcodeexist = TRUE;
				}
				if (uvtable[uvkwpos]) {
					k = uvkwpos;
					n = (UCHAR)strlen((CHAR *) token);
					for (;;) {
						if (!uvtable[k]) {	/* didn't find keyword */
							if (!uvnonposrule) {
								error(ERROR_BADKEYWORD);
								return(NEXT_TERMINATE);
							}
							uvstate = 2; /* reparse non-positional */
							linecnt = uvlinecntsave; /* put back token word */
							putcode1(0xFE);
							return(uvnonposrule + NEXT_NOPARSE);
						}
						numkwparm = uvtable[k + 1];
						if (n == uvtable[k]) {
							if (!memcmp(token, &uvtable[k + 6], n)) break;
						}
						k += uvtable[k] + numkwparm + 6;
					}
					/* found the keyword */
					putcode1((UCHAR) (0xF8 + numkwparm));
					rule = load4hhll(uvtable + k + 2);
					if (rule == -1) {	/* create the literal keyword */
						rule = makeclit(token);
						move4hhll(rule, uvtable + k + 2);
						uvtable = *uvtablepptr;	/* makeclit might move moveable memory */
					}
					putcodeadr(rule);
					if (!numkwparm) {
						if (line[linecnt] == '=') {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
						uvstate = 1;	    /* parse delimiter next */
						return(RULE_NOPARSE + NEXT_LIST);	/* keyword without operands */
					}
					if (line[linecnt++] != '=') {
						error(ERROR_SYNTAX);
						return(NEXT_TERMINATE);
					}
					/* parsing a keyword's operands */
					whitespace();
					uvlinecntsave = (UCHAR)linecnt;
					curkwparm = numkwparm - 1;
					uvkwrulesave =	k + n + 6;
					uvstate = 3;		   /* parse keyword parameter next */
					return(uvtable[uvkwrulesave] + NEXT_NOPARSE);
				}
				else {
					if (uvnonposrule) {
						uvstate = 2; /* reparse non-positional */
						linecnt = uvlinecntsave; /* put back token word */
						putcode1(0xFE);
						return(uvnonposrule + NEXT_NOPARSE);
					}
					else {
						if (codebuf[0] == 111 || (codebuf[0] == 88 && codebuf[1] == 143)) putcode1(0xFF);
						return(NEXT_TERMINATE);
					}
				}
			}
			break;
		case 1:	  /* uvstate == 1, delimiter was just parsed */
			if (!delimiter) {
				if ((INT) uvppcount-1 > 0) {
					error(ERROR_MISSINGOPERAND);
					return(NEXT_TERMINATE);
				}
				if (mtdcall || codebuf[0] == 111 ||
				    (codebuf[0] == 88 && codebuf[1] == 143)) putcode1(0xFF);
				if (classinst) {	/* a class was instantiated to make this verb call - delete it */
					putcodehhll(0x58E3);	/* destroy <obj> */
					putcodeadr(tmpobject);
				}
				return(NEXT_TERMINATE);
			}
			else {
				uvstate = 0;	/* parse parameter next */
				uvlinecntsave = (UCHAR)linecnt;
				delimiter = 0;
				uvtable = *uvtablepptr;
				if (uvppcount)
					if (--uvppcount)
						return(uvtable[++uvparmpos] + NEXT_NOPARSE); /* more pos parms */
				if (uvtable[uvkwpos] || uvnonposrule) {
					return(RULE_PARSEUP + NEXT_NOPARSE);	/* keywords exist */
				}
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
		case 2:	  /* uvstate == 2, non-positional was just reparsed */
			uvstate = 1;	    /* parse delimiter next */
			return(RULE_NOPARSE + NEXT_LIST);
		case 3:	  /* uvstate == 3, keyword parameter was just parsed */
			if (curkwparm) {
				if (line[linecnt++] != ':') {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				i = numkwparm - curkwparm;
				curkwparm--;
				uvlinecntsave = (UCHAR)linecnt;
				return(uvtable[uvkwrulesave + i] + NEXT_NOPARSE);
			}
			else {
				uvstate = 1;	    /* parse delimiter next */
				return(RULE_NOPARSE + NEXT_LIST);
			}
	}
	return(0);
}

UINT s_uverb_fnc()	/* VERB and CVERB processing */
{
	INT i, j, k, curkywrd, clsnamesize, vrbtypepos, linecntsave, classverbflg;
	UCHAR m, n, lblchar, *uvtable;
	CHAR module[32];
	static UINT uvtablesize = 1;		/* current user verb table size */
	static UINT uvtablealloc;	/* current user verb table allocation */

	if (rulecnt == 1) {
		if (uvtablepptr == NULL || uvtablealloc - uvtablesize < UVTABLEMARGIN) {
			if (uvtablepptr == NULL)
			{
				uvtablealloc = UVTABLESTART;
				dbcmem(&uvtablepptr, uvtablealloc);
			}
			else dbcmem(&uvtablepptr, uvtablealloc += UVTABLEINC);
		}
		uvtable = *uvtablepptr;
		if (upcaseflg) for (n = 0; (label[n] = (UCHAR) toupper(label[n])) != 0; n++);
		n = (UCHAR) strlen(label);
		lblchar = (UCHAR)toupper(label[0]);
		if (lblchar < 'A' || lblchar > 'Z') m = 26;
		else m = (UCHAR) (lblchar - 'A');
		i = k = uvindex[m];
		while (k) {
			if (uvtable[k + 4] == n && !memcmp(label, &uvtable[k + 5], n)) {
				error(ERROR_DUPUVERB);
				return(NEXT_TERMINATE);
			}
			k = load4hhll(uvtable + k);
		}
		/**
		 * uvtablesize points at the next available UCHAR slot in uvtable
		 * Save it in uvindex[m] so that the head of the linked list
		 * is the new entry we are about to build
		 */
		uvindex[m] = uvtablesize;
		/**
		 * The 1st four bytes of a uvtable entry are the index to the next entry.
		 * For our new entry, set it to the former head of the linked list
		 */
		move4hhll(i, uvtable + uvtablesize);
		uvtablesize += 4;
		vrbtypepos = uvtablesize++;
		if (verb[0] == 'V')	uvtable[vrbtypepos] = VERBTYPE_VERB; /* flag uverb */
		else uvtable[vrbtypepos] = VERBTYPE_CVERB; 			 /* flag cverb */
		uvtable[uvtablesize++] = n;			 /* length of verb name */
		memcpy(&uvtable[uvtablesize], label, n); /* verb name */
		uvtablesize += n;
		uvtable[uvtablesize] = 0;	/* length of class name */
		uvtable[uvtablesize + 1] = 0;		/* non-pos rule */
		uvtable[uvtablesize + 2] = 0;		/* # pos. parms */
		uvtable[uvtablesize + 3] = 0;		/* keyword parm */
		if (TOKEN_BANG == tokentype) {	/* this is a class verb */
			if ('C' == verb[0]) {
				error(ERROR_SYNTAX);
				uvtablesize += 4;
				return(NEXT_TERMINATE);
			}
			scantoken(SCAN_UPCASE);
			if (tokentype != TOKEN_WORD) {
				error(ERROR_SYNTAX);
				uvtablesize += 4;
				return(NEXT_TERMINATE);
			}
			for (curkywrd = 0; curkywrd < NUMVRBKYWRDS; curkywrd++) {
				if (!strcmp((CHAR *) token, vrbtypekywrd[curkywrd])) break;
			}
			if (NUMVRBKYWRDS == curkywrd) {
				error(ERROR_SYNTAX);
				uvtablesize += 4;
				return(NEXT_TERMINATE);
			}
			switch (curkywrd) {
				case VRBKYWRD_MAKE:
					uvtable[vrbtypepos] = VERBTYPE_MAKE;
					linecntsave = linecnt;
					scantoken(upcaseflg);
					if (TOKEN_LPAREND == tokentype) {
						whitespace();
						scantoken(upcaseflg);
						clsnamesize = (int)strlen((CHAR *) token);
						if (clsnamesize >= LABELSIZ) {
							error(ERROR_INVALCLASS);
							uvtablesize += 4;
							return(NEXT_TERMINATE);
						}
						if (getclass((CHAR *) token, module) == RC_ERROR) {
							error(ERROR_CLASSNOTFOUND);
							uvtablesize += 4;
							return(NEXT_TERMINATE);
						}			
						uvtable[uvtablesize++] = (UCHAR)clsnamesize;	/* length of class name */
						memcpy(&uvtable[uvtablesize], token, (size_t) clsnamesize); /* verb name */
						uvtablesize += clsnamesize;
						whitespace();
						scantoken(upcaseflg);
						if (tokentype != TOKEN_RPAREND) error(ERROR_SYNTAX);
					}
					else {
						linecnt = linecntsave;
						uvtable[uvtablesize++] = 0;	/* length of class name */
					}
					usepsym(label, LABEL_METHOD);
					break;	
				case VRBKYWRD_METHOD:
					uvtable[vrbtypepos] = VERBTYPE_METHOD;
					uvtable[uvtablesize++] = 0;	/* length of class name */
					usepsym(label, LABEL_METHOD);
					break;	
				case VRBKYWRD_DESTROY:
					uvtable[vrbtypepos] = VERBTYPE_DESTROY;
					uvtable[uvtablesize++] = 0;	/* length of class name */
					usepsym(label, LABEL_METHOD);
					break;
				case VRBKYWRD_TRANSIENT:
					usepsym(label, LABEL_METHOD);
					uvtable[vrbtypepos] = VERBTYPE_TRANSIENT;
					scantoken(upcaseflg);
					if (TOKEN_LPAREND == tokentype) {
						whitespace();
						scantoken(upcaseflg);
						clsnamesize = (int)strlen((CHAR *) token);
						if (clsnamesize >= LABELSIZ) {
							error(ERROR_INVALCLASS);
							token[LABELSIZ - 1] = 0;
						}
						if (getclass((CHAR *) token, module) == RC_ERROR) {
							error(ERROR_CLASSNOTFOUND);
							module[0] = 0;
						}			
						uvtable[uvtablesize++] = (UCHAR)clsnamesize;	/* length of class name */
						memcpy(&uvtable[uvtablesize], token, (size_t) clsnamesize); /* verb name */
						uvtablesize += clsnamesize;
						whitespace();
						scantoken(upcaseflg);
						if (tokentype != TOKEN_RPAREND) error(ERROR_SYNTAX);
					}
					else error(ERROR_SYNTAX);
					break;
				default:
					death(DEATH_INTERNAL7);
			}
			token[0] = 0;
			classverbflg = TRUE;
		}
		else {
			uvtable[uvtablesize++] = 0;	/* length of class name */
			classverbflg = FALSE;
		}
		uvtable[uvtablesize++] = 0;		/* non-pos rule */
		uvtable[uvtablesize++] = 0;		/* # pos. parms */
		uvtable[uvtablesize++] = 0;		/* keyword parm */
		su = uvtablesize - 2;  /* offset of count of positional parms */
		sc = TRUE;		   /* positional parameters are allowed */
		nextflag = 0;
		delimiter = 0;
		if (classverbflg) return(RULE_NOPARSE + NEXT_LIST);
	}
	else if (nextflag == 2) {
		return(NEXT_TERMINATE);
	}
	if (delimiter) {
		delimiter = 0;
		return(RULE_PARSEUP + NEXT_NOPARSE);
	}

	if (!token[0]) {
		return(NEXT_TERMINATE);
	}

	if (uvtablealloc - uvtablesize < UVTABLEMARGIN) {
		dbcmem(&uvtablepptr, uvtablealloc += UVTABLEINC);
	}
	uvtable = *uvtablepptr;
	switch (token[0]) {
		case '#' :
			if (!sc) {
				error(ERROR_POSPARMNOTFIRST);
				return(NEXT_TERMINATE);
			}
			scantoken(SCAN_UPCASE);
			if (tokentype == TOKEN_WORD) {
				uvtable[uvtablesize - 1] = (UCHAR) s_uverb_types((CHAR *) token);
				if (uvtable[uvtablesize - 1]) {
					uvtable[uvtablesize++] = 0;
					uvtable[su]++;
				}
				else {
					error(ERROR_BADKEYWORDPARM);
					return(NEXT_TERMINATE);
				}
			}
			else {
				error(ERROR_BADKEYWORDPARM);
				return(NEXT_TERMINATE);
			}
			break;
		case '=' :
			sc = FALSE;
			if (uvtable[su - 1]) {
				error(ERROR_TOOMANYNONPOS);
				return(NEXT_TERMINATE);
			}
			whitespace();
			scantoken(SCAN_UPCASE);
			if (tokentype == TOKEN_WORD)
				uvtable[su - 1] = (UCHAR) s_uverb_types((CHAR *) token);
			if (!uvtable[su - 1]) {
				error(ERROR_BADKEYWORDPARM);
				return(NEXT_TERMINATE);
			}
			break;
		default :
			sc = FALSE;
			if (tokentype != TOKEN_WORD) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			n = (UCHAR)strlen((CHAR *) token);
			j = uvtablesize;
			uvtable[j - 1] = n;
			uvtable[j] = 0;
			move4hhll(-1, uvtable + j + 1);
			memcpy(&uvtable[j + 5], token, n);
			uvtablesize += n + 5;
			if (line[linecnt] == '=') {
				do {
					linecnt++;
					whitespace();
					scantoken(SCAN_UPCASE);
					if (tokentype != TOKEN_WORD) {
						error(ERROR_SYNTAX);
						return(NEXT_TERMINATE);
					}
					if (uvtable[j] == 5) {
						error(ERROR_TOOMANYKEYWRDPARM);
						return(NEXT_TERMINATE);
					}
					uvtable[uvtablesize] = (UCHAR) s_uverb_types((CHAR *) token);
					if (!uvtable[uvtablesize]) {
						uvtablesize++;
						error(ERROR_BADKEYWORDPARM);
						return(NEXT_TERMINATE);
					}
					uvtable[j]++;
					uvtablesize++;
				} while ((line[linecnt] == ':') && (charmap[line[linecnt+1]] != 2) && (charmap[line[linecnt+1]] != 1));
				uvtable[uvtablesize++] = 0;
			}
			else {
				/**
				 * Set the length of the next keyword to zero to indicate the end of the list
				 */
				uvtable[uvtablesize++] = 0;
			}
	}
	return(RULE_NOPARSE + NEXT_LIST);		/* parse delimeter */
}
