#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>


const size_t MAX_HISTORY = 128;
const char* HISTORY_FILE = ".osshell_history";

bool fileExecutableExists(std::string file_path);
void splitString(std::string text, char d, std::vector<std::string>& result);
void vectorOfStringsToArrayOfCharArrays(std::vector<std::string>& list, char ***result);
void freeArrayOfCharArrays(char **array, size_t array_length);
void appendHistory(std::vector<std::string>& history, const std::string& command);
void loadHistory(std::vector<std::string>& history);
void saveHistory(const std::vector<std::string>& history);
void printHistory(const std::vector<std::string>& history, int limit = -1);
bool parsePositiveInteger(const std::string& text, int& value);

int main (int argc, char **argv)
{
    // Get list of paths to binary executables
    std::vector<std::string> os_path_list;
    char* os_path = getenv("PATH");
    if (os_path != nullptr) {
        splitString(os_path, ':', os_path_list);
    }

    // Create list to store history
    std::vector<std::string> history;
    loadHistory(history);

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
    while (true){
        std::cout << "osshell> ";
        std::cout.flush();

        if (!std::getline(std::cin, user_command)){
            break;
        }

        if (!user_command.empty() && user_command.back() == '\r'){
            user_command.pop_back();
        }

        if (user_command.empty()){
            continue;
        }

        splitString(user_command, ' ', command_list);
        if (command_list.empty()){
            continue;
        }

        std::string command_name = command_list[0];

        if (command_name == "exit"){
            appendHistory(history, user_command);
            std::cout << std::endl;
            break;
        }

        if (command_name == "history"){
            if (command_list.size() == 1){
                printHistory(history);
                appendHistory(history, user_command);
                continue;
            }

            if (command_list.size() == 2 && command_list[1] == "clear"){
                history.clear();
                saveHistory(history);
                continue;
            }

            int history_limit;
            if (command_list.size() == 2 && parsePositiveInteger(command_list[1], history_limit))
            {
                printHistory(history, history_limit);
                appendHistory(history, user_command);
                continue;
            }

            std::cout << "Error: history expects an integer > 0 (or 'clear')" << std::endl;
            appendHistory(history, user_command);
            continue;
        }

        appendHistory(history, user_command);

        std::string executable_path;
        bool found = false;

        if (command_name[0] == '.' || command_name[0] == '/')
        {
            if (fileExecutableExists(command_name))
            {
                executable_path = command_name;
                found = true;
            }
        }
        else
        {
            for (const std::string& path_dir : os_path_list)
            {
                std::string path = path_dir;
                if (!path.empty() && path.back() != '/')
                {
                    path += '/';
                }
                path += command_name;

                if (fileExecutableExists(path))
                {
                    executable_path = path;
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            std::cout << command_name << ": Error command not found" << std::endl;
            continue;
        }

        command_list[0] = executable_path;
        vectorOfStringsToArrayOfCharArrays(command_list, &command_list_exec);

        pid_t pid = fork();
        if (pid == 0)
        {
            execv(command_list_exec[0], command_list_exec);
            std::exit(1);
        }
        else if (pid > 0)
        {
            int status = 0;
            waitpid(pid, &status, 0);
        }
        freeArrayOfCharArrays(command_list_exec, command_list.size() + 1);
    }
    saveHistory(history);

    return 0;
}

/*
   file_path: path to a file
   RETURN: true/false - whether or not that file exists and is executable
*/
bool fileExecutableExists(std::string file_path){
    std::filesystem::path p(file_path);
    if (!std::filesystem::exists(p) || std::filesystem::is_directory(p))
    {
        return false;
    }
    return access(file_path.c_str(), X_OK) == 0;
}

void appendHistory(std::vector<std::string>& history, const std::string& command){
    history.push_back(command);
    if (history.size() > MAX_HISTORY)
    {
        size_t remove_count = history.size() - MAX_HISTORY;
        history.erase(history.begin(), history.begin() + remove_count);
    }
}

void loadHistory(std::vector<std::string>& history)
{
    std::ifstream infile(HISTORY_FILE);
    if (!infile.is_open())
    {
        return;
    }
    std::string line;
    while (std::getline(infile, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            appendHistory(history, line);
        }
    }
}

void saveHistory(const std::vector<std::string>& history)
{
    std::ofstream outfile(HISTORY_FILE, std::ios::trunc);
    if (!outfile.is_open())
    {
        return;
    }

    for (const std::string& cmd : history)
    {
        outfile << cmd << "\n";
    }
}

void printHistory(const std::vector<std::string>& history, int limit)
{
    size_t start_index = 0;
    if (limit > 0 && static_cast<size_t>(limit) < history.size())
    {
        start_index = history.size() - static_cast<size_t>(limit);
    }

    for (size_t i = start_index; i < history.size(); i++)
    {
        printf("%3zu: %s\n", i + 1, history[i].c_str());
    }
}

bool parsePositiveInteger(const std::string& text, int& value)
{
    if (text.empty())
    {
        return false;
    }

    for (char c : text)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
    }

    try
    {
        size_t processed = 0;
        long parsed = std::stol(text, &processed);
        if (processed != text.size() || parsed <= 0 || parsed > std::numeric_limits<int>::max())
        {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
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
