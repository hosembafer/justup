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