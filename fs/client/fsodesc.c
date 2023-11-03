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
#define INC_SQL
#define INC_SQLEXT
#include "includes.h"
#include "fsodbc.h"

/**
 * Calls allocmem and needs thread protection from the caller.
 */
LPVOID desc_get(DESCRIPTOR *desc, INT number, INT initflag)
{
	SQLSMALLINT count, stopcount;
	APP_DESC_RECORD *appd_rec, **appd_recp;
	IMP_PARAM_DESC_RECORD *ipd_rec, **ipd_recp;
	IMP_ROW_DESC_RECORD *ird_rec, **ird_recp;

/* NOTE: caller is responsible for calling error_record if error */
	if (!number) return (LPVOID) NULL;

	count = 0;
	stopcount = (number < desc->sql_desc_count) ? number : desc->sql_desc_count;
	if (desc->type == TYPE_APP_DESC) {
		if (desc->sql_desc_count) {
			for (appd_rec = desc->firstrecord.appdrec; ++count < stopcount; appd_rec = appd_rec->nextrecord);
			if (count == number) return (LPVOID) appd_rec;
			appd_recp = &appd_rec->nextrecord;
		}
		else appd_recp = &desc->firstrecord.appdrec;
		for ( ; ; appd_recp = &(*appd_recp)->nextrecord) {
			if (*appd_recp == NULL) {
				appd_rec = (APP_DESC_RECORD *) allocmem(sizeof(APP_DESC_RECORD), 1);
				if (appd_rec == NULL) return (LPVOID) NULL;
				*appd_recp = appd_rec;
			}
			else {
				appd_rec = *appd_recp;
				if (initflag && !appd_rec->initialized) memset(appd_rec, 0, sizeof(APP_DESC_RECORD));
			}
			if (initflag && !appd_rec->initialized) {
				appd_rec->initialized = TRUE;
				appd_rec->sql_desc_concise_type = SQL_C_DEFAULT;
				/* sql_desc_data_ptr = NULL */
				/* sql_desc_datetime_interval_code, NoDefault */
				/* sql_desc_datetime_interval_precision, NoDefault */
				/* sql_desc_indicator_ptr = NULL */
				/* sql_desc_length, NoDefault */
				/* sql_desc_name, NoDefault */
				/* sql_desc_nullable, NoDefault */
				/* sql_desc_num_prec_radix, NoDefault */
				/* sql_desc_octet_length, NoDefault */
				/* sql_desc_octet_length_ptr = NULL */
				/* sql_desc_precision, NoDefault */
				/* sql_desc_scale, NoDefault */
				appd_rec->sql_desc_type = SQL_C_DEFAULT;
				/* sql_desc_unnamed, NoDefault */
			}
			if (++count == number) break;
		}
		return (LPVOID) *appd_recp;
	}
	else if (desc->type == TYPE_IMP_PARAM_DESC) {
		if (desc->sql_desc_count) {
			for (ipd_rec = desc->firstrecord.ipdrec; ++count < stopcount; ipd_rec = ipd_rec->nextrecord);
			if (count == number) {
				return (LPVOID) ipd_rec;
			}
			ipd_recp = &ipd_rec->nextrecord;
		}
		else ipd_recp = &desc->firstrecord.ipdrec;
		for ( ; ; ipd_recp = &(*ipd_recp)->nextrecord) {
			if (*ipd_recp == NULL) {
				ipd_rec = (IMP_PARAM_DESC_RECORD *) allocmem(sizeof(IMP_PARAM_DESC_RECORD), 1);
				if (ipd_rec == NULL) return (LPVOID) NULL;
				*ipd_recp = ipd_rec;
			}
			else {
				ipd_rec = *ipd_recp;
/*** CODE: MAYBE THE MEMSET SHOULD OCCUR ALL OF THE TIME ***/
				if (initflag && !ipd_rec->initialized) memset(ipd_rec, 0, sizeof(IMP_PARAM_DESC_RECORD));
			}
/*** CODE: MAYBE THE ipd_rec->initialized SHOULD NOT BE TESTED ***/
			if (initflag && !ipd_rec->initialized) {
				ipd_rec->initialized = TRUE;
				/* sql_desc_case_sensitive, defined only when automatically populated */
				/* sql_desc_concise_type, NoDefault */
				/* sql_desc_datetime_interval_code, NoDefault */
				/* sql_desc_datetime_interval_precision, NoDefault */
				/* sql_desc_fixed_prec_scale, defined only when automatically populated */
				/* sql_desc_length, NoDefault */
				ipd_rec->sql_desc_local_type_name = (SQLCHAR*)"";  /* defined only when automatically populated */
				/* sql_desc_name, NoDefault */
				/* sql_desc_nullable, NoDefault */
				/* sql_desc_num_prec_radix, NoDefault */
				/* sql_desc_octet_length, NoDefault */
				ipd_rec->sql_desc_parameter_type = SQL_PARAM_INPUT;
				/* sql_desc_precision, NoDefault */
				/* sql_desc_scale, NoDefault */
				/* sql_desc_type, NoDefault */
				ipd_rec->sql_desc_type_name = (SQLCHAR*)"";  /* defined only when automatically populated */
				/* sql_desc_unnamed, NoDefault */
				/* sql_desc_unsigned, defined only when automatically populated */
			}
			if (++count == number) break;
		}
		return (LPVOID) *ipd_recp;
	}
	else if (desc->type == TYPE_IMP_ROW_DESC) {
		if (desc->sql_desc_count) {
			for (ird_rec = desc->firstrecord.irdrec; ++count < stopcount; ird_rec = ird_rec->nextrecord);
			if (count == number) return (LPVOID) ird_rec;
			ird_recp = &ird_rec->nextrecord;
		}
		else ird_recp = &desc->firstrecord.irdrec;
		for ( ; ; ird_recp = &(*ird_recp)->nextrecord) {
			if (*ird_recp == NULL) {
				ird_rec = (IMP_ROW_DESC_RECORD *) allocmem(sizeof(IMP_ROW_DESC_RECORD), 1);
				if (ird_rec == NULL) return (LPVOID) NULL;
				*ird_recp = ird_rec;
			}
			else {
				ird_rec = *ird_recp;
				memset(ird_rec, 0, sizeof(IMP_ROW_DESC_RECORD));
			}
			if (initflag) {
				/*** An IRD has no defaults at allocation time ***/
				/*** but type_name needs to be initialized to prevent GPF ***/
				ird_rec->sql_desc_local_type_name = (SQLCHAR*)"";
				ird_rec->sql_desc_type_name = (SQLCHAR*)"";
			}
			if (++count == number) break;
		}
		return (LPVOID) *ird_recp;
	}
	return (LPVOID) NULL;
}

