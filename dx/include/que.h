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

typedef struct queuestruct **QUEHANDLE;

INT quecreate(INT, INT, INT, QUEHANDLE *);
INT quedestroy(QUEHANDLE);
void quedestroyall(void);
void queempty(QUEHANDLE);
INT quetest(QUEHANDLE);
INT quewait(QUEHANDLE);
INT queget(QUEHANDLE, UCHAR *, INT *);
INT queput(QUEHANDLE, UCHAR *, INT);
INT queputfirst(QUEHANDLE, UCHAR *, INT);


