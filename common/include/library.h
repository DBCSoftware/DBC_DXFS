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

#define CURREVISION	"+000"
#define EMPTY		"    "
#define DELETED	"-000"
#define OLDREVISION "+001"
#define SYNTAX	0
#define INVALID	1
#define EQUALS	2
#define FATAL	3
#define MEMBERSIZE 64
#define NUMMEMBERS 63
#define STATSIZE 4
struct mement {
	char	status[STATSIZE];
	char	name[MEMBERSIZE];
	char	blanks[4];
	char	filepos[9];
	char	length[9];
	char	time[4];
	char	date[6];
	char moreblanks[4];
};

struct headblk {
	char title[16];
	char filepos[9];
	char blanks[23];
};

struct dirblk {
	struct headblk header;
	struct mement member[NUMMEMBERS];
};
	
#define DIRSIZE sizeof(struct dirblk)
#define MEMSIZE sizeof(struct mement)
#define HEADSIZE sizeof(struct headblk)
