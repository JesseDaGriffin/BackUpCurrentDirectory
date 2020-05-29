Authors:	Pierson Carulli
			Jesse Griffin
			Patrick Kent
Date:		3 May 2020
Course:		CS460 Operating Systems
Summary:	Project 4 - BackItUp!
			BackItUp! backs up all accessible files and directories in the current directory under a directory called
            '.backup'. BackItUp! will also restore from the '.backup' directory when run with the '-r' flag. This program
            uses threads when backing up and restoring individual files. BackItUp! only copies files if the backup is older,
            and restores only if the original is older.

COMMANDS

	"make" or "make all"
	This command will compile and link the BackItUp.c and BackItUp.o files into an executable called "backitup".

	"make run"
	This command will run the executable "./backitup", which immediately backs up the current working directory.

	"make clean"
	This command removes all object files, executables, and debug files created with any other Makefile commands.

	"make debug"
	This command creates a debug executable with symbols to be used in gdb or lldb.

	"./backitup <-r>"
	This command can be used to restore files to the current directory using files in the '.backup' directory.
