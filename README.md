### DB/C DX and DB/C FS
DB/C DX is the compiler and runtime for the business-oriented programming language generally called DB/C, which is based on an old programming language created by Datapoint Corporation in the 1970s. The original name of the programming language was DATABUS, but when it was standardized in the 1990s it was called PL/B.

DB/C FS is the file server component that provides for access to DB/C DX data files from other programming languages and interfaces, for example ODBC.

Several file manipulation utilities are provided that are used in conjunction with DB/C DX and DB/C FS.

Most of the code is written in the C programming language and is compatible with the 1999 C Standard. The code uses the POSIX API for most access to operating system APIs.

The executable versions of DX, FS and the utilities, along with end user documentation and a discussion forum, are available at https://www.dbcsoftware.org. Initially, the only executables available there are for 64 bit AMD/Intel Windows and 64 bit AMD/Intel Linux. This GitHub repo is for those who need to know more about the insides of DX, FS and the utilities, or who may want to fork the code to add features, or who may wish to provide ports to other operating systems.

### Organization of this repo
The source code is found in these directories: **common**, **dx** and **fs**.

The **utils** subdirectory of the **common** directory contains the source code for 18 utilities that provide various file manipulation capabilities. The **support** subdirectory of the **common** directory contains source files of code used by DX, FS and the utilities.

The **compiler** subdirectory of the **dx** directory contains the source code for compiler part of DB/C DX that compiles source programs from text into **.dbc** bytecode files.  The **runtime** subdirectory of the **dx** directory contains the source code for the virtual machine (ie. runtime) that runs the code produced by the compiler.  The **jpeg**, **png**, **res** and **zlib** subdirectories of the **dx** directory are the source code of open source projects and miscellaneous resources used by the compiler and runtime.

The **server** subdirectory of the **fs** directory contains the source code for the server part of DB/C FS.  The **client** subdirectory  of the **fs** directory contains code for client side part of DB/C FS.  The other files in the fs directory are resources used to create the ODBC driver.

The OpenSSL libraries are required for the DB/C DX runtime code as well as the DB/C FS server and client code. Creation of SSL source code requires multiple steps as documented at https://openssl.org. So to be able to build executables, the output of the SSL configuration and compilation steps is included in the **openssl** directory.

The **libcups.a** file is the Linux static library that implements CUPS printer support.

### Build the executables

The process of building the executables consists of running the GitHub Actions defined in the **workflows** subdirectory of the **.github** directory. These **.yml** files define actions that are run using GitHub runners. The **Makefile** files are used by the actions to actually compile and build the executables. The result of running these actions are GitHub artifact files that are **.zip** files containing the executable files.
