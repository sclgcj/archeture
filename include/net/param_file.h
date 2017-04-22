#ifndef PARAM_FILE_H
#define PARAM_FILE_H

// The command to control the file.
enum {
	FILE_CMD_ROW_ADD,
	FILE_CMD_ROW_DEL,
	FILE_CMD_COLUMN_ADD,
	FILE_CMD_COLUMN_DEL
};

// The configure of column select type
enum {
	SELECT_COLUMN_BY_NAME,
	SELECT_COLUMN_BY_NUMBER,
};
struct select_column {
	int select_type;
	union {
		int number;
		char name[32];
	}un;
};

struct file_format {
	char col_seperate[16];
	char first_data[8];
};

// The options when the unique value reach the end
enum {
	FILE_OUT_VAL_ABORT_USER,			//abort the user 
	FILE_OUT_VAL_CONTINUE_IN_A_CYCLIC_MANNER,	//continue in a cyclic manner
	FILE_OUT_VAL_CONTINUE_WITH_LAST_VALUE	//continue with last value
};

struct unique_value {
	int out_of_value_oper;
	union {
		int alloc_value_type;
		int alloc_value;
	}un;
};

// The type to choose next value
enum {
	FILE_CHOOSE_TYPE_RANDOM,		//random
	FILE_CHOOSE_TYPE_SEQUENTIAL,		//sequential
	FILE_CHOOSE_TYPE_UNIQUE		//unique
};

// When to choose the next value
enum {
	FILE_CHOOSE_TIME_EACH_ITERATION,	//each iteration
	FILE_CHOOSE_TIME_EACH_OCCURENCE,	//each occurence
	FILE_CHOOSE_TIME_ONCE		//just 	once
};

struct next_value_choose {
	int choose_type;
	int choose_time;
	struct unique_value unique_value;	
};

struct param_file {
	char *file_map;
	char file_path[256];
	int  file_size;
	int row;
	int column;
	struct file_format file_format;
	struct select_column select_col;
	struct next_value_choose value_choose;	
};

#endif
