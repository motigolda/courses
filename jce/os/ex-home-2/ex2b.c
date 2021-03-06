/*
Ex2 - Basic Shell & Old Command Running
Authored by Mordechai Goldstein
318530136

==Description==
This program tends to demonstrate how a shell looks alike.
In this part, you can also select command from history and run it, using !<number from history>
In addition, the program saves the history of the strings that entered to it.
In order to use it just enter your command. 
Note that "cd" command is not supported (yet)
For watching in the history, type "history"
For exit, type "done".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define HISTORY_FILE_PATH "history.txt"

char *current_dir_text = NULL;

unsigned int this_run_commands_counter = 0;
unsigned int this_run_words_counter = 0;

#define WAIT_FOR_USER_INPUT_STR (current_dir_text != NULL ? current_dir_text : "next command")

// When command entered, these are the strings that compared with the enterd text
#define CMD_EXIT_STR "done"
#define CMD_PRINT_HISTORY_STR "history"
#define COMMAND_NOT_SUPPORTED_STR "Command not supported (Yet)\n"
#define MAX_LINE_LENGTH (512)
#define IS_NULL(pointer) (pointer == NULL)
#define EXIT_IF_ALLOCATION_FAILED(pointer) \
    if (IS_NULL(pointer)) \
        exit(1);
// Macro that helps to recognize output parameters in function
#define OUT 

typedef struct _history_t{
    // Assumes that history_string is always dynamically allocated
    char *history_string;
} history_t;

typedef struct _line_stats_t{
    unsigned int word_count;
    unsigned int char_count;
    bool invalid_command;
    char **words_array;
} line_stats_t;

typedef enum _user_command_t{
    CMD_INVALID = -2,
    CMD_UNKNOWN = -1,
    CMD_PRINT_STATS = 0,
    CMD_PRINT_HISTORY,
    CMD_EXIT,
    CMD_EMPTY_LINE,
    CMD_RUN_TERMINAL_PROCESS,
    CMD_RUN_TERMINAL_PROCESS_FROM_HISTRORY,
    CMD_NOT_SUPPORTED
} user_command_t;

void IncrementCommandsCounter();
void AddToThisRunWordsCounter(unsigned int num);
void RunUserTerminalProcess(line_stats_t *stats, history_t* history);
void RunUserTerminalProccessFromHistory(line_stats_t *stats, history_t* history);
// File functions
long GetFileSize(char* file_path){
    if (file_path == NULL)
        return -1;

    FILE *fp = fopen(file_path, "r");
    if (fp == NULL)
        return -1;
    
    fseek(fp, 0L, SEEK_END);
    long text_file_size = ftell(fp);
    
    fclose(fp);

    return text_file_size;
}

void ReadTextFile(char* file_path, OUT char* file_text, long file_size){
    if (file_path == NULL) 
        return;

    if (file_text == NULL) 
        return;

    if (file_size < 0)
        file_size = GetFileSize(file_path);

    FILE *fp = fopen(file_path, "r");
    if (fp == NULL)
        return;

    memset(file_text, 0, file_size);

    char current_line[MAX_LINE_LENGTH] = { 0 };
    
    while((fgets(current_line, MAX_LINE_LENGTH, fp)) != NULL)
        strcat(file_text, current_line); 
    
    fclose(fp);
}       

void SaveTextFile(char* file_path, char* file_text){
    if (file_path == NULL)
        return;

    if (file_text == NULL)
        return;

    FILE *fp = fopen(file_path ,"w");
    if (fp == NULL)
        return;

    fwrite(file_text, strlen(file_text), 1, fp);

    fclose(fp);
}
// History functions
history_t* LoadHistoryFromFile(char* history_file_path){
    if (history_file_path == NULL)
        return NULL;
    
    long file_size = GetFileSize(history_file_path);
    
    static history_t history = { 0 };

    if (file_size > 0){
        history.history_string = malloc(file_size + 1);
        EXIT_IF_ALLOCATION_FAILED(history.history_string);
        memset(history.history_string, 0, file_size + 1);

        ReadTextFile(history_file_path, history.history_string, file_size);
    }
    else{
        // In case of empty or non exist history file - create empty string.
        // Thus when new strings loaded into the history object, 
        // strcat() will work as excepted.
        history.history_string = malloc(1);
        EXIT_IF_ALLOCATION_FAILED(history.history_string);
        *history.history_string = 0;
    }

    return &history;
}

void SaveHistoryToFile(history_t *history, char* history_file_path){
    if (history == NULL)  
        return;

    if (history_file_path == NULL)
        return;

    SaveTextFile(history_file_path, history->history_string);
}

void AppendEntryToHistory(history_t *history, char *entry){
    if (history == NULL || history->history_string == NULL)
        return;

    if (entry == NULL)
        return;

    size_t current_history_string_len = strlen(history->history_string);
    size_t new_entry_len = strlen(entry); 
    
    history->history_string = realloc(history->history_string, current_history_string_len + new_entry_len + 1);
    EXIT_IF_ALLOCATION_FAILED(history->history_string);
    if (current_history_string_len > 0 &&
        history->history_string[current_history_string_len - 1] != '\n')
        strcat(history->history_string, "\n");

    strcat(history->history_string, entry);
}

void PrintHistory(history_t *history){
    if (history == NULL)
        return;

    char *history_copy = malloc(strlen(history->history_string) + 1);
    EXIT_IF_ALLOCATION_FAILED(history_copy);
    strcpy(history_copy, history->history_string);

    char *line_in_history;
    line_in_history = strtok(history_copy, "\n");
    unsigned int history_entry_num = 1;

    while(line_in_history != NULL){
        printf("%u: %s\n", history_entry_num++, line_in_history);
        line_in_history = strtok(NULL, "\n");
    }

    free(history_copy);
    
}
// Statistics functions
line_stats_t* GetLineStats(char *line){
    if (line == NULL)
        return NULL;

    static line_stats_t stats = { 0 };
    char current_word_buffer[MAX_LINE_LENGTH] = { 0 };
    int current_word_ch_index = 0;
    stats.invalid_command = false;
    bool in_word = false;
    int i = 0;
    if(strlen(line) > 0 && (line[0] == ' ' || line[strlen(line) - 2] == ' ')){
        stats.invalid_command = true;
        return &stats;
    }
    if (line[0] == '\n')
        return &stats;

    while(line[i] != 0){
        if(line[i] != ' ' && !in_word){
            in_word = true;
            
        }
        if(line[i] == ' ' || line[i] == '\n' || line[i] == 0){
            in_word = false;

            if (current_word_ch_index > 0){
                stats.words_array = realloc(stats.words_array, sizeof(char**) * (stats.word_count+1));
                EXIT_IF_ALLOCATION_FAILED(stats.words_array);
                stats.words_array[stats.word_count] = malloc(current_word_ch_index + 1);
                EXIT_IF_ALLOCATION_FAILED(stats.words_array);
                strcpy(stats.words_array[stats.word_count], current_word_buffer); 
                memset(current_word_buffer, 0, MAX_LINE_LENGTH);
                current_word_ch_index = 0;
                stats.word_count++;
            }  
        }
        else
            stats.char_count++;

        if(line[i] != ' ' && line[i] != '\n'){
            current_word_buffer[current_word_ch_index] = line[i];
            current_word_ch_index++;
        }

        i++;
    }
    
    return &stats;
}

void ClearStats(line_stats_t *stats){
    if (stats == NULL)
        return;

    for (int i = 0; i < stats->word_count; i++)
        if (stats->words_array[i] != NULL)
            free(stats->words_array[i]);
    
    free(stats->words_array);
    stats->words_array = NULL;
    stats->char_count = 0;
    stats->word_count = 0;
    stats->invalid_command = false; 
}

void PrintLineStats(line_stats_t* stats){
    if (stats == NULL)
        return;

    printf("%u words\n", stats->word_count);
    printf("%u chars\n", stats->char_count);
}
// Command parser
user_command_t GetSelectedCommand(line_stats_t *stats){
    if(stats == NULL)
        return CMD_UNKNOWN;

    if (stats->invalid_command)
        return CMD_INVALID;

    if (stats->words_array == NULL || stats->word_count == 0 || stats->char_count == 0)
        return CMD_EMPTY_LINE;
    
    if (!IS_NULL(stats->words_array) && strcmp(stats->words_array[0], "cd") == 0)
        return CMD_NOT_SUPPORTED;

    if (stats->word_count == 1 && stats->words_array[0][0] == '!')
        return CMD_RUN_TERMINAL_PROCESS_FROM_HISTRORY;

    if (strcmp(stats->words_array[0], CMD_PRINT_HISTORY_STR) == 0)
        return CMD_PRINT_HISTORY;
    
    if (strcmp(stats->words_array[0], CMD_EXIT_STR) == 0)
        return CMD_EXIT;
    
    return CMD_RUN_TERMINAL_PROCESS;
}

void HandleUserSelectedCommand(line_stats_t *stats, user_command_t user_command, history_t *history, char* input_line){
    switch (user_command)
        {
            case CMD_EMPTY_LINE:
                break;
            case CMD_PRINT_HISTORY:
                PrintHistory(history);
                AppendEntryToHistory(history, input_line);
                IncrementCommandsCounter();
                break;
            case CMD_EXIT:
                IncrementCommandsCounter();
                break;
            case CMD_RUN_TERMINAL_PROCESS:
                IncrementCommandsCounter();
                RunUserTerminalProcess(stats, history);
                AppendEntryToHistory(history, input_line);
                AddToThisRunWordsCounter(stats->word_count);
                break;
            case CMD_NOT_SUPPORTED:
                printf(COMMAND_NOT_SUPPORTED_STR);
                IncrementCommandsCounter();
                AddToThisRunWordsCounter(stats->word_count);
                break;
            case CMD_RUN_TERMINAL_PROCESS_FROM_HISTRORY:
                RunUserTerminalProccessFromHistory(stats, history);
                break;
            case CMD_INVALID:
                printf("Invalid command, check for unnecessary spaces\n");
                break;
            default:
                printf("Unknown Failure\n");
                break;
        }
}
// Get input function
void GetLineFromUser(OUT char *line_from_user){
    if (line_from_user == NULL)
        return;

    memset(line_from_user, 0, MAX_LINE_LENGTH);

    printf("%s>", WAIT_FOR_USER_INPUT_STR);

    fgets(line_from_user, MAX_LINE_LENGTH, stdin);

    line_from_user[MAX_LINE_LENGTH - 1] = 0;
}
// Call this to release the memory that allocated for history->history_string
void FreeHistoryInstanceString(history_t* history){
    if (history == NULL)
        return;
    
    if (history->history_string == NULL)
        return;

    free(history->history_string);
    history->history_string = NULL;
}

void IncrementCommandsCounter(){
    ++this_run_commands_counter;
}

void DecrementCommandsCounter(){
    if (this_run_commands_counter != 0)
        --this_run_commands_counter;
}

unsigned int GetThisRunCommandCounter(){
    return this_run_commands_counter;
}

void AddToThisRunWordsCounter(unsigned int num){
    this_run_words_counter += num;
}

unsigned int GetThisRunWordsCounter(){
    return this_run_words_counter;
}

void RunUserTerminalProcess(line_stats_t *stats, history_t* history){
    if (stats == NULL)
        return;

    pid_t check_me_after_fork = fork();

    if (check_me_after_fork == 0){
        stats->words_array = realloc(stats->words_array, (stats->word_count+1) * sizeof(char**));
        EXIT_IF_ALLOCATION_FAILED(stats->words_array);
        stats->words_array[stats->word_count] = NULL;
        bool command_run_successfully = execvp(stats->words_array[0], stats->words_array) != -1;
        if(!command_run_successfully){
            perror("ERR");
            ClearStats(stats);
            SaveHistoryToFile(history, HISTORY_FILE_PATH);
            FreeHistoryInstanceString(history);
            free(current_dir_text);
            exit(1);
        }       
    }
    else if (0 < check_me_after_fork){
        wait(NULL);
    }
    else{
        perror("Cant run the command");
    }
}
// assumes stats are contain the right structure of ! and number: "!<number>"
void RunUserTerminalProccessFromHistory(line_stats_t *stats, history_t* history){
    // extract the number of command
    unsigned int command_number = 0;
    command_number = atoi(&stats->words_array[0][1]);
    char *history_copy = malloc(strlen(history->history_string) + 1);
    EXIT_IF_ALLOCATION_FAILED(history_copy);
    strcpy(history_copy, history->history_string);

    char *line_in_history;
    line_in_history = strtok(history_copy, "\n");
    unsigned int history_entry_num = 1;

    while(line_in_history != NULL && history_entry_num < command_number){
        history_entry_num++;
        line_in_history = strtok(NULL, "\n");  
    }

    if (command_number <= history_entry_num){
        char *line_in_history_with_newline = malloc(strlen(line_in_history) + 2);
        EXIT_IF_ALLOCATION_FAILED(line_in_history_with_newline);
        memset(line_in_history_with_newline, 0, strlen(line_in_history) + 2);
        strcpy(line_in_history_with_newline, line_in_history);
        strcat(line_in_history_with_newline, "\n");
        printf("%s>%s", WAIT_FOR_USER_INPUT_STR, line_in_history_with_newline);
        ClearStats(stats);
        stats = GetLineStats(line_in_history_with_newline);
        HandleUserSelectedCommand(stats, GetSelectedCommand(stats), history, line_in_history_with_newline);
        free(line_in_history_with_newline);
    }
    else
        printf("Not in history\n");

    free(history_copy);
}
// The main loop: get info from user -> do the wanted command -> repeat until exit command.
int main(void){
    current_dir_text = getcwd(NULL, 0);

    history_t* history = LoadHistoryFromFile(HISTORY_FILE_PATH);
    
    if (history == NULL){
        printf("Cant read history from path: %s\nhistory object is null.\n", HISTORY_FILE_PATH);
        exit(1);
    }

    char input_line[MAX_LINE_LENGTH] = { 0 }; 

    line_stats_t *stats = NULL;

    user_command_t current_selected_command = -1;

    while (current_selected_command != CMD_EXIT){
        GetLineFromUser(input_line);

        stats = GetLineStats(input_line);

        current_selected_command = GetSelectedCommand(stats);

        HandleUserSelectedCommand(stats, 
            current_selected_command, history, input_line);

        ClearStats(stats);    
    }
    printf("Num of commands: %u\nTotal number of words in all commands: %u!\n", 
        GetThisRunCommandCounter(), 
        GetThisRunWordsCounter());

    SaveHistoryToFile(history, HISTORY_FILE_PATH);

    FreeHistoryInstanceString(history);
    
    free(current_dir_text);

    return 0;
}