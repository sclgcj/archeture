#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <error.h>
#include <errno.h>

static char *cmake_config_name = "CMakeLists.txt";
static char *readme_name = "README.md";
static char *root_cmake_fmt = 
"project(${PROJNAME})\n"
"include_directories(${PROJECT_SOURCE_DIR}/include)\n"
"link_directories(${PROJECT_SOURCE_DIR}/lib)\n"
"add_subdirectory(%s)\n"
"add_subdirectory(%s)";

static char *src_cmake_fmt = 
"project(${PROJNAME})\n"
"AUX_SOURCE_DIRECTORY(. DIR_SRCS)\n"
"add_executable(%s ${DIR_SRCS})\n"
"target_link_libraries(%s core)";

static char *test_cmake_fmt = 
"project(${PROJNAME})\n"
"AUX_SOURCE_DIRECTORY(. DIR_SRCS)\n"
"add_executable(%s_test ${DIR_SRCS})\n"
"target_link_libraries(%s_test core)";

static char *blank_src_fmt = "project(${PROJNAME})\n";



static void
print_usage() 
{
	printf("usage: project_manager create|delete proj_name [-p app_path] \n");
}

static char *
file_path_get(int argc, char **argv)
{
	int size = 0;
	int name_len = strlen(argv[2]);
	char *file_path = NULL;

	if (argc < 4) {
		file_path = (char*)calloc(1, 3 + name_len);
		strcpy(file_path, "./");
	} else {
		size = strlen(argv[3]);
		file_path = (char*)calloc(1, size + 10 + name_len);
		strcpy(file_path, argv[3]);
		if (file_path[size - 1] != '/')
			strcat(file_path, "/");
	}
	strcat(file_path, "src");

	return file_path;
}

static int
write_config_data(
	char *path, 
	char *data,
	int  len,
	int  flag
)
{
	int ret = 0;
	FILE *fp = NULL;
	fp = fopen(path, "w");
	if (!fp) {
		fprintf(stderr, "fopen %s error: %s\n", path, strerror(errno));
		return -1;
	}
	if (flag == 1) {
		ret = fwrite(data, 1, len, fp);
		if (ret < 0) {
			fprintf(stderr, "fwrite %d bytes to %s error: %s\n",
					len, path, strerror(errno));
			return -1;
		}
	}
	fclose(fp);

	return 0;
}

static int
create_readme(
	char *dir_path,
	char *dir_name,
	int  flag
)
{
	int ret = 0;
	int dir_len = 0;
	int file_path_len = 0;
	char data[128] = { 0 };
	char *readme = NULL;

	file_path_len = strlen(dir_path);
	dir_len = strlen(readme_name);
	readme = (char*)calloc(1, file_path_len + file_path_len + 3);
	if (!readme) {
		fprintf(stderr, "calloc %d bytes error: %s\n", 
				file_path_len + file_path_len + 3);
		return -1;
	}
	fprintf(stderr,"dir_path = %s\n", dir_path);
	strcpy(readme, dir_path);
	strcat(readme, "/");
	strcat(readme, readme_name);
	fprintf(stderr, "read paht = %s\n", readme);
	sprintf(data, "#%s/%s", dir_path, readme_name);
	ret = write_config_data(readme, data, strlen(data), flag);
	free(readme);	

	return ret;
}

