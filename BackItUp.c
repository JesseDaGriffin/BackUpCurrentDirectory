#include "helpers.h"

int backitup(char *original_path, char *back_up_path) {
    int cred;
    char file[PATH_MAX];
    char backup_file[PATH_MAX];
    memset(file, '\0', PATH_MAX);
    memset(backup_file, '\0', PATH_MAX);

    DIR *dir;
    struct dirent *entry;

    // Enter directory passed into function
    dir = opendir(original_path);

    entry = readdir(dir);
    while(entry != NULL) {
    	struct backup_file_t file_thread_struct;
        // Copy name of file or directoy to the path passed into function
        strncpy(file, original_path, strlen(original_path));
        strncat(file, "/", 1);
        strncat(file, entry -> d_name, strlen(entry -> d_name));

        strncpy(backup_file, back_up_path, strlen(back_up_path));
        strncat(backup_file, "/", 1);
        strncat(backup_file, entry -> d_name, strlen(entry -> d_name));

        // Check that proccess has read permission on current file or dir
        cred = access(file, R_OK);

        // Check for backups if entry is a regular file and not a soft link
        if(entry -> d_type == DT_REG && entry -> d_type != DT_LNK && cred == 0) {
        	// Backup buffer
        	char backup_buf[PATH_MAX];
        	strncpy(backup_buf, backup_file, strlen(backup_file));
        	strncat(backup_buf, ".bak", 4);

        	if(access(backup_buf, F_OK) == 0) {
        		// Backup needs to be updated
            	if(timestamp(file, backup_buf) == 1) {
                    // Any strncpy requires atomicity
                    pthread_mutex_lock(&mutex);
            		strncpy(file_thread_struct.source, file, strlen(file));
            		strncpy(file_thread_struct.destination, backup_buf, strlen(backup_buf));
            		file_thread_struct.op_flag = 1;
            		pthread_mutex_unlock(&mutex);

            		struct backup_file_t *file_struct_ptr = malloc(sizeof *file_struct_ptr);
            		*file_struct_ptr = file_thread_struct;
            		pthread_t copy_thread;
            		pthread_create(&copy_thread, NULL, copy, file_struct_ptr);
            		pthread_detach(copy_thread);
            	}
            	// Backup is up to date
            	else {
            		printf("The backup for %s is up to date!\n", basename(file));
            	}
        	}
        	// Backup doesnt exist, create one
        	else {
                pthread_mutex_lock(&mutex);
        		strncpy(file_thread_struct.source, file, strlen(file));
        		strncpy(file_thread_struct.destination, backup_buf, strlen(backup_buf));
        		file_thread_struct.op_flag = 1;
        		pthread_mutex_unlock(&mutex);

        		struct backup_file_t *file_struct_ptr = malloc(sizeof *file_struct_ptr);
            	*file_struct_ptr = file_thread_struct;
        		pthread_t copy_thread;
        		pthread_create(&copy_thread, NULL, copy, file_struct_ptr);
        		pthread_detach(copy_thread);
        	}
        	memset(backup_buf, '\0', PATH_MAX);
        }

        // Recursivly call search on new dir if not a softlink
        if(entry -> d_type == DT_DIR &&
        entry -> d_type != DT_LNK &&
        (strcmp(entry -> d_name, "..") != 0) &&
        (strcmp(entry -> d_name, ".") != 0) &&
        (strcmp(entry -> d_name, ".backup") != 0) &&
        cred == 0) {
        	mkdir(backup_file, 0700);
            backitup(file, backup_file);
        }

        // Move on to next entry in directory
        entry = readdir(dir);
        memset(file, '\0', PATH_MAX);
        memset(backup_file, '\0', PATH_MAX);
        pthread_mutex_lock(&mutex);
        memset(file_thread_struct.source, '\0', PATH_MAX);
        memset(file_thread_struct.destination, '\0', PATH_MAX);
        pthread_mutex_unlock(&mutex);
    }

    // Exit if unable to properly close directory
	if(closedir(dir) != 0) {
        fprintf(stderr, " Could not close DIR:\n%s\n", strerror(errno));
        exit(1);
    }
    return 0;
}

int main(int argc, char **argv) {
        char original_path[PATH_MAX];
        char back_up_path[PATH_MAX];

        // Restore option
        if(argc == 2 && strcmp(argv[1], "-r") == 0) {
                if(access(".backup", F_OK) != 0) {
                        printf("Backup directory does not exist.\n");
                        exit(EXIT_FAILURE);
                }
                else {
                        getcwd(original_path, sizeof(original_path));
                        getcwd(back_up_path, sizeof(original_path));
                        strncat(back_up_path, "/.backup", 8);

                        restore(original_path, back_up_path);
                }
        }
        // Backup option
        else if(argc == 1) {
                getcwd(original_path, sizeof(original_path));
                getcwd(back_up_path, sizeof(original_path));
                strncat(back_up_path, "/.backup", 8);

                if(access(".backup", F_OK) != 0)
                        mkdir(".backup", 0700);

                backitup(original_path, back_up_path);
        }

        // This allows threads to continue to use stdout before the main calling thread exits
        sleep(5);
}
