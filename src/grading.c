#include "grading.h"
#include "test.h"
#include "answer.h"

/** 
 * @file grading.c
 * @brief Grade each student's files one by one.
 * @details 
 ***********************************************************************************
 * - What student file does - 
 *   1. Create "student.hsh" hash file using "student.dat" file which stored several record.
 *   2. Output correct search numbers for cetain records
 *   3. Delete several record
 *   4. Output correct search numbers for cetain records.
 ***********************************************************************************
 * - How Program works-\n
 * 	1. Initiate answer 
 * 	2. Copy some files needed to grade, 
 * 	\n such as Makefile to compile student files, student.dat file to be converted student.hsh file.
 * 	3. Go to student directory.
 * 	4. Compile student's files.
 * 	5. Execute student's file.
 * 	6. Compare answer file and file student made.
 * 	7. Write score to .csv file.
 * 	8. Back to current working directory, then Do it again untill grade every student's
 ***********************************************************************************
 * @author Junyoung Meng
 * @date unknown...
 * @version 1.1
 */

extern const char *search_fname;
extern const char *rsearch_fname;

/** 
 * @brief Indicator for section to grade.
 */
enum {Compile = 0, Create, Search, Delete, RSearch} Sectors; 

int (*fptr_tester[])(void) = {test_compile, test_create, test_search, test_delete, test_rsearch}; ///< Function pointer for functions that compare answer to files student made.
pid_t cur_pid; ///< Currrent running child process pid, which is student's executable files.

/** 
 * @brief Map variables to avoid sending signal to wrong pid.
 * @warning Useless, Need to find better solution to avoid sending signal to wrong pid.
 */
int finished[10000]; 


/** 
 * @brief Filter to get 00000000_6 file name
 * @details 
 * File name format shoud be 8-digit_6.\n6 means #6 homework.
 * @todo Implement more precise logic to check file name format
 * @param Directory name
 * @return Return 1 if file name is proper format, else return 0.
 * */
int filter(const struct dirent *info)
{
	int check;

	check = atoi(&info -> d_name[0]);

	if(check > 0)
		return 1;
	else
		return 0;
}

/**
 * @brief Handler for alaram signal.
 * @details Send SIGTERM current running child-process which is looping endless.
 * @param signal number.
 * @return void
 */

void signal_handler(int sig)
{
	if (!finished[cur_pid])
	{
		kill(cur_pid, SIGTERM);
		printf("killed pid : %d\n", cur_pid);
		finished[cur_pid] = -1;
	}
}

/**
 * @brief Execute student files, then Compare answer to student files.
 * @details 
 * 1. Remove unnecessary files to grade. 
 * 2. Compile student files
 * 3. Main process : fork(), alarm(3) and wait child prcoess, \n\t
 * Child process : execv
 * @param integer array for score
 * @return void
 */