/**
 * Calls freemem and needs thread protection from the caller.
 */
SQLRETURN desc_unbind(DESCRIPTOR *desc, INT number)
{
	SQLSMALLINT count, newcount;
	APP_DESC_RECORD *appd_rec, *appd_rec2;
	IMP_PARAM_DESC_RECORD *ipd_rec, *ipd_rec2;
	IMP_ROW_DESC_RECORD *ird_rec, *ird_rec2;

/* NOTE: caller is responsible for calling error_record if error */
	if (number <= 0) {
		if (desc->type == TYPE_APP_DESC) {
			for (appd_rec = desc->firstrecord.appdrec; appd_rec != NULL; appd_rec = appd_rec2) {
				appd_rec2 = appd_rec->nextrecord;
				if (appd_rec->dataatexecptr != NULL) {
					freemem(appd_rec->dataatexecptr);
					appd_rec->dataatexecptr = NULL;
				}
				if (number < 0) freemem(appd_rec);
				else appd_rec->initialized = FALSE;
			}
		}
		else if (desc->type == TYPE_IMP_PARAM_DESC) {
			for (ipd_rec = desc->firstrecord.ipdrec; ipd_rec != NULL; ipd_rec = ipd_rec2) {
				ipd_rec2 = ipd_rec->nextrecord;
				if (number < 0) freemem(ipd_rec);
				else ipd_rec->initialized = FALSE;
			}
		}
		else if (desc->type == TYPE_IMP_ROW_DESC) {
			for (ird_rec = desc->firstrecord.irdrec; ird_rec != NULL; ird_rec = ird_rec2) {
				ird_rec2 = ird_rec->nextrecord;
				if (number < 0) freemem(ird_rec);
			}
		}
		else return SQL_ERROR;
		desc->sql_desc_count = 0;
		return SQL_SUCCESS;
	}

	if (number > desc->sql_desc_count) return SQL_SUCCESS;

/*** NOTE: MAY ALSO HAVE TO CHECK THE INDICATOR AND LENGTH IN THE RECORD IN BELOW CODE ***/
	count = newcount = 0;
	if (desc->type == TYPE_APP_DESC) {
		for (appd_rec = desc->firstrecord.appdrec; ++count < number; appd_rec = appd_rec->nextrecord)
			if (appd_rec->sql_desc_data_ptr != NULL) newcount = count;
		appd_rec->sql_desc_data_ptr = NULL;
		if (appd_rec->dataatexecptr != NULL) {
			freemem(appd_rec->dataatexecptr);
			appd_rec->dataatexecptr = NULL;
		}
	}
	else if (desc->type == TYPE_IMP_PARAM_DESC) {
		for (ipd_rec = desc->firstrecord.ipdrec; ++count < number; ipd_rec = ipd_rec->nextrecord)
			if (ipd_rec->sql_desc_data_ptr != NULL) newcount = count;
		ipd_rec->sql_desc_data_ptr = NULL;
	}
	else return SQL_ERROR;
	if (number == desc->sql_desc_count) desc->sql_desc_count = newcount;
	return SQL_SUCCESS;
}
