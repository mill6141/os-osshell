#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>

bool fileExecutableExists(std::string file_path);
void splitString(std::string text, char d, std::vector<std::string>& result);
void vectorOfStringsToArrayOfCharArrays(std::vector<std::string>& list, char ***result);
void freeArrayOfCharArrays(char **array, size_t array_length);

void run_history_command(std::vector<std::string>& history_list, int history_limit, int *history_index, std::vector<std::string>& cmd_list);
void execute_commands(char **command_list_exec, std::vector<std::string> os_path_list);
std::string filename = "history.txt";

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

    // Initialize all history values from file:
    { // new scope so the file auto closes
        std::ifstream file(filename); // open file to read
        std::string line{}; // make it empty
        while (std::getline(file, line))
        {
            history[history_index++] = line;
        }
    }

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
            history[history_index % history_limit] = user_input;
            history_index++;
            exited=true;
                // Initialize all history values from file:
                { // new scope so the file auto closes
                    std::ofstream file(filename); // open file to read
                    for (int i = 0; i < history_limit; i++)
                    {
                        if(history[(i+history_index)%history_limit].empty()){
                            continue;
                        }
                        file << history[(i+history_index)%history_limit] << "\n";
                    }
                }
            break;
        } 

        // Split the user input into command and parameters
        splitString(user_input, ' ', command_list);
        
        // check if the command is history for multiple arguments
        if(command_list[0] == "history"){
            run_history_command(history, history_limit, &history_index, command_list); // print history
            if((command_list.size() > 1) && (command_list[1] != "clear")) { // two arguments, but not history clear
                history[history_index % history_limit] = user_input;
                history_index++;
            } else if(command_list.size() == 1){ // one argument history
                history[history_index % history_limit] = user_input;
                history_index++;
            }
            continue;
        } else{
            history[history_index % history_limit] = user_input;
            history_index++; // increment after so that the latest history command does not print with history
        }

        vectorOfStringsToArrayOfCharArrays(command_list, &command_list_exec);

        // Check to see if the command exists in the path directories
        execute_commands(command_list_exec, os_path_list);

        freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);
    }
    return 0;
}

void run_history_command(std::vector<std::string>& history_list, int history_limit, int *history_index, std::vector<std::string>& cmd_list){
    int cnt=1;
    int i = 0;
    

    if(cmd_list.size() == 1){ // basic history command
        for (i = 0; i < history_limit; i++)
        {
            if(history_list[(i+*history_index)%history_limit].empty()){
                continue;
            }
            printf("  %d: %s\n", cnt++, history_list[(i+*history_index)%history_limit].c_str());
        }
    } else if(cmd_list[1] == "clear") { // clear history
        // clear file
        std::ofstream ofs;
        ofs.open("history.txt", std::ofstream::out | std::ofstream::trunc);
        ofs.close();

        // clear array
        for(int j = 0; j < history_limit; j++){
            history_list[j] = "";
        }
        *history_index = 0;
    } else { // number arguments
        try { // necessary for out of bounds catch and history 3x type catches
            size_t pos = 0;
            int arg = std::stoi(cmd_list[1], &pos);
    
            // Reject partial parses like "3x" (stoi would parse 3 and stop at 'x')
            if (pos != cmd_list[1].size() || arg <= 0) {
                throw std::exception();
            }

            if(arg > *history_index){
                throw std::exception();
            }
    
            if(*history_index >= 128){
                cnt = history_limit - arg + 1;
            } else{
                cnt = (*history_index%history_limit) - arg + 1;
            }
            if(arg > 0){
                for (i = arg; i > 0; i--)
                {
                    printf("  %d: %s\n", cnt++, history_list[(*history_index-i)%history_limit].c_str());
                }
            }
        } catch (...) {
            printf("Error: history expects an integer > 0 (or 'clear')\n");
        }

    }
}

void execute_commands(char **command_list_exec, std::vector<std::string> os_path_list){
    int pid = fork();
    if(pid == 0){

        // Check to see if it's a path to an exec
        char c = command_list_exec[0][0];
        if((c == '.') || (c=='/')){
            std::string filePath = command_list_exec[0];
            //printf(filePath.c_str());
            if(fileExecutableExists(filePath)){
                // This is the child, so execute the command
                execv(filePath.c_str(), command_list_exec);
                // If execv returns, there was an error
                printf("%s: Error executing command\n", command_list_exec[0]);
            }
        } else{
            // Default checking PATH
            for(int i=0; i<os_path_list.size(); i++){
                std::string filePath = os_path_list[i] + "/" + command_list_exec[0];
                if(fileExecutableExists(filePath)){
                    // This is the child, so execute the command
                    execv(filePath.c_str(), command_list_exec);
                    // If execv returns, there was an error
                    printf("%s: Error executing command\n", command_list_exec[0]);
                }
            }
        }

        printf( "%s: Error command not found\n", command_list_exec[0]);
        exit(0); // kill child
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
