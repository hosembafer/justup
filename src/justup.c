#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ftplib.h>

#include "inih/ini.c"

#include <sys/stat.h>

#include <dirent.h>
#include <libgen.h>

#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fnmatch.h>

#include <sqlite3.h>

#include "vars.h"
#include "helpers.c"
#include "justup.h"

static int inih_handler(void* user, const char* section, const char* name, const char* value)
{
	if(!strcmp(name, "protocol"))
	{
		strcpy(profile_protocol, value);
	}
	else if(!strcmp(name, "host"))
	{
		strcpy(profile_host, value);
	}
	else if(!strcmp(name, "user"))
	{
		strcpy(profile_user, value);
	}
	else if(!strcmp(name, "pass"))
	{
		strcpy(profile_pass, value);
	}
	else if(!strcmp(name, "basedir"))
	{
		strcpy(profile_basedir, value);
	}
	
	return 1;
}

typedef void (*list_dir_cb)(char * path);

static void list_db()
{
	char *err_msg = 0;
	char *sql = "SELECT * FROM test ORDER BY test_path DESC";
	
	rc = sqlite3_exec(db, sql, list_db_callback, 0, &err_msg);
	
	if(rc != SQLITE_OK)
	{
		fprintf(stderr, "Failed to select data\n");
		fprintf(stderr, "SQL error: %s\n", err_msg);
		
		sqlite3_free(err_msg);
	}
}

int list_db_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	NotUsed = 0;
	
	int i;
	for(i = 0; i < argc; i++)
	{
		if(!strcmp(azColName[i], "test_path"))
		{
			// This line is responsive for ignoring files from ignore list
			if(!fnmatch_multi(ignore_list, argv[i], FNM_PATHNAME)) continue;
			
			prepare_resource(argv[i]);
		}
	}
	
	return 0;
}

