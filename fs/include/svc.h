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

typedef struct {
	char *servicename;
	char *servicedisplayname;
} SVCINFO;

#define SVC_STARTING	1
#define SVC_RUNNING		2
#define SVC_STOPPING	3
#define SVC_STOPPED		4

extern int svcinstall(SVCINFO *, char *, char *, int, char **);
extern int svcremove(SVCINFO *);
extern int svcstart(SVCINFO *);
extern int svcstop(SVCINFO *);
extern void svcrun(SVCINFO *, int (*)(int, char **), void (*)(void));
extern int svcisstarting(SVCINFO *);
extern void svcstatus(int, int);
extern void svclogerror(char *, int);
extern void svcloginfo(char *msg);
extern char *svcgeterror(void);
