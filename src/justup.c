#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <ftplib.h>

#define LIBSSH_STATIC 1
#include <libssh/libssh.h>
#include <libssh/sftp.h>

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
#include <openssl/md5.h>

#include <sqlite3.h>

#include "vars.h"
#include "helpers.c"
#include "justup.h"

int md5file(char *filename, char *md5hash)
{
	unsigned char c[MD5_DIGEST_LENGTH];
	char temp_sybmols[3];
	FILE *inFile = fopen(filename, "rb");
	int i;
	MD5_CTX mdContext;
	int bytes;
	unsigned char data[1024];
	
	if(inFile == NULL)
	{
		memset(md5hash, 0, 32);
		return 0;
	}
	
	MD5_Init(&mdContext);
	while((bytes = fread(data, 1, 1024, inFile)) != 0)
	{
		MD5_Update(&mdContext, data, bytes);
	}
	MD5_Final(c, &mdContext);
	fclose(inFile);
	
	for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
		sprintf(temp_sybmols, "%02x", c[i]);
		
		md5hash[i*2] = temp_sybmols[0];
		md5hash[i*2+1] = temp_sybmols[1];
	}
	
	return 1;
}

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

int path_to_commands_ftp(char path[])
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

int path_to_commands_sftp(char *path)
{
	int i, j, x;
	
	int tokenCount = 1;
	char *pch = strchr(path, '/');
	while(pch != NULL)
	{
		tokenCount++;
		pch = strchr(pch + 1, '/');
	}
	
	char tokens[tokenCount][PATH_MAX];
	
	/* SPLIT - start */
	int tokenPos = -1;
	char lastPath[PATH_MAX] = {0};
	i = 0;
	while(path[i] != '\0')
	{
		if(path[i] == '/')
		{
			tokenPos++;
			strcpy(tokens[tokenPos], lastPath);
			memset(lastPath, 0, strlen(lastPath));
		}
		
		if(path[i] != '/')
		{
			lastPath[strlen(lastPath)] = path[i];
			lastPath[strlen(lastPath) + 1] = '\0';
		}
		
		i++;
	}
	if(strlen(lastPath) > 0)
	{
		tokenPos++;
		strcpy(tokens[tokenPos], lastPath);
		memset(lastPath, 0, strlen(lastPath));
	}
	/* SPLIT - end */
	
	char finalPath[PATH_MAX];
	char finalLocalPath[PATH_MAX];
	
	memset(finalPath, 0, strlen(finalPath));
	memset(finalLocalPath, 0, strlen(finalLocalPath));
	
	for(i = 0; i < tokenCount; i++)
	{
		sprintf(finalPath, "%s%s", profile_basedir, tokens[0]);
		sprintf(finalLocalPath, "%s", tokens[0]);
		
		for(j = 1; j <= i; j++)
		{
			strcat(finalPath, "/");
			strcat(finalLocalPath, "/");
			
			strcat(finalPath, tokens[j]);
			strcat(finalLocalPath, tokens[j]);
		}
		
		if(i != tokenCount - 1)
		{
			sftp_mkdir(sftp_conn, finalPath, fmode(finalLocalPath));
			
			return 1;
		}
		else
		{
			sftp_file remoteFile = sftp_open(sftp_conn, finalPath, O_CREAT | O_RDWR | O_TRUNC, fmode(finalLocalPath));
			if(remoteFile != NULL)
			{
				FILE *localFile = fopen(finalLocalPath, "rb");
				int fileLength;
				fileLength = fsize(finalLocalPath);
				
				if(localFile != NULL)
				{
					int bytes = 0;
					int bytesRead = 0;
					char chunk[CHUNK_SIZE] = {0};
					
					while((bytes = fread(chunk, 1, CHUNK_SIZE, localFile)) > 0)
					{
						fseek(localFile, bytes + bytesRead, SEEK_SET);
						bytesRead += bytes;
						
						sftp_write(remoteFile, chunk, bytes);
					}
					
					fclose(localFile);
				}
			}
			
			return 0;
		}
		
		memset(finalPath, 0, strlen(finalPath));
		memset(finalLocalPath, 0, strlen(finalLocalPath));
	}
	
	return 0;
}

