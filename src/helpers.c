char *strrev(char *str)
{
	int i = strlen(str) - 1, j = 0;
	
	char ch;
	while(i > j)
	{
		ch = str[i];
		str[i] = str[j];
		str[j] = ch;
		
		i--;
		j++;
	}
	
	return str;
}

int chrpos(const char *haystack, char needle)
{
	char *p = strchr(haystack, needle);
	if(p)
	{
		return p - haystack;
	}
	return -1;
}

int strpos(const char *haystack, char *needle)
{
	char *p = strstr(haystack, needle);
	if(p)
	{
		return p - haystack;
	}
	return -1;
}

int file_modify_date(char *path)
{
	struct stat attr;
	char time_stamp[timeStampSize];
	
	if(stat(path, &attr) != -1)
	{
		char buffer[timeStampSize];
		
		struct tm *tm;
		tm = localtime(&attr.st_ctime);
		
		strftime(buffer, timeStampSize + 1, "%s", tm);
		
		strcpy(time_stamp, buffer);
		int resTs = atoi(time_stamp);
		
		return resTs;
	}
	
	return -1;
}

int fs_entity_exists(const char *filename)
{
	struct stat buffer;
	return (stat (filename, &buffer) == 0);
}

off_t fsize(const char *filename)
{
	struct stat st; 
	
	if(stat(filename, &st) == 0)
		return st.st_size;
	
	return -1; 
}

mode_t fmode(const char *filename)
{
	struct stat st;
	
	if(stat(filename, &st) == 0)
		return st.st_mode;
	
	return 0; 
}

int fnmatch_multi(char *patterns /* Separated by new lines(\n) */, const char *string, int flags)
{
	char *ignorable, patterns_tok[strlen(patterns)];
	strcpy(patterns_tok, patterns);
	
	ignorable = strtok(patterns_tok, "\n");
	while(ignorable != NULL)
	{
		int fnr = fnmatch(ignorable, string, flags);
		if(fnr == 0 && /* this is a hack for only my uses, if you want to use as fnmatch_multi, you have to delete the next (ignorable) first symbol check */ ignorable[0] != '#')
		{
			return 0;
		}
		
		ignorable = strtok(NULL, "\n");
	}
	
	return 1;
}

void usage()
{
	printf("Usage: \n"
	"   justup status\n"
	"   justup save\n"
	"   justup push\n"
	"   justup push <path1 path2 ...>\n");
}

int lock(int act)
{
	const char *lockfile = ".justup/.lock";
	if(act == 0) // Check if locked
	{
		return fs_entity_exists(lockfile);
	}
	else if(act == 1) // Create lock file
	{
		int fp = open(lockfile, O_RDONLY, 0777);
		close(fp);
	}
	else if(act == 2) // Remove lock file
	{
		unlink(lockfile);
	}
}


int answer_yn(char *question)
{
	char answer[2]; // 110=n | 121=y
	
	printf("%s y/n: ", question);
	fgets(answer, 2, stdin);
	
	if((int) answer[0] == 110)
	{
		return 0;
	}
	else if((int) answer[0] == 121)
	{
		return 1;
	}
	else
	{
		return 1;
	}
}

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