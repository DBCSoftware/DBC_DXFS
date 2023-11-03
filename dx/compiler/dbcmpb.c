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
#define DBCMPB
#include "dbcmp.h"
#include "base.h"

static UINT s_xcall_fnc(void);
static UINT s_unlock_fnc(void);
static UINT s_close_fnc(void);
static UINT s_add_fnc(void);
static UINT s_power_fnc(void);
static UINT s_bump_fnc(void);
static UINT s_clear_fnc(void);
static UINT s_clock_fnc(void);
static UINT s_clockclient_fnc(void);
static UINT s_cmatch_fnc(void);
static UINT s_delete_fnc(void);
static UINT s_print_fnc(void);
static UINT s_filepi_fnc(void);
static UINT s_pi_fnc(void);
static UINT s_load_fnc(void);
static UINT s_move_fnc(void);
static UINT s_moveadr_fnc(void);
static UINT s_open_fnc(void);
static UINT s_prepare_fnc(void);
static UINT s_read_fnc(void);
static UINT s_splclose_fnc(void);
static UINT s_splopen_fnc(void);
static UINT s_trap_fnc(void);
static UINT s_trapclr_fnc(void);
static UINT s_type_fnc(void);
static UINT s_write_fnc(void);
static UINT s_update_fnc(void);
static UINT s_comwait_fnc(void);
static UINT s_nformat_fnc(void);
static UINT s_sformat_fnc(void);
static UINT s_and_fnc(void);
static UINT s_check10_fnc(void);
static UINT s_unload_fnc(void);
static UINT s_setflag_fnc(void);
static UINT s_sqlexec_fnc(void);
static UINT s_getparm_fnc(void);
static UINT s_wait_fnc(void);
static UINT s_draw_fnc(void);
static UINT s_return_fnc(void);
static UINT s_call_fnc(void);
static UINT s_goto_fnc(void);
static UINT s_for_fnc(void);
static UINT s_loop_fnc(void);
static UINT s_repeat_fnc(void);
static UINT s_break_fnc(void);
static UINT s_while_fnc(void);
static UINT s_if_fnc(void);
static UINT s_else_fnc(void);
static UINT s_switch_fnc(void);
static UINT s_case_fnc(void);
static UINT s_tabpage_fnc(void);
static UINT s__ifz_fnc(void);
static UINT s_ifeq_fnc(void);
static UINT s_keyin_fnc(void);
static UINT s_display_fnc(void);
static UINT s_next_semilist_fnc(void);
static UINT s_next_list_fnc(void);
static UINT s_setendkey_fnc(void);
static UINT s_fill_fnc(void);
static UINT s_show_fnc(void);
//static UINT s_record_fnc(void);
static UINT s_edit_fnc(void);
static UINT s_setnull_fnc(void);
static UINT s_rotate_fnc(void);
static INT putinitaddr(INT, UINT [], UINT, INT);

/* SYNTAX added for DB/C DX 13 */
static SYNTAX s_rotate[] =  {{RULE_MATHSRC + NEXT_PREP, s_rotate_fnc},
				{RULE_NVAR + NEXT_END, (RULEFNC) NULL }};

/* SYNTAX structures added for OOP DB/C in release 9.1 */
static SYNTAX s_object[] = { {RULE_PARSEVAR + NEXT_NOPARSE, s_object_fnc }};
static SYNTAX s_class[] = { {RULE_PARSEUP + NEXT_NOPARSE, s_class_fnc }};
static SYNTAX s_endclass[] = { {RULE_NOPARSE + NEXT_NOPARSE, s_endclass_fnc }};
static SYNTAX s_method[] = { {RULE_NOPARSE + NEXT_NOPARSE, s_method_fnc }};
static SYNTAX s_make[] = { {RULE_OBJECT + NEXT_PREP, s_make_fnc}, 
	   				  {RULE_ANYSRC + (RULE_EQUATE << 8) + NEXT_LIST, s_next_list_fnc} };
static SYNTAX s_destroy[] = { {RULE_OBJECT + NEXT_END, NULL} };
static SYNTAX s_getobject[] = { {RULE_OBJECTPTR + NEXT_END, NULL} };