static void list_dir(const char * dir_name, int allow_type, list_dir_cb list_dir_callback)
{
	DIR * d;
	
	char outputPath[PATH_MAX];
	
	/* Open the directory specified by "dir_name". */
	
	d = opendir(dir_name);
	
	/* Check it was opened. */
	if(!d)
	{
		fprintf(stderr, "Cannot open directory '%s': %s\n", dir_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	while(1)
	{
		struct dirent * entry;
		const char * d_name;
		
		// "Readdir" gets subsequent entries from "d".
		entry = readdir(d);
		if(!entry)
		{
			// There are no more entries in this directory, so break out of the while loop.
			break;
		}
		d_name = entry->d_name;
		
		if(!strcmp(d_name, ".") || !strcmp(d_name, "..")) continue; // We don't need to print some useless shit, one line magic
		
		// Save the name of the file and directory.
		sprintf(outputPath, "%s/%s", dir_name, d_name);
		
		// We cleaning the path's first two symbols for nice view
		int i;
		for(i = 0; i < strlen(outputPath); i++)
			outputPath[i] = outputPath[i + 2];
		
		if(!fnmatch_multi(ignore_list, outputPath, FNM_PATHNAME)) continue; // Budam Budum Bam Baduhbam
		
		if(allow_type == 0)
		{
			list_dir_callback(outputPath);
		}
		else if(allow_type == 1 && !(entry->d_type & DT_DIR))
		{
			list_dir_callback(outputPath);
		}
		else if(allow_type == 2 && (entry->d_type & DT_DIR))
		{
			list_dir_callback(outputPath);
		}
		
		if(entry->d_type & DT_DIR) // Check that the directory is not "d" or d's parent.
		{
			if(strcmp (d_name, "..") != 0 && strcmp (d_name, ".") != 0)
			{
				int path_length;
				char path[PATH_MAX];
				
				path_length = snprintf(path, PATH_MAX, "%s/%s", dir_name, d_name);
				if(path_length >= PATH_MAX)
				{
					fprintf(stderr, "Path length is too long.\n");
					exit(EXIT_FAILURE);
				}
				
				// Recursively call "list_dir" with the new path.
				list_dir(path, allow_type, list_dir_callback);
			}
		}
	}
	
	if(closedir(d))
	{
		fprintf(stderr, "Can't close '%s': %s, Please check your OS, this happens when OS can't work correctly with FS\n", dir_name, strerror (errno));
		exit(EXIT_FAILURE);
	}
}

int path_to_commands(char path[])
{
	int total_size = 0;
	char *s = strdup(path), *p;
	for(p = strtok(s, "/"); p != NULL; p = strtok(NULL, "/"))
	{
		total_size += strlen(p) + 1;
		
		if((total_size - 1) != strlen(path))
		{
			if(!FtpChdir(p, ftp_conn))
			{
				if(!FtpMkdir(p, ftp_conn))
				{
					return 0;
				}
				
				if(!FtpChdir(p, ftp_conn))
				{
					return 0;
				}
			}
		}
	}
	
	return 1;
}

int send_resource(TYPE_RESOURCE resource)
{
	char path_tok[strlen(resource.path)];
	strcpy(path_tok, resource.path);
	
	if(!strcmp(resource.status, "A") || !strcmp(resource.status, "M"))
	{
		if(!path_to_commands(resource.path))
		{
			printf("FTP unable to create a folder in remote server\n");
		}
		if(!FtpPut(resource.path, basename(path_tok), FTPLIB_IMAGE, ftp_conn))
		{
			printf("FTP unable to modify file in remote server\n");
			if(!answer_yn("Shik this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		if(!FtpChdir(profile_basedir, ftp_conn))
		{
			printf("FTP is unable to find base directory or unable to change directory in remote server\n");
			if(!answer_yn("Shik this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		
		return 1;
	}
	else if(!strcmp(resource.status, "R"))
	{
		if(!FtpChdir(profile_basedir, ftp_conn))
		{
			printf("FTP is unable to find base directory or unable to change directory in remote server\n");
			if(!answer_yn("Shik this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		if(!FtpDelete(path_tok, ftp_conn))
		{
			printf("FTP unable to delete a file <%s> in remote server\n", resource.path);
			if(!answer_yn("Shik this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		
		return 1;
	}
	
	return 0;
}

int is_resource_exists(char path[])
{
	int i = 0;
	
	for(; i < resources_size; i++)
	{
		if(!strcmp(resources[i].path, path))
		{
			return 1;
			break;
		}
	}
	
	return 0;
}

int push_resource(char path[], int db_time_stamp, int file_time_stamp, char status[])
{
	if(!is_resource_exists(path))
	{
		resources_size++;
		resources = realloc(resources, sizeof(TYPE_RESOURCE) * resources_size);
		
		strcpy(resources[resources_size - 1].path, path);
		resources[resources_size - 1].db_time_stamp = db_time_stamp;
		resources[resources_size - 1].file_time_stamp = file_time_stamp;
		strcpy(resources[resources_size - 1].status, status);
		
		return resources_size - 1;
	}
	
	return -1;
}


void proceed()
{
	if(COMMAND_PUSH)
	{
		FtpInit();
		if(!FtpConnect(profile_host, &ftp_conn) || !FtpLogin(profile_user, profile_pass, ftp_conn))
		{
			printf("FTP Client can't establishe connection or after connecting can't authorize FTP Server\n");
			exit(EXIT_FAILURE);
		}
		
		FtpChdir(profile_basedir, ftp_conn);
	}
	
	int i;
	for(i = 0; i < resources_size; i++)
	{
		if(COMMAND_PUSH)
		{
			//printf("%s-%i-%i-%s\n", resources[i].path, resources[i].file_time_stamp, resources[i].db_time_stamp, resources[i].status);
			if(send_resource(resources[i]))
			{
				save_resource(resources[i].path, resources[i].file_time_stamp);
				printf("%s|OK \t %s\n", resources[i].status, resources[i].path);
			}
			else
			{
				printf("%s|FAILURE \t %s\n", resources[i].status, resources[i].path);
				exit(EXIT_FAILURE);
			}
		}
		else if(COMMAND_SAVE)
		{
			save_resource(resources[i].path, resources[i].file_time_stamp);
			printf("%s|SAVED \t %s\n", resources[i].status, resources[i].path);
		}
		else if(COMMAND_STATUS)
		{
			printf("%s \t %s\n", resources[i].status, resources[i].path);
		}
	}
	
	if(COMMAND_PUSH && ftp_conn)
	{
		FtpClose(ftp_conn);
	}
}

void save_resource(char *path, int time_stamp)
{
	char *err_msg = 0;
	if(time_stamp == -1)
	{
		char *sqlDelete = "DELETE FROM test WHERE test_path=?";
		
		rc = sqlite3_prepare_v2(db, sqlDelete, strlen(sqlDelete), &res, 0);
		
		if(rc == SQLITE_OK)
		{
			sqlite3_bind_text(res, 1, path, strlen(path), SQLITE_STATIC);
		}
		else
		{
			fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
		}
		
		int step = sqlite3_step(res);
		if(step == SQLITE_DONE)
		{
			// REMOVING
		}
		
		return;
	}
	
	char *sqlInsert = "INSERT INTO test (test_timestamp, test_path) VALUES (?, ?)";
	
	rc = sqlite3_prepare_v2(db, sqlInsert, strlen(sqlInsert), &res, 0);
	
	if(rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
	}
	
	if(rc == SQLITE_OK)
	{
		sqlite3_bind_int(res, 1, time_stamp);
		sqlite3_bind_text(res, 2, path, strlen(path), SQLITE_STATIC);
	}
	else
	{
		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	
	int step = sqlite3_step(res);
	if(step == SQLITE_DONE)
	{
		// ADDING
	}
	else
	{
		char *sqlUpdate = "UPDATE test SET test_timestamp=? WHERE test_path=?";
		rc = sqlite3_prepare_v2(db, sqlUpdate, strlen(sqlUpdate), &res, 0);
		
		if(rc == SQLITE_OK)
		{
			sqlite3_bind_int(res, 1, time_stamp);
			sqlite3_bind_text(res, 2, path, strlen(path), SQLITE_STATIC);
		}
		else
		{
			fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
		}
		
		step = sqlite3_step(res);
		if(step == SQLITE_DONE)
		{
			// UPDATING
		}
	}
	
	sqlite3_finalize(res);
}

int get_resource_ts(char *path)
{
	int savedTimeStamp = -1;
	char *err_msg = 0;
	char *sql = "SELECT * FROM test WHERE test_path=?";
	
	rc = sqlite3_prepare_v2(db, sql, strlen(sql), &res, 0);
	
	if(rc == SQLITE_OK)
	{
		sqlite3_bind_text(res, 1, path, strlen(path), SQLITE_STATIC);
	}
	else
	{
		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	
	int step = sqlite3_step(res);
	if(step == SQLITE_ROW)
	{
		savedTimeStamp = sqlite3_column_int(res, 2);
	}
	
	sqlite3_finalize(res);
	
	if(rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", err_msg);
		
		sqlite3_free(err_msg);
	}
	
	return savedTimeStamp;
}

void proceed_profile(char *new_profile)
{
	FILE *fp = fopen(".justup/profile.current", "w+");
	fprintf(fp, "%s", new_profile);
	fclose(fp);
	
	if(strcmp(new_profile, profile))
	{
		printf("Profile changed to: %s\n", new_profile);
	}
	
	
	char profile_path[117] = { 0 };
	sprintf(profile_path, ".justup/profile.%s", new_profile);
	
	if(!fs_entity_exists(profile_path) || !fsize(profile_path))
	{
		printf("You need to create and set up <%s> file like profile.master\n", profile_path);
		
		const char *profileExample = "protocol = ftp\n"
		"host = localhost\n"
		"user = user\n"
		"pass = root123\n"
		"basedir = /var/www/site/";
		printf("%s\n", profileExample);
		FILE *cufp = fopen(profile_path, "w+");
		fprintf(cufp, "%s", profileExample);
		fclose(cufp);
		
		exit(EXIT_FAILURE);
	}
}

/*
 * prepare_resource() is a callback function, that works when
 * our list_dir() or list_db() functions iterating/reading new resource.
 */
void prepare_resource(char *path)
{ 
	int file_time_stamp = file_modify_date(path);
	int db_time_stamp = get_resource_ts(path);
	
	char entity_status[2];
	
	// printf("%s-%i-%i\n", path, file_time_stamp, db_time_stamp);
	if(file_time_stamp == -1 || db_time_stamp == -1)
	{
		if(file_time_stamp > db_time_stamp)
		{
			strcpy(entity_status, "A");
		}
		else
		{
			strcpy(entity_status, "R");
		}
	}
	else
	{
		strcpy(entity_status, "M");
	}
	
	if(file_time_stamp != db_time_stamp)
	{
		push_resource(path, db_time_stamp, file_time_stamp, entity_status);
	}
}

/*
 * If you know, what I mean.
 */
void init(int argc, char **argv)
{
	if(lock(0))
	{
		printf("Program is locked, maybe runned by another instance\n");
		exit(EXIT_FAILURE);
	}
	
	lock(1);
	
	mkdir(".justup", 0777);
	
	if(!fs_entity_exists(".justup/profile.current"))
	{
		FILE *pfp = fopen(".justup/profile.current", "w+");
		fprintf(pfp, "%s", "master");
		fclose(pfp);
	}
	
	int profile_csize = fsize(".justup/profile.current");
	/*profile = (char *)malloc(sizeof(char) * profile_csize);
	memset(profile, 0, profile_csize);*/
	
	int ipfp = open(".justup/profile.current", O_RDONLY, 0777);
	read(ipfp, profile, profile_csize);
	close(ipfp);
	
	
	char profile_path[117] = { 0 };
	sprintf(profile_path, ".justup/profile.%s", profile);
	
	
	proceed_profile(profile); // We need to check profile config file whether it filled correctly.
	
	if(ini_parse(profile_path, inih_handler, NULL) < 0)
	{
		printf("Can't load <%s>\n", profile_path);
		exit(EXIT_FAILURE);
	}
	
	
	int dbPathSize = 22 + strlen(profile);
	char dbPath[dbPathSize];
	sprintf(dbPath, ".justup/resources.%s.db", profile);
	rc = sqlite3_open(dbPath, &db);
	if(rc)
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
	
	char *err_msg = 0;
	char *sql = "CREATE TABLE test(test_id INTEGER PRIMARY KEY, test_path TEXT UNIQUE, test_timestamp INT);";
	sqlite3_exec(db, sql, 0, 0, &err_msg);
	
	
	if(argc >= 2 && !strcmp(argv[1], "status"))
	{
		COMMAND_STATUS = 1;
	}
	else if(argc >= 2 && !strcmp(argv[1], "push"))
	{
		COMMAND_PUSH = 1;
	}
	else if(argc >= 3 && !strcmp(argv[1], "profile"))
	{
		COMMAND_PROFILE = 1;
	}
	else if(argc >= 2 && !strcmp(argv[1], "save"))
	{
		COMMAND_SAVE = 1;
	}
	
	if(argc >= 3 && (COMMAND_SAVE || COMMAND_PUSH))
	{
		int ai = 2;
		for(; ai < argc; ai++)
		{
			if(fs_entity_exists(argv[ai]) || get_resource_ts(argv[ai]) != -1)
			{
				ONLY_COMMAND = 0;
			}
			else
			{
				printf("Resource <%s> not found.\n", argv[ai]);
				exit(EXIT_FAILURE);
			}
		}
	}
	
	
	
	fp_cvsignore_size = fsize(fp_cvsignore);
	
	if(fp_cvsignore_size == 0) fp_cvsignore_size++;
	
	ignore_list = (char *) malloc(sizeof(char) * (fp_cvsignore_size + strlen(static_ignore_list)));
	memset(ignore_list, 0, fp_cvsignore_size + strlen(static_ignore_list));
	
	int fp = open(fp_cvsignore, O_RDONLY, 0777);
	read(fp, ignore_list, fp_cvsignore_size);
	close(fp);
	
	strcat(ignore_list, static_ignore_list);
	
	
	resources = malloc(sizeof(TYPE_RESOURCE) * 1);
	memset(resources, 0, resources_size);
	
	printf("Current profile: %s\n", profile);
}

void deinit()
{
	sqlite3_close(db);
	free(ignore_list);
	free(resources);
	
	lock(2);
}

int main(int argc, char **argv)
{
	init(argc, argv);
	
	if(COMMAND_PROFILE)
	{
		proceed_profile(argv[2]);
	}
	else if(COMMAND_STATUS || COMMAND_PUSH || COMMAND_SAVE)
	{
		if(ONLY_COMMAND)
		{
			list_dir(".", 1, prepare_resource);
			list_db();
		}
		else
		{
			int ai;
			for(ai = 2; ai < argc; ai++)
			{
				if(fs_entity_exists(argv[ai]) || get_resource_ts(argv[ai]) != -1)
				{
					prepare_resource(argv[ai]);
				}
			}
		}
		
		proceed();
	}
	else
	{
		usage();
	}
	
	
	deinit();
	return EXIT_SUCCESS;
}