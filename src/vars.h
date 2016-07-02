#define PATH_MAX 4096

int COMMAND_STATUS = 0;
int COMMAND_PUSH = 0;
int COMMAND_PROFILE = 0;
int COMMAND_SAVE = 0;
int ONLY_COMMAND = 1;


// FTP
netbuf *ftp_conn = NULL;


// SQLite special variables
sqlite3 *db;
char *zErrMsg = 0;
int  rc;
char *sql;
sqlite3_stmt *res;


// Ignore list variables
char *ignore_list;
char *static_ignore_list = "\n.justup/*\n.justup";

const char fp_cvsignore[] = ".cvsignore";
int fp_cvsignore_size;


// We reading from profile.<profile> file all configurations and put there
char profile[20];
char profile_protocol[7];
char profile_host[80];
char profile_user[40];
char profile_pass[80];
char profile_basedir[80];


// Variables for resources
typedef struct TYPE_RESOURCE {
	char path[PATH_MAX];
	int file_time_stamp;
	int db_time_stamp;
	char status[1];
} TYPE_RESOURCE;

struct TYPE_RESOURCE *resources;
unsigned int resources_size = 0;


// We don't need any problem at 2286 year
const unsigned int timeStampSize = 10;