static char *
create_subdirectory(
	char *file_path,
	char *dir_name,
	int  flag
)
{
	int ret = 0;
	int dir_len = strlen(dir_name);
	int file_path_len = strlen(file_path);
	char *readme = NULL;
	char new_file[1024] = { 0 };
	char *dir_path = NULL; 

	dir_path = (char*)calloc(1, dir_len + file_path_len + 3);
	if (!dir_path) {
		fprintf(stderr, "calloc %d byte error: %s\n", 
				dir_len + file_path + 3, 
				strerror(errno));
		return NULL;
	}
	memcpy(dir_path, file_path, file_path_len);
	strcat(dir_path, "/");
	strcat(dir_path, dir_name);
	umask(0);
	ret = mkdir(dir_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
	if (ret == -1 && errno != EEXIST) {
		fprintf(stderr, "create dir %s error: %s\n", dir_path, strerror(errno));
		return NULL;
	}

	ret = create_readme(dir_path, dir_name, 1);
	if (ret == -1)
		return NULL;

	if ( flag == 1) {
		sprintf(new_file, "%s/%s.c", dir_path, dir_name);
		ret = write_config_data(new_file, NULL, 0, 0);
		if (ret == -1)
			return NULL;
	}
	return dir_path;
}

static char *
cmake_config_path_get(
	char *dir_path
)
{
	int ret = 0;
	int dir_len = strlen(dir_path);
	int cmake_len = strlen(cmake_config_name);
	char *file_path = NULL;

	file_path = (char *)calloc(1, dir_len + cmake_len + 3);
	if (!file_path) {
		fprintf(stderr, "calloc %d bytes error: %s\n", 
				dir_len + cmake_len + 3, 
				strerror(errno));
		return  NULL;
	}
	memcpy(file_path, dir_path, dir_len);
	if (file_path[dir_len - 1] != '/')
		strcat(file_path, "/");
	strcat(file_path, cmake_config_name);
	
	return file_path;
}

static int
create_cmake_config(
	char *dir_path,
	char *fmt,
	char *project_name,
	char *test_name
)
{
	int ret = 0;
	int fmt_len = strlen(fmt);
	int proj_name_len = strlen(project_name);
	char *file_path = NULL;
	char *cmake_config = NULL;
	FILE *fp = NULL;

	file_path = cmake_config_path_get(dir_path);
	cmake_config = (char*)calloc(1, fmt_len + proj_name_len * 2 + 3);
	if (!cmake_config) {
		fprintf(stderr, "calloc %d byte error: %s\n", 
				fmt_len + proj_name_len * 2 + 3, 
				strerror(errno));
		goto err;
	}
	sprintf(cmake_config, fmt, project_name, test_name);
	ret = write_config_data(file_path, cmake_config, strlen(cmake_config), 1);
	goto out;
err:
	ret = -1;
out:
	if (file_path != NULL)
		free(file_path);
	if (cmake_config != NULL)
		free(cmake_config);
	return ret;
}

static int
create_subdirectory_cmakeconfig(
	char *file_path,
	char *dir_name,
	char *fmt,
	char *project_name
)
{
	int ret = 0;
	char *dir_path = NULL;

	dir_path = create_subdirectory(file_path, dir_name, 1);
	if (dir_path == NULL)
		return -1;
	ret = create_cmake_config(dir_path, fmt, project_name, project_name);

	free(dir_path);
	return ret;
}

static int
project_create(int argc, char **argv, char *file_path)
{
	int ret = 0;
	char *dir_path = NULL;

	umask(0);
	ret = mkdir(file_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
	//ret = mkdir(file_path, S_IRWXU | S_IRWXG | S_IRWXO );
	if (ret == -1 && errno != EEXIST) {
		fprintf(stderr, "create dir %s error: %s\n", file_path, strerror(errno));
		return -errno;
	}
	ret = create_readme(file_path, argv[2], 1);
	if (ret == -1)
		return ret;
	ret = create_cmake_config(file_path, root_cmake_fmt, argv[2], "test");
	if (ret == -1)
		return ret;
	ret = create_subdirectory_cmakeconfig(file_path, argv[2], src_cmake_fmt, argv[2]);
	if (ret == -1)
		return ret;
	ret = create_subdirectory_cmakeconfig(file_path, "test", test_cmake_fmt, argv[2]);
	if (ret == -1)
		return ret;
	dir_path = create_subdirectory(file_path, "include", 0);
	if (!dir_path)
		return -1;
	free(dir_path);
	dir_path = create_subdirectory(file_path, "lib", 0);
	if (!dir_path)
		return -1;
	free(dir_path);

	return 0;
}

static int
is_dir(
	char *path
)
{
	struct stat buf;  
	int ret = stat(path,&buf);  
	if (ret == -1)
		return ret;
	if (buf.st_mode & S_IFDIR)
		return 1;
	return 0;
}

static int
project_delete(
	char *file_path
)
{
	int ret = 0;
	DIR *dir = opendir(file_path);
	char buf[1024] = { 0 };
	struct dirent *dirent = NULL;

	if (!dir) {
		fprintf(stderr, "opendir %s error: %s\n", file_path, strerror(errno));
		return -1;
	}

	while (1) {
		dirent = readdir(dir);
		if (!dirent) {
			if (errno == 0)
				break;
			else {
				fprintf(stderr, "readdir error: %s\n", strerror(errno));
				return -1;
			}
		}
		if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
			continue;
		sprintf(buf, "%s/%s", file_path, dirent->d_name);
		ret = is_dir(buf);
		if (ret == -1)
			return -1;
		if (ret) {
			ret = project_delete(buf);
			if (ret == -1)
				return ret;
			ret = rmdir(buf);
			if (ret == -1) {
				fprintf(stderr, "rmdir %s error: %s\n", buf, strerror(errno));
				return -1;
			}
		} else {
			ret = unlink(buf);
			if (ret == -1) {
				fprintf(stderr, "unlink %s error: %s\n", buf, strerror(errno));
				return -1;
			}
		}
	}


	return 0;
}

int 
main(int argc, char **argv)
{
	int ret = 0;
	char cmake[1024] = { 0 };
	char *file_path = NULL;

	if (argc < 3) {
		printf("wrong paramters.\n");
		print_usage();
		return -1;
	}


	file_path = file_path_get(argc, argv);

	fprintf(stderr, "file_path = %s\n", file_path);
	if (strcmp(argv[1], "create") == 0)
		ret = project_create(argc, argv, file_path);
	else if (strcmp(argv[1], "delete") == 0) {
		ret = project_delete(file_path);
		if (ret != -1) {
			sprintf(cmake, "%s/%s", file_path, cmake_config_name);
			ret = write_config_data(cmake, blank_src_fmt, strlen(blank_src_fmt), 1);
		}
/*		ret = rmdir(file_path);
		if (ret == -1) {
			fprintf(stderr, "delete %s error: %s\n", file_path, strerror(errno));
		}*/
	}

	free(file_path);
	return ret;
}
