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

#define TIMMINTIME "1901010100000000"
#define TIMMAXTIME "2099123123595999"

typedef struct {
	INT yr;
	INT mon;
	INT day;
	INT hr;
	INT min;
	INT sec;
	INT hun;
} TIMSTRUCT;

extern INT timinit(void);
extern void timexit(void);
extern INT timset(UCHAR *, INT);
extern void timstop(INT);
extern INT timadd(UCHAR *, INT);
extern void timgettime(TIMSTRUCT *dest);
extern CHAR* getUTCOffset(void);
