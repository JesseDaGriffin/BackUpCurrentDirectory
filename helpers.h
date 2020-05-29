#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct backup_file_t {
    char source[PATH_MAX];
    char destination[PATH_MAX];
    int op_flag; // 0: restore, 1: backup
};


int timestamp(char *file, char *backup) {
    struct stat sb1;
    struct stat sb2;

    pthread_mutex_lock(&mutex);
    stat(file, &sb1);
    stat(backup, &sb2);

    double dt = difftime(sb1.st_mtime, sb2.st_mtime);
    pthread_mutex_unlock(&mutex);

    if(dt < 0) {
        // file is older than the backup
        return -1;
    }
    else if(dt == 0) {
        // file and backup have the same timestamp
        return 0;
    }
    else {
        // backup is older than the file
        return 1;
    }
    // Should never get here
    return 100;
}

void *copy(void *params) {
    // flag:    1 = backing up
    //          0 = restore
    // To keep threads from overwriting this struct, a copy of the data has to be made
    struct backup_file_t copy_struct = *(struct backup_file_t*) params;
    struct backup_file_t copy_local;

    char* action_type;

    // <-- BEGIN CRITICAL SECTION -->
    pthread_mutex_lock(&mutex);
    strncpy(copy_local.source, copy_struct.source, PATH_MAX);
    strncpy(copy_local.destination, copy_struct.destination, PATH_MAX);
    copy_local.op_flag = copy_struct.op_flag;
    pthread_mutex_unlock(&mutex);
    // <-- END CRITICAL SECTION -->

    if(copy_local.op_flag == 0) action_type = "RESTORING";
    else action_type = "BACKING UP";

    // Storing the source file permissions to copy to the destination file
    struct stat source_stat;
    stat(copy_local.source, &source_stat);
    mode_t source_mode = (source_stat.st_mode & S_IRUSR)|(source_stat.st_mode & S_IWUSR)|(source_stat.st_mode & S_IXUSR)|\
                        (source_stat.st_mode & S_IRGRP)|(source_stat.st_mode & S_IWGRP)|(source_stat.st_mode & S_IXGRP)|\
                        (source_stat.st_mode & S_IROTH)|(source_stat.st_mode & S_IWOTH)|(source_stat.st_mode & S_IXOTH);

    // Buffers for basename_r to store names in a thread-safe manner
    char source_filename[PATH_MAX], destination_filename[PATH_MAX];

    // Storing the POSIX thread id conveniently
    pthread_t thread_id = pthread_self();
    
    ssize_t total_bytes_written = 0;
    char buffer[101];
    int source_fd = open(copy_local.source, O_RDONLY);
    int destination_fd = open(copy_local.destination, O_CREAT | O_WRONLY, source_mode);
    ssize_t bytesRead = 0;
    ssize_t bytesWritten = 0;
    for(;;) {
        bytesRead = read(source_fd, buffer, 100);
        bytesWritten = write(destination_fd, buffer, bytesRead);
        total_bytes_written += bytesWritten;
        if(bytesRead < 100) break;
    }
    // basename_r() is a recommended threadsafe function in MacOS
    #ifdef __APPLE__
    printf("%s (%2x): Copied %ld bytes from %s to %s\n", action_type, (uint)thread_id, total_bytes_written, \
        basename_r(copy_local.source, source_filename), \
        basename_r(copy_local.destination, destination_filename));
    #else
    printf("%s (%2x): Copied %ld bytes from %s to %s\n", action_type, (uint)thread_id, total_bytes_written, \
        basename(copy_local.source), basename(copy_local.destination));
    #endif
    close(source_fd);
    close(destination_fd);

    // <-- BEGIN CRITICAL SECTION -->
    pthread_mutex_lock(&mutex);
    memset(copy_local.source, '\0', PATH_MAX);
    memset(copy_local.destination, '\0', PATH_MAX);
    pthread_mutex_unlock(&mutex);
    // <-- END CRITICAL SECTION -->

    free(params);
    return 0;
}

