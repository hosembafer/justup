#define PATH_MAX 4096 // 4k
#define CHUNK_SIZE 16384 // 16k

int COMMAND_STATUS = 0;
int COMMAND_PUSH = 0;
int COMMAND_PROFILE = 0;
int COMMAND_SAVE = 0;
int ONLY_COMMAND = 1;

int TRANSFER_PROTOCOL_FTP = 0;
int TRANSFER_PROTOCOL_SFTP = 0;


// Colors for terminal
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// FTP & SFTP
netbuf *ftp_conn = NULL;
sftp_session sftp_conn = NULL;
ssh_session my_ssh_session = NULL;


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
	char file_hash[33];
	char db_hash[33];
	char status[2];
} TYPE_RESOURCE;

struct TYPE_RESOURCE *resources;
unsigned int resources_size = 0;


// We don't need any problem at 2286 year
const unsigned int timeStampSize = 10;