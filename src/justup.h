int md5file(char *filename, char *md5hash);

static int inih_handler(void* user, const char* section, const char* name, const char* value);

typedef void (*list_dir_cb)(char * path);

static void list_db();

int list_db_callback(void *NotUsed, int argc, char **argv, char **azColName);

static void list_dir(const char * dir_name, int allow_type, list_dir_cb list_dir_callback);

int path_to_commands_ftp(char path[]);
int path_to_commands_sftp(char path[]);

int send_resource_ftp(TYPE_RESOURCE resource);
int send_resource_sftp(TYPE_RESOURCE resource);

int is_resource_exists(char path[]);

int push_resource(char path[], char db_hash[], char file_hash[], char status[]);

void proceed();

void save_resource(char *path, char *hash);

int lock(int act);

void proceed_profile(char *new_profile);

void init(int argc, char **argv);

void deinit();

void prepare_resource(char * path);