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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dlfcn.h>

int error(char *);

#if defined (__MACOSX)
#define ODBCLIBNAME "libodbc.dylib"
#else
#define ODBCLIBNAME "libodbc.so"
#endif

#if defined(__MACOSX)
#define LIB1_32 "./fsodbc.dylib"
#define LIB2_32 "./fsodbcu.dylib"
#else
#define LIB1_32 "./fsodbc.a"
#define LIB2_32 "./fsodbcu.a"
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

int main(int argc, char **argv) {
	FILE *lib1_32, *lib2_32;
	void *handle;
	char directory[256], work[256];
	int fhandle;

	printf("Welcome to the FS 101 Unix ODBC driver setup utility.\n");

	/* STEP 1 - Check for installation of unixODBC */
	handle = dlopen(ODBCLIBNAME, RTLD_LAZY);
	if (!handle) {
		printf("WARNING: ");
		printf("Could not find " ODBCLIBNAME "\n");
		printf("\tMake sure that you have installed unixODBC from http://www.unixodbc.org.\n");
	}
	dlclose(handle);

	/* STEP 2 - Check to see if FS ODBC driver libraries are in current directory */
	lib1_32 = fopen(LIB1_32, "r");
	lib2_32 = fopen(LIB2_32, "r");
	if (lib1_32 == NULL) {
		sprintf(work, "Could not find %s in the current directory.\n", LIB1_32);
		return error(work);
	}
	if (lib2_32 == NULL) {
		sprintf(work, "Could not find %s in the current directory.\n", LIB2_32);
		return error(work);
	}
	fclose(lib1_32);
	fclose(lib2_32);

	/* STEP 3 - Create destination directory if necessary */
	printf("Where would you like to install the ODBC Driver? [/usr/local/lib] ");
	fgets(directory, 256, stdin);
	directory[strlen(directory) - 1] = '\0'; /* remove LF from input */
	if (!strlen(directory)) strcpy(directory, "/usr/local/lib"); /* default */
	if (!mkdir(directory, 0)) {
		switch(errno) {
			case EPERM:
				return error("The filesystem does not support the creation of directories in the given path.\n");
			case EEXIST:
				break;
			case EACCES:
				return error("The parent directory does not allow write permission to the process, or one of the directories in pathname did not allow search (execute) permission.\n");
			case ENAMETOOLONG:
				return error("The path name you have specified was too long.\n");
			case ENOENT:
				return error("A directory component in pathname does not exist or is a dangling symbolic link.\n");
			case ENOTDIR:
				return error("A component used as a directory in pathname is not, in fact, a directory.\n");
			case ELOOP:
				return error("Too many symbolic links were encountered in resolving pathname.\n");
			case ENOSPC:
				return error("The  device containing pathname has no room for the new directory or your disk quota is exhausted.\n");
			default:
				return error("Unknown error creating destination directory.\n");
		}
	}

	/* STEP 4 - Copy libraries to destination */
	work[0] = '\0';
	strcat(work, "cp " LIB1_32 " ");
	strcat(work, directory);
	if (system(work) != 0) {
		return error("Copy of " LIB1_32 " to destination failed.\n");
	}
	work[0] = '\0';
	strcat(work, "cp " LIB2_32 " ");
	strcat(work, directory);
	if (system(work) != 0) {
		return error("Copy of " LIB2_32 " to destination failed.\n");
	}

	printf("Installation successful.\n");

	return 0;
}

int error(char *msg) {
	printf("%s", msg);
	printf("** FS 101 ODBC Driver installation aborted! **\n");
	return 1;
}