/* SYNTAX structures, added for DB/C release 9.1 */
static SYNTAX s_clearlabel[] = { {RULE_LBLDEST + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_getname[] = { {RULE_ANYVAR + NEXT_PREP, (RULEFNC) NULL},
					     {RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_pushreturn[] = { {RULE_LBLSRC + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_setnull[] = { {RULE_CNDATADEST + NEXT_LIST, s_setnull_fnc} };
static SYNTAX s_unlock[] = { {RULE_AFIFILE + NEXT_PREPOPT, (RULEFNC) s_unlock_fnc} };
static SYNTAX s_xcall[] = { {RULE_CSRC + NEXT_PREPOPT, s_xcall_fnc},
				{RULE_ANYSRC + (RULE_EQUATE << 8) + NEXT_LIST, s_next_list_fnc} };

/* standard SYNTAX structures, DB/C release 8.0 */
static SYNTAX s_add[] = { {RULE_MATHSRC + NEXT_PREP, s_add_fnc} };
static SYNTAX s_append[] = { {RULE_CNSRC + NEXT_PREP, (RULEFNC) NULL},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_beep[] = { {RULE_NOPARSE + NEXT_ENDOPT, (RULEFNC) NULL} };
static SYNTAX s_branch[] = { {RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP, (RULEFNC) NULL},
				{RULE_LBLSRC + NEXT_LIST, s_next_list_fnc} };
static SYNTAX s_bump[] = { {RULE_CVAR + NEXT_PREPOPT, s_bump_fnc},
				{RULE_NSRCCONST + (RULE_EQUATE << 8) + NEXT_END, s_bump_fnc} };
static SYNTAX s_reset[] = { {RULE_CVAR + NEXT_PREPOPT, s_bump_fnc},
				{RULE_CNSRCCONST + (RULE_EQUATE << 8) + NEXT_END, s_bump_fnc} };
static SYNTAX s_chain[] = { {RULE_CSRC + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_clear[] = { {RULE_CNDATADEST + NEXT_LIST, s_clear_fnc} };
static SYNTAX s_clock[] = { {RULE_PARSEUP + NEXT_PREP, s_clock_fnc},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_clockclient[] = { {RULE_PARSEUP + NEXT_PREP, s_clockclient_fnc},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_close[] = { {RULE_AFIDEVRES + NEXT_COMMAOPT, s_close_fnc} };
static SYNTAX s_cmatch[] = { {RULE_CVAR + (RULE_SLITERAL << 8) + ((INT) RULE_XCON << 16) + NEXT_PREP, s_cmatch_fnc},
				{RULE_CVAR + (RULE_SLITERAL << 8) + ((INT) RULE_XCON << 16) + NEXT_END, s_cmatch_fnc} };
static SYNTAX s_cmove[] = { {RULE_CVAR + (RULE_SLITERAL << 8) + ((INT) RULE_XCON << 16) + NEXT_PREP, s_cmatch_fnc},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_compare[] = { {RULE_MATHSRC + NEXT_PREP, (RULEFNC) NULL},
				{RULE_MATHSRC + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_delete[] = { {RULE_AFIFILE + NEXT_COMMAOPT, s_delete_fnc},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_deletek[] = { {RULE_IFILE + NEXT_COMMA, (RULEFNC) NULL},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_keyin[] = { {RULE_NOPARSE + NEXT_NOPARSE, s_keyin_fnc},
				{RULE_DISPLAYSRC + (RULE_KYCC << 8) + NEXT_SEMILIST, s_next_semilist_fnc} };
static SYNTAX s_display[] = { {RULE_NOPARSE + NEXT_NOPARSE, s_display_fnc},
				{RULE_DISPLAYSRC + (RULE_DSCC << 8) + NEXT_SEMILIST, s_next_semilist_fnc} };
static SYNTAX s_ccall[] = { {RULE_CSRC + NEXT_PREPOPT, s_next_list_fnc},
				{RULE_CNSRC + (RULE_QUEUE << 8) + NEXT_LIST, s_next_list_fnc}};
static SYNTAX s_edit[] = { {RULE_CNVAR + NEXT_PREP, (RULEFNC) NULL},
				{RULE_CSRC + NEXT_GIVINGOPT, s_edit_fnc} };
static SYNTAX s_endset[] = { {RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_count[] = { {RULE_NVAR + NEXT_PREP, (RULEFNC) NULL},
				{RULE_CNDATADEST + NEXT_LIST, s_next_list_fnc} };
static SYNTAX s_filepi[] = { {RULE_DCON + NEXT_SEMIOPT, s_filepi_fnc},
				{RULE_AFIFILE + NEXT_LIST, s_filepi_fnc} };
static SYNTAX s_pi[] = { {RULE_DCON + NEXT_END, s_pi_fnc} };
static SYNTAX s_fposit[] = { {RULE_AFIFILE + NEXT_COMMA, (RULEFNC) NULL},
				{RULE_NVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_insert[] = { {RULE_IFILE + (RULE_AFILE << 8) + NEXT_COMMAOPT, s_delete_fnc},
				{RULE_CVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_load[] = { {RULE_LOADSRC + (RULE_IMAGE << 8) + NEXT_PREP, s_load_fnc} };
static SYNTAX s_readkg[] = { {RULE_AFILE + NEXT_SEMICOLON, (RULEFNC) NULL},
				{RULE_CNDATADEST + (RULE_RDCC << 8) + ((INT) RULE_EMPTYLIST << 16)
				+ NEXT_SEMILIST, s_next_semilist_fnc} };
static SYNTAX s_match[] = { {RULE_CSRC + NEXT_PREP, (RULEFNC) NULL},
				{RULE_CSRC + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_move[] = { {RULE_MOVESRC + NEXT_PREP, s_move_fnc},
				{RULE_MOVEDEST + NEXT_LIST, s_move_fnc} };
static SYNTAX s_movefptr[] = { {RULE_CVAR + NEXT_PREP, (RULEFNC) NULL},
				{RULE_NVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_moveadr[] = { {RULE_ANYVAR + (RULE_LITERAL << 8) + NEXT_PREP, s_moveadr_fnc} };
static SYNTAX s_open[] = { {RULE_AFIDEVRES + NEXT_COMMA, s_open_fnc},
				{RULE_CSRC + NEXT_COMMAOPT, s_open_fnc} };
static SYNTAX s_pack[] = { {RULE_CVAR + NEXT_PREP, (RULEFNC) NULL},
				{RULE_CNDATASRC + NEXT_LIST, s_next_list_fnc} };
static SYNTAX s_pause[] = { {RULE_NSRC + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_power[] = { {RULE_NOPARSE + NEXT_NOPARSE, s_power_fnc},
				{RULE_NSRC + NEXT_PREP, s_power_fnc},
				{RULE_NSRC + NEXT_PREPGIVING, s_power_fnc},
				{RULE_NVAR + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_test[] = { {RULE_CNVAR + (RULE_DAFB << 8) + NEXT_END, (RULEFNC) NULL} };
static SYNTAX s_prepare[] = { {RULE_AFIDEVRES + NEXT_COMMA, s_prepare_fnc},
				{RULE_CSRC + NEXT_COMMAOPT, s_prepare_fnc} };
static SYNTAX s_print[] = { {RULE_PRINTSRC + (RULE_PRCC << 8) + ((INT) RULE_PFILE << 16) + NEXT_SEMILISTNOEND, s_print_fnc},
				{RULE_PRINTSRC + (RULE_PRCC << 8) + NEXT_SEMILIST, s_print_fnc}};
static SYNTAX s_read[] = { RULE_AFIFILE + NEXT_COMMA, s_read_fnc };
static SYNTAX s_readks[] = { RULE_IFILE + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_CNDATADEST + (RULE_RDCC << 8) + ((INT) RULE_EMPTYLIST << 16) +
				NEXT_SEMILIST, s_next_semilist_fnc };
static SYNTAX s_replace[] = { RULE_CSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_CVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_reposit[] = { RULE_AFIFILE + NEXT_COMMA, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_splclose[] = { RULE_CSRC + (RULE_PFILE << 8) + ((INT) RULE_OPTIONAL << 16) +
						 NEXT_COMMAOPT, s_splclose_fnc };
static SYNTAX s_search[] = { RULE_CSRC + (RULE_CNVAR << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_CNVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_splopen[] = { RULE_CSRC + (RULE_PFILE << 8) + NEXT_COMMAOPT, s_splopen_fnc,
				RULE_CSRC + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_store[] = { RULE_LOADSRC + (RULE_IMAGE << 8) + NEXT_PREP, s_load_fnc };
static SYNTAX s_trap[] = { RULE_LBLSRC + NEXT_NOPARSE, s_trap_fnc };
static SYNTAX s_trapclr[] = { RULE_NOPARSE + NEXT_NOPARSE, s_trapclr_fnc };
static SYNTAX s_type[] = { RULE_ANYVAR + NEXT_PREPOPT, s_type_fnc,
				RULE_NVAR + NEXT_LIST, s_type_fnc };
static SYNTAX s_shutdown[] = { RULE_NOPARSE + NEXT_ENDOPT, (RULEFNC) NULL };
static SYNTAX s_unpack[] = { RULE_CNSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_CNDATADEST + NEXT_LIST, s_next_list_fnc	};
static SYNTAX s_update[] = { RULE_AFIFILE + NEXT_SEMICOLON, s_update_fnc,
				RULE_CNDATASRC + (RULE_WTCC << 8) + NEXT_SEMILIST, s_update_fnc };
static SYNTAX s_write[] = { RULE_AFIFILE + NEXT_COMMASEMI, s_write_fnc,
				RULE_CNDATASRC + (RULE_WTCC << 8) + NEXT_SEMILIST, s_write_fnc };
static SYNTAX s_comopen[] = { RULE_COMFILE + NEXT_COMMA, (RULEFNC) NULL,
				RULE_CSRC + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_comclose[] = { RULE_COMFILE + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_comctl[] = { RULE_COMFILE + NEXT_COMMA, (RULEFNC) NULL,
				RULE_CVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_comclr[] = { RULE_COMFILE + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_comwait[] = { RULE_PARSEVAR + NEXT_END, s_comwait_fnc };
static SYNTAX s_send[] = { RULE_COMFILE + NEXT_COMMA, (RULEFNC) NULL,
				RULE_NVAR + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_CNDATASRC + (RULE_CHGCC << 8) + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_recv[] = { RULE_COMFILE + NEXT_COMMA, (RULEFNC) NULL,
				RULE_NVAR + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_CNDATADEST + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_nformat[] = { RULE_NARRAYELEMENT + (RULE_NVAR << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_NVAR + (RULE_DCON << 8) + NEXT_PREP, s_nformat_fnc,
				RULE_NVAR + (RULE_DCON << 8) + NEXT_END, s_nformat_fnc };
static SYNTAX s_sformat[] = { RULE_CARRAYELEMENT + (RULE_CVAR << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_NVAR + (RULE_DCON << 8) + NEXT_END, s_sformat_fnc };
static SYNTAX s_check10[] = { RULE_CVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_CSRC + NEXT_COMMAOPT, s_check10_fnc,
				RULE_CVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_and[] = { RULE_CNSRCXCON + NEXT_PREP, s_and_fnc,
				RULE_CNVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_unload[] = { RULE_CSRC + (RULE_OPTIONAL << 8) + NEXT_END, s_unload_fnc };
static SYNTAX s_sound[] = { RULE_NSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_NSRC + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_flush[] = { RULE_AFIFILE + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_console[] = { RULE_CNDATASRC + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_retcount[] = { RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_loadadr[] = { RULE_ANYVARPTR + NEXT_PREP, (RULEFNC) NULL,
				RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_ANYVAR + (RULE_LIST << 8) + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_storeadr[] = { RULE_ANYVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_ANYVARPTR + (RULE_LIST << 8) + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_fill[] = { RULE_CVAR + (RULE_SLITERAL << 8) + NEXT_PREP, s_fill_fnc,
				RULE_CDATADEST + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_movesize[] = { RULE_CNVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_sqrt[] = { RULE_NSRC + NEXT_PREPGIVING, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_chop[] = { RULE_CSRC + NEXT_PREP, (RULEFNC) NULL,
						RULE_CVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_aimdex[] = { RULE_CSRC + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_erase[] = { RULE_CSRC + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_rename[] = { RULE_CSRC + NEXT_PREP, (RULEFNC) NULL,
					    RULE_CSRC + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getcolor[] = { RULE_IMAGE + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getposition[] = { RULE_IMAGE + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_NVAR + NEXT_COMMA, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_setflag[] = { RULE_PARSEUP + NEXT_REPEAT, s_setflag_fnc };
static SYNTAX s_sqlexec[] = { RULE_CSRC + NEXT_PREPOPT, s_sqlexec_fnc };
static SYNTAX s_getparm[] = { RULE_CVARPTR + NEXT_COMMAOPT, s_getparm_fnc,
				RULE_ANYVARPTR + NEXT_LIST, s_getparm_fnc };
static SYNTAX s_loadparm[] = { RULE_NVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_CVARPTR + NEXT_COMMAOPT, s_getparm_fnc,
				RULE_CNVVARPTR + (RULE_VVARPTR << 8) + NEXT_LIST, s_getparm_fnc };
static SYNTAX s_movelv[] = { RULE_LBLSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_VVARPTR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_movevl[] = { RULE_VVARPTR + NEXT_PREP, (RULEFNC) NULL,
				RULE_LBLDEST + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_movelabel[] = { RULE_LBLSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_LBLDEST + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_loadlabel[] = { RULE_LBLDEST + NEXT_PREP, (RULEFNC) NULL,
				RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_LBLSRC + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_storelabel[] = { RULE_LBLSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_LBLDEST + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_unpacklist[] = { RULE_LIST + NEXT_PREP, (RULEFNC) NULL,
				RULE_ANYVARPTR + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_clearadr[] = { RULE_ANYVARPTR + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_compareadr[] = { RULE_ANYVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_ANYVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_testadr[] = { RULE_ANYVARPTR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getcursor[] = { RULE_NVAR + NEXT_PREP, (RULEFNC) NULL,
				RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_show[] = { RULE_IMAGE + (RULE_RESOURCE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_DEVICE + NEXT_PREPOPT, s_show_fnc, RULE_NSRC + NEXT_PREP,
				(RULEFNC) NULL, RULE_NSRC + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_hide[] = { RULE_IMAGE + (RULE_RESOURCE << 8) + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_unlink[] = { RULE_DEVICE + (RULE_RESOURCE << 8) + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_change[] = { RULE_DEVICE + (RULE_RESOURCE << 8) + NEXT_COMMA, (RULEFNC) NULL,
				RULE_CSRC + NEXT_SEMIOPT, s_next_list_fnc,
				RULE_CNDATASRC + (RULE_CHGCC << 8) + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_query[] = { RULE_DEVICE + (RULE_RESOURCE << 8) + NEXT_COMMA, (RULEFNC) NULL,
				RULE_CSRC + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_CNDATADEST + NEXT_LIST, s_next_list_fnc	};
static SYNTAX s_get[] = { RULE_QUEUE + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_CNDATADEST + NEXT_LIST, s_next_list_fnc	};
static SYNTAX s_put[] = { RULE_QUEUE + NEXT_SEMICOLON, (RULEFNC) NULL,
				RULE_CNDATASRC + (RULE_CHGCC << 8) + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_wait[] = { RULE_PARSEVAR + NEXT_LIST, s_wait_fnc };
static SYNTAX s_draw[] = { RULE_IMAGE + NEXT_SEMICOLON, s_draw_fnc };
static SYNTAX s_link[] = { RULE_DEVICE + (RULE_RESOURCE << 8) + NEXT_PREP, (RULEFNC) NULL,
				RULE_QUEUE + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_empty[] = { RULE_QUEUE + NEXT_LIST, s_next_list_fnc };
static SYNTAX s_makevar[] = { RULE_CSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_MAKEVARDEST + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getlabel[] = { RULE_CSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_LBLDEST + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_testlabel[] = { RULE_LBLVAREXT + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getmodules[] = { RULE_CARRAY + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getprinters[] = { RULE_CARRAY + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getreturnmodules[] = { RULE_CARRAY + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getpaperbins[] = { RULE_CSRC + NEXT_COMMA, (RULEFNC) NULL,
				RULE_CARRAY + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_getpapernames[] = { RULE_CSRC + NEXT_COMMA, (RULEFNC) NULL,
				RULE_CARRAY + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_format[] = { RULE_CNSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_CSRC + NEXT_PREP, (RULEFNC) NULL,
				RULE_CVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_return[] = { RULE_NOPARSE + NEXT_IFOPT, s_return_fnc };
static SYNTAX s_routine[] = { RULE_PARSEVAR + NEXT_NOPARSE, s_routine_fnc };
static SYNTAX s_lroutine[] = { RULE_PARSEVAR + NEXT_NOPARSE, s_routine_fnc };
static SYNTAX s_call[] = { RULE_PARSEVAR + NEXT_NOPARSE, s_call_fnc };
static SYNTAX s_goto[] = { RULE_LBLGOTO + NEXT_IFOPT, s_goto_fnc };
static SYNTAX s_for[] = { RULE_NVAR + NEXT_PREP, s_for_fnc };
static SYNTAX s_loop[] = { RULE_PARSEUP + NEXT_NOPARSE, s_loop_fnc };
static SYNTAX s_break[] = { RULE_NOPARSE + NEXT_IFOPT, s_break_fnc };
static SYNTAX s_repeat[] = { RULE_PARSEUP + NEXT_NOPARSE, s_repeat_fnc };
static SYNTAX s_while[] = { RULE_NOPARSE + NEXT_NOPARSE, s_while_fnc };
static SYNTAX s_if[] = { RULE_NOPARSE + NEXT_NOPARSE, s_if_fnc };
static SYNTAX s_else[] = { RULE_NOPARSE + NEXT_IFOPT, s_else_fnc };
static SYNTAX s_endif[] = { RULE_NOPARSE + NEXT_ENDOPT, s_endif_fnc };
static SYNTAX s_switch[] = { RULE_CNSRC + NEXT_END, s_switch_fnc };
static SYNTAX s_case[] = { RULE_NOPARSE + NEXT_NOPARSE, s_case_fnc };
static SYNTAX s_endswitch[] = { RULE_NOPARSE + NEXT_ENDOPT, s_endswitch_fnc };
static SYNTAX s_tabpage[] = { RULE_NOPARSE + NEXT_ENDOPT, s_tabpage_fnc };
static SYNTAX s__if[] = { {RULE_NOPARSE + NEXT_NOPARSE, s__conditional_fnc},
				{RULE_XCON + (RULE_SLITERAL << 8) + NEXT_RELOP, s__if_fnc},
				{RULE_XCON + (RULE_SLITERAL << 8) + NEXT_END, s__if_fnc} };
static SYNTAX s__ifz[] = { RULE_NOPARSE + NEXT_NOPARSE, s__conditional_fnc,
				RULE_XCON + NEXT_LIST, s__ifz_fnc };
static SYNTAX s__ifdef[] = { RULE_NOPARSE + NEXT_NOPARSE, s__conditional_fnc,
				RULE_PARSEVAR + NEXT_LIST, s__ifdef_fnc };
static SYNTAX s_ifeq[] = { RULE_NOPARSE + NEXT_NOPARSE, s__conditional_fnc,
				RULE_XCON + NEXT_COMMA, s_ifeq_fnc,
				RULE_XCON + NEXT_END, s_ifeq_fnc };
static SYNTAX s__else[] = { RULE_NOPARSE + NEXT_ENDOPT, s__conditional_fnc };
static SYNTAX s_define[] = { RULE_NOPARSE + NEXT_NOPARSE, s_define_fnc };
static SYNTAX s_equate[] = { RULE_XCON + (RULE_SLITERAL << 8) + NEXT_END, s_equate_fnc };
static SYNTAX s_include[] = { {RULE_NOPARSE + NEXT_NOPARSE, s_include_fnc} };
static SYNTAX s_liston[] = { {RULE_NOPARSE + NEXT_ENDOPT, s_liston_fnc} };
static SYNTAX s_verb[] = { {RULE_PARSEUP + NEXT_NOPARSE, s_uverb_fnc} };
static SYNTAX s_charnum[] = { RULE_PARSEVAR + NEXT_NOPARSE, s_charnum_fnc };
static SYNTAX s_init[] = { RULE_XCON + (RULE_LITERAL << 8) +
					  ((INT) RULE_ASTERISK << 16) + NEXT_LIST, s_init_fnc };
static SYNTAX s_list[] = { RULE_PARSEUP + NEXT_NOPARSE, s_list_fnc };
static SYNTAX s_varlist[] = { RULE_PARSEVAR + NEXT_NOPARSE, s_varlist_fnc };
static SYNTAX s_file[] = { RULE_PARSEUP + NEXT_REPEAT, s_file_fnc };
static SYNTAX s_pfile[] = { RULE_PARSE + NEXT_NOPARSE, s_pfile_fnc };
static SYNTAX s_label[] = { RULE_NOPARSE + NEXT_ENDOPT, s_label_fnc };
static SYNTAX s_getendkey[] = { RULE_NVAR + NEXT_END, (RULEFNC) NULL };
static SYNTAX s_setendkey[] = { RULE_PARSEVAR + NEXT_NOPARSE, s_setendkey_fnc };
static SYNTAX s_record[] = { RULE_PARSEUP + NEXT_NOPARSE, s_record_fnc };
static SYNTAX s_getwindow[] = { {RULE_NVAR + NEXT_COMMA, (RULEFNC) NULL},
						 { RULE_NVAR + NEXT_COMMA, (RULEFNC) NULL},
						 { RULE_NVAR + NEXT_COMMA, (RULEFNC) NULL},
						 { RULE_NVAR + NEXT_END, (RULEFNC) NULL} };

#define v85 0x5500
#define v88 0x5800

/* the verbs hash codes are created according to this formula: */
/* (<1st char> + <2nd char> + 47 * <last char> + 2 * <length>) modulo 128 */

static VERB v_call         = { NULL, 112, &s_call[0], "CALL" };  /* code   0 */
static VERB v_setendkey	  = { NULL, v88 + 211, &s_setendkey[0], "SETENDKEY" }; /* code	1 */
static VERB v_xif		  = { &v_setendkey, 178, &s__else[0], "XIF" }; /* code   1 */
static VERB v_filepi	  = { NULL, 28, &s_filepi[0], "FILEPI" };  /* code   2 */
static VERB v_popreturn	  = { NULL, v88 + 230, &s_clearlabel[0], "POPRETURN" }; /* code 3 */
static VERB v_iflt         = { &v_popreturn, 172, &s_ifeq[0], "IFLT" };  /* code 3 */
static VERB v_ifgt         = { &v_iflt, 171, &s_ifeq[0], "IFGT" };  /* code 3 */
static VERB v_splopen      = { &v_ifgt, 70, &s_splopen[0], "SPLOPEN" };  /* code 3 */
static VERB v_default      = { &v_splopen, 141, &s_endswitch[0], "DEFAULT" };  /* code 3 */
static VERB v_pi		  = { NULL, 28, &s_pi[0], "PI" };  /* code   4 */
static VERB v_gint         = { &v_pi, 221, &s_charnum[0], "GINT" };  /* code   4 */
static VERB v_ccall        = { &v_gint, 22, &s_ccall[0], "CCALL" };  /* code   4 */
static VERB v_gfloat       = { NULL, 222, &s_charnum[0], "GFLOAT" };  /* code   5 */
static VERB v_query        = { NULL, v88 + 177, &s_query[0], "QUERY" };  /* code   7 */
static VERB v_charrest     = { &v_query, v88 + 138, &s_chain[0], "CHARREST" };  /* code   7 */
static VERB v_add          = { &v_charrest, 2, &s_add[0], "ADD" };  /* code   7 */
static VERB v_float        = { NULL, 204, &s_charnum[0], "FLOAT" };  /* code   8 */
static VERB v_count        = { &v_float, 27, &s_count[0], "COUNT" };  /* code   8 */
static VERB v_object	  = { NULL, 233, &s_object[0], "OBJECT" }; /* code 9 */
static VERB v_lenset       = { &v_object, 33, &s_endset[0], "LENSET" };  /* code   9 */
static VERB v_multiply     = { &v_lenset, 41, &s_add[0], "MULTIPLY" };  /* code   9 */
static VERB v_list         = { &v_multiply, 207, &s_list[0], "LIST" };  /* code   9 */
static VERB v_int          = { &v_list, 203, &s_charnum[0], "INT" };  /* code   9 */
static VERB v_comtst       = { NULL, v85 + 5, &s_comclr[0], "COMTST" };  /* code  10 */
static VERB v_getobject    = { &v_comtst, v88 + 235, &s_getobject[0], "GETOBJECT" };  /* code  10 */
static VERB v_set          = { &v_getobject, 89, &s_clear[0], "SET" };  /* code  10 */
static VERB v_pushreturn	  = { NULL, v88 + 229, &s_pushreturn[0], "PUSHRETURN" }; /* code 11 */
static VERB v_ifnl         = { &v_pushreturn, 173, &s_ifeq[0], "IFNL" };  /* code  11 */
static VERB v_label        = { &v_ifnl, 229, &s_label[0], "LABEL" };  /* code  11 */
static VERB v_fill		  = { &v_label, v88 + 37, &s_fill[0], "FILL" };	/* code  11 */
static VERB v_endset       = { &v_fill, 24, &s_endset[0], "ENDSET" };  /* code  11 */
static VERB v_init         = { &v_endset, 205, &s_init[0], "INIT" };  /* code  11 */
static VERB v_comwait      = { NULL, v85 + 6, &s_comwait[0], "COMWAIT" };  /* code  12 */
static VERB v_wait         = { &v_comwait, v88 + 181, &s_wait[0], "WAIT" };  /* code  12 */
static VERB v_test         = { NULL, 49, &s_test[0], "TEST" };  /* code  13 */
static VERB v_format	  = { &v_test, v88 + 210, &s_format[0], "FORMAT" };  /* code  13 */
static VERB v_reset 	  = { &v_format, 62, &s_reset[0], "RESET" };	/* code	13 */
static VERB v_flagrest     = { NULL, v88 + 20, &s_chain[0], "FLAGREST" };  /* code  14 */
static VERB v_nformat      = { &v_flagrest, 86, &s_nformat[0], "NFORMAT" };  /* code  14 */
static VERB v_fposit       = { &v_nformat, 29, &s_fposit[0], "FPOSIT" };  /* code  14 */
static VERB v_not          = { NULL, v88 + 8, &s_and[0], "NOT" };  /* code  15 */
static VERB v_insert       = { &v_not, 31, &s_insert[0], "INSERT" };  /* code  15 */
static VERB v_repeat       = { &v_insert, 131, &s_repeat[0], "REPEAT" };  /* code  15 */
static VERB v_gobject	  = { NULL, 234, &s_object[0], "GOBJECT" }; /* code 16 */
static VERB v_getlabel	  = { &v_gobject, v88 + 204, &s_getlabel[0], "GETLABEL" };  /* code  16 */
static VERB v_clockclient  = {NULL, 36, &s_clockclient[0], "CLOCKCLIENT" };   /* code  17 */
static VERB v_and          = { &v_clockclient, v88 + 6, &s_and[0], "AND" };  /* code  17 */
static VERB v_varlist      = { &v_and, 209, &s_varlist[0], "VARLIST" };  /* code  17 */
static VERB v_reposit      = { &v_varlist, 60, &s_reposit[0], "REPOSIT" };  /* code  17 */
static VERB v_comctl       = { NULL, v85 + 3, &s_comctl[0], "COMCTL" };  /* code  18 */
static VERB v_getglobal	  = { &v_comctl, v88 + 203, &s_makevar[0], "GETGLOBAL" };  /* code  18 */
static VERB v_scrnrest     = { &v_getglobal, v88 + 85, &s_endset[0], "SCRNREST" };  /* code  18 */
static VERB v_reformat	  = { NULL, v88 + 102, &s_aimdex[0], "REFORMAT" };	/* code	19 */
static VERB v_exist        = { &v_reformat, v88 + 105, &s_aimdex[0], "EXIST" };  /* code  19 */
static VERB v_closeall	  = { &v_exist, v88 + 208, &s_beep[0], "CLOSEALL" };  /* code  19 */
static VERB v_sformat      = { &v_closeall, 87, &s_sformat[0], "SFORMAT" };  /* code  19 */
static VERB v_retcount     = { &v_sformat, v88 + 23, &s_retcount[0], "RETCOUNT" };  /* code  19 */
static VERB v_clientrollout = { NULL, v88 + 240, &s_chain[0], "CLIENTROLLOUT" };  /* code  21 */
static VERB v_ck11		  = { &v_clientrollout, v88 + 3, &s_check10[0], "CK11" };  /* code	21 */
static VERB v_sort         = { NULL, v88 + 104, &s_aimdex[0], "SORT" };  /* code  22 */
static VERB v_makeglobal   = { &v_sort, v88 + 202, &s_makevar[0], "MAKEGLOBAL" };  /* code  22 */
static VERB v_mult         = { &v_makeglobal, 41, &s_add[0], "MULT" };  /* code  22 */
static VERB v_clearlabel   = { NULL, v88 + 228, &s_clearlabel[0], "CLEARLABEL" }; /* code 23 */
static VERB v_noeject      = { &v_clearlabel, v88 + 29, &s_beep[0], "NOEJECT" };  /* code 23 */
static VERB v_put		  = { &v_noeject, v88 + 180, &s_put[0], "PUT" };	/* code 23 */
static VERB v_draw         = { &v_put, v88 + 183, &s_draw[0], "DRAW" };  /* code 23 */
static VERB v_getwindow	  = { &v_draw, v88 + 214, &s_getwindow[0], "GETWINDOW" }; /* code 23 */
static VERB v_sqrt		  = { NULL, v88 + 50, &s_sqrt[0], "SQRT" };	/* code  24 */
static VERB v_check11	  = { &v_sqrt, v88 + 3, &s_check10[0], "CHECK11" };  /* code  24 */
static VERB v_print        = { &v_check11, 51, &s_print[0], "PRINT" };  /* code  24 */
static VERB v_xcall		  = { NULL, 208, &s_xcall[0], "XCALL" }; /* code 25 */
static VERB v_append       = { &v_xcall, 3, &s_append[0], "APPEND" };  /* code  25 */
static VERB v_setnull	  = { NULL, v88 + 233, &s_setnull[0], "SETNULL" };	/* code 26 */
static VERB v_method	  = { &v_setnull, 235, &s_method[0], "METHOD" };  /* code 26 */
static VERB v_winrest      = { &v_method, v88 + 17, &s_endset[0], "WINREST" };  /* code 26 */
static VERB v_splopt       = { NULL, v88 + 14, &s_chain[0], "SPLOPT" };  /* code  27 */
static VERB v_rollout      = { &v_splopt, 46, &s_chain[0], "ROLLOUT" };  /* code  27 */
static VERB v_ifz          = { &v_rollout, 162, &s__ifz[0], "IFZ" };  /* code  27 */
static VERB v_read         = { &v_ifz, 52, &s_read[0], "READ" };  /* code  27 */
static VERB v_send         = { NULL, v85 + 8, &s_send[0], "SEND" };  /* code  28 */
static VERB v_movevl       = { &v_send, v88 + 145, &s_movevl[0], "MOVEVL" };  /* code  28 */
static VERB v_debug        = { &v_movevl, 17, &s_beep[0], "DEBUG" };  /* code  28 */
static VERB v_show         = { &v_debug, v88 + 172, &s_show[0], "SHOW" };  /* code  28 */
static VERB v_build        = { NULL, v88 + 93, &s_aimdex[0], "BUILD" };  /* code  29 */
static VERB v_ifnz         = { &v_build, 163, &s__ifz[0], "IFNZ" };  /* code  29 */
static VERB v_mod		  = { NULL, v88 + 13, &s_add[0], "MOD" };	/* code	30 */
static VERB v_testlabel	  = { NULL, v88 + 205, &s_testlabel[0], "TESTLABEL" };	/* code  31 */
static VERB v__else 	  = { &v_testlabel, 176, &s__else[0], "%ELSE" };	/* code  31 */
static VERB v_listend      = { &v__else, 208, &s_list[0], "LISTEND" };  /* code  31 */
static VERB v_load         = { &v_listend, 34, &s_load[0], "LOAD" };  /* code  31 */
static VERB v_goto		  = { &v_load, 116, &s_goto[0], "GOTO" };  /* code  31 */
static VERB v_record	  = { &v_goto, 231, &s_record[0], "RECORD" }; /* code 31 */
static VERB v_ifng		  = { NULL, 174, &s_ifeq[0], "IFNG" };	/* code	32 */
static VERB v_putfirst	  = { NULL, v88 + 207, &s_put[0], "PUTFIRST" };  /* code  33 */
static VERB v_loadlabel    = { &v_putfirst, v88 + 147, &s_loadlabel[0], "LOADLABEL" };  /* code  33 */
static VERB v_until        = { &v_loadlabel, 135, &s_while[0], "UNTIL" };  /* code  33 */
static VERB v_external     = { &v_until, 230, &s_label[0], "EXTERNAL" };  /* code  33 */
static VERB v_char         = { &v_external, 201, &s_charnum[0], "CHAR" };  /* code  33 */
static VERB v_gchar        = { NULL, 219, &s_charnum[0], "GCHAR" };  /* code  34 */
static VERB v_movelabel    = { &v_gchar, v88 + 146, &s_movelabel[0], "MOVELABEL" };  /* code  34 */
static VERB v_traprest     = { &v_movelabel, v88 + 28, &s_endset[0], "TRAPREST" };  /* code  34 */
static VERB v_unpacklist   = { NULL, v88 + 150, &s_unpacklist[0], "UNPACKLIST" };  /* code  35 */
static VERB v_subtract     = { NULL, 74, &s_add[0], "SUBTRACT" };  /* code  36 */
static VERB v_staterest    = { NULL, v88 + 136, &s_endset[0], "STATEREST" };  /* code  37 */
static VERB v_loadmod      = { &v_staterest, v88 + 9, &s_chain[0], "LOADMOD" };  /* code  37 */
static VERB v_extend	  = { &v_loadmod, 26, &s_bump[0], "EXTEND" };  /* code  37 */
static VERB v_recordend	  = { &v_extend, 232, &s_record[0], "RECORDEND" }; /* code 37 */
static VERB v_clear        = { NULL, 10, &s_clear[0], "CLEAR" };  /* code  39 */
static VERB v_ploadmod	  = { NULL, v88 + 206, &s_chain[0], "PLOADMOD" };	/* code  40 */
static VERB v_sound 	  = { &v_ploadmod, v88 + 12, &s_sound[0], "SOUND" };  /* code  40 */
static VERB v_for		  = { NULL, 129, &s_for[0], "FOR" };	/* code	41 */
static VERB v_log          = { NULL, v88 + 48, &s_sqrt[0], "LOG" };  /* code  42 */
static VERB v_makevar	  = { &v_log, v88 + 201, &s_makevar[0], "MAKEVAR" };  /* code  42 */
static VERB v_getcolor     = { &v_makevar, v88 + 110, &s_getcolor[0], "GETCOLOR" };  /* code  42 */
static VERB v_unload       = { NULL, v88 + 11, &s_unload[0], "UNLOAD" };  /* code  43 */
static VERB v_character    = { &v_unload, 201, &s_charnum[0], "CHARACTER" };  /* code  43 */
static VERB v_var          = { &v_character, 206, &s_charnum[0], "VAR" };  /* code  43 */
static VERB v_comclr       = { NULL, v85 + 4, &s_comclr[0], "COMCLR" };  /* code  44 */
static VERB v_getcursor    = { &v_comclr, v88 + 160, &s_getcursor[0], "GETCURSOR" };  /* code  44 */
static VERB v_readkg       = { &v_getcursor, 35, &s_readkg[0], "READKG" };  /* code  44 */
static VERB v_clearadr	  = { NULL, v88 + 151, &s_clearadr[0], "CLEARADR" };  /* code  45 */
static VERB v_ginteger	  = { NULL, 221, &s_charnum[0], "GINTEGER" };	/* code 46 */
static VERB v_storelabel   = { NULL, v88 + 148, &s_storelabel[0], "STORELABEL" };	/* code	47 */
static VERB v_setflag      = { &v_storelabel, v88 + 120, &s_setflag[0], "SETFLAG" };  /* code  47 */
static VERB v_gchararacter = { NULL, 219, &s_charnum[0], "GCHARARACTER" };  /* code  48 */
static VERB v_gnumber      = { NULL, 220, &s_charnum[0], "GNUMBER" };  /* code  49 */
static VERB v_recvclr	  = { NULL, v85 + 11, &s_comclose[0], "RECVCLR" }; /* code 51 */
static VERB v_or		  = { &v_recvclr, v88 + 5, &s_and[0], "OR" };	/* code  51 */
static VERB v_integer	  = { &v_or, 203, &s_charnum[0], "INTEGER" };  /* code  51 */
static VERB v_sendclr	  = { NULL, v85 + 10, &s_comclose[0], "SENDCLR" }; /* code 52 */
static VERB v_compareadr   = { &v_sendclr, v88 + 152, &s_compareadr[0], "COMPAREADR" };	/* code 52 */
static VERB v_setlptr	  = { &v_compareadr, 67, &s_reset[0], "SETLPTR" };	/* code  52 */
static VERB v_testadr      = { NULL, v88 + 153, &s_testadr[0], "TESTADR" };  /* code  53 */
static VERB v_gdim         = { NULL, 219, &s_charnum[0], "GDIM" };  /* code  54 */
static VERB v_dim          = { &v_gdim, 201, &s_charnum[0], "DIM" };  /* code  54 */
static VERB v_power 	  = { NULL, v88 + 52, &s_power[0], "POWER" };	/* code	55 */
static VERB v_loadadr      = { &v_power, v88 + 25, &s_loadadr[0], "LOADADR" };  /* code  55 */
static VERB v_case         = { &v_loadadr, 140, &s_case[0], "CASE" };  /* code  55 */
static VERB v_equ          = { &v_case, 194, &s_equate[0], "EQU" };  /* code  55 */
static VERB v_moveadr      = { NULL, 42, &s_moveadr[0], "MOVEADR" };  /* code  56 */
static VERB v_sqlmsg	  = { NULL, v88 + 130, &s_endset[0], "SQLMSG" };	/* code  57 */
static VERB v_gform 	  = { NULL, 220, &s_charnum[0], "GFORM" };	/* code	58 */
static VERB v_movelptr     = { &v_gform, 40, &s_movefptr[0], "MOVELPTR" };  /* code  58 */
static VERB v_movefptr     = { &v_movelptr, 39, &s_movefptr[0], "MOVEFPTR" };  /* code  58 */
static VERB v_xor          = { NULL, v88 + 7, &s_and[0], "XOR" };  /* code  59 */
static VERB v_afile        = { NULL, 212, &s_file[0], "AFILE" };  /* code  60 */
static VERB v_getparm      = { NULL, v88 + 141, &s_getparm[0], "GETPARM" };  /* code  61 */
static VERB v_number       = { &v_getparm, 202, &s_charnum[0], "NUMBER" };  /* code  61 */
static VERB v_aimdex       = { NULL, v88 + 92, &s_aimdex[0], "AIMDEX" };  /* code  62 */
static VERB v_beep         = { NULL, 4, &s_beep[0], "BEEP" };  /* code  63 */
static VERB v_gnum         = { NULL, 220, &s_charnum[0], "GNUM" };  /* code  64 */
static VERB v_define       = { &v_gnum, 193, &s_define[0], "DEFINE" };  /* code  64 */
static VERB v_device       = { &v_define, 215, &s_pfile[0], "DEVICE" };  /* code  64 */
static VERB v_delete       = { &v_device, 18, &s_delete[0], "DELETE" };  /* code  64 */
static VERB v_form         = { &v_delete, 202, &s_charnum[0], "FORM" };  /* code  64 */
static VERB v_cverb        = { NULL, 199, &s_verb[0], "CVERB" };  /* code  65 */
static VERB v_make		  = { &v_cverb, v88 + 226, &s_make[0], "MAKE" }; /* code 65 */
static VERB v_verb         = { &v_make, 198, &s_verb[0], "VERB" };  /* code  65 */
static VERB v_change       = { NULL, v88 + 176, &s_change[0], "CHANGE" };  /* code  66 */
static VERB v_ifne         = { &v_change, 170, &s_ifeq[0], "IFNE" };  /* code  66 */
static VERB v_ifle         = { &v_ifne, 174, &s_ifeq[0], "IFLE" };  /* code  66 */
static VERB v_ifge         = { &v_ifle, 173, &s_ifeq[0], "IFGE" };  /* code  66 */
static VERB v_trapclr      = { &v_ifge, 76, &s_trapclr[0], "TRAPCLR" };  /* code  66 */
static VERB v_file         = { &v_trapclr, 210, &s_file[0], "FILE" };  /* code  66 */
static VERB v_chop         = { NULL, v88 + 60, &s_chop[0], "CHOP" };  /* code  67 */
static VERB v_readtab	  = { &v_chop, 52, &s_read[0], "READTAB" };	/* code  67 */
static VERB v_ifile        = { NULL, 211, &s_file[0], "IFILE" };  /* code  68 */
static VERB v_gdevice      = { &v_ifile, 225, &s_pfile[0], "GDEVICE" };  /* code  68 */
static VERB v_hide		  = { &v_gdevice, v88 + 219, &s_hide[0], "HIDE" };	/* code	68 */
static VERB v_close        = { &v_hide, 1, &s_close[0], "CLOSE" };  /* code  68 */
static VERB v_divide	   = { &v_close, 21, &s_add[0], "DIVIDE" };	/* code  68 */
static VERB v_else         = { &v_divide, 137, &s_else[0], "ELSE" };  /* code  68 */
static VERB v_getname	  = { NULL, v88 + 231, &s_getname[0], "GETNAME" }; /* code 69 */
static VERB v_storeadr     = { &v_getname, v88 + 26, &s_storeadr[0], "STOREADR" };  /* code  69 */
static VERB v_readgptb     = { &v_storeadr, v88 + 199, &s_readkg[0], "READGPTB" };  /* code  69 */
static VERB v_readkgtb     = { &v_readgptb, 35, &s_readkg[0], "READKGTB" };  /* code  69 */
static VERB v_readkptb     = { &v_readkgtb, 54, &s_readks[0], "READKPTB" };  /* code  69 */
static VERB v_readkstb     = { &v_readkptb, 56, &s_readks[0], "READKSTB" };  /* code  69 */
static VERB v_cmove        = { &v_readkstb, 14, &s_cmove[0], "CMOVE" };  /* code  69 */
static VERB v_abs          = { NULL, v88 + 51, &s_sqrt[0], "ABS" };  /* code  70 */
static VERB v_lcmove       = { &v_abs, v88 + 58, &s_chop[0], "LCMOVE" };  /* code  70 */
static VERB v_disable      = { &v_lcmove, v88 + 166, &s_beep[0], "DISABLE" };  /* code  70 */
static VERB v_charsave     = { &v_disable, v88 + 137, &s_endset[0], "CHARSAVE" };  /* code  70 */
static VERB v_pause        = { &v_charsave, 48, &s_pause[0], "PAUSE" };  /* code  70 */
static VERB v_perform      = { &v_pause, v88 + 24, &s_branch[0], "PERFORM" };  /* code  70 */
static VERB v_gimage       = { NULL, 227, &s_file[0], "GIMAGE" };  /* code  71 */
static VERB v_save         = { &v_gimage, v88 + 16, &s_endset[0], "SAVE" };  /* code  71 */
static VERB v_index        = { NULL, v88 + 100, &s_aimdex[0], "INDEX" };  /* code  73 */
static VERB v_encode       = { NULL, v88 + 97, &s_aimdex[0], "ENCODE" };  /* code  74 */
static VERB v_enable       = { &v_encode, v88 + 165, &s_beep[0], "ENABLE" };  /* code  74 */
static VERB v_comp         = { &v_enable, 16, &s_compare[0], "COMP" };  /* code  74 */
static VERB v_pfile        = { NULL, 213, &s_pfile[0], "PFILE" };  /* code  75 */
static VERB v_console      = { &v_pfile, v88 + 22, &s_console[0], "CONSOLE" };  /* code  75 */
static VERB v_comfile      = { &v_console, 214, &s_pfile[0], "COMFILE" };  /* code  75 */
static VERB v_image        = { &v_comfile, 217, &s_file[0], "IMAGE" };  /* code  75 */
static VERB v_compare      = { &v_image, 16, &s_compare[0], "COMPARE" };  /* code  75 */
static VERB v_resetparm    = { NULL, v88 + 149, &s_beep[0], "RESETPARM" };  /* code  76 */
static VERB v_create       = { &v_resetparm, v88 + 95, &s_aimdex[0], "CREATE" };  /* code  76 */
static VERB v_erase 	  = { &v_create, v88 + 107, &s_erase[0], "ERASE" };	/* code	76 */
static VERB v_charrestore  = { &v_erase, v88 + 138, &s_chain[0], "CHARRESTORE" };  /* code  76 */
static VERB v_sub          = { &v_charrestore, 74, &s_add[0], "SUB" };  /* code  76 */
static VERB v_num          = { &v_sub, 202, &s_charnum[0], "NUM" };  /* code  76 */
static VERB v_comclose     = { NULL, v85 + 2, &s_comclose[0], "COMCLOSE" };  /* code  77 */
static VERB v_flagsave     = { &v_comclose, v88 + 19, &s_endset[0], "FLAGSAVE" };  /* code  77 */
static VERB v_equate       = { &v_flagsave, 194, &s_equate[0], "EQUATE" };  /* code  77 */
static VERB v_rep          = { &v_equate, 59, &s_replace[0], "REP" };  /* code  77 */
static VERB v_continue     = { &v_rep, 132, &s_break[0], "CONTINUE" };  /* code  77 */
static VERB v_tabpage      = { NULL, 143, &s_tabpage[0], "TABPAGE" };  /* code  78 */
static VERB v_gpfile       = { &v_tabpage, 224, &s_pfile[0], "GPFILE" };  /* code  78 */
static VERB v_rename	  = { &v_gpfile, v88 + 108, &s_rename[0], "RENAME" };	/* code  78 */
static VERB v_loadparm     = { &v_rename, v88 + 142, &s_loadparm[0], "LOADPARM" };  /* code  78 */
static VERB v__if          = { &v_loadparm, 164, &s__if[0], "%IF" };  /* code  78 */
static VERB v_gqueue       = { NULL, 228, &s_file[0], "GQUEUE" };  /* code  79 */
static VERB v_bump         = { &v_gqueue, 6, &s_bump[0], "BUMP" };  /* code  79 */
static VERB v_move		  = { &v_bump, 38, &s_move[0], "MOVE" };  /* code	79 */
static VERB v_restore      = { NULL, v88 + 17, &s_endset[0], "RESTORE" };  /* code  80 */
static VERB v_release      = { &v_restore, 58, &s_beep[0], "RELEASE" };  /* code  80 */
static VERB v_replace      = { &v_release, 59, &s_replace[0], "REPLACE" };  /* code  80 */
static VERB v__endif       = { &v_replace, 177, &s__else[0], "%ENDIF" };  /* code  80 */
static VERB v_include      = { &v__endif, 192, &s_include[0], "INCLUDE" };  /* code  80 */
static VERB v_match        = { &v_include, 37, &s_match[0], "MATCH" };  /* code  80 */
static VERB v_scrnsize     = { NULL, v88 + 132, &s_retcount[0], "SCRNSIZE" };  /* code  81 */
static VERB v_scrnsave     = { &v_scrnsize, v88 + 84, &s_endset[0], "SCRNSAVE" };  /* code  81 */
static VERB v_trim         = { &v_scrnsave, v88 + 241, &s_chop[0], "TRIM" };  /* code  81 */
static VERB v_updatab	  = { &v_trim, 81, &s_update[0], "UPDATAB" };	/* code	81 */
static VERB v_endroutine   = { NULL, 111, &s_routine[0], "ENDROUTINE" };  /* code  82 */
static VERB v_resource     = { &v_endroutine, 216, &s_pfile[0], "RESOURCE" };  /* code  82 */
static VERB v__elseif      = { &v_resource, 175, &s__if[0], "%ELSEIF" };  /* code  82 */
static VERB v_ifs          = { &v__elseif, 163, &s__ifz[0], "IFS" };  /* code  82 */
static VERB v_exp          = { NULL, v88 + 47, &s_sqrt[0], "EXP" };  /* code  83 */
static VERB v_writab	  = { &v_exp, 84, &s_write[0], "WRITAB" };	/* code  83 */
static VERB v_flagrestore  = { &v_writab, v88 + 20, &s_chain[0], "FLAGRESTORE" };	/* code  83 */
static VERB v_loop         = { &v_flagrestore, 130, &s_loop[0], "LOOP" };  /* code  83 */
static VERB v_readkp	  = { &v_loop, 54, &s_readks[0], "READKP" };	/* code	83 */
static VERB v_clientputfile = { NULL, v88 + 66, &s_rename[0], "CLIENTPUTFILE"};  /* code  84 */
static VERB v_clientgetfile = { &v_clientputfile, v88 + 65, &s_rename[0], "CLIENTGETFILE"};  /* code  84 */
static VERB v_flush        = { &v_clientgetfile, v88 + 18, &s_flush[0], "FLUSH" };  /* code  84 */
static VERB v__ifdef       = { &v_flush, 167, &s__ifdef[0], "%IFDEF" };  /* code  84 */
static VERB v_while        = { &v__ifdef, 134, &s_while[0], "WHILE" };  /* code  84 */
static VERB v_cmatch       = { &v_while, 12, &s_cmatch[0], "CMATCH" };  /* code  84 */
static VERB v_cos          = { NULL, v88 + 42, &s_sqrt[0], "COS" };  /* code  85 */
static VERB v_getpos	  = { &v_cos, v88 + 111, &s_getposition[0], "GETPOS" };  /* code  85 */
static VERB v_readkgp	  = { &v_getpos, v88 + 199, &s_readkg[0], "READKGP" }; /* code 85 */
static VERB v_gresource    = { NULL, 226, &s_pfile[0], "GRESOURCE" };  /* code  86 */
static VERB v_class		  = { &v_gresource, 236, &s_class[0], "CLASS" }; /* code 86 */
static VERB v_execute      = { &v_class, 25, &s_chain[0], "EXECUTE" };  /* code  86 */
static VERB v__ifndef      = { &v_execute, 168, &s__ifdef[0], "%IFNDEF" };  /* code  86 */
static VERB v_movesize     = { NULL, v88 + 39, &s_movesize[0], "MOVESIZE" };  /* code  87 */
static VERB v_scrnrestore  = { &v_movesize, v88 + 85, &s_endset[0], "SCRNRESTORE" };	/* code	87 */
static VERB v_rotate       = { NULL, v88 + 242, &s_rotate[0], "ROTATE" };  /* code  88 */
static VERB v_branch       = { &v_rotate, 5, &s_branch[0], "BRANCH" };  /* code  88 */
static VERB v_winsize      = { NULL, v88 + 131, &s_retcount[0], "WINSIZE" };  /* code  89 */
static VERB v_winsave      = { &v_winsize, v88 + 16, &s_endset[0], "WINSAVE" };  /* code  89 */
static VERB v_lroutine     = { &v_winsave, 110, &s_lroutine[0], "LROUTINE" };  /* code  89 */
static VERB v_routine      = { NULL, 110, &s_routine[0], "ROUTINE" };  /* code  90 */
static VERB v_prep         = { &v_routine, 50, &s_prepare[0], "PREP" };  /* code  90 */
static VERB v_queue        = { NULL, 218, &s_file[0], "QUEUE" };  /* code  91 */
static VERB v_prepare      = { &v_queue, 50, &s_prepare[0], "PREPARE" };  /* code  91 */
static VERB v_arccos       = { NULL, v88 + 45, &s_sqrt[0], "ARCCOS" };  /* code  92 */
static VERB v_search       = { &v_arccos, 66, &s_search[0], "SEARCH" };  /* code  92 */
static VERB v_deletek      = { &v_search, 19, &s_deletek[0], "DELETEK" };  /* code  92 */
static VERB v_update       = { &v_deletek, 80, &s_update[0], "UPDATE" };  /* code  92 */
static VERB v_store 	  = { &v_update, 72, &s_store[0], "STORE" };  /* code	92 */
static VERB v_div		  = { NULL, 21, &s_add[0], "DIV" };  /* code  93 */
static VERB v_getmodules   = { &v_div, v88 + 209, &s_getmodules[0], "GETMODULES" };	/* code  93 */
static VERB v_sqlcode      = { &v_getmodules, v88 + 129, &s_retcount[0], "SQLCODE" };  /* code  93 */
static VERB v_squeeze      = { &v_sqlcode, v88 + 59, &s_chop[0], "SQUEEZE" };  /* code  93 */
static VERB v_endswitch    = { &v_squeeze, 142, &s_endswitch[0], "ENDSWITCH" };  /* code  93 */
static VERB v_splclose     = { NULL, v88 + 15, &s_splclose[0], "SPLCLOSE" };  /* code  94 */
static VERB v_clock        = { &v_splclose, 11, &s_clock[0], "CLOCK" };  /* code  94 */
static VERB v_pack         = { &v_clock, 47, &s_pack[0], "PACK" };  /* code  94 */
static VERB v_trap         = { &v_pack, 75, &s_trap[0], "TRAP" };  /* code  94 */
static VERB v_write        = { &v_trap, 83, &s_write[0], "WRITE" };  /* code  94 */
static VERB v_getprinters  = { NULL, v88 + 237, &s_getprinters[0], "GETPRINTERS" };	/* code  95 */
static VERB v_winrestore   = { &v_getprinters, v88 + 17, &s_endset[0], "WINRESTORE" };  /* code  95 */
static VERB v_stop         = { &v_winrestore, 100, &s_return[0], "STOP" };  /* code  95 */
static VERB v_endclass	  = { NULL, 237, &s_endclass[0], "ENDCLASS" };	/* code 96 */
static VERB v_type         = { &v_endclass, 77, &s_type[0], "TYPE" };  /* code  96 */
static VERB v_readks       = { &v_type, 56, &s_readks[0], "READKS" };  /* code  96 */
static VERB v_getpaperbins = { NULL, v88 + 239, &s_getpaperbins[0], "GETPAPERBINS" };	/* code  97 */
static VERB v_trapsize     = { &v_getpaperbins, v88 + 133, &s_retcount[0], "TRAPSIZE" };  /* code  97 */
static VERB v_trapsave     = { &v_trapsize, v88 + 27, &s_endset[0], "TRAPSAVE" };  /* code  97 */
static VERB v_filechk	  = { NULL, v88 + 99, &s_aimdex[0], "FILECHK" };	/* code 98 */
static VERB v_link		  = { &v_filechk, v88 + 184, &s_link[0], "LINK" };	/* code	98 */
static VERB v_ifc          = { &v_link, 162, &s__ifz[0], "IFC" };  /* code  98 */
static VERB v_break        = { NULL, 133, &s_break[0], "BREAK" };  /* code  99 */
static VERB v_getpapernames= { &v_break, v88 + 34, &s_getpapernames[0], "GETPAPERNAMES" }; /* code 99 */
static VERB v_statesize    = { NULL, v88 + 134, &s_retcount[0], "STATESIZE" };  /* code 100 */
static VERB v_statesave    = { &v_statesize, v88 + 135, &s_endset[0], "STATESAVE" };  /* code 100 */
static VERB v_ck10         = { NULL, v88 + 1, &s_check10[0], "CK10" };  /* code 102 */
static VERB v_traprestore  = { NULL, v88 + 28, &s_endset[0], "TRAPRESTORE" };  /* code 103 */
static VERB v_chain        = { &v_traprestore, 9, &s_chain[0], "CHAIN" };  /* code 103 */
static VERB v_movelength   = { NULL, v88 + 40, &s_movesize[0], "MOVELENGTH" };  /* code 104 */
static VERB v_readlk       = { &v_movelength, v88 + 194, &s_read[0], "READLK" };  /* code 104 */
static VERB v_recv		  = { NULL, v85 + 9, &s_recv[0], "RECV" };	/* code 105 */
static VERB v_getreturnmodules   = { &v_recv, v88 + 238, &s_getreturnmodules[0], "GETRETURNMODULES" };	/* code 105 */
static VERB v_check10      = { &v_getreturnmodules, v88 + 1, &s_check10[0], "CHECK10" };  /* code 105 */
static VERB v_staterestore = { NULL, v88 + 136, &s_endset[0], "STATERESTORE" };  /* code 106 */
static VERB v_inc          = { &v_staterestore, 192, &s_include[0], "INC" };  /* code 106 */
static VERB v_readgplk     = { NULL, v88 + 198, &s_readkg[0], "READGPLK" };  /* code 108 */
static VERB v_readkglk     = { &v_readgplk, v88 + 197, &s_readkg[0], "READKGLK" };  /* code 108 */
static VERB v_readkplk     = { &v_readkglk, v88 + 196, &s_readks[0], "READKPLK" };  /* code 108 */
static VERB v_readkslk     = { &v_readkplk, v88 + 195, &s_readks[0], "READKSLK" };  /* code 108 */
static VERB v_keyin        = { &v_readkslk, 32, &s_keyin[0], "KEYIN" };  /* code 108 */
static VERB v_tan          = { NULL, v88 + 43, &s_sqrt[0], "TAN" };  /* code 109 */
static VERB v_if           = { &v_tan, 136, &s_if[0], "IF" };  /* code 109 */
static VERB v_destroy	  = { NULL, v88 + 227, &s_destroy[0], "DESTROY" }; /* code 110 */
static VERB v_switch       = { &v_destroy, 139, &s_switch[0], "SWITCH" };  /* code 110 */
static VERB v_scan         = { NULL, 65, &s_append[0], "SCAN" };  /* code 112 */
static VERB v_arcsin       = { NULL, v88 + 44, &s_sqrt[0], "ARCSIN" };  /* code 113 */
static VERB v_arctan       = { &v_arcsin, v88 + 46, &s_sqrt[0], "ARCTAN" };  /* code 113 */
static VERB v_copy		  = { &v_arctan, v88 + 94, &s_aimdex[0], "COPY" };  /* code 113 */
static VERB v_packlen      = { &v_copy, v88 + 236, &s_pack[0], "PACKLEN" };  /* code 113 */
static VERB v_comopen	  = { NULL, v85 + 1, &s_comopen[0], "COMOPEN" };	/* code 114 */
static VERB v_movelv       = { &v_comopen, v88 + 144, &s_movelv[0], "MOVELV" };  /* code 114 */
static VERB v_display	  = { &v_movelv, 20, &s_display[0], "DISPLAY" };	/* code 114 */
static VERB v__iflabel	  = { &v_display, 179, &s__ifdef[0], "%IFLABEL" }; /* code 114 */
static VERB v_liston       = { NULL, 144, &s_liston[0], "LISTON" };  /* code 115 */
static VERB v_empty 	  = { &v_liston, v88 + 187, &s_empty[0], "EMPTY" };  /* code 115 */
static VERB v_sin          = { NULL, v88 + 41, &s_sqrt[0], "SIN" };  /* code 116 */
static VERB v_unlock       = { &v_sin, v88 + 200, &s_unlock[0], "UNLOCK" };  /* code 116 */
static VERB v_unlink	  = { &v_unlock, v88 + 220, &s_unlink[0], "UNLINK" };	/* code 116 */
static VERB v_getposition  = { &v_unlink, v88 + 111, &s_getposition[0], "GETPOSITION" };  /* code 116 */
static VERB v_unpack	  = { &v_getposition, 79, &s_unpack[0], "UNPACK" };  /* code  116 */
static VERB v__ifnlabel	  = { &v_unpack, 180, &s__ifdef[0], "%IFNLABEL" };  /* code 116 */
static VERB v_log10        = { NULL, v88 + 49, &s_sqrt[0], "LOG10" };  /* code 117 */
static VERB v_getendkey	  = { &v_log10, v88 + 213, &s_getendkey[0], "GETENDKEY" }; /* code 117 */
static VERB v_return	  = { &v_getendkey, 90, &s_return[0], "RETURN" };	/* code 117 */
static VERB v_ifeq		  = { NULL, 169, &s_ifeq[0], "IFEQ" };	/* code 118 */
static VERB v_endif        = { NULL, 138, &s_endif[0], "ENDIF" };  /* code 119 */
static VERB v_open         = { NULL, 45, &s_open[0], "OPEN" };  /* code 121 */
static VERB v_flusheof     = { NULL, v88 + 18, &s_flush[0], "FLUSHEOF" };  /* code 124 */
static VERB v_clearendkey  = { &v_flusheof, v88 + 212, &s_setendkey[0], "CLEARENDKEY" }; /* code 124 */
static VERB v__ifz		  = { &v_clearendkey, 165, &s__ifz[0], "%IFZ" };	/* code 124 */
static VERB v_shutdown	  = { NULL, 78, &s_shutdown[0], "SHUTDOWN" };  /* code 125 */
static VERB v_listoff      = { &v_shutdown, 145, &s_liston[0], "LISTOFF" };  /* code 125 */
static VERB v_edit         = { &v_listoff, 23, &s_edit[0], "EDIT" };  /* code 125 */
static VERB v_weof         = { NULL, 82, &s_reposit[0], "WEOF" };  /* code 126 */
static VERB v_get		  = { &v_weof, v88 + 179, &s_get[0], "GET" };	/* code 126 */
static VERB v__ifnz        = { &v_get, 166, &s__ifz[0], "%IFNZ" };  /* code 126 */
static VERB v_sqlexec      = { NULL, v88 + 128, &s_sqlexec[0], "SQLEXEC" };  /* code 127 */
static VERB v_noreturn     = { &v_sqlexec, 43, &s_beep[0], "NORETURN" };  /* code 127 */

VERB *verbptr[128] = {
		&v_call,			/* code 0 */
		&v_xif,			/* code 1 */
		&v_filepi,		/* code 2 */
		&v_default,		/* code 3 */
		&v_ccall,			/* code 4 */
		&v_gfloat,		/* code 5 */
		NULL,			/* code 6 */
		&v_add,			/* code 7 */
		&v_count,			/* code 8 */
		&v_int,			/* code 9 */
		&v_set,			/* code 10 */
		&v_init,			/* code 11 */
		&v_wait,			/* code 12 */
		&v_reset,			/* code 13 */
		&v_fposit,		/* code 14 */
		&v_repeat,		/* code 15 */
		&v_getlabel,		/* code 16 */
		&v_reposit,		/* code 17 */
		&v_scrnrest,		/* code 18 */
		&v_retcount,		/* code 19 */
		NULL,			/* code 20 */
		&v_ck11,			/* code 21 */
		&v_mult,			/* code 22 */
		&v_getwindow,		/* code 23 */
		&v_print,			/* code 24 */
		&v_append,		/* code 25 */
		&v_winrest,		/* code 26 */
		&v_read,			/* code 27 */
		&v_show,			/* code 28 */
		&v_ifnz,			/* code 29 */
		&v_mod,			/* code 30 */
		&v_record,		/* code 31 */
		&v_ifng,			/* code 32 */
		&v_char,			/* code 33 */
		&v_traprest,		/* code 34 */
		&v_unpacklist,		/* code 35 */
		&v_subtract,		/* code 36 */
		&v_recordend,		/* code 37 */
		NULL,			/* code 38 */
		&v_clear,			/* code 39 */
		&v_sound,			/* code 40 */
		&v_for,			/* code 41 */
		&v_getcolor,		/* code 42 */
		&v_var,			/* code 43 */
		&v_readkg,		/* code 44 */
		&v_clearadr,		/* code 45 */
		&v_ginteger,		/* code 46 */
		&v_setflag,		/* code 47 */
		&v_gchararacter,	/* code 48 */
		&v_gnumber,		/* code 49 */
		NULL,			/* code 50 */
		&v_integer,		/* code 51 */
		&v_setlptr,		/* code 52 */
		&v_testadr,		/* code 53 */
		&v_dim,			/* code 54 */
		&v_equ,			/* code 55 */
		&v_moveadr,		/* code 56 */
		&v_sqlmsg,		/* code 57 */
		&v_movefptr,		/* code 58 */
		&v_xor,			/* code 59 */
		&v_afile,			/* code 60 */
		&v_number,		/* code 61 */
		&v_aimdex,		/* code 62 */
		&v_beep,			/* code 63 */
		&v_form,			/* code 64 */
		&v_verb,			/* code 65 */
		&v_file,			/* code 66 */
		&v_readtab,		/* code 67 */
		&v_else,			/* code 68 */
		&v_cmove,			/* code 69 */
		&v_perform,		/* code 70 */
		&v_save,			/* code 71 */
		NULL,			/* code 72 */
		&v_index,			/* code 73 */
		&v_comp,			/* code 74 */
		&v_compare,		/* code 75 */
		&v_num,			/* code 76 */
		&v_continue,		/* code 77 */
		&v__if,			/* code 78 */
		&v_move,			/* code 79 */
		&v_match,			/* code 80 */
		&v_updatab,		/* code 81 */
		&v_ifs,			/* code 82 */
		&v_readkp,		/* code 83 */
		&v_cmatch,		/* code 84 */
		&v_readkgp,		/* code 85 */
		&v__ifndef,		/* code 86 */
		&v_scrnrestore,	/* code 87 */
		&v_branch,		/* code 88 */
		&v_lroutine,		/* code 89 */
		&v_prep,			/* code 90 */
		&v_prepare,		/* code 91 */
		&v_store,			/* code 92 */
		&v_endswitch,		/* code 93 */
		&v_write,			/* code 94 */
		&v_stop,			/* code 95 */
		&v_readks,		/* code 96 */
		&v_trapsave,		/* code 97 */
		&v_ifc,			/* code 98 */
		&v_getpapernames,	/* code 99 */
		&v_statesave,		/* code 100 */
		NULL,			/* code 101 */
		&v_ck10,			/* code 102 */
		&v_chain,			/* code 103 */
		&v_readlk,		/* code 104 */
		&v_check10,		/* code 105 */
		&v_inc,			/* code 106 */
		NULL,			/* code 107 */
		&v_keyin,			/* code 108 */
		&v_if,			/* code 109 */
		&v_switch,		/* code 110 */
		NULL,			/* code 111 */
		&v_scan,			/* code 112 */
		&v_packlen,		/* code 113 */
		&v__iflabel,		/* code 114 */
		&v_empty,			/* code 115 */
		&v__ifnlabel,		/* code 116 */
		&v_return,		/* code 117 */
		&v_ifeq,			/* code 118 */
		&v_endif,			/* code 119 */
		NULL,			/* code 120 */
		&v_open,			/* code 121 */
		NULL,			/* code 122 */
		NULL,			/* code 123 */
		&v__ifz,			/* code 124 */
		&v_edit,			/* code 125 */
		&v__ifnz,			/* code 126 */
		&v_noreturn		/* code 127 */
};

//#define NUMKYWRD(a) (sizeof(a) / sizeof(*a))

static CHAR *clkkywrd[] = { "TIME", "DATE", "YEAR", "DAY", "VERSION",
		"PORT", "USER", "ENV", "CALENDAR", "TIMESTAMP", "ERROR", "CMDLINE",
		"LICENSE", "WEEKDAY", "RELEASE", "PID", "UTILERROR",
		"TIMEZONE", "UTCOFFSET", "UTCCALENDAR", "UTCTIMESTAMP" , "UI"};

static CHAR *clockClientKeywords[] = { "?", "?", "?", "?", "VERSION" };

static CHAR *openkywrd[] = { "SHARE", "READ", "EXCLUSIVE", "MATCH",
		"LOCKMANUAL", "LOCKAUTO", "MULTIPLE", "SINGLE", "WAIT", "NOWAIT", "SHARENF" };

static CHAR *prepkywrd[] = { "MATCH", "CASE", "KEYS" };

static CHAR *trapkywrd[] = { "IF", "GIVING", "NORESET", "PRIOR", "NOCASE", "DISABLE" } ;

static CHAR *flagkywrd[] = { "EQUAL", "LESS", "OVER", "EOS", "ZERO" };

static struct kwlen {
	CHAR *name;
	UCHAR code;
} drawkywrd[] = {
	"ATEXT", 45,		/* 0 */
	"BIGDOT", 27,		/* 1 */
	"BOX", 38,			/* 2 */
	"CIRCLE", 26,		/* 3 */
	"CLIPIMAGE", 41,	/* 4 */
	"COLOR", 24,		/* 5 */
	"CTEXT", 43,		/* 6 */
	"DOT", 5,			/* 7 */
	"ERASE", 4,			/* 8 */
	"FONT", 33,			/* 9 */
	"H", 28,			/* 10 */
	"HA", 30,			/* 11 */
	"IMAGE", 35,		/* 12 */
	"IMAGEROTATE", 46,	/* 13 */
	"LINE", 37,			/* 14 */
	"LINEFEED", 3,		/* 15 */
	"LINETYPE", 8,		/* 16 */
	"LINEWIDTH", 25,	/* 17 */
	"NEWLINE", 2,		/* 18 */
	"P", 36,			/* 19 */
	"RECTANGLE", 39,	/* 20 */
	"REPLACE", 50,		/* 21 */
	"RTEXT", 44,		/* 22 */
	"STRETCHIMAGE", 42, /* 23 */
	"TEXT", 34,			/* 24 */
	"TRIANGLE", 40,		/* 25 */
	"V", 29,			/* 26 */
	"VA", 31,			/* 27 */
	"WIPE", 32			/* 28 */
};

static CHAR *drawcolors[] = { "BLACK", "RED", "GREEN", "YELLOW",
		"BLUE", "MAGENTA", "CYAN", "WHITE" } ;


#define FKW_FILE 1
#define FKW_IFILE 2
#define FKW_AFILE 4
#define FKW_IMAGE 8
#define FKW_QUEUE 16
#define FKW_FAIFILE 7

static struct filekw {
	CHAR *name;
	UCHAR minlen;
	UCHAR valid;
	UCHAR equalvalu;
} filekywrd[] = {
	{"AIM", 0, FKW_FAIFILE, 1},			/* 0   =dcon		 */
	{"BINARY", 0, FKW_FAIFILE, 0},		/* 1 				 */
	{"COBOL", 0, FKW_FAIFILE, 0},			/* 2 				 */
	{"COLORBITS", 5, FKW_IMAGE, 1},		/* 3   =dcon		 */
	{"COMPRESSED", 4, FKW_FAIFILE, 0},	/* 4 				 */
	{"CRLF", 0, FKW_FAIFILE, 0},			/* 5 				 */
	{"DATA", 0, FKW_FAIFILE, 0},			/* 6 				 */
	{"DOS", 0, FKW_FAIFILE, 0},			/* 7 				 */
	{"DUPLICATES", 3, FKW_IFILE, 0},		/* 8 				 */
	{"DYNAMIC", 3, FKW_FAIFILE, 0},		/* 9 				 */
	{"EBCDIC", 0, FKW_FAIFILE, 0},		/* 10 				 */
	{"ENTRIES", 0, FKW_QUEUE, 1},			/* 11  =dcon		 */
	{"FIXED", 3, FKW_FAIFILE, 2},			/* 12  =dcon, =nvar  */
	{"H", 0, FKW_IMAGE, 1},				/* 13  =dcon		 */
	{"INCREMENT", 3, FKW_FAIFILE, 0},		/* 14				 */
	{"KEYLENGTH", 3, FKW_IFILE, 2},		/* 15  =dcon, =nvar  */
	{"NATIVE", 0, FKW_FAIFILE, 0},		/* 16				 */
	{"NODUPLICATES", 5, FKW_IFILE, 0},	/* 17				 */
	{"OVERLAP", 4, FKW_FAIFILE, 0}, 		/* 18				 */
	{"SIZE", 0, FKW_QUEUE, 1},			/* 19  =dcon		 */
	{"STATIC", 4, FKW_FAIFILE, 1},		/* 20  =dcon		 */
	{"STANDARD", 0, FKW_FAIFILE, 0},		/* 21				 */
	{"TEXT", 0, FKW_FAIFILE, 0},			/* 22				 */
#if 0  /* never implemented in dbc */
	{"TRANSLATE", 0, FKW_FAIFILE, 0},		/* 23				 */
#endif
	{"UNCOMPRESSED", 6, FKW_FAIFILE, 0},	/* 23				 */
	{"V", 0, FKW_IMAGE, 1},				/* 24  =dcon		 */
	{"VARIABLE", 3, FKW_FAIFILE, 3},		/* 25  =dcon, =nvar, none */
	{"~", 0, 0, 0}						/* 26				 */
};
#define FILEKW_AIM 0
#define FILEKW_BINARY 1
#define FILEKW_COBOL 2
#define FILEKW_COLORBITS 3
#define FILEKW_COMP 4
#define FILEKW_CRLF 5
#define FILEKW_DATA 6
#define FILEKW_DOS 7
#define FILEKW_DUP 8
#define FILEKW_DYNAMIC 9
//#define FILEKW_EBCDIC 10
#define FILEKW_ENTRIES 11
#define FILEKW_FIXED 12
#define FILEKW_H 13
#define FILEKW_INC 14
#define FILEKW_KEYLEN 15
#define FILEKW_NATIVE 16
#define FILEKW_NODUP 17
#define FILEKW_OVERLAP 18
#define FILEKW_SIZE 19
#define FILEKW_STATIC 20
#define FILEKW_STANDARD 21
#define FILEKW_TEXT 22
#if 0  /* never implemented in dbc */
#define FILEKW_TRANSLATE 23
#endif
#define FILEKW_UNCOMP 23
#define FILEKW_V 24
#define FILEKW_VARIABLE 25
#define FILEKW_NULL 26

/* loop/for/repeat variables */
#define LOOPLEVELMAX 32
static UCHAR looplevel, loopnextid;
static UINT looplabel[LOOPLEVELMAX];
static UCHAR loopid[LOOPLEVELMAX];
static UCHAR loopswitchid[LOOPLEVELMAX], loopifid[LOOPLEVELMAX];

/* switch/case/default variables */
#define SWITCHLEVELMAX 8
static UCHAR switchlevel, switchnextid;
static UCHAR switchstate[SWITCHLEVELMAX];
static UINT switchlabel1[SWITCHLEVELMAX];
static UINT switchlabel2[SWITCHLEVELMAX];
static UCHAR switchtype[SWITCHLEVELMAX];
static UCHAR switchvar[SWITCHLEVELMAX][20];
static UCHAR switchvarlen[SWITCHLEVELMAX];
static UCHAR switchid[SWITCHLEVELMAX];
static UCHAR switchifid[SWITCHLEVELMAX], switchloopid[SWITCHLEVELMAX];

/* if/else/endif variables */
#define IFLEVELMAX 32
static UCHAR iflevel, ifnextid;
static UCHAR ifstate[IFLEVELMAX];
static UINT iflabel1[IFLEVELMAX], iflabel2[IFLEVELMAX];
static UCHAR ifid[IFLEVELMAX];
static UCHAR ifswitchid[IFLEVELMAX], ifloopid[IFLEVELMAX];

/* conditional compilation directive (ccd) macros and variables */
#define CCDSTACKMAX 64	/* allows 32 nested conditional compilation directive statements */	
#define CCD_IF_COMPILED	  0x01 /* state 1: cc if or elseif code block compiled or being compiled */
#define CCD_IF_NOTCOMPILED 0x02 /* state 2: cc if or elseif code block has not been compiled yet */
#define CCD_ELSE		  0x03 /* state 3: in cc else code block, waiting for endif */
#define CCD_IFOPDIR		  0x80 /* cc ifop-directive_statement (see ansi standard) */
#define PUSH_CCDSTACK(x)	(ccdstack[ccdstacktop++] = (x))
#define POP_CCDSTACK()	(ccdstack[--ccdstacktop])
#define TOPOF_CCDSTACK()	(ccdstack[ccdstacktop - 1])
#define CCD_STACKOVERFLOW() (ccdstacktop + 1 >= CCDSTACKMAX)
#define CCD_STACKEMPTY()	(!ccdstacktop)
static INT ccdstacktop = 0;	
static UCHAR ccdifopdir = CCD_IFOPDIR;
static UCHAR ccdstack[CCDSTACKMAX];

static UCHAR listlevel;
static INT nameslevel;	/* LIST/LISTEND level of WITH NAMES state, -1 for RECORD/RECORDEND WITH NAMES */
static UINT arrayspec[4];
static UCHAR codesave[8][20];
static INT codelength[8];
static UCHAR kwdflag[8];

/* if the return value for syntax parse functions is zero, */
/*   then processing continues normally */
/* otherwise, the return value is the new next processing rule */
/*   when the new rule has processed, this function is called again */
/*   this overrides the NEXT_REPEAT or NEXT_END from the previous rule */

static UCHAR sc, sd, se, sf;
static UINT su;
static INT sx, sy;
static UINT sl;

#define MAXNESTED 100

EXTERN UCHAR charmap[256];

#if OS_UNIX
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
typedef INT BOOL;
#endif

static UINT s_close_fnc()
{
	if (rulecnt == 1) {
		if (symbasetype == TYPE_RESOURCE || symbasetype == TYPE_DEVICE) {
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 171;
		}
		else if (delimiter) return(RULE_PARSEUP + NEXT_NOPARSE);
	}
	else {
		if (tokentype == TOKEN_WORD) {
			if (!strcmp((CHAR *) token, "DELETE")) {
				vexpand();
				codebuf[0] = 88;
				codebuf[1] = 10;
			}
		}
	}
	return(NEXT_TERMINATE);
}

static UINT s_add_fnc()
{
	static UCHAR basetype1, basetype2;
	UCHAR c, d;

	if (rulecnt == 1) {
		basetype1 = basetype2 = 0;
		if (symtype != TYPE_EXPRESSION && !elementref &&
		    symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) {
			sc = 3;
			basetype1 = symbasetype;
		}
		else sc = 1;
		return(RULE_MATHSRC + NEXT_GIVINGOPT);
	}
	switch(sc) {
		case 1: /* first operand was nvar or referenced nvar array */
			if (symtype != TYPE_EXPRESSION && !elementref &&
			    symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) sc = 4; /* done */
			else {
				if (delimiter) {
					sc = 2;
					return(RULE_MATHDEST + NEXT_END);
				}
				else {
					sc = 4; /* done */
					if (!elementref && (symtype < TYPE_NVAR || symtype > TYPE_NVARPTRARRAY3)) {
						error(ERROR_SYNTAX);
						return(NEXT_TERMINATE);
					}
				}
			}
			break;
		case 2: /* first and second operands were nvar or ref. nvar array */
			if (symtype != TYPE_EXPRESSION && !elementref &&
			    symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) {
				error(ERROR_BADARRAYSPEC);
				return(NEXT_TERMINATE);
			}
			else sc = 5; /* done */
			break;
		case 3: /* first operand was an unreferenced array var */
			if (symtype != TYPE_EXPRESSION && !elementref &&
			    delimiter && symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) {
				sc = 5; /* done following parse of array var */
				basetype2 = symbasetype;
				return(RULE_NARRAY + NEXT_END);
			}
			else sc = 4; /* done */
			break;
	}
	if (sc == 4 && !elementref && basetype1 &&
	    symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) {
		if (symbasetype != basetype1) {
			error(ERROR_DIFFARRAYDIM);
			return(NEXT_TERMINATE);
		}
	}
	if (sc == 5) {
		if (!elementref && symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) {
			if (symbasetype != basetype1 || symbasetype != basetype2) {
				error(ERROR_DIFFARRAYDIM);
				return(NEXT_TERMINATE);
			}
		}
		c = codebuf[0];
		if (c != 88) vexpand();
		if (c == 2) d = 1;
		else if (c == 74) d = 2;
		else if (c == 41) d = 3;
		else if (c == 21) d = 4;
		else d = 5;
		codebuf[0] = 30;
		codebuf[1] = d;
	}
	return(NEXT_TERMINATE);
}

static UINT s_power_fnc()
{
	switch (rulecnt) {
		case 1:
			codebufcnt = 0;
			break;
		case 2:
			savecode(1);
			codebufcnt = 0;
			break;
		case 3:
			savecode(2);
			codebufcnt = 0;
			putcode1(88);
			putcode1(52);
			restorecode(2);
			restorecode(1);
	}
	return(0);
}

static UINT s_bump_fnc()
{
	if (rulecnt == 1) {
		if (!delimiter) return(NEXT_TERMINATE);
		return(0);
	}
	if (symtype == TYPE_BYTECONST) sd = 1;
	else sd = 2;
	if (codebuf[0] == 26) {
		vexpand();
		codebuf[0] = 88;
		codebuf[1] = (UCHAR) 34 + sd;
	}
	else codebuf[0] += sd;
	return(NEXT_TERMINATE);
}

static UINT s_clear_fnc()
{
	if (delimiter) putcodehhll((UINT) 0xC000 + codebuf[0]);
	return(0);
}

static UINT s_clock_fnc()
{
	UCHAR i;

	for (i = 0; i < ARRAYSIZE(clkkywrd) && strcmp((CHAR *) token, clkkywrd[i]); i++);
	if (i < ARRAYSIZE(clkkywrd)) putcode1((UCHAR) (i + 1));
	else error(ERROR_CLOCK);
	return(0);
}

static UINT s_clockclient_fnc()
{
	UCHAR i;

	for (i = 0; i < ARRAYSIZE(clockClientKeywords) && strcmp((CHAR *) token, clockClientKeywords[i]); i++);
	if (i < ARRAYSIZE(clockClientKeywords)) putcode1((UCHAR) (i + 1));
	else error(ERROR_CLOCKCLIENT);
	return(0);
}

static UINT s_cmatch_fnc()
{
	if (rulecnt == 1) {
		if (symtype == TYPE_LITERAL || symtype == TYPE_INTEGER) {
			if (symvalue < 256) {
				codebuf[0]++;
				putcode1((UCHAR) symvalue);
			}
			else error(ERROR_DCONTOOBIG);
		}
	}
	else if (symtype == TYPE_LITERAL || symtype == TYPE_INTEGER) {
		token[0] = (UCHAR) symvalue;
		token[1] = 0;
		putcodeadr(makeclit(token));
	}
	return(0);
}

static UINT s_delete_fnc()
{
	if (symbasetype != TYPE_IFILE) {
		putcodehhll(0xFFFF);
		return(NEXT_TERMINATE);
	}
	if (!delimiter) error(ERROR_SYNTAX);
	return(0);
}

static UINT s_print_fnc()
{
	if (rulecnt == 1 && symbasetype == TYPE_PFILE) {
		if (!delimiter || delimiter != TOKEN_SEMICOLON) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
	}
	else {
		if (delimiter == TOKEN_SEMICOLON) {
			putcode1(0xFE);
			return(NEXT_TERMINATE);
		}
		else	if (!delimiter) {
			putcode1(0xFF);
			return(NEXT_TERMINATE);
		}
	}
	return(0); 
}

static UINT s_filepi_fnc()
{
	if (rulecnt == 1) {
		if (symvalue > 254) error(ERROR_BADVALUE);
		if (!symvalue) {
			putcodehhll(0x00FF);
			if (delimiter) error(ERROR_SYNTAX);
		}
		else if (delimiter) {
			putcode1((UCHAR) symvalue);
			return(0);
		}
		else error(ERROR_SYNTAX);
	}
	else if (delimiter) return(0);
	else putcode1(0xFF);
	return(NEXT_TERMINATE);
}

static UINT s_pi_fnc()
{
	if (rulecnt == 1) {
		if (symvalue) error(ERROR_BADVALUE);
		putcodehhll(0x00FF);
	}
	return(NEXT_TERMINATE);
}

static UINT s_load_fnc()
{
	if (rulecnt == 1) {
		if (tokentype == TOKEN_WORD) {
			if (symbasetype == TYPE_IMAGE) {
				sc = 3;
				vexpand();
				if (codebuf[0] == 34) codebuf[1] = 174;  /* load */
				else codebuf[1] = 175;  /* store */
				codebuf[0] = 88;
				return(RULE_DEVICE + NEXT_END);
			}
			if (baseclass(symtype) == TYPE_CVAR) {
				sc = 1;
				return(RULE_NSRC + (RULE_EQUATE << 8) + ((INT) RULE_DEVICE << 16) + NEXT_NOPARSE);
			}
			else {
				sc = 0;
				return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP);
			}
		}
		else if (tokentype == TOKEN_LITERAL) {
			if (symtype == TYPE_LITERAL || symtype == TYPE_EXPRESSION) {
				sc = 1;
				return(RULE_NSRC + (RULE_EQUATE << 8) + ((INT) RULE_DEVICE << 16) + NEXT_NOPARSE);
			}
			else {
				sc = 0;
				return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP);
			}
		}
		else {
			sc = 0;
			return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP);
		}
	}
	switch (sc) {
		case 0: /* just parsed 2nd prep of LOAD var prep numvar prep list */
			sc = 2;
			return(RULE_CNDATASRC + NEXT_LIST);
		case 1: /* parsed device of LOAD cvar prep device -or- numvar of LOAD cvar prep numvar prep list */
			if (symbasetype == TYPE_DEVICE) {
				vexpand();
				if (codebuf[0] == 34) codebuf[1] = 174;  /* load */
				else codebuf[1] = 175;  /* store */
				codebuf[0] = 88;
				return(NEXT_TERMINATE);
			}
			sc = 0;
			return(RULE_NOPARSE + NEXT_PREP);
		case 2:
			if (delimiter) return(RULE_CNDATASRC + NEXT_LIST);
			putcode1(0xFF);
			// fall through
			/* no break */
		case 3:
			return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_move_fnc()
{
	static UCHAR basetype1;

	if (rulecnt == 1) {
		basetype1 = 0;
		if (symtype != TYPE_EXPRESSION && !elementref) {
			if ((symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) ||
			    (symbasetype >= TYPE_CARRAY1 && symbasetype <= TYPE_CARRAY3))
				basetype1 = symbasetype;
		}
	}
	else if (rulecnt == 2) {
		if (basetype1) {
			if (elementref) error(ERROR_BADARRAYSPEC);
			else if (symbasetype != basetype1)	{
				if ((basetype1 == TYPE_CARRAY1 && symbasetype != TYPE_NARRAY1) ||
				    (basetype1 == TYPE_CARRAY2 && symbasetype != TYPE_NARRAY2) ||
				    (basetype1 == TYPE_CARRAY3 && symbasetype != TYPE_NARRAY3) ||
				    (basetype1 == TYPE_NARRAY1 && symbasetype != TYPE_CARRAY1) ||
				    (basetype1 == TYPE_NARRAY2 && symbasetype != TYPE_CARRAY2) ||
				    (basetype1 == TYPE_NARRAY3 && symbasetype != TYPE_CARRAY3)) {
					error(ERROR_DIFFARRAYDIM);
					return(NEXT_TERMINATE);
				}
			}
		}
		if (delimiter) {
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 161;
		}
	}
	else {
		if (codebuf[0] == 88 && !delimiter) {
			putcode1(0xFF);
			return(NEXT_TERMINATE);
		}
	}
	return(0);
}

static UINT s_moveadr_fnc()
{
	if (rulecnt == 1) {
		if (symtype == TYPE_LITERAL) {
			symvalue = makeclit(token);
			putcodeadr(symvalue);
			return(RULE_CVARPTR + (RULE_VVARPTR << 8) + NEXT_END);
		}
		return(RULE_ANYVARPTR + NEXT_END);
	}
	return(NEXT_TERMINATE);
}


static UINT s_open_fnc()
{
	UCHAR i;

	if (rulecnt == 1) {
		if (symbasetype == TYPE_RESOURCE || symbasetype == TYPE_DEVICE) {
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 170;
		}
		return(0);
	}
	if (rulecnt == 2) {
		if (codebuf[0] == 88) {
			if (delimiter) error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		if (!delimiter) return(NEXT_TERMINATE);
		kwdupchk(0xFF);
		codebuf[0] = 44;
		sy = codebufcnt;
		putcode1(0);  /* open mode filled in later */
		sc = sd = 0;
	}
	if (sc == 1) sd |= 0x10;  /* match char */
	if (delimiter) {
		sx = linecnt;
		scantoken(SCAN_UPCASE);
		if (tokentype == TOKEN_WORD) {
			for (i = 0; i < ARRAYSIZE(openkywrd) && strcmp(openkywrd[i], (CHAR *) token); i++);
		}
		else if (tokentype != TOKEN_DBLQUOTE) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		if (tokentype == TOKEN_DBLQUOTE || i == ARRAYSIZE(openkywrd)) {
			linecnt = (UCHAR) sx;
			sc = 1;
			return(RULE_CSRC + NEXT_COMMAOPT);
		}
		if (i == 10) i = 0; /* SHARENF if equivalent to SHARE */
		if (i == 3) {
			if (line[linecnt++] != '=') {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			whitespace();
			sc = 1;
			return(RULE_CSRC + NEXT_COMMAOPT);
		}
		if (i < 3) {
			kwdupchk(0);
			sd |= i;
		}
		else if (i < 6) {
			kwdupchk(1);
			if (i == 4) sd |= 0x40;
			else sd |= 0x80;
		}
		else if (i < 8) {
			kwdupchk(2);
			if (i == 7) sd |= 0x08;
		}
		else {
			kwdupchk(3);
			if (i == 9) sd |= 0x04;
		}
		sc = 0;
		return(RULE_NOPARSE + NEXT_COMMAOPT);
	}
	codebuf[sy] = sd;
	return(NEXT_TERMINATE);
}

static UINT s_prepare_fnc()
{
	UCHAR linecntsave;
	static UCHAR i, numkeywords;

	if (rulecnt == 1) {
		numkeywords = 0;
		kwdupchk(0xFF);
		switch (symbasetype) {
			case TYPE_FILE:
				sc = 1;
				return(RULE_CSRC + NEXT_COMMAOPT);
			case TYPE_IFILE:
				sc = 2;
				return(RULE_CSRC + NEXT_COMMAOPT);
			case TYPE_AFILE:
				sc = 3;
				return(RULE_CSRC + NEXT_COMMAOPT);
			case TYPE_DEVICE:
				sc = 7;
				return(RULE_CSRC + NEXT_END);
			case TYPE_RESOURCE:
				sc = 7;
				return(RULE_CSRC + NEXT_END);
		}
	}
	switch (sc) {
		case 1:  /* FILE var, cvarlit parsed */
			if (delimiter) {
				linecntsave = linecnt;
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_WORD) {
					if (!strcmp((CHAR *) token, "PREPARE") || !strcmp((CHAR *) token, "PREP")) {
						sc = 9;
						putcodehhll(0xFF01);
						break;
					}
					else if (!strcmp((CHAR *) token, "CREATE")) {
						sc = 9;
						putcodehhll(0xFF02);
						break;
					}
				}
				sc = 8;
				linecnt = linecntsave;
				return(RULE_CSRC + NEXT_END);
			}
			return(NEXT_TERMINATE);
		case 2:  /* IFILE var, cvarlit parsed */
			if (delimiter) {
				sc = 8;
				return(RULE_CSRC + NEXT_END);
			}
			return(NEXT_TERMINATE);
		case 3:  /* AFILE var, cvarlit parsed */
			if (!delimiter) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			// fall through
		case 4:  /* AFILE var, cvarlit, cvarlit parse or above */
			if (!delimiter) {
				vexpand();
				codebuf[0] = 88;
				codebuf[1] = 30;
				return(NEXT_TERMINATE);
			}
			linecntsave = linecnt;
			scantoken(SCAN_UPCASE);
			if (tokentype == TOKEN_WORD) {
				for (i = 0; i < ARRAYSIZE(prepkywrd) && strcmp(prepkywrd[i], (CHAR *) token); i++);
				if (i < ARRAYSIZE(prepkywrd)) {
					kwdupchk(i);
					if (line[linecnt++] != '=') {
						error(ERROR_SYNTAX);
						return(NEXT_TERMINATE);
					}
					if (sc == 3) putcodehhll(0xFFFF);
					sc = 5;
					savecode(0);
					codebufcnt = 0;
					whitespace();
					return(RULE_CSRC + NEXT_COMMAOPT);
				}
			}
			if (sc == 4) sc = 6;
			else sc = 4;
			linecnt = linecntsave;
			return(RULE_CSRC + NEXT_COMMAOPT);
		case 5: /* AFILE var, cvarlit, [cvarlit], MATCH=cvarlit | CASE=cvarlit | KEYS=cvarlit parsed */
			numkeywords++;
			savecode(i+1);
			codebufcnt = 0;
			if (delimiter && numkeywords < 3) {
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_WORD) {
					for (i = 0; i < ARRAYSIZE(prepkywrd) && strcmp(prepkywrd[i], (CHAR *) token); i++);
					if (i < ARRAYSIZE(prepkywrd)) {
						kwdupchk(i);
						if (line[linecnt++] != '=') {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
						sc = 5;
						whitespace();
						return(RULE_CSRC + NEXT_COMMAOPT);
					}
				}
			}
			sc = 10;
			break;
		case 6: /* AFILE var, cvarlit, cvarlit, cvarlit parse */
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 31;
			return(NEXT_TERMINATE);
		case 7: /* DEVICE/RESOURCE var, cvarlit parsed */
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 169;
			return(NEXT_TERMINATE);
		case 8: /* FILE/IFILE cvarlit cvarlit */
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 30;
			return(NEXT_TERMINATE);
	}
	switch (sc) {
		case 9: /* FILE var, cvarlit, PREP-MODE */
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 30;
			break;
		case 10:  /* AFILE var, cvarlit, [cvarlit], MATCH=cvarlit | CASE=cvarlit | KEYS=cvarlit parsed */ 
			restorecode(0);
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 33;
			if (kwdflag[0]) restorecode(1);
			else putcodehhll(0xFFFF);
			if (kwdflag[1]) restorecode(2);
			else putcodehhll(0xFFFF);
			if (kwdflag[2]) restorecode(3);
			else error(ERROR_MISSINGKEYS);
	}
	return(NEXT_TERMINATE);
}

static UINT s_unlock_fnc()
{
	if (rulecnt == 1 && delimiter) {
		codebuf[1] = 232;
		return(RULE_NSRC + NEXT_END);
	}
	return(NEXT_TERMINATE);
}

static UINT s_read_fnc()
{

	if (rulecnt == 1) {
		sx = 1;
		if (symbasetype == TYPE_FILE) return(RULE_NVAR + NEXT_SEMICOLON);
		if (symbasetype == TYPE_IFILE) return(RULE_CNVAR + NEXT_SEMICOLON);
		sx = 2;
		return(RULE_CNDATASRC + NEXT_SEMILIST);
	}
	switch (sx) {
		case 1:
			sx = 3;
			return(RULE_CNDATADEST + (RULE_RDCC << 8) + ((INT) RULE_EMPTYLIST << 16) + NEXT_SEMILIST);
		case 2:
			if (delimiter == TOKEN_SEMICOLON) {
				putcodehhll(0xFFFF);
				sx = 3;
				return(RULE_CNDATADEST + (RULE_RDCC << 8) + ((INT) RULE_EMPTYLIST << 16) + NEXT_SEMILIST);
			}
			else return(RULE_CNDATASRC + NEXT_SEMILIST);
		case 3:
			if (delimiter == TOKEN_SEMICOLON) {
				putcode1(0xFE);
				return(NEXT_TERMINATE);
			}
			else if (!delimiter) {
				putcode1(0xFF);
				return(NEXT_TERMINATE);
			}
			return(RULE_CNDATADEST + (RULE_RDCC << 8) + NEXT_SEMILIST);
	}
	return(0);
}

static UINT s_splclose_fnc()
{
	if (rulecnt == 1) {
		if (symbasetype == TYPE_PFILE) {
			if (!delimiter) putcodehhll(0xFFFF);
			else return(RULE_CSRC + NEXT_END);
		}
		else if (codebufcnt == 2) {
			codebufcnt = 0;
			putcode1(61);
		}
	}
	return(NEXT_TERMINATE);
}

static UINT s_splopen_fnc()
 {
	if (rulecnt == 1 && symbasetype == TYPE_PFILE) {
		if (!delimiter) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		return(RULE_CSRC + NEXT_COMMAOPT);
	}
	if (delimiter) {
		codebuf[0] = 71;
		return(0);
	}
	return(NEXT_TERMINATE);
}

static UINT s_trap_fnc()
{
	UCHAR i;
	static UCHAR linecntsave;	/* save keyword start linecnt */
	static UCHAR noresetflg;		

	whitespace();
	if (rulecnt == 1) {
		noresetflg = FALSE;
		sc = sd = se = 0;
		return(RULE_PARSEUP + NEXT_NOPARSE);
	}
	switch (sc) {
		case 1:  /* just parsed GIVING */
			sc = 0;
			return(RULE_PARSEUP + NEXT_NOPARSE);
		case 2: /* just finished parsing keyword following IF */
			if (tokentype == TOKEN_WORD) {
				if ('A' == token[0]) {
					if (!strcmp("ANYQUEUE", (CHAR *) token)) {
						if (noresetflg) {
							error(ERROR_ILLEGALNORESET);
							return(NEXT_TERMINATE);
						}
					}
				}
				else if ('C' == token[0]) {
					if (!strcmp("CHAR", (CHAR *) token)) {
						sc = 4;
						putcode1(0x80);
						return(RULE_CSRC + NEXT_END);
					}
					if (!strcmp("COMFILE", (CHAR *) token)) {
						if (noresetflg) {
							error(ERROR_ILLEGALNORESET);
							return(NEXT_TERMINATE);
						}
						sc = 3;
						putcode1(0x8B);
						return(RULE_COMFILE + NEXT_END);
					}
				}
				else if ('Q' == token[0]) {
					if (!(dbcmpflags & DBCMPFLAG_VERSION8) && !strcmp("QUEUE", (CHAR *) token)) {
						if (noresetflg) {
							error(ERROR_ILLEGALNORESET);
							return(NEXT_TERMINATE);
						}
						sc = 3;
						putcode1(0x8C);
						return(RULE_QUEUE + NEXT_END);
					}
				}
				else if ('T' == token[0]) {
					if (!strcmp("TIMEOUT", (CHAR *) token)) {
						sc = 3;
						putcode1(0x89);
						return(RULE_NSRC + NEXT_END);
					}
					if (!strcmp("TIMESTAMP", (CHAR *) token)) {
						sc = 3;
						putcode1(0x8A);
						return(RULE_CSRC + NEXT_END);
					}
				}
			}
			linecnt = linecntsave; /* put back last token */
			sc = 3;
			return(RULE_TRAPEVENT + NEXT_END);
		case 3:  /* just parsed a trap event following IF */
			return(NEXT_TERMINATE);
		case 4:  /* just parsed a cvarslit or nvardcon following IF CHAR */
			if (tokentype == TOKEN_NUMBER) {  /* currently not supported */
				codebufcnt--;	 /* replace code 128 */
				if (symvalue < 256) {
					putcode1(0xFE);
					putcode1((UCHAR) symvalue);
				}
				else	if ((symvalue >= 256) && (symvalue <= 511)) {
					putcode1(0xFC);
					putcode1((UCHAR) (symvalue - 256));
				}
				else	error(ERROR_BADENDKEYVALUE);
			}
			return(NEXT_TERMINATE);
	}
	for (i = 0; i < ARRAYSIZE(trapkywrd) && strcmp(trapkywrd[i], (CHAR *) token); i++);
	if (i == 0) {	/* IF */
		sc = 2;
		linecntsave = linecnt;
		if (sd || se) {  /* complex syntax */
			codebuf[0] = 73;
			codebuf[3] = sd;
		}
		if (sd && !se) {  /* complex syntax but no GIVING */
			codebufcnt = 4;
			putcodehhll(0xFFFF);
		}
		if (line[linecnt] != '"') return(RULE_PARSEUP + NEXT_NOPARSE);
		else {
			sc = 3;
			return(RULE_TRAPEVENT + NEXT_END);
		}
	}
	if (i == 1) {	/* GIVING */
		if (se) error(ERROR_DUPKEYWORD);
		else {
			sc = se = 1;  /* GIVING has occurred flag */
			codebufcnt = 4;
			return(RULE_CVAR + NEXT_NOPARSE);
		}
	}
	else if (i < ARRAYSIZE(trapkywrd)) {  /* NORESET, PRIOR, NOCASE, DISABLE */
		i = (UCHAR) (1 << (i - 2));  /* 1, 2, 4, 8 */
		if (1 == i) noresetflg = TRUE;
		if (sd & i) error(ERROR_DUPKEYWORD);
		else {
			sd |= i;
			return(RULE_PARSEUP + NEXT_NOPARSE);
		}
	}
	else	error(ERROR_SYNTAX);
	return(NEXT_TERMINATE);
}

static UINT s_trapclr_fnc()
{
	static UCHAR linecntsave;	/* save keyword start linecnt */

	whitespace();
	if (rulecnt == 1) {
		linecntsave = linecnt;
		scantoken(SCAN_UPCASE);
		whitespace();
		if (tokentype == TOKEN_WORD) {
			if ('C' == token[0]) {
				if (!strcmp("CHAR", (CHAR *) token)) {
					putcode1(0x80);
					return(RULE_CSRC + NEXT_END);
				}
				if (!strcmp("COMFILE", (CHAR *) token)) {
					putcode1(0x8B);
					return(RULE_COMFILE + NEXT_END);
				}
			}
			else if ('Q' == token[0]) {
				if (!(dbcmpflags & DBCMPFLAG_VERSION8) && !strcmp("QUEUE", (CHAR *) token)) {
					putcode1(0x8C);
					return(RULE_QUEUE + NEXT_END);
				}
			}
			else if ('T' == token[0]) {
				if (!strcmp("TIMEOUT", (CHAR *) token)) {
					putcode1(0x89);
					return(NEXT_TERMINATE);
				}
				if (!strcmp("TIMESTAMP", (CHAR *) token)) {
					putcode1(0x8A);
					return(NEXT_TERMINATE);
				}
			}
		}
		linecnt = linecntsave; /* put back last token */
		return(RULE_TRAPEVENT + NEXT_END);
	}
	return(NEXT_TERMINATE);
}

static UINT s_type_fnc()
{
	if (rulecnt == 1) {
		if (!delimiter) {
			if (baseclass(symtype) != TYPE_CVAR) error(ERROR_BADVARTYPE);
			return(NEXT_TERMINATE);
		}
		vexpand();
		codebuf[0] = 88;
		codebuf[1] = 140;
	}
	if (rulecnt == 5 || !delimiter) {
		putcode1(0xFF);
		return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_write_fnc()
{
	if (rulecnt == 1) {
		su = 0;
		tabflag = FALSE;
		if (delimiter == TOKEN_SEMICOLON) {
			if (symbasetype != TYPE_AFILE) error(ERROR_SYNTAX);
			su = 1;
			putcodehhll(0xFFFF);
		}
		else if (symbasetype == TYPE_AFILE) {
			return(RULE_NVAR + NEXT_SEMICOLON);
		}
		else if (symbasetype == TYPE_FILE) return(RULE_NVAR + NEXT_SEMICOLON); 
		else if (symbasetype == TYPE_IFILE) return(RULE_CNVAR + NEXT_SEMICOLON);
	}
	else if (rulecnt > 2) {
		if (delimiter == TOKEN_SEMICOLON || !delimiter) {
			if (delimiter == TOKEN_SEMICOLON) putcode1(0xFE);
			else if (!delimiter) putcode1(0xFF);
			if (tabflag) codebuf[0] = 84;
			return(NEXT_TERMINATE);
		}
	}
	else if (!delimiter && su == 1) {
		putcode1(0xFF);
		return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_update_fnc()
{
	if (rulecnt == 1) tabflag = FALSE;
	else if (delimiter == TOKEN_SEMICOLON || !delimiter) {
		if (delimiter == TOKEN_SEMICOLON) putcode1(0xFE);
		else if (!delimiter) putcode1(0xFF);
		if (tabflag) codebuf[0] = 81;
		return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_comwait_fnc()
{
	if (!tokentype) {
		codebuf[1] = 7;
		codebufcnt = 2;
	}
	else {
		if (getdsym((CHAR *) token) == -1) error(ERROR_VARNOTFOUND);
		if ((symtype & ~TYPE_PTR) != TYPE_COMFILE) {
			error(ERROR_BADVARTYPE);
			return(NEXT_TERMINATE);
		}
		putcodeadr(symvalue);
	}
	return(0);
}

static UINT s_nformat_fnc()
{
	static INT n;

	if (rulecnt == 2) n = 0;
	if (symtype == TYPE_INTEGER) {
		putcodehhll((UINT) 0xFF00 + (USHORT) symvalue);
		n += symvalue;
		if (rulecnt > 2 && symvalue) n++;
		if (n > 31) {
			error(ERROR_BADVALUE);
			return(NEXT_TERMINATE);
		}
	}
	return(0);
}

static UINT s_sformat_fnc()
{
	if (symtype == TYPE_INTEGER) {
		if (symvalue >= 64 * 1024) error(ERROR_BADVALUE);
		else if (symvalue < 256) putcodehhll((USHORT) (0xFF00 + symvalue));
		else putcodeadr(vmakenlit(symvalue));
	}
	return(0);
}

static UINT s_and_fnc()
{
	if (symtype == TYPE_INTEGER) {
		if (symvalue < 256 && symvalue >= 0) putcodehhll((USHORT) (0xFF00 + (UCHAR) symvalue));
		else putcodeadr(vmakenlit(symvalue));
	}
	return(0);
}

static UINT s_check10_fnc()
{
	if (delimiter != TOKEN_COMMA) return(NEXT_TERMINATE);
	codebuf[1]++;
	return(0);
}

static UINT s_unload_fnc()
{
	if (codebufcnt > 2) codebuf[1] = 32;
	return(NEXT_TERMINATE);
}

static UINT s_setflag_fnc()
{
	UCHAR c;

	if (rulecnt == 1 && !strcmp((CHAR *) token, "NOT")) {
		whitespace();
		return(0);
	}
	for (c = 0; c < ARRAYSIZE(flagkywrd) && strcmp((CHAR *) token, flagkywrd[c]); c++);
	if (c == ARRAYSIZE(flagkywrd)) error(ERROR_BADKEYWORD);
	if (c == 4) c = 0;	/* ZERO is equivalent to EQUAL */
	if (rulecnt == 2) c += 4;
	codebuf[1] += c;
	return(NEXT_TERMINATE);
}

static UINT s_sqlexec_fnc()
{
	if (!delimiter) {
		putcode1(0xFF);
		return(NEXT_TERMINATE);
	}
	if (rulecnt == 1) {
		if (delimtype == TOKEN_FROM) sc = 1;
		else if (delimtype == TOKEN_INTO) {
			putcode1(0xFE);
			sc = 2;
		}
		else {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
	}
	else if (delimtype == TOKEN_INTO) {
		if (sc == 1) {
			putcode1(0xFE);
			sc = 2;
		}
		else {
			putcode1(0xFF);
			return(NEXT_TERMINATE);
		}
	}
	if (sc == 1) return(RULE_CNSRC + (RULE_LIST << 8)	+ NEXT_PREPOPT);
	else return(RULE_CNVAR + (RULE_LIST << 8) + NEXT_PREPOPT);
}

static UINT s_getparm_fnc()
{
	if (delimiter) {
		if ((rulecnt == 6 && codebuf[1] == 141) || (rulecnt == 7 && codebuf[1] == 142)) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
	}
	else {
		putcode1(0xFF);
		return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_wait_fnc()
{
	if (tokentype == TOKEN_WORD) {
		if (getdsym((CHAR *) token) == -1) error(ERROR_VARNOTFOUND);
		if ((symtype & ~TYPE_PTR) == TYPE_QUEUE ||
		    (symtype & ~TYPE_PTR) == TYPE_COMFILE) {
			putcodeadr(symvalue);
			if (rulecnt == 1) codebuf[1] = 182;
			if (!delimiter) putcode1(0xFF);
			return(0);
		}
	}
	else if (!tokentype) return(NEXT_TERMINATE);
	error(ERROR_BADVARTYPE);
	return(NEXT_TERMINATE);
}

static UINT s_draw_fnc()
{
	INT i1 = 0;
	UCHAR c1;

	if (rulecnt == 1) sc = 0;
	if (!sc) {  /* go parse another keyword, if last parse had a delimiter */
		if (!delimiter) {
			putcode1(0xFF);
			return(NEXT_TERMINATE);
		}
		sc = 1;
		return(RULE_PARSEUP + NEXT_NOPARSE);
	}
	else if (sc == 1) {  /* its a keyword, find it */
		whitespace();
		switch (token[0]) {
			case 'A': i1 = 0; break;
			case 'B': i1 = 1; break;
			case 'C': i1 = 3; break;
			case 'D': i1 = 7; break;
			case 'E': i1 = 8; break;
			case 'F': i1 = 9; break;
			case 'H': i1 = 10; break;
			case 'I': i1 = 12; break;
			case 'L': i1 = 14; break;
			case 'N': i1 = 18; break;
			case 'P': i1 = 19; break;
			case 'R': i1 = 20; break;
			case 'S': i1 = 23; break;
			case 'T': i1 = 24; break;
			case 'V': i1 = 26; break;
			case 'W': i1 = 28; break;
			default: i1 = sizeof(drawkywrd) / sizeof(*drawkywrd);
		}
		for ( ; i1 < (INT) (sizeof(drawkywrd) / sizeof(*drawkywrd)) && strcmp(drawkywrd[i1].name, (CHAR *) token); i1++);
		if (i1 == sizeof(drawkywrd) / sizeof(*drawkywrd)) {
			error(ERROR_BADKEYWORD);
			return(NEXT_TERMINATE);
		}
		c1 = drawkywrd[i1].code;
		if (c1 < 8) {	   /* NEWLINE, LINEFEED, ERASE, DOT */
			putcode1(c1);
			sc = 0;
			return(RULE_NOPARSE + NEXT_LIST);
		}
		else {
			if (line[linecnt++] != '=') goto syntaxerror;
			whitespace();
			if (line[linecnt] == '*') {  /* check for LINETYPE or COLOR */
				if (c1 == 24) c1 = 16;
				else if (c1 == 50) c1 = 48;
				else if (c1 != 8) goto syntaxerror;
				linecnt++;
				sc = 6;
				sd = c1;
				if (c1 == 48) return (RULE_PARSEUP + NEXT_COLON);
				return (RULE_PARSEUP + NEXT_NOPARSE);
			}
			if (line[linecnt] == '-') {
				/* check for HA, VA, IMAGEROTATE, only they can have a negative */
				if (c1 != 46 && (c1 < 30 || c1 > 31)) goto syntaxerror;
				/* if (line[linecnt] == ' ' || line[linecnt] == 9) goto syntaxerror; Cannot possibly happen */
			}
			putcode1(c1);
			if ((c1 >= 24 && c1 <= 31) || c1 == 46) {	/* single numeric operand keyword */
				UINT rule2 = RULE_NSRCDCON + NEXT_NOPARSE;
				sc = 2;
				/**
				 * Many parameters of the draw verb take a single numeric value.
				 * But only HA, VA, and IMAGEROTATE can have a negative value.
				 * (IMAGEROTATE is new to this negative number thing as of 16.1.5)
				 * All others must be >= zero
				 * RULE_DCON forces non-negative, RULE_SCON allows negative.
				 * This code was locked into DCON until I fixed it on 08MAY13 so
				 * the three mentioned above were *not* allowed negative numbers.
				 * Basically HA and VA were broken and nobody noticed.
				 * jpr
				 */
				if (c1 == 46 || c1 == 30 || c1 == 31) rule2 += (RULE_SCON << 8);
				else rule2 += (RULE_DCON << 8);
				return rule2;
			}
			else if (c1 == 32 || c1 == 33) {	/* single cvarlit operand */
				sc = 7;
				return (RULE_CSRC + NEXT_NOPARSE);
			}
			else if (c1 == 34 || c1 == 44) {	/* single cvarlit/nvar operand */
				sc = 7;
				return (RULE_CNSRC + NEXT_NOPARSE);
			}
			else if (c1 == 35) { /* single image operand, IMAGE */
				sc = 7;
				return(RULE_IMAGE + NEXT_NOPARSE);
			}
			else if (c1 >= 36 && c1 <= 39) sc = 3;	/* two operand numeric operand keyword */
			else if (c1 == 40) sc = 5;	/* four operand numeric keyword */
			else if (c1 == 41) {  /* CLIPIMAGE */
				sc = 11;
				return(RULE_IMAGE + NEXT_COLON);
			}
			else if (c1 == 42) {  /* STRETCHIMAGE */
				sc = 9;
				return(RULE_IMAGE + NEXT_COLON);
			}
			else if (c1 == 43 || c1 == 45) {  /* CTEXT, ATEXT */
				sc = 8;
				return(RULE_CNSRC + NEXT_COLON);
			}
			else if (c1 == 50) { /* two operands with second unknown */
				sc = 12;
				sx = codebufcnt - 1;
			}
			return(RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_COLON);
		}
	}
	else {
		switch(sc) {
		case 6:  /* its *BLACK ... or *SOLID ... */
			if (sd == 16 || (sd >= 47 && sd <= 49)) {	    /* COLOR */
				for (i1 = 0; i1 < 8 && strcmp(drawcolors[i1], (CHAR *) token); i1++);
				if (i1 == 8) goto syntaxerror;
				if (sd == 16) c1 = (UCHAR) (i1 + 16);
				else {
					c1 = (UCHAR) i1;
					if (sd == 48) {
						if (line[linecnt] == '*') {  /* check for COLOR */
							linecnt++;
							i1 = 47;
						}
						else i1 = 48;
						putcode1((UCHAR) i1);
					}
				}
			}
			else if (sd == 8) {  /* LINETYPE */
				if (!strcmp((CHAR *) token, "SOLID")) c1 = 8;
				else if (!strcmp((CHAR *) token, "REVDOT")) c1 = 13;
				else goto syntaxerror;
			}
			else goto syntaxerror;
			putcode1(c1);
			if (sd == 48) {  /* get second operand */
				if (i1 == 47) {
					sd = 47;
					return(RULE_PARSEUP + NEXT_NOPARSE);
				}
				sc = 2;
				return(RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_NOPARSE);
			}
			// fall through
		case 7:  /* single non-numeric operand */
			sc = 0;
			return(RULE_NOPARSE + NEXT_LIST);
		case 8:  /* ctext atext */
		case 9:  /* stretchimage */
		case 11:  /* clipimage */
			whitespace();
			sc -= 6;
			if (sc == 2) return(RULE_NSRCDCON + (RULE_SCON << 8) + NEXT_NOPARSE);
			return(RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_COLON);
		case 12:  /* replace nvar:? */
			if (line[linecnt] == '*') {  /* check for COLOR */
				linecnt++;
				codebuf[sx] = sd = 49;
				sc = 7;
			}
			else sc = 3;
			// fall through
		default:	/* multiple numeric operands */
			if (symtype == TYPE_INTEGER) {
				if (symvalue >= 0 && symvalue < 256) putcodehhll((USHORT) (0xFE00 + symvalue));
				else if (symvalue > -256 && symvalue < 0) putcodehhll((USHORT) (0xFD00 - symvalue));
				else if (symvalue < 64 * 1024 && symvalue > 0) {
					putcode1(0xFC);
					putcodellhh((USHORT) symvalue);
				}
				else if (symvalue > -64 * 1024 && symvalue < 0) {
					putcode1(0xFB);
					putcodellhh((USHORT) -symvalue);
				}
				else {
					putcode1(0xFA);
					putcodellhh((USHORT) symvalue);
					putcodellhh((USHORT) (symvalue >> 16));
				}
			}
			if (--sc == 1) {
				sc = 0;
				return(RULE_NOPARSE + NEXT_LIST);
			}
			else {
				whitespace();
				if (sc == 2) return(RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_NOPARSE);
				else if (sc == 6) return(RULE_PARSEUP + NEXT_NOPARSE);
				return(RULE_NSRCDCON + (RULE_DCON << 8) + NEXT_COLON);
			}
		}
	}

syntaxerror:
	error(ERROR_SYNTAX);
	return(NEXT_TERMINATE);
}

static UINT s_return_fnc()
{
	UINT gotolabel;
	INT n, flag;
	UCHAR linecntsave, opcodesave;

	if (delimiter) {
		flag = TRUE;
		n = -1;
		linecntsave = linecnt;
		scantoken(SCAN_UPCASE);
		if (tokentype != TOKEN_LPAREND) {
			if ((n = parseflag(!flag)) < 0) {
				linecnt = linecntsave;  /* put token back */
				scantoken(SCAN_UPCASE); /* rescan not */
				if (!strcmp((CHAR *) token, "NOT")) {
					flag = !flag;
					whitespace();
					linecntsave = linecnt;
					scantoken(SCAN_UPCASE);
				}
				else {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
			}
		}
		if (tokentype == TOKEN_LPAREND && n < 0) {
			linecnt = linecntsave; /* put token back */
			opcodesave = codebuf[0];
			codebufcnt = 0;
			gotolabel = getlabelnum();
			if (parseifexp(gotolabel, flag) < 0) return(NEXT_TERMINATE);
			putcode1(opcodesave);
			deflabelnum(gotolabel);
		}
		else {
			if (n < 8) codebuf[0] += n + 1;
			else {
				codebuf[0] += 9;
				codebuf[1] = (UCHAR) n;
				codebufcnt = 2;
			}
		}
 	}
	else if ((warninglevel & WARNING_LEVEL_1) && 5 == charmap[line[linecnt]]) warning(WARNING_IFCOMMENT);
	return(NEXT_TERMINATE);
}

UINT s_routine_fnc()
{
	UCHAR *undefaddr, n, i, bracketstack[MAXNESTED];
	INT stacksize, arraysizepos, arraylen;

	memset(bracketstack, ' ', MAXNESTED * sizeof(UCHAR));
	if (rulecnt == 1) {
		sc = 0;
		addrfixupsize = 0;
		undfnexist = 0;
		if (codebuf[0] == 110) {  /* routine or lroutine */
			if (dbgflg) {
				dbgrtnrec(label);
				dbglinerec((INT) savlinecnt, savcodeadr);
			}
			if (rlevel + 1 >= RLEVELMAX) {
				error(ERROR_ROUTINEDEPTH);
				return(NEXT_TERMINATE);
			}
			rlevelid[++rlevel] = ++rlevelidcnt;
		}
		else {				 /* endroutine */
			codebufcnt = 0;
			if (!rlevel) {
				error(ERROR_MISSINGROUTINE);
				return(NEXT_TERMINATE);
			}
			if (dbgflg) dbgendrec();
			rlevel--;
			return(NEXT_TERMINATE);
		}
		if (tokentype != TOKEN_WORD && tokentype != TOKEN_TILDE) {
			putcode1(0xFF);
			return(NEXT_TERMINATE);
		}
	}
	if (sc == 2) {  /* just parsed delimiter */
		if (!delimiter) return(NEXT_TERMINATE);
		sc = 0;
		return(RULE_PARSEVAR + NEXT_NOPARSE);
	}
	if (tokentype == TOKEN_WORD) {
		rtncode = codebuf[0];
		codebufcnt = 0;
		if (addrfixup == NULL || addrfixupalloc - addrfixupsize < 100) {
			addrfixupalloc += 500;
			dbcmem(&addrfixup, addrfixupalloc);
		}
		undfnexist++;
		undefaddr = (*addrfixup);
		n = (UCHAR)strlen((CHAR *) token);
		arraysizepos = addrfixupsize;
		undefaddr[addrfixupsize++] = n + sc;
		if (sc) undefaddr[addrfixupsize++] = '~';
		for (i = 0; i < n; i++) undefaddr[addrfixupsize + i] = token[i];
		addrfixupsize += n;
		if (line[linecnt] == '[' || line[linecnt] == '(') {
			if (sc) {
				error(ERROR_BADARRAYSPEC);
				return(NEXT_TERMINATE);
			}
			stacksize = -1;
			arraylen = 0;
			do {
				if (addrfixupalloc - addrfixupsize - arraylen < 100) {
					addrfixupalloc += 500;
					dbcmem(&addrfixup, addrfixupalloc);
				}
				switch (line[linecnt]) {
					case 0:
						error(ERROR_BADARRAYSPEC);
						return(NEXT_TERMINATE);
					case '(':
						if (++stacksize >= MAXNESTED) {
							error(ERROR_EXPSYNTAX);
							return(NEXT_TERMINATE);
						}
						bracketstack[stacksize] = ')';
						break;
					case ')':
						if (bracketstack[stacksize] == ')') stacksize--;
						else {
							error(ERROR_BADARRAYSPEC);
							return(NEXT_TERMINATE);
						}
						break;
					case ':':
						if (getln() == 1) {
							error(ERROR_BADARRAYSPEC);
							return(NEXT_TERMINATE);
						}
						undefaddr[addrfixupsize + arraylen] = ' ';
						arraylen++;
						linecnt = 0;
						whitespace();
						break;
					case '[':
						if (++stacksize >= MAXNESTED) {
							error(ERROR_EXPSYNTAX);
							return(NEXT_TERMINATE);
						}
						bracketstack[stacksize] = ']';
						break;
					case ']':
						if (bracketstack[stacksize] == ']') stacksize--;
						else {
							error(ERROR_BADARRAYSPEC);
							return(NEXT_TERMINATE);
						}
						break;
				}
				undefaddr[addrfixupsize + arraylen] = line[linecnt++];
				arraylen++;
			} while (stacksize || line[linecnt] != bracketstack[stacksize]);
			undefaddr[addrfixupsize + arraylen] = line[linecnt++];
			arraylen++;
			addrfixupsize += arraylen;
			undefaddr[arraysizepos] += arraylen;
		}
		sc = 2; /* parse delimiter next */
		return(RULE_NOPARSE + NEXT_LIST);
	}
	if (tokentype == TOKEN_TILDE) {
		sc = 1;  /* parse label var next */
		return(RULE_PARSEVAR + NEXT_NOPARSE);
	}
	return(NEXT_TERMINATE);
}

static UINT s_xcall_fnc()
{
	if (delimiter) return(0);	/* XCALL with parms */
	putcode1(0xFF);
	return(NEXT_TERMINATE);
}

#define CALLSTATE_PARSEIFPREP 0
#define CALLSTATE_PARSELABEL 1
#define CALLSTATE_PARSEPARAM 2

static UINT s_call_fnc()
{
	INT n, flag;
	static UINT calllabel;
	static UCHAR mthdcallflg;
	UINT gotolabel;
	UCHAR linecntsave;

	if (1 == rulecnt) {
		codebufcnt = 0;
		if (':' == line[linecnt]) {	/* CALL method:obj syntax */
			if (tokentype == TOKEN_WORD) {
				if (usepsym((CHAR *) token, LABEL_METHODREF) < 0) return(NEXT_TERMINATE);
			}
			else {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			mthdcallflg = TRUE;
			sc = CALLSTATE_PARSEPARAM;
			linecnt++;
			whitespace();
			if (makemethodref((UINT) symvalue) < 0) return(NEXT_TERMINATE);
			return(RULE_OBJECT + NEXT_PREPOPT);
		}
		else {
			if (tokentype == TOKEN_WORD) {
				usepsym((CHAR *) token, LABEL_REF);
			}
			else {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			if (TYPE_PLABELMREF == symtype && !childclsflag) {
				error(ERROR_BADMETHODCALL);
				return(NEXT_TERMINATE);
			}
			calllabel = symvalue;
			sc = CALLSTATE_PARSEIFPREP;
			mthdcallflg = FALSE;
			return(RULE_NOPARSE + NEXT_IFPREPOPT);
		}
	}
	switch (sc) {
		case CALLSTATE_PARSEIFPREP:
			if (delimiter == TOKEN_PREP) break;
			if (delimiter == TOKEN_IF) {
				flag = TRUE;
				n = -1;
				linecntsave = linecnt;
				scantoken(SCAN_UPCASE);
				if (tokentype != TOKEN_LPAREND) {
					if ((n = parseflag(!flag)) < 0) {
						linecnt = linecntsave;  /* put token back */
						scantoken(SCAN_UPCASE); /* rescan not */
						if (!strcmp((CHAR *) token, "NOT")) {
							flag = !flag;
							whitespace();
							linecntsave = linecnt;
							scantoken(SCAN_UPCASE);
						}
						else {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
					}
				}
				if (tokentype == TOKEN_LPAREND && n < 0) {
					linecnt = linecntsave; /* put token back */
					gotolabel = getlabelnum();
					if (parseifexp(gotolabel, flag) < 0) return(NEXT_TERMINATE);
					makegoto(calllabel);
					if (codebuf[0] == 201) codebuf[0] = 203;
					else codebuf[0] -= 4;
					deflabelnum(gotolabel);
					return(NEXT_TERMINATE);
				}
				makecondgoto(calllabel, n);
			}
			else {
				if (warninglevel & WARNING_LEVEL_1) {
					/* If following the label there is a '\0' or whitespace-'\0'
					 * then it is *not* a problem.
					 */
					if (line[linecnt] != '\0') {
						if (!isspace(line[linecnt])) warning(WARNING_IFCOMMENT);
						else {
							int i99 = linecnt;
							while (isspace(line[i99++])) ;
							if (line[linecnt] != '\0') warning(WARNING_IFCOMMENT);
						}
					}
				}
				if (warninglevel & WARNING_LEVEL_2) {
					if (strlen((const char*)token) > 0) warning(WARNING_IFCOMMENT);
				}
				makegoto(calllabel);
			}
			if (codebuf[0] > 200) codebuf[0] += 2;
			else if (codebuf[0] < 160) codebuf[0] -= 4;
			else codebuf[0] -= 32;
			return(NEXT_TERMINATE);
		case CALLSTATE_PARSELABEL:
			usepsym((CHAR *) token, LABEL_REFFORCE);
			putcodeadr(0x800000 + symvalue);
			// fall through
			/* no break */
		case CALLSTATE_PARSEPARAM:
			if (!delimiter) {
				if (!mthdcallflg) {
					if (calllabel < SIMPLELABELBIAS) { /* label table type label */
						putmain1(111);
						putmainhhll(calllabel);
					}
					else {
						putmain1(205);
						mainpoffset(calllabel);
					}
				}
				putcode1(0xFF);
				return(NEXT_TERMINATE);
			}
			break;
	}
	if ('~' == line[linecnt]) {
		linecnt++;
		sc = CALLSTATE_PARSELABEL;
		return(RULE_PARSEVAR + NEXT_LIST);
	}
	sc = CALLSTATE_PARSEPARAM;
	tmpvarflg = 3;  /* do NOT reuse temporary variables in parm expressions */
	return(RULE_ANYSRC + (RULE_EQUATE << 8) + NEXT_LIST);
}

static UINT s_goto_fnc()
{
	UINT gotolabel, gotoaround;

	codebufcnt = 0;
	gotolabel = symvalue;
	if (delimiter) {
		if ((UINT)symvalue >= SIMPLELABELBIAS) parseifexp((UINT) symvalue, FALSE);
		else {
			gotoaround = getlabelnum();
			parseifexp(gotoaround, TRUE);
			makegoto(gotolabel);
			deflabelnum(gotoaround);
		}
	}
	else {
		if ((warninglevel & WARNING_LEVEL_1) && 5 == charmap[line[linecnt]]) warning(WARNING_IFCOMMENT);
		makegoto((UINT) symvalue);
	}
	return(NEXT_TERMINATE);
}

static UINT s_for_fnc()
{
	static UCHAR flag;
	UINT linecntsave;
	static UINT labelnum1, labelnum2;

	if (rulecnt == 1) {
		if (looplevel == LOOPLEVELMAX - 1) {
			error(ERROR_LOOPDEPTH);
			return(NEXT_TERMINATE);
		}
		su = 1;
	}
	switch (rulecnt) {
		case 1:
			loopid[++looplevel] = ++loopnextid;
			loopifid[looplevel] = ifid[iflevel];
			loopswitchid[looplevel] = switchid[switchlevel];
			codebufcnt--;
			memmove(codebuf, &codebuf[1], (size_t) codebufcnt); // @suppress("Type cannot be resolved")
			savecode(1);
			codebufcnt = 0;
			flag = 0;  /* increment for by variable amount (default value) */
			return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREP);
		case 2:
			savecode(2);
			codebufcnt = 0;
			putcodehhll(0xC026);  /* no filepi/flag change, move op2, op1 */
			restorecode(2);
			restorecode(1);
			linecntsave = linecnt;
			scantoken(SCAN_UPCASE);
			su = FALSE;
			if (tokentype == TOKEN_WORD) {
				scantoken(SCAN_UPCASE);
				if (tokentype != TOKEN_LPAREND && tokentype != TOKEN_LBRACKET) {
					su = TRUE;    /* generate "simple" for code */
					makegoto(getlabelnum());
					deflabelnum(getlabelnum());
				}
			}
			linecnt = linecntsave;
			if (!su) {
				labelnum1 = getlabelnum();
				looplabel[looplevel] = getlabelnum();
				labelnum2 = getlabelnum();
				deflabelnum(labelnum2);
			}
			putcodeout();	  /* guarantee that simplefixup address will remain correct */
			return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_PREPOPT);
		case 3:
			savecode(3);
			codebufcnt = 0;
			if (delimiter) return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_END);
			else {
				flag = 1;   /* increment for by pos. constant value */
				symvalue = makenlit("1");
				symtype = TYPE_NUMBER;
				putcodeadr(symvalue);
				savecode(4);
			}
			break;
		case 4:
			savecode(4);
			if (symtype == TYPE_NUMBER) {
				if (token[0] == '-') flag = 2;	 /* increment for by neg. constant value */
				else flag = 1;	/* increment for by pos. constant value */
			}
			break;
	}
	codebufcnt = 0;
	if (su) {     /* generate "simple" for code - do not have expressions */
		looplabel[looplevel] = getlabelnum();
		putcodehhll(0xC002);	/* op1 += op4 */
		restorecode(4);
		restorecode(1);
		deflabelnum(looplabel[looplevel] - 2);
		if (!flag) {
			putcode1(206); /* gotoiffor */
			restorecode(4);
		}
		else if (flag == 1) putcode1(198);	/* gotoifgt */
		else putcode1(197);	/* gotoiflt */
		restorecode(1);
		restorecode(3);
		makepoffset(looplabel[looplevel]);
	}
	else {
		if (!flag) {
			putcode1(206); /* gotoiffor */
			restorecode(4);
		}
		else if (flag == 1) putcode1(198);	/* gotoifgt */
		else putcode1(197);	/* gotoiflt */
		restorecode(1);
		restorecode(3);
		makepoffset(looplabel[looplevel]);
		makegoto(getlabelnum());
		deflabelnum(labelnum1);
		putcodehhll(0xC002);	/* ADD op4, op1, op1 */
		restorecode(4);
		restorecode(1);
		makegoto(labelnum2);
		deflabelnum(labelnum2 + 1);
	}
	return(NEXT_TERMINATE);
}

static UINT s_loop_fnc()
{
	UCHAR c;

	codebufcnt = 0;
	if (looplevel == LOOPLEVELMAX - 1) error(ERROR_LOOPDEPTH);
	else {
		loopid[++looplevel] = ++loopnextid;
		loopifid[looplevel] = ifid[iflevel];
		loopswitchid[looplevel] = switchid[switchlevel];
		deflabelnum(getlabelnum());
		looplabel[looplevel] = getlabelnum();
	}
	if (tokentype == TOKEN_WORD) {
		if (!strcmp((CHAR *) token, "WHILE")) c = 134;
		else if (!strcmp((CHAR *) token, "UNTIL")) c = 135;
		else c = 0;
		if (c) {
			putcode1(c);
			s_while_fnc();
		}
	}
	return(NEXT_TERMINATE);
}

static UINT s_break_fnc()
{
	UINT labelnum;

	codebufcnt = 0;
	if (!looplevel) error(ERROR_MISSINGLOOP);
	else {
		labelnum = looplabel[looplevel];
		if (codebuf[0] == 132) labelnum--;
		if (delimiter) parseifexp(labelnum, FALSE);
		else {
			if ((warninglevel & WARNING_LEVEL_1) && 5 == charmap[line[linecnt]]) warning(WARNING_IFCOMMENT);
			makegoto(labelnum);
		}
	}
	return(NEXT_TERMINATE);
}

static UINT s_while_fnc()
{
	UCHAR flag;

	codebufcnt = 0;
	if (!looplevel) error(ERROR_MISSINGLOOP);
	else {
		if (codebuf[0] == 134) flag = TRUE;
		else flag = FALSE;
		whitespace();
		if (parseifexp(looplabel[looplevel], flag) < 0) return(NEXT_TERMINATE);
	}
	return(NEXT_TERMINATE);
}

static UINT s_repeat_fnc()
{
	UCHAR c;
	UINT gotolabel;

	codebufcnt = 0;
	if (tokentype == TOKEN_WORD) {
		if (!strcmp((CHAR *) token, "WHILE")) c = 134;
		else if (!strcmp((CHAR *) token, "UNTIL")) c = 135;
		else c = 0;
		if (c) {
			putcode1(c);
			s_while_fnc();
		}
	}
	if (!looplevel) error(ERROR_MISSINGLOOP);
	else if (loopifid[looplevel] != ifid[iflevel]) error(ERROR_BADIFLEVEL);
	else if (loopswitchid[looplevel] != switchid[switchlevel]) error(ERROR_BADSWITCHLEVEL);
	else {
		gotolabel = looplabel[looplevel--];
		makegoto(gotolabel - 1);
		deflabelnum(gotolabel);
	}
	return(NEXT_TERMINATE);
}

static UINT s_if_fnc()
{
	codebufcnt = 0;
	if (iflevel == IFLEVELMAX - 1) error(ERROR_IFDEPTH);
	else {
		ifid[++iflevel] = ++ifnextid;
		ifloopid[iflevel] = loopid[looplevel];
		ifswitchid[iflevel] = switchid[switchlevel];
		ifstate[iflevel] = 1;
		iflabel1[iflevel] = getlabelnum();
		if (parseifexp(iflabel1[iflevel], TRUE) < 0) return(NEXT_TERMINATE);
	}
	return(NEXT_TERMINATE);
}

static UINT s_else_fnc()
{

	codebufcnt = 0;
	if (!iflevel || ifstate[iflevel] == 3) error(ERROR_MISSINGIF);
	else if (ifloopid[iflevel] != loopid[looplevel]) error(ERROR_BADLOOPLEVEL);
	else if (ifswitchid[iflevel] != switchid[switchlevel]) error(ERROR_BADSWITCHLEVEL);
	else {
		if (ifstate[iflevel] == 1) {
			ifstate[iflevel] = 2;
			iflabel2[iflevel] = getlabelnum();
		}
		makegoto(iflabel2[iflevel]);
		deflabelnum(iflabel1[iflevel]);
		if (delimiter) {
			iflabel1[iflevel] = getlabelnum();
			putcodeout();
			if (parseifexp(iflabel1[iflevel], TRUE) < 0) return(NEXT_TERMINATE);
		}
		else ifstate[iflevel] = 3;
	}
	savcodeadr += 4;
	return(NEXT_TERMINATE);
}

UINT s_endif_fnc()
{

	codebufcnt = 0;
	if (!iflevel) error(ERROR_MISSINGIF);
	else {
		if (ifloopid[iflevel] != loopid[looplevel]) error(ERROR_BADLOOPLEVEL);
		else if (ifswitchid[iflevel] != switchid[switchlevel]) error(ERROR_BADSWITCHLEVEL);
		else {
			switch (ifstate[iflevel]) {
				case 1:
					deflabelnum(iflabel1[iflevel]);
					break;
				case 2:
					deflabelnum(iflabel1[iflevel]);
					/* no break */
					// fall through
				case 3:
					deflabelnum(iflabel2[iflevel]);
					break;
			}
		}
		iflevel--;
	}
	return(NEXT_TERMINATE);
}

static UINT s_switch_fnc()
{
	if (switchlevel == SWITCHLEVELMAX - 1) error(ERROR_SWITCHDEPTH);
	else {
		switchid[++switchlevel] = ++switchnextid;
		switchloopid[switchlevel] = loopid[looplevel];
		switchifid[switchlevel] = ifid[iflevel];
		memcpy(switchvar[switchlevel], &codebuf[1], (size_t) (codebufcnt - 1)); // @suppress("Symbol is not resolved")
		switchvarlen[switchlevel] = (UCHAR) (codebufcnt - 1);
		switchtype[switchlevel] = baseclass((UCHAR) symtype);
		switchstate[switchlevel] = 1;
		switchlabel1[switchlevel] = getlabelnum();
		switchlabel2[switchlevel] = 0;
		codebufcnt = 0;
	}
	return(NEXT_TERMINATE);
}

static UINT s_case_fnc()
{
	static UINT gotolabel;

	if (rulecnt == 1) {
		codebufcnt = 0;
		if (!switchlevel || switchstate[switchlevel] != 1) {
			error(ERROR_MISSINGSWITCH);
			return(NEXT_TERMINATE);
		}
		else if (switchloopid[switchlevel] != loopid[looplevel]) error(ERROR_BADLOOPLEVEL);
		else if (switchifid[switchlevel] != ifid[iflevel]) error(ERROR_BADIFLEVEL);
		else if (switchlabel2[switchlevel]) {
			makegoto(switchlabel1[switchlevel]);
			deflabelnum(switchlabel2[switchlevel]);
			savcodeadr += 4;
			putcodeout();
		}
	}
	else if (delimiter) {
		codebuf[sx] = 195;  /* gotoifeq */
		if (rulecnt == 2) gotolabel = getlabelnum();
		makepoffset(gotolabel);
		putcodeout();
	}
	else {
		switchlabel2[switchlevel] = getlabelnum();
		makepoffset(switchlabel2[switchlevel]);
		if (rulecnt > 2) deflabelnum(gotolabel);
		return(NEXT_TERMINATE);
	}
	sx = codebufcnt;
	putcode1(196);  /* gotoifneq */
	memcpy(&codebuf[codebufcnt], switchvar[switchlevel], switchvarlen[switchlevel]);
	codebufcnt += switchvarlen[switchlevel];
	if (switchtype[switchlevel] == TYPE_CVAR) return(RULE_CSRC + NEXT_CASE);
	else return(RULE_NSRC + (RULE_EQUATE << 8) + NEXT_CASE);
}

UINT s_endswitch_fnc()
{

	codebufcnt = 0;
	if (!switchlevel || (switchstate[switchlevel] == 2 && codebuf[0] == 141)) error(ERROR_MISSINGSWITCH);
	else if (switchloopid[switchlevel] != loopid[looplevel]) error(ERROR_BADLOOPLEVEL);
	else if (switchifid[switchlevel] != ifid[iflevel]) error(ERROR_BADIFLEVEL);
	else {
		if (codebuf[0] == 141) {  /* default */
			if (switchlabel2[switchlevel]) {
				makegoto(switchlabel1[switchlevel]);
				savcodeadr += 4;
				deflabelnum(switchlabel2[switchlevel]);
				switchlabel2[switchlevel] = 0;
			}
			switchstate[switchlevel] = 2;
		}
		else {  /* endswitch */
			if (switchlabel2[switchlevel])
				deflabelnum(switchlabel2[switchlevel]);
			deflabelnum(switchlabel1[switchlevel--]);
		}
	}
	return(NEXT_TERMINATE);
}

static UINT s_tabpage_fnc()
{
	codebufcnt = 0;
	return(NEXT_TERMINATE);
}

UINT s__conditional_fnc()
{
	UCHAR curstate;

	codebufcnt = 0;	
	sx = codebuf[0];
	switch (sx) {
		case 175: /* %ELSEIF */
			if (CCD_STACKEMPTY()) {
				nocompile = FALSE;
				error(ERROR_MISSINGCCDIF);
				return(NEXT_TERMINATE);
			}
			curstate = POP_CCDSTACK() & ~CCD_IFOPDIR;
			nocompile = POP_CCDSTACK();
			if (curstate == CCD_ELSE) {
				nocompile = FALSE;
				error(ERROR_MISSINGCCDENDIF);
				return(NEXT_TERMINATE);
			}
			if (curstate != CCD_IF_NOTCOMPILED || nocompile) {
				PUSH_CCDSTACK(nocompile);
				PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
				nocompile = TRUE;	/* turn off compilation */
				return(NEXT_TERMINATE);
			}
			break;
		case 176: /* %ELSE */
			if (CCD_STACKEMPTY()) {
				nocompile = FALSE;
				error(ERROR_MISSINGCCDIF);
				return(NEXT_TERMINATE);
			}
			curstate = POP_CCDSTACK() & ~CCD_IFOPDIR;
			nocompile = POP_CCDSTACK();
			if (curstate == CCD_ELSE) {
				nocompile = FALSE;
				error(ERROR_MISSINGCCDENDIF);
				return(NEXT_TERMINATE);
			}
			PUSH_CCDSTACK(nocompile);
			PUSH_CCDSTACK(CCD_ELSE | ccdifopdir);
			if (curstate == CCD_IF_NOTCOMPILED && !nocompile) {
				nocompile = FALSE;	/* turn on compilation */
			}
			else {
				nocompile = TRUE;	/* turn off compilation */
			}
			return(NEXT_TERMINATE);
		case 177:	/* %ENDIF */
			if (CCD_STACKEMPTY()) {
				nocompile = FALSE;
				error(ERROR_MISSINGCCDIF);
				return(NEXT_TERMINATE);
			}
			//A static analyzer will say that curstate is not used before the return,
			//but POP_CCDSTACK has a side effect
			curstate = POP_CCDSTACK() & ~CCD_IFOPDIR;
			nocompile = POP_CCDSTACK();
			return(NEXT_TERMINATE);
		case 178:	/* XIF */
			while (!CCD_STACKEMPTY() && (TOPOF_CCDSTACK() & CCD_IFOPDIR)) {
				//A static analyzer will say that curstate is not used before the return,
				//but POP_CCDSTACK has a side effect
				curstate = POP_CCDSTACK() & ~CCD_IFOPDIR;
				nocompile = POP_CCDSTACK();
			}
			if (CCD_STACKEMPTY()) nocompile = FALSE;
			return(NEXT_TERMINATE);
		default:
			if (CCD_STACKOVERFLOW()) {
				error(ERROR_CCIFDEPTH);
				return(NEXT_TERMINATE);
			}
			if (nocompile) {
				PUSH_CCDSTACK(nocompile);
				if (sx < 164 || (sx > 168 && sx < 175)) PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | CCD_IFOPDIR);
				else PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | ccdifopdir);
				return(NEXT_TERMINATE);
			}
			break;
	}
	return(0);	
}

UINT s__if_fnc()
{
	UINT y;

	if (rulecnt == 2) {
		su = (UINT) symvalue;
		sc = delimiter;
		return(0);
	}
	y = (UINT) symvalue;
	PUSH_CCDSTACK(nocompile);
	nocompile = FALSE;
	switch (sc) {
		case TOKEN_EQUAL:
			if (su != y) nocompile = TRUE;
			break;
		case TOKEN_NOTEQUAL:
			if (su == y) nocompile = TRUE;
			break;
		case TOKEN_GREATER:
			if (su <= y) nocompile = TRUE;
			break;
		case TOKEN_LESS:
			if (su >= y) nocompile = TRUE;
			break;
		case TOKEN_NOTGREATER:
			if (su > y) nocompile = TRUE;
			break;
		case TOKEN_NOTLESS:
			if (su < y) nocompile = TRUE;
			break;
 	}
	if (nocompile) PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | ccdifopdir);
	else PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
	return(NEXT_TERMINATE);
}

static UINT s__ifz_fnc()
{
	if (((sx == 165 || sx == 162) && !symvalue) || ((sx == 166 || sx == 163) && symvalue)) {
		PUSH_CCDSTACK(nocompile);
		nocompile = FALSE;
		if (sx < 164) PUSH_CCDSTACK(CCD_IF_COMPILED | CCD_IFOPDIR);
		else PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
		return(NEXT_TERMINATE);
	}
	if (!delimiter) {
		PUSH_CCDSTACK(nocompile);
		nocompile = TRUE;
		if (sx < 164) PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | CCD_IFOPDIR);
		else PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | ccdifopdir);
		return(NEXT_TERMINATE);
	}
	return(0);
}

UINT s__ifdef_fnc()
{
	if (tokentype != TOKEN_WORD) {
		error(ERROR_SYNTAX);
		return(NEXT_TERMINATE);
	}
	switch (sx) {
		case 167:		/* %IFDEF */
			if (getdsym((CHAR *) token) != -1) {
				PUSH_CCDSTACK(nocompile);
				nocompile = FALSE;
				PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
				return(NEXT_TERMINATE);
			}
			break;
		case 168:		/* %IFNDEF */
			if (getdsym((CHAR *) token) == -1) {
				PUSH_CCDSTACK(nocompile);
				nocompile = FALSE;
				PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
				return(NEXT_TERMINATE);
			}
			break;
		case 179:		/* %IFLABEL */
			if (getpsym((CHAR *) token) != -1) {
				PUSH_CCDSTACK(nocompile);
				nocompile = FALSE;
				PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
				return(NEXT_TERMINATE);
			}
			break;
		case 180:		/* %IFNLABEL */
			if (getpsym((CHAR *) token) == -1) {
				PUSH_CCDSTACK(nocompile);
				nocompile = FALSE;
				PUSH_CCDSTACK(CCD_IF_COMPILED | ccdifopdir);
				return(NEXT_TERMINATE);
			}
			break;
	}
	if (!delimiter) {
		PUSH_CCDSTACK(nocompile);
		nocompile = TRUE;
		PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | ccdifopdir);
		return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_ifeq_fnc()
{
	UINT y;

	if (rulecnt == 2) {
		su = (UINT) symvalue;
		return(0);
	}
	y = (UINT) symvalue;
	PUSH_CCDSTACK(nocompile);
	nocompile = FALSE;
	switch (sx) {
		case 169:
			if (su != y) nocompile = TRUE;
			break;
		case 170:
			if (su == y) nocompile = TRUE;
			break;
		case 171:
			if (su <= y) nocompile = TRUE;
			break;
		case 172:
			if (su >= y) nocompile = TRUE;
			break;
		case 173:
			if (su < y) nocompile = TRUE;
			break;
		case 174:
			if (su > y) nocompile = TRUE;
			break;
	}
	if (nocompile) PUSH_CCDSTACK(CCD_IF_NOTCOMPILED | CCD_IFOPDIR);
	else PUSH_CCDSTACK(CCD_IF_COMPILED | CCD_IFOPDIR);
	return(NEXT_TERMINATE);
}

UINT s_define_fnc()
{
	codebufcnt = 0;
	if (chkdefine(label)) {
		error(ERROR_DUPDEFINELABEL);
		return(NEXT_TERMINATE);
	}
	else {
		whitespace();
		putdefine(label);
	}
	return(NEXT_TERMINATE);
}

UINT s_equate_fnc()
{
	codebufcnt = 0;
	symtype = TYPE_EQUATE;
	putdsym(label, FALSE);
	return(NEXT_TERMINATE);
}

UINT s_include_fnc()
{
	INT n;

	codebufcnt = 0;
	n = 0;
	whitespace();
	while (line[linecnt] && charmap[line[linecnt]] != 2) {
		token[n++] = line[linecnt];
		linecnt++;
	}
	token[n] = '\0';
	n = srcopen((CHAR *) token);
	if (n == 1) error(ERROR_INCLUDENOTFOUND);
	else if (n == 2) error(ERROR_INCLUDEDEPTH);
	else if (n) error(ERROR_INCLUDEFILEBAD);
	return(NEXT_TERMINATE);
}

UINT s_liston_fnc()
{
	codebufcnt = 0;
	if (codebuf[0] == 144) liston = TRUE;
	else liston = FALSE;
	return(NEXT_TERMINATE);
}

void putdataname(CHAR *varname)
{
	UINT recnamelen = 0, namelen;
	struct recordaccess *currecptr;
	BOOL reclvl = recordlevel; // @suppress("Type cannot be resolved")

	putdata1(0xE2);
	namelen = (int)strlen(varname);
	if (reclvl) {
		currecptr = &(*recindexptr)[currechash];
		recnamelen = (int)strlen(currecptr->name) + 1;
	}
 	putdata1(namelen + recnamelen);
	if (reclvl) {
		putdata((UCHAR *) currecptr->name, (INT) (recnamelen - 1));
		putdata1('.');
	}
	putdata((UCHAR *) varname, (INT) namelen);
	savdataadr = dataadr += namelen + recnamelen + 2;
}

/* define the Finite State Machines used by the s_charnum_fnc() */
#define FSM_CHAR_NUM_SPEC 1		/* main FSM */
#define FSM_SIZE_SPEC 2			/* parse the field size portion */
#define FSM_ARRAY_SPEC 3			/* parse the array dimension portion */
#define FSM_INITIALIZER_LIST 4	/* parse an initializer list following , INITIAL */

/* define the states within the Finite State Machine FSM_CHAR_NUM_SPEC */
#define CNUM_FSM_START 0
#define CNUM_FSM_COMMONVAR 1
#define CNUM_FSM_SIZE 2
#define CNUM_FSM_ARRAY 3
#define CNUM_FSM_ADDROFARRAY 4
#define CNUM_FSM_COMMAINITIAL 5
#define CNUM_FSM_LITERAL 6
#define CNUM_FSM_FINISH 7

/* define the states within the Finite State Machine FSM_SIZE_SPEC */
#define SIZE_FSM_PERIODSTART 0
#define SIZE_FSM_WORDSTART 1
#define SIZE_FSM_NUMSTART 2
#define SIZE_FSM_NUMPERIOD 3
#define SIZE_FSM_PERIODEQUATE 4
#define SIZE_FSM_NUMPERIODNUM 5

/* define the states within the Finite State Machine FSM_ARRAY_SPEC */
#define ARRAY_FSM_START 0
#define ARRAY_FSM_1STDIM 1
#define ARRAY_FSM_2NDDIM 2
#define ARRAY_FSM_3RDDIM 3

/* define the states within the Finite State Machine FSM_INITIALIZER_LIST */
#define INITIALIZER_ADDRVAR 0
#define INITIALIZER_ELEMENT 1 
#define INITIALIZER_DELIMITER 2
#define INITIALIZER_WRITEARRAYADDR 3
#define INITIALIZER_WRITEADDR 4
#define INITIALIZER_NEXTADDR 5
#define INITIALIZER_ADDRDELIMITER 6

/* define the data types parsed by the s_charnum_fnc() */
#define DATATYPE_CHAR 0
#define DATATYPE_NUM 1
#define DATATYPE_INT 2
#define DATATYPE_FLOAT 3
#define DATATYPE_VAR 5

UINT s_charnum_fnc()  /* CHAR, NUM, INT, FLOAT, VAR processing */
{
	static INT inheritableflg;
 	static INT curfsm, returnfsm;
	static INT cnumstate, sizestate, arraystate, initstate;
	static UINT numdim, arraydim1, arraydim2, arraydim3;
	static INT commonflg, addrvarflg, globalflg, initialflg;
	static INT datatype, leftsize, rightsize;
	static INT savdadr;
	static UINT addrvartype, savsymtype;
	static UINT retrule;
	static UINT dimcnt, targetdim, dim1, dim2, dim3;
	UCHAR linecntsave;
	INT curtokpos, toklen, tokpos;
	CHAR curchar;
	INT arraysize;
	UINT arrayshape[4];

	if (1 == rulecnt) {		/* initial call to s_charnum_fnc() */
		codebufcnt = 0;
		savdadr = dataadr;
		curfsm = FSM_CHAR_NUM_SPEC;
		cnumstate = CNUM_FSM_START;
		numdim = 0;
		arraydim1 = arraydim2 = arraydim3 = 0;
		rightsize = leftsize = -1;
		commonflg = addrvarflg = initialflg = FALSE;
		inheritableflg = FALSE;
		if (recordlevel) recdefhdr(label);
		if (codebuf[0] > 218) {
			putdata1(0xF3);	/* prefix byte - global var follows */
			putdata((UCHAR *) label, (INT) (strlen(label) + 1));
			datatype = codebuf[0] - 219;
			globalflg = TRUE;
		}
		else {
			if (withnamesflg) {
				putdataname(label);
				savdadr = dataadr;
			}
			datatype = codebuf[0] - 201;
			globalflg = FALSE;
		}
 	}

	/* parse spec following var type */
	do {
		switch (curfsm) {	/* execute within current Finite State Machine */
			case FSM_CHAR_NUM_SPEC:		/* DIM, FORM, INT, FLOAT parse FSM */
				switch (cnumstate) {
					case CNUM_FSM_START:
						switch (tokentype) {
							case TOKEN_WORD:
								if (DATATYPE_VAR == datatype) {
									error(ERROR_SYNTAX);	
									return(NEXT_TERMINATE);
								}
								curfsm = FSM_SIZE_SPEC;
								cnumstate = CNUM_FSM_SIZE;
								sizestate = SIZE_FSM_WORDSTART;
								break;
							case TOKEN_LITERAL:
								cnumstate = CNUM_FSM_LITERAL;
								break;
							case TOKEN_NUMBER:
								if (DATATYPE_VAR == datatype) {
									error(ERROR_SYNTAX);	
									return(NEXT_TERMINATE);
								}
								curfsm = FSM_SIZE_SPEC;
								cnumstate = CNUM_FSM_SIZE;
								sizestate = SIZE_FSM_NUMSTART;
								break;
							case TOKEN_PERIOD:
								if (DATATYPE_VAR == datatype) {
									error(ERROR_SYNTAX);	
									return(NEXT_TERMINATE);
								}
								curfsm = FSM_SIZE_SPEC;
								cnumstate = CNUM_FSM_SIZE;
								sizestate = SIZE_FSM_PERIODSTART;
								break;
							case TOKEN_ATSIGN:
								if (globalflg) {
									error(ERROR_GLOBALADRVAR);
									return(NEXT_TERMINATE);
								}
								addrvarflg = TRUE;
								if (line[linecnt] && charmap[line[linecnt]] != 2) {
									if (line[linecnt] != '[' && line[linecnt] != '(') {
										cnumstate = CNUM_FSM_COMMAINITIAL;
										return(RULE_NOPARSE + NEXT_COMMA);
									}
									if (DATATYPE_VAR == datatype) {
										error(ERROR_SYNTAX);	
										return(NEXT_TERMINATE);
									}
									linecnt++;
									cnumstate = CNUM_FSM_ADDROFARRAY;
									break;
								}
								cnumstate = CNUM_FSM_FINISH;
								break;
							case TOKEN_AMPERSAND:
								if (inheritableflg) {	/* ampersand already parsed */
									error(ERROR_SYNTAX);
									return(NEXT_TERMINATE);
								}
								if (globalflg) error(ERROR_NOGLOBALINHERITS);
								if ('&' == line[linecnt]) {
									linecnt++;
									if (DATATYPE_VAR == datatype) s_inherited_fnc(TYPE_VVARPTR);
									else if (datatype != DATATYPE_CHAR) s_inherited_fnc(TYPE_NVAR);
									else s_inherited_fnc(TYPE_CVAR);
								}
								else {
									if (!classlevel) error(ERROR_INHRTABLENOTALLOWED);
									else { 
										inheritableflg = TRUE;
										return(RULE_PARSEVAR + NEXT_NOPARSE);
									}
								}
								return(NEXT_TERMINATE);
							case TOKEN_ASTERISK:
								if (inheritableflg) {	/* ampersand already parsed */
									error(ERROR_SYNTAX);
									return(NEXT_TERMINATE);
								}
								if (globalflg) {
									error(ERROR_BADCOMMON);
									return(NEXT_TERMINATE);
								}
								if (DATATYPE_VAR == datatype) {
									error(ERROR_SYNTAX);	
									return(NEXT_TERMINATE);
								}
								putdata1(0xFD);	/* prefix byte - common var follows */
								commonflg = TRUE;
								cnumstate = CNUM_FSM_COMMONVAR;
								return(RULE_PARSEVAR + NEXT_NOPARSE);
							case TOKEN_LPAREND:
							case TOKEN_LBRACKET:
								returnfsm = FSM_CHAR_NUM_SPEC;
								cnumstate = CNUM_FSM_ARRAY;
								curfsm = FSM_ARRAY_SPEC;
								arraystate = ARRAY_FSM_START;
								break;
							default:
								error(ERROR_SYNTAX);
								return(NEXT_TERMINATE);
						}
						break;
					case CNUM_FSM_COMMONVAR:
						switch (tokentype) {
							case TOKEN_WORD:
								curfsm = FSM_SIZE_SPEC;
								cnumstate = CNUM_FSM_SIZE;
								sizestate = SIZE_FSM_WORDSTART;
								break;
							case TOKEN_LITERAL:
								cnumstate = CNUM_FSM_LITERAL;
								break;
							case TOKEN_NUMBER:
								curfsm = FSM_SIZE_SPEC;
								cnumstate = CNUM_FSM_SIZE;
								sizestate = SIZE_FSM_NUMSTART;
								break;
							case TOKEN_PERIOD:
								curfsm = FSM_SIZE_SPEC;
								cnumstate = CNUM_FSM_SIZE;
								sizestate = SIZE_FSM_PERIODSTART;
								break;
							default:
								error(ERROR_SYNTAX);
								return(NEXT_TERMINATE);
						}
						break;
					case CNUM_FSM_SIZE:			/* x   x.  x.y */
						if (line[linecnt] && charmap[line[linecnt]] != 2) {
							if (line[linecnt] == '[' ||
							    line[linecnt] == '(') {
								linecnt++;
								returnfsm = FSM_CHAR_NUM_SPEC;
								cnumstate = CNUM_FSM_ARRAY;
								curfsm = FSM_ARRAY_SPEC;
								arraystate = ARRAY_FSM_START;
							}
							else {
								cnumstate = CNUM_FSM_COMMAINITIAL;
								return(RULE_NOPARSE + NEXT_COMMA);
							}
						}		
						else cnumstate = CNUM_FSM_FINISH;
						break;
					case CNUM_FSM_ARRAY:		/* [x] [x, y] [x, y, z] */ 
						numdim = dimcnt; 
						arraydim1 = dim1;
						arraydim2 = dim2;
						arraydim3 = dim3;
						if (line[linecnt] && charmap[line[linecnt]] != 2) {
							if (line[linecnt] == '@') {
								addrvarflg = TRUE;
								if (rightsize != -1 || globalflg || commonflg) {
									error(ERROR_SYNTAX);
									return(NEXT_TERMINATE);
								}
								linecnt++;
								if (line[linecnt] && charmap[line[linecnt]] != 2) {
									cnumstate = CNUM_FSM_COMMAINITIAL;
									return(RULE_NOPARSE + NEXT_COMMA);
								}
							}
							else {
								cnumstate = CNUM_FSM_COMMAINITIAL;
								return(RULE_NOPARSE + NEXT_COMMA);
							}
						}		
						cnumstate = CNUM_FSM_FINISH;
						break;
					case CNUM_FSM_ADDROFARRAY:	/* @[] @[,] @[,,] */
						if (line[linecnt] == ',') {
							linecnt++;
							if (line[linecnt] == ',') {
								linecnt++;
								numdim = 3;
							}
							else numdim = 2;
						}
						else numdim = 1;
						if (line[linecnt] != ']' && line[linecnt] != ')') {
							error(ERROR_BADARRAYSPEC);
							return(NEXT_TERMINATE);
						}
						if (line[++linecnt] && charmap[line[linecnt]] != 2) {
							cnumstate = CNUM_FSM_COMMAINITIAL;
							return(RULE_NOPARSE + NEXT_COMMA);
						}						
						cnumstate = CNUM_FSM_FINISH;
						break;
					case CNUM_FSM_COMMAINITIAL:		/* , INITIAL */
 						whitespace();
						scantoken(SCAN_UPCASE);
						if (tokentype != TOKEN_WORD || 
						    strcmp((CHAR *) token, "INITIAL")) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
 						whitespace();
						initialflg = TRUE;
						cnumstate = CNUM_FSM_FINISH;
						break;
					case CNUM_FSM_LITERAL:		/* LITERAL */
						if (DATATYPE_CHAR == datatype) {
							error(ERROR_BADSIZE);
							return(NEXT_TERMINATE);
						}
						if (checknum(token) == -1) {
							error(ERROR_BADNUMLIT);
							return(NEXT_TERMINATE);
						}
						if (DATATYPE_VAR == datatype ||
						    (line[linecnt] && charmap[line[linecnt]] != 2)) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
						symtype = TYPE_NVAR;
						symvalue = savdadr;
						toklen = (int)strlen((CHAR *) token);
						if (token[toklen - 1] == '.') toklen--;
						putdatahhll((UINT) 0xF700 + (datatype - 1) * 0x20 + toklen);
						putdata(token, toklen);
						if (DATATYPE_NUM == datatype) toklen++;
						else if (DATATYPE_INT == datatype) toklen = 6;
						else toklen = 10;  /* FLOAT */
						if (!recdefonly) {
							if (globalflg) dataadr += 6;
							else dataadr += toklen;
						}
						if (recordlevel) {
							if (globalflg) recdeftail(symtype, 6);
							else recdeftail(symtype, (UINT) toklen);
						}
						if (recordlevel) putrecdsym(label);
						else if (!recdefonly) putdsym(label, inheritableflg);
						return(NEXT_TERMINATE);
				}
				break;
			case FSM_SIZE_SPEC:			/* field size spec parse FSM */
				switch (sizestate) {
					case SIZE_FSM_PERIODSTART:  /* parsed period first */
						if (DATATYPE_CHAR == datatype || 
						    DATATYPE_INT == datatype) {
							error(ERROR_BADSIZE);
							return(NEXT_TERMINATE);
						}
						scantoken(upcaseflg);
						if (tokentype != TOKEN_WORD) {
							error(ERROR_BADVARTYPE);
							return(NEXT_TERMINATE);
						}
						sizestate = SIZE_FSM_PERIODEQUATE;
						break;
					case SIZE_FSM_WORDSTART:  /* parsed equate label first */
						for (curtokpos = 0; (curchar = token[curtokpos]); curtokpos++) {
							if (curchar == '.') {
								toklen = (int)strlen((CHAR *) token);
								token[curtokpos] = 0;
								linecnt -= (toklen - curtokpos);
								break;
							}
						}
						if (getdsym((CHAR *) token) == -1) error(ERROR_VARNOTFOUND);
						if (symtype != TYPE_EQUATE) {
							error(ERROR_BADVARTYPE);
							return(NEXT_TERMINATE);
						}
						leftsize = (INT) symvalue;
						if (line[linecnt] == '.') {
							if (DATATYPE_CHAR == datatype || 
							    DATATYPE_INT == datatype) {
								error(ERROR_BADSIZE);
								return(NEXT_TERMINATE);
							}
							linecnt++;
							sizestate = SIZE_FSM_NUMPERIOD;
						}
						else {
							tokentype = 0;
							rightsize = 0;
							curfsm = FSM_CHAR_NUM_SPEC;	/* finished */
						}
						break;
					case SIZE_FSM_NUMSTART:  /* parsed number literal first */
						for (curtokpos = 0; 
							(curchar = token[curtokpos]) && curchar != '.'; 
							curtokpos++);
						if (curchar == '.') {
							if (DATATYPE_CHAR == datatype || 
							    DATATYPE_INT == datatype) {
								error(ERROR_BADSIZE);
								return(NEXT_TERMINATE);
							}
							token[curtokpos] = 0;
							if (!curtokpos) {
								curtokpos++;
								sizestate = SIZE_FSM_NUMPERIODNUM;
								leftsize = 0;
								break;
							}
						}
						if (checkdcon(token)) {
							error(ERROR_BADSIZE);
							return(NEXT_TERMINATE);
						}
						leftsize = (UINT) symvalue;
						if (line[linecnt - 1] == '.') {
							sizestate = SIZE_FSM_NUMPERIOD;
						}
						else if (curchar == '.') {
							curtokpos++;
							sizestate = SIZE_FSM_NUMPERIODNUM;
						}
						else {
							rightsize = 0;
							curfsm = FSM_CHAR_NUM_SPEC;	/* finished */
						}
						break;
					case SIZE_FSM_NUMPERIOD:
						linecntsave = linecnt;
						scantoken(upcaseflg);
						if (tokentype == TOKEN_NUMBER) {
							curtokpos = 0;
							sizestate = SIZE_FSM_NUMPERIODNUM;
						}
						else if (tokentype == TOKEN_WORD) {
							sizestate = SIZE_FSM_PERIODEQUATE;
						}
						else {
							linecnt = linecntsave; /* put token back */
							rightsize = 0;
							curfsm = FSM_CHAR_NUM_SPEC;	/* finished */
						}
						break;
					case SIZE_FSM_PERIODEQUATE:
						if (getdsym((CHAR *) token) == -1) error(ERROR_VARNOTFOUND);
						if (symtype != TYPE_EQUATE) {
							error(ERROR_BADVARTYPE);
							return(NEXT_TERMINATE);
						}
						rightsize = (UINT) symvalue;
						curfsm = FSM_CHAR_NUM_SPEC;	/* finished */
						break;
					case SIZE_FSM_NUMPERIODNUM:
						if (checkdcon(&token[curtokpos]))  {
							error(ERROR_BADSIZE);
							return(NEXT_TERMINATE);
						}
						rightsize = (UINT) symvalue;
						curfsm = FSM_CHAR_NUM_SPEC;	/* finished */
						break;
				}
				break;
			case FSM_ARRAY_SPEC:		/* array spec parse FSM */
				switch (arraystate) {
					case ARRAY_FSM_START:
						dimcnt = 0;
						dim1 = dim2 = dim3 = 0;
						arraystate = ARRAY_FSM_1STDIM;
						return(RULE_XCON + NEXT_COMMARBRKT);
					case ARRAY_FSM_1STDIM:
						dimcnt++;
						dim1 = symvalue;
						if (delimiter == TOKEN_COMMA) {
							whitespace();			
							arraystate = ARRAY_FSM_2NDDIM;
							return(RULE_XCON + NEXT_COMMARBRKT);
						}
						break;
					case ARRAY_FSM_2NDDIM:
						dimcnt++;
						dim2 = symvalue;
						if (delimiter == TOKEN_COMMA) {
							whitespace();			
							arraystate = ARRAY_FSM_3RDDIM;
							return(RULE_XCON + NEXT_RBRACKET);
						}
						break;
					case ARRAY_FSM_3RDDIM:
						dimcnt++;
						dim3 = symvalue;
						break;
				}
				curfsm = returnfsm;
				break;
 			case FSM_INITIALIZER_LIST:		/* parse initializer list following , INITIAL */
				switch (initstate) {
					case INITIALIZER_ADDRVAR:
						if (tokentype != TOKEN_WORD) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
 						}
						if (getdsym((CHAR *) token) == -1) {
							error(ERROR_VARNOTFOUND);
							return(NEXT_TERMINATE);
 						}
						if (symtype & TYPE_PTR) {
							error(ERROR_BADVARTYPE);
							return(NEXT_TERMINATE);
						}
						if (DATATYPE_CHAR == datatype) {
							if (TYPE_CVAR == symtype) targetdim = 0;
							else if (TYPE_CARRAY1 == symtype) targetdim = 1;
							else if (TYPE_CARRAY2 == symtype) targetdim = 2;
							else if (TYPE_CARRAY3 == symtype) targetdim = 3;
							else {
								error(ERROR_BADVARTYPE);
								return(NEXT_TERMINATE);
							}
						}
						else if (DATATYPE_VAR == datatype) {
							if (TYPE_NARRAY1 == symtype || TYPE_CARRAY1 == symtype) targetdim = 1;
							else if (TYPE_NARRAY2 == symtype || TYPE_CARRAY2 == symtype) targetdim = 2;
							else if (TYPE_NARRAY3 == symtype || TYPE_CARRAY3 == symtype) targetdim = 3;
							else if (symtype <= TYPE_OBJECT) targetdim = 0;
							else {
								error(ERROR_BADVARTYPE);
								return(NEXT_TERMINATE);
							}
						}
						else {
							if (TYPE_NVAR == symtype) targetdim = 0;
							else if (TYPE_NARRAY1 == symtype) targetdim = 1;
							else if (TYPE_NARRAY2 == symtype) targetdim = 2;
							else if (TYPE_NARRAY3 == symtype) targetdim = 3;
							else {
								error(ERROR_BADVARTYPE);
								return(NEXT_TERMINATE);
							}
						}
						if (targetdim && (line[linecnt] == '[' ||
									   line[linecnt] == '(')) {
							linecnt++;	
							initstate = INITIALIZER_WRITEARRAYADDR;
						}
						else initstate = INITIALIZER_WRITEADDR;
						break;
					case INITIALIZER_ELEMENT:
						if (tokentype == TOKEN_WORD && datatype != DATATYPE_CHAR) {
							if (getdsym((CHAR *) token) == -1) {
								error(ERROR_EQNOTFOUND);
								return(NEXT_TERMINATE);
							}
							if (symtype == TYPE_EQUATE) {
								symtype = TYPE_NUMBER;
								if (symvalue) {
									for (tokpos = 4; tokpos >= 0 && symvalue; tokpos--) {
										token[tokpos] = (UCHAR)((symvalue % 10) + '0');
										symvalue /= 10;
									}
									toklen = 4 - tokpos;
									putdata1((UCHAR) toklen);
									putdata(&token[tokpos + 1], toklen);
								}
								else {
									putdata1(1);
									putdata1('0');
								}
							}
							else {
								error(ERROR_SYNTAX);
								return(NEXT_TERMINATE);
							}
						}
						else {
							if (DATATYPE_CHAR == datatype) {
								if (tokentype != TOKEN_LITERAL) {
									error(ERROR_SYNTAX);
									return(NEXT_TERMINATE);
								}
							}
							else {
								if (tokentype == TOKEN_MINUS) {
									scantoken(upcaseflg);
									memmove(&token[1], token, strlen((CHAR *) token) + 1);
									token[0] = '-';
								}
								if (tokentype != TOKEN_NUMBER) {
									error(ERROR_SYNTAX);
									return(NEXT_TERMINATE);
								}
								if (checknum(token) == -1) {
									error(ERROR_BADNUMLIT);
									return(NEXT_TERMINATE);
								}
							}
							toklen = (int)strlen((CHAR *) token);
							if (DATATYPE_CHAR != datatype && token[toklen - 1] == '.') toklen--;
							putdata1((UCHAR) toklen);
							putdata(token, toklen);
						}
						if (numdim && arraydim1) {
							if (line[linecnt] && charmap[line[linecnt]] != 2) {
								initstate = INITIALIZER_DELIMITER;
								return(RULE_NOPARSE + NEXT_COMMA);
							}						
							putdata1(0xFF);
						}
						return(NEXT_TERMINATE);
					case INITIALIZER_DELIMITER:
						initstate = INITIALIZER_ELEMENT;
						return(RULE_PARSEVAR + NEXT_NOPARSE);
					case INITIALIZER_WRITEARRAYADDR:
						returnfsm = FSM_INITIALIZER_LIST;
						curfsm = FSM_ARRAY_SPEC;
						arraystate = ARRAY_FSM_START;
						initstate = INITIALIZER_NEXTADDR;
						savdadr = symvalue;
						savsymtype = symtype;
						break;
					case INITIALIZER_WRITEADDR:
						arrayshape[0] = targetdim;
						arrayshape[1] = 0;
						if (DATATYPE_VAR == datatype) {
							if (putinitaddr(symvalue, arrayshape, TYPE_VVARPTR, numdim && arraydim1) == RC_ERROR) {
								return(NEXT_TERMINATE);
							}
						}
						else if (putinitaddr(symvalue, arrayshape, symtype, numdim && arraydim1) == RC_ERROR) {
							return(NEXT_TERMINATE);
						}
						dimcnt = 0;
						// fall through
					case INITIALIZER_NEXTADDR:
						if (dimcnt) {
							if (dimcnt != targetdim) {
								error(ERROR_BADARRAYINDEX);
								return(NEXT_TERMINATE);
							}
							arrayshape[0] = dimcnt;
							arrayshape[1] = dim1;
							arrayshape[2] = dim2;
							arrayshape[3] = dim3;
							if (DATATYPE_VAR == datatype) {
								if (putinitaddr(savdadr, arrayshape, TYPE_VVARPTR, numdim && arraydim1) == RC_ERROR) {
									return(NEXT_TERMINATE);
								}
							}
							else if (putinitaddr(savdadr, arrayshape, savsymtype, numdim && arraydim1) == RC_ERROR) {
								return(NEXT_TERMINATE);
							}
						}
						if (line[linecnt] && charmap[line[linecnt]] != 2) {
							if (numdim && arraydim1) {
								initstate = INITIALIZER_ADDRDELIMITER;
								return(RULE_NOPARSE + NEXT_COMMA);
							}						
							error(ERROR_SYNTAX);
						}
						else {
							if (numdim && arraydim1) {
								putdatallhh(0xFFFF);
								putdata1(0xFF);
							}
							else if (recordlevel) recdeftail(addrvartype, 6);
						}
						return(NEXT_TERMINATE);
					case INITIALIZER_ADDRDELIMITER:
						initstate = INITIALIZER_ADDRVAR;
						return(RULE_PARSEVAR + NEXT_NOPARSE);
				}
		}
 	} while (cnumstate != CNUM_FSM_FINISH);
	
	/* generate data area */
	symvalue = savdadr;
	if (DATATYPE_CHAR == datatype) {
		if (addrvarflg) {
			if (!numdim) {		/* CHAR @ */
				symtype = TYPE_CVARPTR;
				if (initialflg) {
					initstate = INITIALIZER_ADDRVAR;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else {
					putdata1(0xFA);
					if (recordlevel) recdeftail(symtype, 6);
				}
				if (!recdefonly) dataadr += 6;
			}
			else if (!arraydim1) {	/* CHAR @[]  CHAR @[,]  CHAR @[,,] */
				symtype = (UCHAR) ((TYPE_CARRAY1 + 2 * (numdim - 1)) | TYPE_PTR);
				if (initialflg) {
					initstate = INITIALIZER_ADDRVAR;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else {
					putdatahhll(0xFC20 + numdim);
					if (recordlevel) recdeftail(symtype, 6);
				}
				if (!recdefonly) dataadr += 6;
			}
			else {			/* CHAR [x]@  CHAR [x, y]@  CHAR [x, y, z]@ */
				symtype = (UCHAR) (TYPE_CVARPTRARRAY1 - 2 + (numdim * 2));
				if (initialflg) {
					putdatahhll(0xFF00 + numdim + 3);
					initstate = INITIALIZER_ADDRVAR;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else putdatahhll(0xFF00 + numdim);
				putdatallhh(arraydim1);
				if (numdim > 1) putdatallhh(arraydim2);
				if (numdim > 2) putdatallhh(arraydim3);
				putdata1(0xFA);
				arraysize = (INT) arraydim1 * 6;
				if (numdim > 1) arraysize *= (INT) arraydim2;
				if (numdim > 2) arraysize *= (INT) arraydim3;
				if (!recdefonly) dataadr += (arraysize + numdim * 2 + 4);
				if (recordlevel) {
					recdeftail(symtype, arraysize + numdim * 2 + 4);
					if (initialflg) putdef1(0xFE);
				}
			}
		}
		else {
			if (leftsize < 0) {
				error(ERROR_BADSIZE);
				return(NEXT_TERMINATE);
			}
			if (leftsize > 65500) {
				error(ERROR_BADNUMSPEC);
				return(NEXT_TERMINATE);
			}
			if (!numdim) {		/* CHAR n */
				symtype = TYPE_CVAR;
				if (initialflg) {
					putdatahhll(0xFF00);
					initstate = INITIALIZER_ELEMENT;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				if (leftsize < 128) {
					putdata1((UCHAR) leftsize);	/* small CHAR */
					leftsize += 3;
				}
				else {
					putdatahhll(0xFC1F);		/* big CHAR */
					putdatallhh((UINT) leftsize);
					leftsize += 7;
				}
				if (!recdefonly) {
					if (globalflg) dataadr += 6;
					else dataadr += leftsize;
				}
				if (recordlevel) {
					if (globalflg) recdeftail(symtype, 6);
					else recdeftail(symtype, (UINT) leftsize);
				}
			}
			else {	/* CHAR n[x]  CHAR n[x, y]  CHAR n[x, y, z] */
				symtype = (UCHAR) (TYPE_CARRAY1 - 2 + (numdim * 2));
				if (initialflg) {
					putdatahhll(0xFF00 + numdim + 3);
					initstate = INITIALIZER_ELEMENT;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else putdatahhll(0xFF00 + numdim);
				putdatallhh(arraydim1);
				if (numdim > 1) putdatallhh(arraydim2);
				if (numdim > 2) putdatallhh(arraydim3);
				if (leftsize < 128) {
					putdata1((UCHAR) leftsize);	/* small CHAR */
					leftsize += 3;
				}
				else {
					putdatahhll(0xFC1F);		/* big CHAR */
					putdatallhh((UINT) leftsize);
					leftsize += 7;
				}
				arraysize = (INT) arraydim1 * (INT) leftsize;
				if (numdim > 1) arraysize *= (INT) arraydim2;
				if (numdim > 2) arraysize *= (INT) arraydim3;
				if (!recdefonly) {
					if (globalflg) dataadr += 6;
					else dataadr += (arraysize + numdim * 2 + 4);
				}
				if (recordlevel) {
					if (globalflg) recdeftail(symtype, 6);
					else recdeftail(symtype, arraysize + numdim * 2 + 4);
				}
				arrayshape[0] = leftsize;
				arrayshape[1] = (UINT) arraydim1;
				arrayshape[2] = (UINT) arraydim2;
				arrayshape[3] = (UINT) arraydim3;
				putarrayshape(savdadr, arrayshape);
				if (recordlevel && initialflg) putdef1(0xFE);
			}
		}
 	}
	else if (DATATYPE_VAR == datatype) {
		if (!numdim) {		/* VAR @ */
			symtype = TYPE_VVARPTR;
			if (initialflg) {
				initstate = INITIALIZER_ADDRVAR;
				retrule = RULE_PARSEVAR + NEXT_NOPARSE;
			}
			else {
				putdatahhll(0xFC07);
				if (recordlevel) recdeftail(symtype, 6);
			}
			if (!recdefonly) dataadr += 6;
		}
		else {			/* VAR [x]@  VAR [x, y]@  VAR [x, y, z]@ */
			symtype = (UCHAR) (TYPE_VVARPTRARRAY1 - 2 + (numdim * 2));
			if (initialflg) {
				putdatahhll(0xFF00 + numdim + 3);
				initstate = INITIALIZER_ADDRVAR;
				retrule = RULE_PARSEVAR + NEXT_NOPARSE;
			}
			else putdatahhll(0xFF00 + numdim);
			putdatallhh(arraydim1);
			if (numdim > 1) putdatallhh(arraydim2);
			if (numdim > 2) putdatallhh(arraydim3);
			putdatahhll(0xFC07);
			arraysize = (INT) arraydim1 * 6;
			if (numdim > 1) arraysize *= (INT) arraydim2;
			if (numdim > 2) arraysize *= (INT) arraydim3;
			if (!recdefonly) dataadr += (arraysize + numdim * 2 + 4);
			if (recordlevel) {
				recdeftail(symtype, arraysize + numdim * 2 + 4);
				if (initialflg) putdef1(0xFE);
			}
		}
	}
	else {		/* NUM, INT, FLOAT */
		if (addrvarflg) {
			if (!numdim) {		/* NUM @ */
				symtype = TYPE_NVARPTR;
				if (initialflg) {
					initstate = INITIALIZER_ADDRVAR;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else {
					putdata1(0xFB);
					if (recordlevel) recdeftail(symtype, 6);
				}
				if (!recdefonly) dataadr += 6;
			}
			else if (!arraydim1) {	/* NUM @[]  NUM @[,]  NUM @[,,] */
				symtype = (UCHAR) ((TYPE_NARRAY1 + 2 * (numdim - 1)) | TYPE_PTR);
				if (initialflg) {
					initstate = INITIALIZER_ADDRVAR;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else {
					putdatahhll(0xFC24 + numdim);
					if (recordlevel) recdeftail(symtype, 6);
				}
				if (!recdefonly) dataadr += 6;
			}
			else {			/* NUM [x]@  NUM [x, y]@  NUM [x, y, z]@ */
				symtype = (UCHAR) (TYPE_NVARPTRARRAY1 - 2 + (numdim * 2));
				if (initialflg) {
					putdatahhll(0xFF00 + numdim + 3);
					initstate = INITIALIZER_ADDRVAR;
					retrule = RULE_PARSEVAR + NEXT_NOPARSE;
				}
				else putdatahhll(0xFF00 + numdim);
				putdatallhh(arraydim1);
				if (numdim > 1) putdatallhh(arraydim2);
				if (numdim > 2) putdatallhh(arraydim3);
				putdata1(0xFB);
				arraysize = (INT) arraydim1 * 6;
				if (numdim > 1) arraysize *= (INT) arraydim2;
				if (numdim > 2) arraysize *= (INT) arraydim3;
				if (!recdefonly) dataadr += (arraysize + numdim * 2 + 4);
				if (recordlevel) {
					recdeftail(symtype, arraysize + numdim * 2 + 4);
					if (initialflg) putdef1(0xFE);
				}
			}
 		}
		else {  /* non-address variable */
			if (leftsize + rightsize <= 0 && leftsize <= 0) {
				error(ERROR_BADSIZE);
				return(NEXT_TERMINATE);
			}
			switch (datatype) {
				case DATATYPE_NUM:
					if (leftsize + rightsize + (rightsize ? 1 : 0) > 31) {
						error(ERROR_BADNUMSPEC);
						return(NEXT_TERMINATE);
					}
					if (!numdim) {		/* NUM n  NUM n.  NUM n.m */
						symtype = TYPE_NVAR;
						if (!rightsize) {
							putdata1((UCHAR) (0x80 + leftsize));
						}
						else if (rightsize < 3 && leftsize < 16) {
							putdata1((UCHAR) (0xA0 + leftsize * 2 + rightsize - 1));
						}
						else {
							putdatahhll((UINT) 0xC000 + (leftsize << 8) + rightsize);
						}
						if (rightsize) leftsize += rightsize + 2;
						else leftsize += 1;
						if (!recdefonly) {
							if (globalflg) dataadr += 6;
							else dataadr += leftsize;
						}
						if (recordlevel) {
							if (globalflg) recdeftail(symtype, 6);
							else recdeftail(symtype, (UINT) leftsize);
						}
						if (initialflg) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
					}
					else {	/* NUM n.m[x]  NUM n.m[x, y]  NUM n.m[x, y, z] */
						symtype = (UCHAR) (TYPE_NARRAY1 - 2 + (numdim * 2));
						if (initialflg) {
							putdatahhll(0xFF00 + numdim + 3);
							initstate = INITIALIZER_ELEMENT;
							retrule = RULE_PARSEVAR + NEXT_NOPARSE;
						}
						else putdatahhll(0xFF00 + numdim);
						putdatallhh(arraydim1);
						if (numdim > 1) putdatallhh(arraydim2);
						if (numdim > 2) putdatallhh(arraydim3);
						if (!rightsize) {
							putdata1((UCHAR) (0x80 + leftsize));
						}
						else if (rightsize < 3 && leftsize < 16) {
							putdata1((UCHAR) (0xA0 + leftsize * 2 + rightsize - 1));
						}
						else {
							putdatahhll((UINT) 0xC000 + (leftsize << 8) + rightsize);
						}
						if (rightsize) leftsize += rightsize + 2;
						else leftsize += 1;
						arraysize = (INT) arraydim1 * (INT) leftsize;
						if (numdim > 1) arraysize *= (INT) arraydim2;
						if (numdim > 2) arraysize *= (INT) arraydim3;
						if (!recdefonly) {
							if (globalflg) dataadr += 6;
							else dataadr += (arraysize + numdim * 2 + 4);
						}
						if (recordlevel) {
							if (globalflg) recdeftail(symtype, 6);
							else recdeftail(symtype, arraysize + numdim * 2 + 4);
						}
						arrayshape[0] = leftsize;
						arrayshape[1] = (UINT) arraydim1;
						arrayshape[2] = (UINT) arraydim2;
						arrayshape[3] = (UINT) arraydim3;
						putarrayshape(savdadr, arrayshape);
						if (recordlevel && initialflg) putdef1(0xFE);
					}
					break;
				case DATATYPE_INT:
					if (leftsize > 31) {
						error(ERROR_BADNUMSPEC);
						return(NEXT_TERMINATE);
					}
					if (!numdim) {		/* INT n */
						symtype = TYPE_NVAR;
						putdatahhll((UINT) 0xF7A0 + leftsize);
						if (!recdefonly) dataadr += 6;
						if (recordlevel) recdeftail(symtype, 6);
						if (initialflg) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
					}
					else {	/* INT n[x]  n[x, y]  n[x, y, z] */
						symtype = (UCHAR) (TYPE_NARRAY1 - 2 + (numdim * 2));
						if (initialflg) {
							putdatahhll(0xFF00 + numdim + 3);
							initstate = INITIALIZER_ELEMENT;
							retrule = RULE_PARSEVAR + NEXT_NOPARSE;
						}
						else putdatahhll(0xFF00 + numdim);
						putdatallhh(arraydim1);
						if (numdim > 1) putdatallhh(arraydim2);
						if (numdim > 2) putdatallhh(arraydim3);
						putdatahhll((UINT) 0xF7A0 + leftsize);
						arraysize = (INT) arraydim1 * (INT) 6;
						if (numdim > 1) arraysize *= (INT) arraydim2;
						if (numdim > 2) arraysize *= (INT) arraydim3;
						if (!recdefonly) {
							if (globalflg) dataadr += 6;
							else dataadr += (arraysize + numdim * 2 + 4);
						}
						if (recordlevel) {
							if (globalflg) recdeftail(symtype, 6);
							else recdeftail(symtype, arraysize + numdim * 2 + 4);
						}
						arrayshape[0] = 6;
						arrayshape[1] = (UINT) arraydim1;
						arrayshape[2] = (UINT) arraydim2;
						arrayshape[3] = (UINT) arraydim3;
						putarrayshape(savdadr, arrayshape);
						if (recordlevel && initialflg) putdef1(0xFE);
					}
					break;
				case DATATYPE_FLOAT:
					if (leftsize + rightsize + (rightsize ? 1 : 0) > 31) {
						error(ERROR_BADNUMSPEC);
						return(NEXT_TERMINATE);
					}
					if (!numdim) {		/* FLOAT n.m */
						symtype = TYPE_NVAR;
						putdatahhll((UINT) 0xF7C0 + leftsize);
						putdata1((UCHAR) rightsize);
						if (!recdefonly) {
							if (globalflg) dataadr += 6;
							else dataadr += 10;
						}
						if (recordlevel) {
							if (globalflg) recdeftail(symtype, 6);
							else recdeftail(symtype, 10);
						}
						if (initialflg) {
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
						}
					}
					else {	/* FLOAT n.m[x]  */
						symtype = (UCHAR) (TYPE_NARRAY1 - 2 + (numdim * 2));
						if (initialflg) {
							putdatahhll(0xFF00 + numdim + 3);
							initstate = INITIALIZER_ELEMENT;
							retrule = RULE_PARSEVAR + NEXT_NOPARSE;
						}
						else putdatahhll(0xFF00 + numdim);
						putdatallhh(arraydim1);
						if (numdim > 1) putdatallhh(arraydim2);
						if (numdim > 2) putdatallhh(arraydim3);
						putdatahhll((UINT) 0xF7C0 + leftsize);
						putdata1((UCHAR) rightsize);
						arraysize = (INT) arraydim1 * (INT) 10;
						if (numdim > 1) arraysize *= (INT) arraydim2;
						if (numdim > 2) arraysize *= (INT) arraydim3;
						if (!recdefonly) {
							if (globalflg) dataadr += 6;
							else dataadr += (arraysize + numdim * 2 + 4);
						}
						if (recordlevel) {
							if (globalflg) recdeftail(symtype, 6);
							else recdeftail(symtype, arraysize + numdim * 2 + 4);
						}
						arrayshape[0] = 10;
						arrayshape[1] = (UINT) arraydim1;
						arrayshape[2] = (UINT) arraydim2;
						arrayshape[3] = (UINT) arraydim3;
						putarrayshape(savdadr, arrayshape);
						if (recordlevel && initialflg) putdef1(0xFE);
					}
					break;
			}
		}
	}
	if (recordlevel) putrecdsym(label);
	else if (!recdefonly) putdsym(label, inheritableflg);
	if (initialflg) {
		cnumstate = CNUM_FSM_START;
		curfsm = FSM_INITIALIZER_LIST;
		addrvartype = symtype;
		return(retrule);
	}
	return(NEXT_TERMINATE);
}

static INT putinitaddr(INT baseadr, UINT spec[], UINT vartype, INT nocodeflg)
{
	UCHAR typebyte;
	UINT arrayshape[4];

	if (!nocodeflg) {
		switch (vartype) {
			case TYPE_CVAR: typebyte = 0x40; break;
	 		case TYPE_CARRAY1: 
				if (spec[0] != 1) {
					error(ERROR_BADARRAYINDEX);
					return(RC_ERROR);
				}
				if (!spec[1]) typebyte = 0x41;
				else typebyte = 0x40;
				break;
			case TYPE_CARRAY2: 
				if (spec[0] != 2) {
					error(ERROR_BADARRAYINDEX);
					return(RC_ERROR);
				}
				if (!spec[1]) typebyte = 0x42;
				else typebyte = 0x40;
				break;
			case TYPE_CARRAY3: 
				if (spec[0] != 3) {
					error(ERROR_BADARRAYINDEX);
					return(RC_ERROR);
				}
				if (!spec[1]) typebyte = 0x43;
				else typebyte = 0x40;
				break;
			case TYPE_NVAR: typebyte = 0x44; break;
			case TYPE_NARRAY1: 
				if (spec[0] != 1) {
					error(ERROR_BADARRAYINDEX);
					return(RC_ERROR);
				}
				if (!spec[1]) typebyte = 0x45;
				else typebyte = 0x44;
				break;
			case TYPE_NARRAY2: 
				if (spec[0] != 2) {
					error(ERROR_BADARRAYINDEX);
					return(RC_ERROR);
				}
				if (!spec[1]) typebyte = 0x46;
				else typebyte = 0x44;
				break;
			case TYPE_NARRAY3: 
				if (spec[0] != 3) {
					error(ERROR_BADARRAYINDEX);
					return(RC_ERROR);
				}
				if (!spec[1]) typebyte = 0x47;
				else typebyte = 0x44;
				break;
			case TYPE_LIST: 
			case TYPE_CVARLIST:
				typebyte = 0x48;
				break;
			case TYPE_FILE: typebyte = 0x52; break;
			case TYPE_IFILE: typebyte = 0x53; break;
			case TYPE_AFILE: typebyte = 0x54; break;
			case TYPE_COMFILE: typebyte = 0x56; break;
			case TYPE_VVARPTR: typebyte = 0x57; break;
			case TYPE_PFILE: typebyte = 0x5C; break;
			case TYPE_DEVICE: typebyte = 0x5B; break;
			case TYPE_QUEUE: typebyte = 0x59; break;
			case TYPE_RESOURCE: typebyte = 0x5A; break;
			case TYPE_IMAGE: typebyte = 0x58; break;
			default: 
				error(ERROR_BADINITLIST);
				return(RC_ERROR);
		}
		putdatahhll((UINT) 0xFC00 + typebyte);
	}
	if (spec[0] && spec[1]) {
		getarrayshape(baseadr, arrayshape);
		if (!arrayshape[1]) {
			error(ERROR_NOTARRAY);
			return(RC_ERROR);
		}
		switch (spec[0]) {
			case 1:
				baseadr += 5 + arrayshape[0] * (spec[1] - 1);
				break;
			case 2:
				baseadr += 7 + arrayshape[0] * (arrayshape[2] * (spec[1] - 1) + spec[2] - 1);
				break;
			case 3:
				baseadr += 9 + arrayshape[0] * (arrayshape[3] * (arrayshape[2] * (spec[1] - 1) + (spec[2] - 1)) + spec[3] - 1);
				break;
		}
	}
	putdatallhh((USHORT) baseadr);
	putdata1((UCHAR) (baseadr >> 16));
	return(0);
}

UINT s_init_fnc()
{
	INT n, inheritableflg = FALSE;

	if (rulecnt == 1) {
		if (recordlevel) recdefhdr(label);
		codebufcnt = 0;
		inheritableflg = FALSE;
		if (withnamesflg) putdataname(label);
		if (TOKEN_ASTERISK == tokentype) {
			putdata1(0xFD);
			return(RULE_XCON + (RULE_LITERAL << 8) + NEXT_LIST);
		}
		else if (TOKEN_AMPERSAND == tokentype) {
			if (!classlevel) {
				error(ERROR_INHRTABLENOTALLOWED);
				return(NEXT_TERMINATE);
			}
			//inheritableflg = TRUE; Not used before the return
			return(RULE_XCON + (RULE_LITERAL << 8) + NEXT_LIST);
		} 
	}
	if (tokentype == TOKEN_LITERAL) {
		n = (int)strlen((CHAR *) token);
		if (n > CODEBUFSIZE - codebufcnt) n = CODEBUFSIZE - codebufcnt;
		memcpy(codebuf + codebufcnt, token, (size_t) n); // @suppress("Type cannot be resolved")
		codebufcnt += n;
	}
	else putcode1((UCHAR) symvalue);
	if (!delimiter) {
		if (codebufcnt > 65500) {
			error(ERROR_CHRLITTOOBIG);
			return(NEXT_TERMINATE);
		}
		else {
			symvalue = dataadr;
			symtype = TYPE_CVAR;
			if (codebufcnt > 255) {
				putdatahhll(0xFC17);
				putdatallhh((USHORT) codebufcnt);
			}
			else putdatahhll((UINT) 0xF600 + codebufcnt);
			putdata(codebuf, codebufcnt);
			if (recordlevel) {
				if (putrecdsym(label) < 0) return(NEXT_TERMINATE);
			}
			else if (!recdefonly) putdsym(label, inheritableflg);
			if (!recdefonly) {
				if (codebufcnt > 127) dataadr += codebufcnt + 7;
				else dataadr += codebufcnt + 3;
			}
			if (recordlevel) {
				if (codebufcnt > 127) recdeftail(symtype, (UINT) (codebufcnt + 7));
				else recdeftail(symtype, (UINT) (codebufcnt + 3));
			}
		}
		codebufcnt = 0;
		return(NEXT_TERMINATE);
	}
	return(RULE_XCON + (RULE_LITERAL << 8) + NEXT_LIST);
}

#define LISTSTATE_START 1
#define LISTSTATE_COMMAINITIAL 2
#define LISTSTATE_INITIALIZER 3

UINT s_list_fnc()	/* LIST, LISTEND processing */
{
	static int inheritableflg;
	
	if (1 == rulecnt) {
	 	codebufcnt = 0;
		if (208 == codebuf[0]) {		/* LISTEND */
			if (!listlevel) error(ERROR_MISSINGLIST);
			else {
				label[0] = 0;
				if (recordlevel) recdefhdr(label);
				if (withnamesflg && listlevel == nameslevel) {
					withnamesflg = FALSE;
				}
				listlevel--;
				putdata1(0xF4);
				if (recordlevel) recdeftail(symtype, 1);
				if (!recdefonly) dataadr++;
			}
			return(NEXT_TERMINATE);
		}
		sc = LISTSTATE_START;
		if (recordlevel) recdefhdr(label);
		symtype = TYPE_LIST;
		symvalue = dataadr;
		inheritableflg = FALSE;
	}
	switch (sc) {
		case LISTSTATE_START:
			if (TOKEN_WORD == tokentype && !strcmp((CHAR *) token, "WITH")) {
				whitespace();
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_WORD && !strcmp((CHAR *) token, "NAMES")) {
					if (!withnamesflg) {
						withnamesflg = TRUE;	
						nameslevel = listlevel + 1;
					}
				}
				else	error(ERROR_SYNTAX);
				tokentype = TOKEN_WORD;
			}
			if (withnamesflg) {
				putdataname(label);
				symvalue = dataadr;
			}
			if (TOKEN_AMPERSAND == tokentype) {
				if (inheritableflg) {
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				if ('&' == line[linecnt]) {
					linecnt++;
					s_inherited_fnc(TYPE_LIST);
					return(NEXT_TERMINATE);
				}
				else {
					if (!classlevel) {
						error(ERROR_INHRTABLENOTALLOWED);
						return(NEXT_TERMINATE);
					}
					inheritableflg = TRUE;
					scantoken(SCAN_UPCASE);
				}
			}
			if (TOKEN_ATSIGN == tokentype) {  /* address variable */
				symtype |= TYPE_PTR;
				if (!recdefonly) dataadr += 6;
				if (recordlevel) {
					if (putrecdsym(label) < 0) return(NEXT_TERMINATE);
				}
				else if (!recdefonly) putdsym(label, inheritableflg);
				if (line[linecnt] && charmap[line[linecnt]] != 2) {
					sc = LISTSTATE_COMMAINITIAL;
					return(RULE_NOPARSE + NEXT_COMMA);
				}						
				putdatahhll(0xFC00 + 0x05);
				if (recordlevel) recdeftail(symtype, 6);
				break;
			}
			else if (inheritableflg) {
				error(ERROR_LISTNOTINHERITABLE);
				return(NEXT_TERMINATE);
			}
			listlevel++;
			putdata1(0xF9);
			if (!recdefonly) dataadr++;
			if (recordlevel) {
				if (putrecdsym(label) < 0) return(NEXT_TERMINATE);
			}
			else if (!recdefonly) putdsym(label, inheritableflg);
			if (recordlevel) recdeftail(symtype, 1);
			break;
		case LISTSTATE_COMMAINITIAL:
			whitespace();
			scantoken(SCAN_UPCASE);
			if (tokentype != TOKEN_WORD || 
			    strcmp((CHAR *) token, "INITIAL")) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			whitespace();
			sc = LISTSTATE_INITIALIZER;
			return(RULE_PARSEVAR + NEXT_NOPARSE);
		case LISTSTATE_INITIALIZER:
			if (tokentype != TOKEN_WORD) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			if (getdsym((CHAR *) token) == -1) {
				error(ERROR_VARNOTFOUND);
				return(NEXT_TERMINATE);
 			}
			if (symtype != TYPE_LIST && symtype != TYPE_CVARLIST) {
				error(ERROR_BADVARTYPE);
				return(NEXT_TERMINATE);
			}
			arrayspec[0] = arrayspec[1] = 0;
			putinitaddr(symvalue, arrayspec, TYPE_LIST, FALSE);
			if (recordlevel) recdeftail(symtype | TYPE_PTR, 6);
			if (line[linecnt] && charmap[line[linecnt]] != 2) {
				error(ERROR_SYNTAX);
			}
			break;
	}
	return(NEXT_TERMINATE);
}
		
UINT s_varlist_fnc()
{
	INT n;
	UINT arrayshape[4];
	static UINT saveadr;

	if (rulecnt == 1) {
		if (withnamesflg) putdataname(label);
		if (recordlevel) recdefhdr(label);
		saveadr = dataadr;
		codebufcnt = 0;
		putdata1(0xF9);
		sc = 0;
	}
	switch (sc) {
		case 0:  /* just parsed for variable name or literal */
			switch (tokentype) {
				case TOKEN_WORD:
					if (getdsym((CHAR *) token) == -1) error(ERROR_VARNOTFOUND);
					switch (symtype) {
						case TYPE_CVAR: sd = 0; se = 0x40; break;
						case TYPE_NVAR: sd = 0; se = 0x44; break;
						case TYPE_LIST: sd = 0; se = 0x48; break;
						case TYPE_CARRAY1: sd = 1; se = 0x41; break;
						case TYPE_CARRAY2: sd = 2; se = 0x42; break;
						case TYPE_CARRAY3: sd = 3; se = 0x43; break;
						case TYPE_NARRAY1: sd = 1; se = 0x45; break;
						case TYPE_NARRAY2: sd = 2; se = 0x46; break;
						case TYPE_NARRAY3: sd = 3; se = 0x47; break;
						case TYPE_FILE:    sd = 0; se = 0x52; break;
						case TYPE_IFILE:   sd = 0; se = 0x53; break;
						case TYPE_AFILE:   sd = 0; se = 0x54; break;
						case TYPE_IMAGE:   sd = 0; se = 0x58; break;
						case TYPE_QUEUE:   sd = 0; se = 0x59; break;
						case TYPE_RESOURCE: sd = 0; se = 0x5A; break;
						case TYPE_DEVICE:   sd = 0; se = 0x5B; break;
						default:
							error(ERROR_SYNTAX);
							return(NEXT_TERMINATE);
					}
					if (sd) {
						if (line[linecnt] == '[' || line[linecnt] == '(') {
							linecnt++;
							whitespace();
							sl = symvalue;
							sc = 1;
							if (se < 0x45) se = 0x40;
							else se = 0x44;
							if (sd > 1) return(RULE_XCON + NEXT_COMMA);
							else return(RULE_XCON + NEXT_RBRACKET);
						}
					}
					putdatahhll((UINT) 0xFC00 + se);
					putdatallhh((USHORT) symvalue);
					putdata1((UCHAR) (symvalue >> 16));
					dataadr += 6;
					break;
				case TOKEN_LITERAL:
					n = (int)strlen((CHAR *) token);
					putdatahhll((UINT) 0xF600 + n);
					putdata(token, n);
					if (n > 127) dataadr += n + 7;
					else dataadr += n + 3;
					break;
				default:
					error(ERROR_BADLISTENTRY);
					return(NEXT_TERMINATE);
			}
			sc = 4;
			return(RULE_NOPARSE + NEXT_LIST);
		case 1:
		case 2:
		case 3:	 /* just parsed an array index */
			if (!symvalue) error(ERROR_BADARRAYINDEX);
			arrayspec[sc - 1] = (UINT) symvalue;
			if (sc < sd) {
				if (++sc != sd) return(RULE_XCON + NEXT_COMMA);
				else return(RULE_XCON + NEXT_RBRACKET);
			}
			getarrayshape((INT) sl, arrayshape);
			if (!arrayshape[1]) error(ERROR_NOTARRAY);
			else {
				if (arrayshape[3]) symvalue = sl + 9 + arrayshape[0] * (arrayshape[2] * arrayshape[3] * (arrayspec[0] - 1) + arrayshape[3] * (arrayspec[1] - 1) + arrayspec[2] - 1);
				else if (arrayshape[2]) symvalue = sl + 7 + arrayshape[0] * (arrayshape[2] * (arrayspec[0] - 1) + arrayspec[1] - 1);
				else symvalue = sl + 5 + arrayshape[0] * (arrayspec[0] - 1);
				se &= 0xFC;  /* turn off two low order bits */
				putdatahhll((UINT) 0xFC00 + se);
				putdatallhh((USHORT) symvalue);
				putdata1((UCHAR) (symvalue >> 16));
				dataadr += 6;
			}
			sc = 4;
			return(RULE_NOPARSE + NEXT_LIST);
		default:
			sc = 0;
			if (delimiter) return(RULE_PARSEVAR + NEXT_NOPARSE);
			symtype = TYPE_LIST;
			symvalue = saveadr;
			savdataadr = saveadr;
			if (!recdefonly) {
				dataadr += 2;
				if (recordlevel) {
					if (putrecdsym(label) < 0) return(NEXT_TERMINATE);
				}
				else if (!recdefonly) putdsym(label, FALSE);
			}
			putdata1(0xF4);
			if (recordlevel) recdeftail(symtype, 2);
			return(NEXT_TERMINATE);
	}
}

UINT s_file_fnc()
{
	UCHAR i, j, k;
	static UCHAR fixflg, varflg, compflg, uncompflg, dupflg, nodupflg;
	static UCHAR initialflg;
	static UCHAR davbinfo[9];
	static UCHAR globflg;
	static INT inheritableflg;
	INT n;

	if (rulecnt == 1) {
		if (recordlevel) recdefhdr(label);
		fixflg = varflg = compflg = uncompflg = dupflg = nodupflg = FALSE;
		inheritableflg = FALSE;
		i = codebuf[0];
		initialflg = FALSE;
		codebufcnt = sd = 0;
		memset(davbinfo, 0, sizeof(davbinfo));	 /* clear information header */
		kwdupchk(0xFF);
		if (i >= 227) {
			putdata1(0xF3);
			putdata((UCHAR *) label, (INT) (strlen(label) + 1));
			globflg = 1;
		}
		else {
			if (withnamesflg) putdataname(label);
			globflg = 0;
		}
		sl = dataadr;
		switch (i) {
			case 210:
				sc = FKW_FILE;
				symtype = TYPE_FILE;
				j = filedflt;
				break;
			case 211:
				sc = FKW_IFILE;
				symtype = TYPE_IFILE;
				j = ifiledflt;
				break;
			case 212:
				sc = FKW_AFILE;
				symtype = TYPE_AFILE;
				j = afiledflt;
				break;
			case 217:
			case 227:
				sc = FKW_IMAGE;
				symtype = TYPE_IMAGE;
				break;
			default:
				sc = FKW_QUEUE;
				symtype = TYPE_QUEUE;
		}
 		if (TOKEN_AMPERSAND == tokentype) {
			if ('&' == line[linecnt]) {
				linecnt++;
				if (globflg) error(ERROR_NOGLOBALINHERITS);
				else s_inherited_fnc(symtype);
				return(NEXT_TERMINATE);
			}
			if (!classlevel) error(ERROR_INHRTABLENOTALLOWED);
			inheritableflg = TRUE;
			if ('@' == line[linecnt]) {
				linecnt++;
				tokentype = TOKEN_ATSIGN;
			}
			else {
				whitespace();
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_ATSIGN) tokentype = 0;	/* treat as @ as a comment */
			}
		}
		if (i > 209 && i < 213) {
			davbinfo[0] |= 0x03;	/* set default to VAR=256, COMP */
			davbinfo[4] = 0x01;		/* 256 record length */
			switch (j) {
				case 1:			/* compile with -xFILE=TEXT */
					davbinfo[1] |= 0x08;
					break;
				case 2:			/* compile with -xFILE=NATIVE */
					davbinfo[0] |= 0x20;
					break;
				case 3:			/* compile with -xFILE=BINARY */
					davbinfo[1] |= 0x20;
					break;
				case 4:			/* compile with -xFILE=DATA */
					davbinfo[1] |= 0x10;
					break;
				case 5:			/* compile with -xFILE=CRLF */
					davbinfo[1] |= 0x18;
					break;
			}
		}
		if (tokentype == TOKEN_ATSIGN) {
			if (globflg) error(ERROR_GLOBALADRVAR);
			if (i < 213) sd = (UCHAR) (i - 208);
			else if (sc == FKW_IMAGE) sd = 8;
			else sd = 9;
			symvalue = dataadr;
			symtype |= TYPE_PTR;
 			if (!recdefonly) dataadr += 6;
			if (recordlevel) putrecdsym(label);
			else if (!recdefonly) putdsym(label, inheritableflg);
			if (line[linecnt] && charmap[line[linecnt]] != 2) {
				initialflg = TRUE;
				su = symtype;
				return(RULE_NOPARSE + NEXT_COMMA);
			}						
			putdatahhll((UINT) 0xFC00 + sd);
			if (recordlevel) recdeftail(symtype, 6);
			return(NEXT_TERMINATE);
		}
		if (tokentype == TOKEN_ASTERISK && sc <= FKW_AFILE) {
			if (inheritableflg) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			if (globflg) {
				error(ERROR_BADCOMMON);
				return(NEXT_TERMINATE);
			}
			putdata1(0xFD);
			return(RULE_PARSEUP + NEXT_REPEAT);
		}
	}
	else if (initialflg) {	/* parse , INITIAL following @ */
		if (2 == rulecnt) {	/* just parsed comma */
			whitespace();
			scantoken(SCAN_UPCASE);
			if (tokentype != TOKEN_WORD || 
			    strcmp((CHAR *) token, "INITIAL")) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			whitespace();
			return(RULE_PARSEVAR + NEXT_NOPARSE);
		}
		else {	
			if (tokentype != TOKEN_WORD) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			if (getdsym((CHAR *) token) == -1) {
				error(ERROR_VARNOTFOUND);
				return(NEXT_TERMINATE);
 			}
			if (symtype != (su & ~TYPE_PTR)) {
				error(ERROR_BADVARTYPE);
				return(NEXT_TERMINATE);
			}
			arrayspec[0] = arrayspec[1] = 0;
			putinitaddr(symvalue, arrayspec, symtype, FALSE);
			if (recordlevel) recdeftail(su, 6);
			if (line[linecnt] && charmap[line[linecnt]] != 2) error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
	}
	if (!sd) {
		if (token[0] && token[0] != '.') {
			switch (token[0]) {
				case 'A': i = FILEKW_AIM; break;
				case 'B': i = FILEKW_BINARY; break;
				case 'C': i = FILEKW_COBOL; break;
				case 'D': i = FILEKW_DATA; break;
				case 'E': i = FILEKW_ENTRIES; break;
				case 'F':
					i = FILEKW_FIXED;
					codebufcnt = 0;  /* should not be needed to to changes 05/01/02 */
					break;
				case 'H': i = FILEKW_H; break;
				case 'I': i = FILEKW_INC; break;
				case 'K':
					i = FILEKW_KEYLEN;
					codebufcnt = 0;  /* should not be needed to to changes 05/01/02 */
					break;
				case 'N': i = FILEKW_NATIVE; break;
				case 'O': i = FILEKW_OVERLAP; break;
				case 'S': i = FILEKW_SIZE; break;
				case 'T': i = FILEKW_TEXT; break;
				case 'U': i = FILEKW_UNCOMP; break;
				case 'V':
					i = FILEKW_V;
					codebufcnt = 0;  /* should not be needed to to changes 05/01/02 */
					break;
				default: i = FILEKW_NULL;
			}
			j = (UCHAR) strlen((CHAR *) token);
			while (TRUE) {
				if (!filekywrd[i].minlen) {
					if ((n = strcmp(filekywrd[i].name, (CHAR *) token)) >= 0) break;
				}
				else {
					if (j >= filekywrd[i].minlen) {
						k = (UCHAR) strlen(filekywrd[i].name);
						if (j <= k && (n = strncmp(filekywrd[i].name, (CHAR *) token, j)) >= 0) break;
					}
				}
				i++;
			}
			if (n || !(filekywrd[i].valid & sc)) {
				error(ERROR_BADKEYWORD);
				return(NEXT_TERMINATE);
			}
			su = i;
			sd = 1;
			if (filekywrd[i].equalvalu) {
				if (line[linecnt] != '=') {
					if (filekywrd[i].equalvalu == 3) {
						if (i == FILEKW_VARIABLE) {
							if (fixflg) {
								error(ERROR_FIXVARFILE);
								return(NEXT_TERMINATE);
							}
							varflg = TRUE;
							kwdupchk(0);
							su = FILEKW_NULL;
						}
					}
					else {
						error(ERROR_SYNTAX);
						return(NEXT_TERMINATE);
					}
				}
				else {
					linecnt++;
					whitespace();
					if (filekywrd[i].equalvalu == 1) return(RULE_DCON + NEXT_LIST);
					return(RULE_NVAR + (RULE_DCON << 8) + NEXT_LIST);
				}
			}
		}
		else {
			su = FILEKW_NULL;
			sd = 1;
		}
		return(RULE_NOPARSE + NEXT_LIST);
	}
	switch (su) {
		case FILEKW_FIXED:	/* FIXED */
		case FILEKW_VARIABLE:	/* VARIABLE */
			if (su == FILEKW_FIXED) {
				if (varflg) {
					error(ERROR_FIXVARFILE);
					return(NEXT_TERMINATE);
				}
				davbinfo[0] &= ~0x01;  /* turn VAR off - FIXED length file */
				davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
				fixflg = TRUE;
			}
			else {
				if (fixflg) {
					error(ERROR_FIXVARFILE);
					return(NEXT_TERMINATE);
				}
				varflg = TRUE;
			}
			kwdupchk(0);
			if (symbasetype == TYPE_NVAR) davbinfo[0] |= 0x40;
			else if (symvalue < 1 || symvalue > 65500) error(ERROR_BADRECSIZE);
			davbinfo[3] = (UCHAR) symvalue;
			davbinfo[4] = (UCHAR)(symvalue >> 8);
			davbinfo[5] = (UCHAR)(symvalue >> 16);
			break;
		case FILEKW_COMP:	/* COMPRESSED */
			if (uncompflg) {
				error(ERROR_COMPUNCOMPFILE);
				return(NEXT_TERMINATE);
			}
			if (fixflg) {
				error(ERROR_FIXCOMPFILE);
				return(NEXT_TERMINATE);
			}
			compflg = TRUE;
			kwdupchk(3);
			davbinfo[0] |= 0x02;
			break;
		case FILEKW_UNCOMP:	/* UNCOMPRESSED */
			if (compflg) {
				error(ERROR_COMPUNCOMPFILE);
				return(NEXT_TERMINATE);
			}
			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
			uncompflg = TRUE;
			kwdupchk(3);
			break;
		case FILEKW_STANDARD: /* STANDARD */
			kwdupchk(4);
			davbinfo[0] &= ~0x20;  /* turn off NATIVE */
			davbinfo[1] &= ~0x38;  /* turn off other types & set to standard */
			break;
		case FILEKW_DATA:	/* DATA */
			kwdupchk(4);
			davbinfo[0] &= ~0x20;  /* turn off NATIVE */
			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
			davbinfo[1] &= ~0x38;  /* turn off other types */
			davbinfo[1] |= 0x10;   /* set to data */
			break;
		case FILEKW_CRLF:	/* CRLF */
			kwdupchk(4);
			davbinfo[0] &= ~0x20;  /* turn off NATIVE */
			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
			davbinfo[1] &= ~0x38;  /* turn off other types */
			davbinfo[1] |= 0x18;   /* set to crlf */
			break;
		case FILEKW_TEXT:	/* TEXT */
		case FILEKW_DOS:	/* DOS */
			kwdupchk(4);
			davbinfo[0] &= ~0x20;  /* turn off NATIVE */
			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
			davbinfo[1] &= ~0x38;  /* turn off other types */
			davbinfo[1] |= 0x08;   /* set to text */
			break;
		case FILEKW_BINARY:	/* BINARY */
			kwdupchk(4);
			davbinfo[0] &= ~0x20;  /* turn off NATIVE */
			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
			davbinfo[1] &= ~0x38;  /* turn off other types */
			davbinfo[1] |= 0x20;   /* set to text */
			break;
//		case FILEKW_EBCDIC:	/* EBCDIC */
//			kwdupchk(4);
//			davbinfo[0] &= ~0x20;  /* turn off NATIVE */
//			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
//			davbinfo[1] &= ~0x38;  /* turn off other types */
//			davbinfo[1] |= 0x28;   /* set to ebcdic */
//			break;
		case FILEKW_NATIVE:	/* NATIVE */
			kwdupchk(4);
			davbinfo[0] &= ~0x02;  /* Turn COMP (default) off */
			davbinfo[0] |= 0x20;   /* set to NATIVE */
			davbinfo[1] &= ~0x38;  /* turn off file types */
			break;
		case FILEKW_KEYLEN:	/* KEYLENGTH */
			kwdupchk(1);
			davbinfo[0] |= 0x80;
			if (symbasetype == TYPE_NVAR) davbinfo[0] |= 0x10;
			else if (symvalue < 1 || symvalue > 255) error(ERROR_BADKEYLENGTH);
			davbinfo[6] = (UCHAR) symvalue;
			davbinfo[7] = (UCHAR)(symvalue >> 8);
			davbinfo[8] = (UCHAR)(symvalue >> 16);
			break;
		case FILEKW_DUP:	/* DUPLICATES */
			if (nodupflg) {
				error(ERROR_DUPNODUPFILE);
				return(NEXT_TERMINATE);
			}
			kwdupchk(7);
			davbinfo[0] |= 0x04;
			dupflg = TRUE;
			break;
		case FILEKW_NODUP:	/* NODUPLICATES */
			if (dupflg) {
				error(ERROR_DUPNODUPFILE);
				return(NEXT_TERMINATE);
			}
			kwdupchk(7);
			davbinfo[1] |= 0x40;
			nodupflg = TRUE;
			break;
		case FILEKW_STATIC:	/* STATIC */
			kwdupchk(2);
			if (symvalue > 255) symvalue = 255;
			davbinfo[2] = (UCHAR) symvalue;
			break;
#if 0  /* never implemented in dbc */
		case FILEKW_TRANSLATE:	/* TRANSLATE */
			kwdupchk(6);
			davbinfo[1] |= 0x01;
			break;
#endif
		case FILEKW_COBOL:	/* COBOL */
			kwdupchk(5);
			davbinfo[1] |= 0x80;
			break;
		case FILEKW_ENTRIES:	/* ENTRIES */
		case FILEKW_H:	/* H */
			kwdupchk(0);
			davbinfo[0] = (UCHAR) symvalue;
			davbinfo[1] = (UCHAR)(symvalue >> 8);
			break;
		case FILEKW_SIZE:	/* SIZE */
		case FILEKW_V:	/* V */
			kwdupchk(1);
			davbinfo[2] = (UCHAR) symvalue;
			davbinfo[3] = (UCHAR)(symvalue >> 8);
			break;
		case FILEKW_COLORBITS:	/* COLORBITS */
			kwdupchk(2);
			davbinfo[4] = (UCHAR) symvalue;
			if (symvalue != 1 &&
			    symvalue != 4 &&
			    symvalue != 8 &&
			    symvalue != 16 && 
			    symvalue != 24) {
				error(ERROR_BADCOLORBITS);
				return(NEXT_TERMINATE);
			}
			break;
		case FILEKW_NULL:	/* no options specified */
			break;
	}
	if (delimiter) {
		sd = 0;
		codebufcnt = 0;
		return(0);
	}
	if (sc <= FKW_AFILE) {
		if (sc == FKW_FILE) {
			putdata1(0xF0);
			symtype = TYPE_FILE;
		}
		else if (sc == FKW_IFILE) {
			putdata1(0xF1);
			symtype = TYPE_IFILE;
		}
		else {
			putdata1(0xF2);
			symtype = TYPE_AFILE;
		}
		sd = 9;
	}
	else if (sc == FKW_IMAGE) {
		if (!davbinfo[0] && !davbinfo[1]) error(ERROR_NOH);
		if (!davbinfo[2] && !davbinfo[3]) error(ERROR_NOV);
		putdatahhll(0xFC11);
		symtype = TYPE_IMAGE;
		sd = 5;
	}
	else {  /* FKW_QUEUE */
		if (!davbinfo[0] && !davbinfo[1]) {
			kwdupchk(0);
			davbinfo[0] = (UCHAR) 32;
		}
		if (!davbinfo[2] && !davbinfo[3]) {
			kwdupchk(1);
			davbinfo[2] = (UCHAR) 32;
		}
		putdatahhll(0xFC12);
		symtype = TYPE_QUEUE;
		sd = 4;
	}
	putdata(davbinfo, sd);
	symvalue = sl;
	if (recordlevel) {
		if (putrecdsym(label) < 0) return(NEXT_TERMINATE);
		if (globflg) recdeftail(symtype, 6);
		else recdeftail(symtype, 44);
	}
	else if (!recdefonly) putdsym(label, inheritableflg);
	if (!recdefonly) {
		if (globflg) dataadr += 6;
		else dataadr += 44;
	}
	codebufcnt = 0;
	return(NEXT_TERMINATE);
}

#define PFILESTATE_START 1
#define PFILESTATE_COMMAINITIAL 2
#define PFILESTATE_INITIALIZER 3

UINT s_pfile_fnc()
{
	static UINT datatype;
	static INT inheritableflg;
	UCHAR datacode;

	if (1 == rulecnt) {
		if (recordlevel) recdefhdr(label);
		codebufcnt = 0;
		inheritableflg = FALSE;
		sc = PFILESTATE_START;
		datatype = codebuf[0];
		if (datatype > 216) {	
			putdata1(0xF3);		/* global prefix */
			putdata((UCHAR *) label, (INT) (strlen(label) + 1));
		}
		else if (withnamesflg) putdataname(label);
	}
	switch (sc) {
		case PFILESTATE_START:
			if (tokentype == TOKEN_ASTERISK) {
				if (datatype > 216) {
					error(ERROR_BADCOMMON);
					return(NEXT_TERMINATE);
				}
				else putdata1(0xFD);	/* common prefix */
			}
			else if (TOKEN_AMPERSAND == tokentype) {
				if (inheritableflg) {	/* ampersand already parsed */
					error(ERROR_SYNTAX);
					return(NEXT_TERMINATE);
				}
				if ('&' == line[linecnt]) {
					linecnt++;
					if (datatype > 216) error(ERROR_NOGLOBALINHERITS);
					else {
						switch (datatype) {
							case 213: s_inherited_fnc(TYPE_PFILE); break;
							case 214: s_inherited_fnc(TYPE_COMFILE); break;
							case 215: s_inherited_fnc(TYPE_DEVICE); break;
							default: s_inherited_fnc(TYPE_RESOURCE);
						}
					}
				}
				else if (!classlevel) error(ERROR_INHRTABLENOTALLOWED);
				else {
					inheritableflg = TRUE;
					return(RULE_PARSE + NEXT_NOPARSE);
				}
				return(NEXT_TERMINATE);
			}
			symvalue = dataadr;
			if (tokentype == TOKEN_ATSIGN) {  /* address variable */
				if (datatype > 216) {
					error(ERROR_GLOBALADRVAR);
					return(NEXT_TERMINATE);
				}
				switch (datatype) {
					case 213:		/* PFILE @ */
						datacode = 0x0C;
						symtype = TYPE_PFILE | TYPE_PTR;
						break;
					case 214:		/* COMFILE @ */
						datacode = 0x06;
						symtype = TYPE_COMFILE | TYPE_PTR;
						break;
					case 215:		/* DEVICE @ */
						datacode = 0x0B;
						symtype = TYPE_DEVICE | TYPE_PTR;
						break;
					default:		/* RESOURCE @ */
						datacode = 0x0A;
						symtype = TYPE_RESOURCE | TYPE_PTR;
				}
				if (recordlevel) {
					if (putrecdsym(label) < 0) return(NEXT_TERMINATE);
				}
				else if (!recdefonly) putdsym(label, inheritableflg);
				if (!recdefonly) dataadr += 6;
				if (line[linecnt] && charmap[line[linecnt]] != 2) {
					sc = PFILESTATE_COMMAINITIAL;
					datatype = symtype & ~TYPE_PTR;
					return(RULE_NOPARSE + NEXT_COMMA);
				}						
				putdatahhll((UINT) 0xFC00 + datacode);
				if (recordlevel) recdeftail(symtype, 6);
			}
			else {
				switch (datatype) {
					case 213:		/* PFILE */
					case 224:		/* GPFILE */
						putdatahhll(0xFC15);
						symtype = TYPE_PFILE;
						break;
					case 214:		/* COMFILE */
						putdatahhll(0xFC10);
						symtype = TYPE_COMFILE;
						break;
					case 215:		/* DEVICE */
					case 225:		/* GDEVICE */
						putdatahhll(0xFC14);
						symtype = TYPE_DEVICE;
						break;
					default:		/* RESOURCE */
						putdatahhll(0xFC13);
						symtype = TYPE_RESOURCE;
				}
				if (datatype > 216) {	/* global */
					if (!recdefonly) dataadr += 6;
					if (recordlevel) recdeftail(symtype, 6);
				}
				else {
					if (!recdefonly) dataadr += 44;
					if (recordlevel) recdeftail(symtype, 44);
				}
				if (recordlevel) putrecdsym(label);
				else if (!recdefonly) putdsym(label, inheritableflg);
			}
			break;
		case PFILESTATE_COMMAINITIAL:
			whitespace();
			scantoken(SCAN_UPCASE);
			if (tokentype != TOKEN_WORD || 
			    strcmp((CHAR *) token, "INITIAL")) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			whitespace();
			sc = PFILESTATE_INITIALIZER;
			return(RULE_PARSEVAR + NEXT_NOPARSE);
		case PFILESTATE_INITIALIZER:
			if (tokentype != TOKEN_WORD) {
				error(ERROR_SYNTAX);
				return(NEXT_TERMINATE);
			}
			if (getdsym((CHAR *) token) == -1) {
				error(ERROR_VARNOTFOUND);
				return(NEXT_TERMINATE);
 			}
			if (symtype != datatype) {
				error(ERROR_BADVARTYPE);
				return(NEXT_TERMINATE);
			}
			arrayspec[0] = arrayspec[1] = 0;
			putinitaddr(symvalue, arrayspec, symtype, FALSE);
			if (recordlevel) recdeftail(symtype | TYPE_PTR, 6);
			if (line[linecnt] && charmap[line[linecnt]] != 2) error(ERROR_SYNTAX);
			break;
	}
	return(NEXT_TERMINATE);
}

UINT s_label_fnc()
{
	codebufcnt = 0;
	if (230 == codebuf[0]) {
		if (usepsym(label, LABEL_EXTERNAL) == RC_ERROR) return (NEXT_TERMINATE);
	}
	else usepsym(label, LABEL_VARDEF);
	return(0);
}

static UINT s_fill_fnc()
{
	if (rulecnt == 1) {
		if (symtype == TYPE_LITERAL) {
			symvalue = makeclit(token);
			putcodeadr(symvalue);
		}
	}
	return(0);
}

static UINT s_show_fnc()
{
	if (delimiter) {
		codebuf[1] = 168;	/* long form of show verb */
		return(0);
	}
	else return(NEXT_TERMINATE);
}

UINT s_record_fnc()
{
	INT curdefstart, newdefstart;

	whitespace();
	codebufcnt = 0;
	if (codebuf[0] == 231 /* 0xE7 */) {
		if (recordlevel + 1 > 1) {
			error(ERROR_RECORDDEPTH);
			return(NEXT_TERMINATE);
		}
		recdefonly = FALSE;
		if (tokentype == TOKEN_WORD) {
			if (!strcmp((CHAR *) token, "LIKE")) {
				scantoken(upcaseflg);
				if (tokentype == TOKEN_WORD) {
					curdefstart = getrecdef((CHAR *) token);
					if (curdefstart >= 0) {
						whitespace();
						scantoken(SCAN_UPCASE);
						if (tokentype == TOKEN_WORD && !strcmp((CHAR *) token, "WITH")) {
							whitespace();
							scantoken(SCAN_UPCASE);
							if (tokentype == TOKEN_WORD && !strcmp((CHAR *) token, "NAMES")) {
								if (!withnamesflg) {
									withnamesflg = TRUE;	
									nameslevel = -1;
								}
							}
							else	error(ERROR_SYNTAX);
						}		
						newdefstart = putrecdef(label, curdefstart);
						if (newdefstart >= 0) makelist(curdefstart);
						if (withnamesflg && nameslevel == -1) {
							withnamesflg = FALSE;
						}
					}
					else error(ERROR_RECNOTFOUND);
					return(NEXT_TERMINATE);
				}
			}
			else if (!strcmp((CHAR *) token, "DEFINITION")) recdefonly = TRUE;
			else if (!strcmp((CHAR *) token, "WITH")) {
				scantoken(SCAN_UPCASE);
				if (tokentype == TOKEN_WORD && !strcmp((CHAR *) token, "NAMES")) {
					if (!withnamesflg) {
						withnamesflg = TRUE;	
						nameslevel = -1;
					}
				}
				else error(ERROR_SYNTAX);
			}
		}
		if (!recdefonly) {
			if (withnamesflg) putdataname(label);
			symtype = TYPE_LIST;
			symvalue = dataadr;
			putdata1(0xF9);
			putdsym(label, FALSE);
			dataadr++;
		}
		newdefstart = putrecdef(label, -1);  /* allocate new rec def space */
		if (newdefstart < 0) return(NEXT_TERMINATE);
		putdef1(0x00);	 /* field name length zero */
		putdef1(0xF9);  /* code for list */
		putdef1(0xF5);  /* end of data area code */
		putdeflmh(0x000001); /* increase amnt for dataadr */
		putdef1(TYPE_LIST);  /* symbol table type */
		recordlevel++;
	}
	else {
		if (!recordlevel) error(ERROR_MISSINGRECORD);
		else {
			if (withnamesflg && nameslevel == -1) {
				withnamesflg = FALSE;
			}
			putdef1(0x00);	 /* field name length zero */
			putdef1(0xF4);  /* code for listend */
			putdef1(0xF5);  /* end of data area code */
			putdeflmh(0x000001); /* increase amnt for dataadr */
			putdef1(TYPE_LIST); /* symbol table type */
			putdef1(0xFF);
			recordlevel--;
			if (!recdefonly) {
				putdata1(0xF4);
				dataadr++;
			}
			recdefonly = FALSE;
		}
	}
	return(NEXT_TERMINATE);
}

static UINT s_edit_fnc()
{
	if (rulecnt == 2) {
		if (delimiter) {
			vexpand();
			codebuf[0] = 88;
			codebuf[1] = 164;
			return(RULE_CVAR + NEXT_END);
		}
		else if (symtype == TYPE_LITERAL) error(ERROR_SYNTAX);
	}
	return(NEXT_TERMINATE);
}

static UINT s_keyin_fnc()
{
	tmpvarflg = 3;  			/* do NOT reuse temporary variables */
	constantfoldflg = FALSE;		/* turn constant folding off */
	return(0);
}

static UINT s_display_fnc()
{
	constantfoldflg = FALSE;		/* turn constant folding off */
	return(0);
}

static UINT s_next_semilist_fnc()
{
	if (delimiter == TOKEN_SEMICOLON) {
		/* syntax error if something immediately follows the semicolon */
		if (line[linecnt] && !isspace(line[linecnt]) && linecnt && line[linecnt - 1] == ';') error(ERROR_SYNTAX);
		putcode1(0xFE);
#if 0
		if (tokentype == TOKEN_SEMICOLON) return(NEXT_TERMINATE);  /* ;; case following read verb */
#else
		return(NEXT_TERMINATE);
#endif
	}
	else if (!delimiter) {
		putcode1(0xFF);
		return(NEXT_TERMINATE);
	}
	return(0);
}

static UINT s_next_list_fnc()
{
	if (delimiter) return(0);
	putcode1(0xFF);
	return(NEXT_TERMINATE);
}

static UINT s_setendkey_fnc()
{
	if (sc && rulecnt != 1) {	/* delimiter was just parsed */
		if (!delimiter) {
			putcode1(0xFF);
			return(NEXT_TERMINATE);
		}
		sc = 0;
		return(RULE_PARSEVAR + NEXT_NOPARSE);  /* scan token next */
	}
	sc = 1;  /* token just scanned, parse delimiter next */
	if (tokentype == TOKEN_NUMBER) symvalue = atol((CHAR *) token);
	if (tokentype == TOKEN_WORD) {
		if (getdsym((CHAR *) token) == -1) {
			error(ERROR_VARNOTFOUND);
			return(NEXT_TERMINATE);
		}
		if (symtype != TYPE_EQUATE) {
			sf = 1;
			symbasetype = (UCHAR) (symtype & ~TYPE_PTR);
			if (line[linecnt] == '[' || line[linecnt] == '(') {
				if (symbasetype == TYPE_NARRAY1 || symtype == TYPE_NVARPTRARRAY1) sf = arraysymaddr(1);
				else if (symbasetype == TYPE_NARRAY2 || symtype == TYPE_NVARPTRARRAY2) sf = arraysymaddr(2);
				else if (symbasetype == TYPE_NARRAY3 || symtype == TYPE_NVARPTRARRAY3) sf = arraysymaddr(3);
				else error(ERROR_BADVARTYPE);
			}
			else {
				if (symbasetype == TYPE_NVAR) putcodeadr(symvalue);
				else if (symbasetype == TYPE_NARRAY1 || symbasetype == TYPE_NARRAY2 || symbasetype == TYPE_NARRAY3) putcodeadr(symvalue);
				else if (symbasetype == TYPE_NVARPTRARRAY1 || symbasetype == TYPE_NVARPTRARRAY2 || symbasetype == TYPE_NVARPTRARRAY3) putcodeadr(symvalue);
				else error(ERROR_BADVARTYPE);
			}
			if (!sf) return(NEXT_TERMINATE);
			return(RULE_NOPARSE + NEXT_LIST);
		}
		else tokentype = TOKEN_NUMBER;
	}
	if (tokentype == TOKEN_NUMBER) {
		if (symvalue < 256) {
			putcode1(0xFE);
			putcode1((UCHAR) symvalue);
		}
		else if (symvalue < 512) {
			putcode1(0xFD);
			putcode1((UCHAR) (symvalue - 256));
		}
		else if (symvalue < 0xFFFF) {
			putcode1(0xFC);
			putcodehhll((UINT) symvalue);
		}
		else {
			error(ERROR_BADENDKEYVALUE);
			return(NEXT_TERMINATE);
		}
		return(RULE_NOPARSE + NEXT_LIST);
	}
	else if (tokentype == TOKEN_MINUS) {
		scantoken(upcaseflg);
		if (tokentype != TOKEN_NUMBER) {
			error(ERROR_SYNTAX);
			return(NEXT_TERMINATE);
		}
		return(RULE_NOPARSE + NEXT_LIST);
	}
	else {
		error(ERROR_SYNTAX);
		return(NEXT_TERMINATE);
	}
}

static UINT s_setnull_fnc()
{
	if (delimiter) {
		putcode1(0xC0);
		putcodehhll(0x58E9);
	}
	return(0);
}

static UINT s_rotate_fnc()
{
	if (symtype != TYPE_EXPRESSION && !elementref && symbasetype >= TYPE_NARRAY1 && symbasetype <= TYPE_NARRAY3) {
		error(ERROR_SYNTAX);
		return(NEXT_TERMINATE);
	}
	return(0);
}

INT kwdupchk(UINT n)  /* check for duplication of keyword or keyword group */
{
	if (n == 0xFF) memset(kwdflag, 0, 8);
	else if (n > 7 || kwdflag[n]) {
		error(ERROR_DUPKEYWORD);
		return(-1);
	}
	else kwdflag[n] = 1;
	return(0);
}

void checknesting()  /* check if matching endif, repeat, and endswitch exist */
{
	if (iflevel) error(ERROR_MISSINGENDIF);
	if (looplevel) error(ERROR_MISSINGREPEAT);
	if (switchlevel) error(ERROR_MISSINGENDSWITCH);
	if (recordlevel) error(ERROR_MISSINGRECORDEND);
	if (listlevel) error(ERROR_MISSINGLISTEND);
	if (classlevel) error(ERROR_MISSINGCLASSEND);
	if (warning_2fewendroutine == TRUE && rlevel > 0) warning(WARNING_NOTENOUGHENDROUTINES);
	if (!CCD_STACKEMPTY()) error(ERROR_MISSINGCCDENDIF);
}

void savecode(INT n)
{
	memcpy(codesave[n], codebuf, (size_t) codebufcnt); // @suppress("Type cannot be resolved")
	codelength[n] = codebufcnt;
}

void restorecode(INT n)
{
	memcpy(&codebuf[codebufcnt], codesave[n], (size_t) codelength[n]); // @suppress("Type cannot be resolved")
	codebufcnt += codelength[n];
}

void setifopdir(INT ansiflag)
{
	if (ansiflag) ccdifopdir = 0x00;
	else ccdifopdir = CCD_IFOPDIR;
}
