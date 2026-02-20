#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>

bool fileExecutableExists(std::string file_path);
void splitString(std::string text, char d, std::vector<std::string>& result);
void vectorOfStringsToArrayOfCharArrays(std::vector<std::string>& list, char ***result);
void freeArrayOfCharArrays(char **array, size_t array_length);


void run_history_command(std::vector<std::string>& history_list, int history_limit, int history_index);
void execute_commands(char **command_list_exec, std::vector<std::string> os_path_list);

int main (int argc, char **argv)
{
    // Get list of paths to binary executables
    std::vector<std::string> os_path_list;
    char* os_path = getenv("PATH");
    splitString(os_path, ':', os_path_list);

    // Create list to store history
    int history_limit = 128;
    int history_index = 0;
    std::vector<std::string> history(history_limit);
    

    // Create variables for storing command user types
    std::string user_command;               // to store command user types in
    std::vector<std::string> command_list;  // to store `user_command` split into its variour parameters
    char **command_list_exec;               // to store `command_list` converted to an array of character arrays

    // Welcome message
    printf("Welcome to OSShell! Please enter your commands ('exit' to quit).\n");

    // Repeat:
    //  Print prompt for user input: "osshell> " (no newline)
    //  Get user input for next command
    //  If command is `exit` exit loop / quit program
    //  If command is `history` print previous N commands
    //  For all other commands, check if an executable by that name is in one of the PATH directories
    //   If yes, execute it
    //   If no, print error statement: "<command_name>: Error command not found" (do include newline)
    std::string user_input;
    bool exited = false;
    while (!exited)
    {
        printf("osshell> ");
        std::getline(std::cin, user_input);

        if(user_input.empty()){
            continue;
        } else if(user_input == "exit"){
            exited=true;
            continue;
        } else if (user_input == "history"){
            run_history_command(history, history_limit, history_index);
            continue;
        }

        history[history_index % history_limit] = user_input;
        history_index++;

        
        // Split the user input into command and parameters
        splitString(user_input, ' ', command_list);
        vectorOfStringsToArrayOfCharArrays(command_list, &command_list_exec);

        // Check to see if the command exists in the path directories
        execute_commands(command_list_exec, os_path_list);

        freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);

    }
    

    /************************************************************************************
     *   Example code - remove in actual program                                        *
     ************************************************************************************/
    // Shows how to loop over the directories in the PATH environment variable
    int i;
    for (i = 0; i < os_path_list.size(); i++)
    {
        printf("PATH[%2d]: %s\n", i, os_path_list[i].c_str());
    }
    printf("------\n");
    
    // Shows how to split a command and prepare for the execv() function
    std::string example_command = "ls -lh";
    splitString(example_command, ' ', command_list);
    vectorOfStringsToArrayOfCharArrays(command_list, &command_list_exec);
    // use `command_list_exec` in the execv() function rather than looping and printing
    i = 0;
    while (command_list_exec[i] != NULL)
    {
        printf("CMD[%2d]: %s\n", i, command_list_exec[i]);
        i++;
    }
    // free memory for `command_list_exec`
    freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);
    printf("------\n");

    // Second example command - reuse the `command_list` and `command_list_exec` variables
    example_command = "echo \"Hello world\" I am alive!";
    splitString(example_command, ' ', command_list);
    vectorOfStringsToArrayOfCharArrays(command_list, &command_list_exec);
    // use `command_list_exec` in the execv() function rather than looping and printing
    i = 0;
    while (command_list_exec[i] != NULL)
    {
        printf("CMD[%2d]: %s\n", i, command_list_exec[i]);
        i++;
    }
    // free memory for `command_list_exec`
    freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);
    printf("------\n");
    /************************************************************************************
     *   End example code                                                               *
     ************************************************************************************/


    return 0;
}

void run_history_command(std::vector<std::string>& history_list, int history_limit = 128, int history_index = 0){
    // TODO: Make the history command work
    // For now:
    int i;
    int cnt=1;
    for (i = 0; i < history_limit; i++)
    {
        if(history_list[(i+history_index)%history_limit].empty()){
            continue;
        }
        printf("  %d: %s\n", cnt++, history_list[(i+history_index)%history_limit].c_str());
    }
}

void execute_commands(char **command_list_exec, std::vector<std::string> os_path_list){
    int pid = fork();
    if(pid == 0){
        for(int i=0; i<os_path_list.size(); i++){
            std::string filePath = os_path_list[i] + "/" + command_list_exec[0];
            if(fileExecutableExists(filePath)){
                // This is the child, so execute the command
                execv(filePath.c_str(), command_list_exec);
                // If execv returns, there was an error
                printf("%s: Error executing command\n", command_list_exec[0]);
            }
        }

        printf("Command not found\n");
        
    } else{
        // This is the parent, so just wait for the child to finsih its task.
        waitpid(pid, NULL, 0);
    }
}

/*
   file_path: path to a file
   RETURN: true/false - whether or not that file exists and is executable
*/
bool fileExecutableExists(std::string file_path)
{
    bool exists = false;
    // check if `file_path` exists
    // if so, ensure it is not a directory and that it has executable permissions

    if(std::filesystem::exists(file_path) && !std::filesystem::is_directory(file_path) && access(file_path.c_str(), X_OK) == 0){
        exists = true;
    }

    return exists;
}

/*
   text: string to split
   d: character delimiter to split `text` on
   result: vector of strings - result will be stored here
*/
void splitString(std::string text, char d, std::vector<std::string>& result)
{
    enum states { NONE, IN_WORD, IN_STRING } state = NONE;

    int i;
    std::string token;
    result.clear();
    for (i = 0; i < text.length(); i++)
    {
        char c = text[i];
        switch (state) {
            case NONE:
                if (c != d)
                {
                    if (c == '\"')
                    {
                        state = IN_STRING;
                        token = "";
                    }
                    else
                    {
                        state = IN_WORD;
                        token = c;
                    }
                }
                break;
            case IN_WORD:
                if (c == d)
                {
                    result.push_back(token);
                    state = NONE;
                }
                else
                {
                    token += c;
                }
                break;
            case IN_STRING:
                if (c == '\"')
                {
                    result.push_back(token);
                    state = NONE;
                }
                else
                {
                    token += c;
                }
                break;
        }
    }
    if (state != NONE)
    {
        result.push_back(token);
    }
}

/*
   list: vector of strings to convert to an array of character arrays
   result: pointer to an array of character arrays when the vector of strings is copied to
*/
void vectorOfStringsToArrayOfCharArrays(std::vector<std::string>& list, char ***result)
{
    int i;
    int result_length = list.size() + 1;
    *result = new char*[result_length];
    for (i = 0; i < list.size(); i++)
    {
        (*result)[i] = new char[list[i].length() + 1];
        strcpy((*result)[i], list[i].c_str());
    }
    (*result)[list.size()] = NULL;
}

/*
   array: list of strings (array of character arrays) to be freed
   array_length: number of strings in the list to free
*/
void freeArrayOfCharArrays(char **array, size_t array_length)
{
    int i;
    for (i = 0; i < array_length; i++)
    {
        if (array[i] != NULL)
        {
            delete[] array[i];
        }
    }
    delete[] array;
}
