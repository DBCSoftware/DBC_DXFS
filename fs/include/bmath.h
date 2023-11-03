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

extern int badd(char *src, char *dest);
extern int bcomp(char *src, char *dest);
extern int bdiv(char *src, char *dest);
extern void bfmt(char *src, int rgt, int neg);	/* converts digital string back to numeric */
extern void bform(char *dest, int lft, int rgt);
extern void binfo(char *src, int *lft, int *rgt, int *neg);
extern int bisnum(char *src);					/* return true if valid number */
extern int biszero(char *src);
extern int bmove(char *src, char *dest);
extern int bmult(char *src, char *dest);
extern void bnorm(char *src);					/* pads src with digits */
extern void bsetzero(char *src);
extern int bsub(char *src, char *dest);
extern void bsubx(char *src, char *dest);		/* subtract routine used by badd and bsub */