int restore(char *original_path, char *backup_path) {
    // original_path -> path to restore to
    // backup_path -> backup folder from which to restore
    // Directory walk
    int cred;
    char file[PATH_MAX];
    char backup_file[PATH_MAX];
    memset(file, '\0', PATH_MAX);
    memset(backup_file, '\0', PATH_MAX);

    // For walking the original directory and the backup directory
    DIR *backup_dir;
    struct dirent *backup_entry;

    // Enter directory passed into function
    backup_dir = opendir(backup_path);

    backup_entry = readdir(backup_dir);
    while(backup_entry != NULL) {
        struct backup_file_t file_thread_struct;
        // Copy name of file or directoy to the path passed into function
        strncpy(file, original_path, strlen(original_path));
        strncat(file, "/", 1);
        strncat(file, backup_entry -> d_name, strlen(backup_entry -> d_name));

        strncpy(backup_file, backup_path, strlen(backup_path));
        strncat(backup_file, "/", 1);
        strncat(backup_file, backup_entry -> d_name, strlen(backup_entry -> d_name));

        // Check that proccess has read permission on current backup file or backup dir
        cred = access(backup_file, R_OK);

        // Check for backups if backup_entry is a regular file and not a soft link
        if(backup_entry -> d_type == DT_REG && backup_entry -> d_type != DT_LNK && cred == 0) {
            // Remove ".bak" from file pathname
            char file_no_bak[PATH_MAX];
            int sl = strlen(file)-4;
            strncpy(file_no_bak, file, sl);
            file_no_bak[sl] = '\0'; // Copy abs path minus '.bak' extension

            // File operations
        	if(access(file_no_bak, (F_OK|W_OK)) == 0) {
        		// File needs to be restored
            	if(timestamp(file_no_bak, backup_file) < 1) {
                    // Get the lock
                    pthread_mutex_lock(&mutex);
                    strncpy(file_thread_struct.source, backup_file, strlen(backup_file));
                    strncpy(file_thread_struct.destination, file_no_bak, strlen(file_no_bak));
                    file_thread_struct.op_flag = 0;
                    pthread_mutex_unlock(&mutex);

                    struct backup_file_t *file_struct_ptr = malloc(sizeof *file_struct_ptr);
                    *file_struct_ptr = file_thread_struct;
                    pthread_t restore_thread;
                    pthread_create(&restore_thread, NULL, copy, file_struct_ptr);
                    pthread_detach(restore_thread);
            	}
            	// Our file is the latest version
            	else {
            		printf("NOTE: %s is the latest version. Skipping restore operation.\n", backup_entry->d_name);
            	}
        	}
        	// Backup doesnt exist, create one
        	else {
                pthread_mutex_lock(&mutex);
                strncpy(file_thread_struct.source, backup_file, strlen(backup_file));
                strncpy(file_thread_struct.destination, file_no_bak, strlen(file_no_bak));
                file_thread_struct.op_flag = 0;
                pthread_mutex_unlock(&mutex);

                struct backup_file_t *file_struct_ptr = malloc(sizeof *file_struct_ptr);
                *file_struct_ptr = file_thread_struct;
                pthread_t restore_thread;
                pthread_create(&restore_thread, NULL, copy, file_struct_ptr);
                pthread_detach(restore_thread);
        	}

            memset(file_no_bak, '\0', PATH_MAX);
        }

        // Recursivly call search on new dir if not a softlink
        if(backup_entry -> d_type == DT_DIR &&
        backup_entry -> d_type != DT_LNK &&
        (strcmp(backup_entry -> d_name, "..") != 0) &&
        (strcmp(backup_entry -> d_name, ".") != 0) &&
        (strcmp(backup_entry -> d_name, ".backup") != 0) &&
        cred == 0) {
        	printf("WARNING: creating directory %s\n", file);
        	mkdir(file, 0700);
            restore(file, backup_file);
        }

        // Move on to next backup_entry in directory
        backup_entry = readdir(backup_dir);
        memset(file, '\0', PATH_MAX);
        memset(backup_file, '\0', PATH_MAX);
        pthread_mutex_lock(&mutex);
        memset(file_thread_struct.source, '\0', PATH_MAX);
        memset(file_thread_struct.destination, '\0', PATH_MAX);
        pthread_mutex_unlock(&mutex);
    }

    // Exit if unable to properly close directory
	if(closedir(backup_dir) != 0) {
        fprintf(stderr, "Could not close DIR:\n%s\n", strerror(errno));
        exit(1);
    }

    return 0;
}
