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

#include <sys/ioctl.h>
#define __SID		('S' << 8)  /* 0x5300 */
#define I_SETSIG    (__SID | 9)	/* Inform the STREAM head that the process
				   wants the SIGPOLL signal issued.  */
/* Possible arguments for `I_SETSIG'.  */
#define S_INPUT		0x0001	/* A message, other than a high-priority
				   message, has arrived.  */
#define S_HIPRI		0x0002	/* A high-priority message is present.  */
#define S_OUTPUT	0x0004	/* The write queue for normal data is no longer
				   full.  */
#define S_ERROR		0x0010	/* Notification of an error condition.  */
#define S_HANGUP	0x0020	/* Notification of a hangup.  */
#define S_RDNORM	0x0040	/* A normal message has arrived.  */
#define S_WRNORM	S_OUTPUT
#define S_RDBAND	0x0080	/* A message with a non-zero priority has
				   arrived.  */
#define S_WRBAND	0x0100	/* The write queue for a non-zero priority
				   band is no longer full.  */