int send_resource_ftp(TYPE_RESOURCE resource)
{
	char path_tok[strlen(resource.path)];
	strcpy(path_tok, resource.path);
	
	if(!strcmp(resource.status, "A") || !strcmp(resource.status, "M"))
	{
		if(!path_to_commands_ftp(resource.path))
		{
			printf("FTP unable to create a folder in remote server\n");
		}
		if(!FtpPut(resource.path, basename(path_tok), FTPLIB_IMAGE, ftp_conn))
		{
			printf("FTP unable to modify file in remote server\n");
			if(!answer_yn("Skip this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		if(!FtpChdir(profile_basedir, ftp_conn))
		{
			printf("FTP is unable to find base directory or unable to change directory in remote server\n");
			if(!answer_yn("Skip this operation ?"))
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
			if(!answer_yn("Skip this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		if(!FtpDelete(path_tok, ftp_conn))
		{
			printf("FTP unable to delete a file <%s> in remote server\n", resource.path);
			if(!answer_yn("Skip this operation ?"))
			{
				exit(EXIT_FAILURE);
			}
		}
		
		return 1;
	}
	
	return 0;
}

int send_resource_sftp(TYPE_RESOURCE resource)
{
	char path_tok[strlen(resource.path)];
	strcpy(path_tok, resource.path);
	
	/*printf("%i\n", S_IRUSR);
	printf("%i\n", S_IRWXG);
	printf("%i\n", S_IRGRP);
	printf("%i\n", S_IWGRP);
	printf("%i\n", S_IXGRP);
	printf("MODE: %i\n", fmode("eee"));*/
	
	if(!strcmp(resource.status, "A") || !strcmp(resource.status, "M"))
	{
		path_to_commands_sftp(resource.path);
		
		return 1;
	}
	else if(!strcmp(resource.status, "R"))
	{
		char fullPath[PATH_MAX] = {0};
		sprintf(fullPath, "%s%s", profile_basedir, resource.path);
		sftp_unlink(sftp_conn, fullPath);
		
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

int push_resource(char path[], char db_hash[], char file_hash[], char status[])
{
	if(!is_resource_exists(path))
	{
		resources_size++;
		resources = realloc(resources, sizeof(TYPE_RESOURCE) * resources_size);
		
		strcpy(resources[resources_size - 1].path, path);
		strcpy(resources[resources_size - 1].db_hash, db_hash);
		strcpy(resources[resources_size - 1].file_hash, file_hash);
		strcpy(resources[resources_size - 1].status, status);
		
		return resources_size - 1;
	}
	
	return -1;
}


void proceed()
{
	if(COMMAND_PUSH)
	{
		if(TRANSFER_PROTOCOL_FTP)
		{
			FtpInit();
			if(!FtpConnect(profile_host, &ftp_conn) || !FtpLogin(profile_user, profile_pass, ftp_conn))
			{
				printf("FTP Client can't establishe connection or after connecting can't authorize FTP Server\n");
				exit(EXIT_FAILURE);
			}
			
			FtpChdir(profile_basedir, ftp_conn);
		}
		else if(TRANSFER_PROTOCOL_SFTP)
		{
			my_ssh_session = ssh_new();
			ssh_options_set(my_ssh_session, SSH_OPTIONS_HOST, profile_host);
			
			ssh_connect(my_ssh_session);
			ssh_userauth_password(my_ssh_session, profile_user, profile_pass);
			
			sftp_conn = sftp_new(my_ssh_session);
			sftp_init(sftp_conn);
		}
	}
	
	int i;
	for(i = 0; i < resources_size; i++)
	{
		char status_color[10] = {0};
		if(resources[i].status[0] == 'M')
		{
			strcpy(status_color, ANSI_COLOR_CYAN);
		}
		else if(resources[i].status[0] == 'R')
		{
			strcpy(status_color, ANSI_COLOR_RED);
		}
		else if(resources[i].status[0] == 'A')
		{
			strcpy(status_color, ANSI_COLOR_GREEN);
		}
		
		if(COMMAND_PUSH)
		{
			//printf("%s-%s-%s-%s\n", resources[i].path, resources[i].file_hash, resources[i].db_hash, resources[i].status);
			if(TRANSFER_PROTOCOL_FTP)
			{
				if(send_resource_ftp(resources[i]))
				{
					save_resource(resources[i].path, resources[i].file_hash);
					printf("%s%s|OK \t %s\n" ANSI_COLOR_RESET, status_color, resources[i].status, resources[i].path);
				}
				else
				{
					printf("%s%s|FAILURE \t %s\n" ANSI_COLOR_RESET, status_color, resources[i].status, resources[i].path);
					exit(EXIT_FAILURE);
				}
			}
			else if(TRANSFER_PROTOCOL_SFTP)
			{
				if(send_resource_sftp(resources[i]))
				{
					save_resource(resources[i].path, resources[i].file_hash);
					printf("%s%s|OK \t %s\n" ANSI_COLOR_RESET, status_color, resources[i].status, resources[i].path);
				}
				else
				{
					printf("%s%s|FAILURE \t %s\n" ANSI_COLOR_RESET, status_color, resources[i].status, resources[i].path);
					exit(EXIT_FAILURE);
				}
			}
		}
		else if(COMMAND_SAVE)
		{
			save_resource(resources[i].path, resources[i].file_hash);
			printf("%s%s|SAVED \t %s\n" ANSI_COLOR_RESET, status_color, resources[i].status, resources[i].path);
		}
		else if(COMMAND_STATUS)
		{
			printf("%s%s \t %s\n" ANSI_COLOR_RESET, status_color, resources[i].status, resources[i].path);
		}
	}
	
	if(COMMAND_PUSH)
	{
		if(TRANSFER_PROTOCOL_FTP && ftp_conn)
		{
			FtpClose(ftp_conn);
		}
		else if(TRANSFER_PROTOCOL_SFTP && sftp_conn)
		{
			//ssh_disconnect(my_ssh_session);
			sftp_free(sftp_conn);
			ssh_free(my_ssh_session);
		}
	}
}

void save_resource(char *path, char *hash)
{
	char *err_msg = 0;
	if(!strlen(hash))
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
			// printf("REMOVING\n");
		}
		
		return;
	}
	
	char *sqlInsert = "INSERT INTO test (test_hash, test_path) VALUES (?, ?)";
	
	rc = sqlite3_prepare_v2(db, sqlInsert, strlen(sqlInsert), &res, 0);
	
	if(rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
	}
	
	if(rc == SQLITE_OK)
	{
		sqlite3_bind_text(res, 1, hash, strlen(hash), SQLITE_STATIC);
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
		// printf("ADDING\n");
	}
	else
	{
		char *sqlUpdate = "UPDATE test SET test_hash=? WHERE test_path=?";
		rc = sqlite3_prepare_v2(db, sqlUpdate, strlen(sqlUpdate), &res, 0);
		
		if(rc == SQLITE_OK)
		{
			sqlite3_bind_text(res, 1, hash, strlen(hash), SQLITE_STATIC);
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
			// printf("UPDATING\n");
		}
	}
	
	sqlite3_finalize(res);
}

int get_resource_hash(char *path, char *hash)
{
	char *savedHash;
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
		savedHash = (char*)sqlite3_column_text(res, 2);
		strcpy(hash, savedHash);
	}
	
	sqlite3_finalize(res);
	
	if(rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", err_msg);
		
		sqlite3_free(err_msg);
	}
	
	return 1;
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
	char file_hash[33] = {0};
	char db_hash[33] = {0};
	char entity_status[2];
	
	md5file(path, file_hash);
	get_resource_hash(path, db_hash);
	
	//printf("%s-%s-%s\n", file_hash, db_hash, path);
	if(!strlen(file_hash))
	{
		strcpy(entity_status, "R");
	}
	else if(!strlen(db_hash))
	{
		strcpy(entity_status, "A");
	}
	else if(strcmp(file_hash, db_hash))
	{
		strcpy(entity_status, "M");
	}
	
	if(strcmp(file_hash, db_hash))
	{
		push_resource(path, db_hash, file_hash, entity_status);
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
	char *sql = "CREATE TABLE test(test_id INTEGER PRIMARY KEY, test_path TEXT UNIQUE, test_hash TEXT);";
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
			char resource_hash[32] = {0};
			get_resource_hash(argv[ai], resource_hash);
			
			if(fs_entity_exists(argv[ai]) || strlen(resource_hash) > 0)
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
	
	if(!strcmp(profile_protocol, "ftp"))
	{
		TRANSFER_PROTOCOL_FTP = 1;
	}
	else if(!strcmp(profile_protocol, "sftp"))
	{
		TRANSFER_PROTOCOL_SFTP = 1;
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
	printf("Transfer protocol: %s\n", profile_protocol);
	printf("---------------------------\n");
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
			char resource_hash[32] = {0};
			int ai;
			for(ai = 2; ai < argc; ai++)
			{
				get_resource_hash(argv[ai], resource_hash);
				if(fs_entity_exists(argv[ai]) || strlen(resource_hash) > 0)
				{
					prepare_resource(argv[ai]);
					memset(resource_hash, 0, strlen(resource_hash));
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
