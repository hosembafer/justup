static int inih_handler(void* user, const char* section, const char* name, const char* value);

typedef void (*list_dir_cb)(char * path);

static void list_db();

int inih_callback(void *NotUsed, int argc, char **argv, char **azColName);

static void list_dir(const char * dir_name, int allow_type, list_dir_cb list_dir_callback);

int path_to_commands(char path[]);

int send_resource(TYPE_RESOURCE resource);

int is_resource_exists(char path[]);

int push_resource(char path[], int db_time_stamp, int file_time_stamp, char status[]);

void proceed();

void save_resource(char *path, int time_stamp);

int get_resource_ts(char *path);

int lock(int act);

void proceed_profile(char *new_profile);

void init(int argc, char **argv);

void deinit();

void prepare_resource(char * path);