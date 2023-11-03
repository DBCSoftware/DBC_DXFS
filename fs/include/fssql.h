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

extern INT sqlconnect(CHAR *cfgfile, CHAR *dbdfile, CHAR *user, CHAR *password, CHAR *logfile);
extern INT sqldisconnect(INT connectid);
extern INT sqlcatalog(INT connectid, UCHAR *buffer, INT buflen, UCHAR *result, INT *resultsize);
extern INT sqlexecute(INT connectid, UCHAR *stmt, INT stmtlen, UCHAR *result, INT *resultsize);
extern INT sqlexecnone(INT connectid, UCHAR *stmt, INT stmtlen, UCHAR *result, INT *resultsize);
extern INT sqlrowcount(INT connectid, INT rsid, UCHAR *result, INT *resultsize);
extern INT sqlgetrow(INT connectid, INT rsid, INT type, LONG row, UCHAR *result, INT *resultsize);
extern INT sqlposupdate(INT connectid, INT rsid, UCHAR *stmt, INT stmtlen);
extern INT sqlposdelete(INT connectid, INT rsid);
extern INT sqldiscard(INT connectid, INT rsid);

extern char *sqlcode(void);
extern char *sqlmsg(void);
extern void sqlmsgclear(void);
