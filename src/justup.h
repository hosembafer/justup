void usage();


static int inih_handler(void* user, const char* section, const char* name, const char* value);

int file_modify_date(char *path);

typedef void (*list_dir_cb)(char * path);

static void list_db();

int callback(void *NotUsed, int argc, char **argv, char **azColName);

static void list_dir(const char * dir_name, int allow_type, list_dir_cb list_dir_callback);

off_t fsize(const char *filename);

int path_to_commands(char path[]);

int send_resource(TYPE_RESOURCE resource);

int is_resource_exists(char path[]);

void push_resource(char path[], int db_time_stamp, int file_time_stamp, char status[]);

void print_resources();

void set_resource_ts(char *path, int time_stamp);

int get_resource_ts(char *path);

int fnmatch_multi(char *patterns /* Separated by new lines(\n) */, const char *string, int flags);

int lock(int act);

void proceed_profile(char *new_profile);

void init(int argc, char **argv);

void deinit();

void prepare_resource(char * path);

int fs_entity_exists(const char *filename);

int callback(void *NotUsed, int argc, char **argv, char **azColName);