void do_grading(int *score)
{
	int status;

	// Commands that how student's program will be compiled
	char *execv_argv[NUM_OF_SECTOR][3] = 
	{
		{NULL  , NULL    , NULL}, // NULL for compile sector.
		{"make", "create", NULL},
		{"make", "search", NULL},
		{"make", "delete", NULL},
		{"make", "rsearch", NULL}
	}; 

	// Delete unnecessary files for grading
	system("make clean");

	// Compiles all the student's files before execute his program
	if ((score[Compile] = fptr_tester[Compile]()) == 0)
	{
		printf("Compile error.. No need to grade\n");
		return ;
	}

	// Execute student's program
	for (int curr_sector = 1; curr_sector < NUM_OF_SECTOR; curr_sector++)
	{
		// Student program's output file descriptor
		int st_output_fd = -1;

		// Create a file to save student's program output for sector Search, Re-Search
		// These sector are to print proper search length for certain record in hash file
		// The others sector are to write data in correct home address in hash file
		if (curr_sector == Search)
		{
			st_output_fd = open(search_fname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		}
		else if (curr_sector == RSearch)
		{
			st_output_fd = open(rsearch_fname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		}

		// Create new process for executing student's process
		if ((cur_pid = fork()) < 0)
		{
			fprintf(stderr, "fork() error\n");
			exit(1);
		}
		else if (cur_pid == 0)
		{
			// No need to display student's output when grading create, delete.
			if (st_output_fd == -1)
			{
				close(STDOUT_FILENO);
			}
			// Need to save student's output for search, re-search.
			else
			{
				dup2(st_output_fd, stdout);
			}
			close(STDERR_FILENO);

			// Execute student's program
			execv("/usr/bin/make", execv_argv[curr_sector]);
			printf("never printed...:%d\n", curr_sector);
			exit(1);
		}
		// Wait for student's program, 
		// finished[pid] will be negative if st's program was executing longer than 2 second
		else
		{
			alarm(2); // Alarm to kill child-process if he's looping forever.
			waitpid(cur_pid, &status, 0);
			alarm(0); // Cancel previous alarm, just in case.
		}

		// Don't Test if the program was longer than 2 second
		if (finished[cur_pid] == 0)
		{
			// Compare the student's file that his program made to answer file
			score[curr_sector] = fptr_tester[curr_sector](); 
		}

		// Close student's output file 
		if (st_output_fd != -1) close(st_output_fd);
	}
}

/**
 * @brief Explore each student directory to grade one by one.
 * @details
 * Do Initiating stuff like, \n
 * get current working directory, register signal_handler, create ".csv" file, Set answer ans explore each student directory.
 * */

int main(void)
{
	// Get current working directory
	char cwd[PATH_MAX] = { 0 };
	if (getcwd(cwd, PATH_MAX) == NULL)
	{
		fprintf(stderr, "getcwd() error\n");
		exit(1);
	}

	// Create .csv file to write student's score
	int csv_fd;
	if ((csv_fd = open("Grading.csv", O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
	{
		fprintf(stderr, "open(%s) error\n", "Grading.csv");
		exit(1);
	}

	// Register signal handler to catch looping student's program
	struct sigaction sig_act;
	sig_act.sa_flags = 0;
	sig_act.sa_handler = signal_handler;
	if (sigaction(SIGALRM, &sig_act, NULL) != 0)
	{
		fprintf(stderr, "sigaction() error\n");
		exit(1);
	}

	// Get all student's directory
	struct dirent** student_list;
	int student_cnt;
	if ((student_cnt = scandir(cwd, &student_list, filter, alphasort)) < 0)
	{
		fprintf(stderr, "scandir() error\n");
		exit(1);
	}

	init_answer();

	// Round each student's directory and grade his score
	for (int i = 0; i < student_cnt; i++)
	{
		printf("------------------------------------------\n");
		printf("## %s start! ##\n", student_list[i]->d_name);
		int score[NUM_OF_SECTOR] = { 0 }, sum = 0;
		char cur_student_dir[BUFFER_SIZE] = { 0 }, cur_student_id[ID_LENGTH + 1] = { 0 };
		char result_buf[BUFFER_SIZE] = { 0 };
		char cp_cmm[BUFFER_SIZE] = { 0 };

		// Get current student id
		memcpy(cur_student_id, student_list[i]->d_name, ID_LENGTH);

		// Get current student directory
		sprintf(cur_student_dir, "%s/%s", cwd, student_list[i]->d_name);

		// Copy files for grading students
		sprintf(cp_cmm, "cp %s %s", "./answer/src/*", cur_student_dir);
		system(cp_cmm);

		// Go to student directory, Do grading
		chdir(cur_student_dir);
		do_grading(score);
		chdir(cwd);

		// Set total score
		for (int j = 0; j < NUM_OF_SECTOR; j++) sum += score[j];

		// Write result to .csv
		sprintf(result_buf, "%s %d %d %d %d %d %d\n", cur_student_id, score[Compile], score[Create], score[Search], score[Delete], score[RSearch], sum);
		write(csv_fd, result_buf, strlen(result_buf));

		// Print result on console
		printf("%-15s %-15s %-15s %-15s %-15s %-15s %-15s\n", "ID", "Compile", "Create", "Search", "Delete", "RSearch", "SUM");
		printf("%-15s %-15d %-15d %-15d %-15d %-15d %-15d\n", cur_student_id, score[Compile], score[Create], score[Search], score[Delete], score[RSearch], sum);
	}

	close(csv_fd);
	printf("======================================\n");
	printf("Grading is done\n");

	exit(0);
}
