#include <iostream>
#include <map>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <string>
#include <string.h>

#include <fstream>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

/* ------------------- TODO --------------------- */
// create a background PID vector
vector<pid_t> background;

char * currPath = new char[200];
char * prevPath = new char[200];
vector<string> previousInputs;

string lowerCase(string s){
    string newStr = s;
    int diff = 'A' - 'a';
    for (size_t i = 0; i < s.size(); i++){
        if ((s.at(i) > 'A') && (s.at(i) < 'Z')){
            newStr.at(i) = s.at(i) - diff;
        }
    }
    return newStr;
}

string removeWhitespace(string s){
    string newStr;
    for (size_t i = 0; i < s.size(); i++){
        if (std::isspace(s.at(i))){
            continue;
        }else{
            newStr.push_back(s.at(i));
        }
    }
    return newStr;
}

void printProcesses(vector<pid_t> bg){
    std::cout << "Size: " << bg.size() << endl;
    for (size_t i = 0; i < bg.size(); i++){
        std::cout << bg.at(i) << " \n"; 
    }
    std::cout << endl;
}

void runLine(string input) {
    int oldIn = dup(0);
    int oldOut = dup(1);
    
    // get tokenized commands from user input
    Tokenizer tknr(input);
    if (tknr.hasError()) {  // continue to next prompt if input had an error
        return;
    }


    // print out every command token-by-token on individual lines
    // prints to cerr to avoid influencing autograder
    for (auto cmd : tknr.commands) {
        for (auto str : cmd->args) {
            cerr << "|" << str << "| ";
        }
        if (cmd->hasInput()) {
            cerr << "in< " << cmd->in_file << " ";
        }
        if (cmd->hasOutput()) {
            cerr << "out> " << cmd->out_file << " ";
        }
        cerr << endl;
    }

    /* ---------------- TODO --------------------- */
    // chdir()
    // If dir (cd <dir>) is "-" then go to previous working directory
    // variable storing previous working directory must be declared outside the loop

    if (tknr.commands.at(0)->args.at(0) == "cd"){
        //cout << "cool" << endl;
        // if there is no more arguments, we go to the root directory
        if (tknr.commands.at(0)->args.size() == 1){
            string home = getenv("HOME");
            chdir(home.c_str());
            memcpy(prevPath,currPath,200);
            getcwd(currPath,200);
        }else{
            string args1 = tknr.commands.at(0)->args.at(1);

            // If args[1] is '-', we have to go to the previous directory
            if (args1 == "-"){
                chdir(prevPath);
                memcpy(prevPath, currPath,200);
                getcwd(currPath,200);
            }

            // If args[1] is some directory, we go to the specified directory
            else{
                chdir(args1.c_str());
                memcpy(prevPath, currPath,200);
                getcwd(currPath,200);
            }
        }
        return;
    }

    /* ------------------- TODO --------------------- */
    // for every command:
    //      call pipe(fds[2]) to make the pipe
    //      fork() <- already written
    //      In the child, redirect stdout. In the parent, redirect stdin
    // add checks for first/last command
    for (size_t i = 0; i < tknr.commands.size(); i++){

        // Create pipe:
        int pipeFds[2];
        pipe(pipeFds);

        // fork to create child
        pid_t pid = fork();
        if (pid < 0) {  // error check
            perror("fork");
            exit(2);
        }

        /* ------------------- TODO --------------------- */
        // add check for background process
        // add pid to vector if isBackground() returns true and don't waitpid() in parent

        if (pid == 0) {  // if child, exec to run command 

            // If we are not in the last command, we want to redirect our output to the next command using pipe:
            if (i != tknr.commands.size()-1){
                dup2(pipeFds[1], STDOUT_FILENO);
            }

            // Close the read end of the pipe on the child side.
            close(pipeFds[0]);

            /* ------------------- TODO --------------------- */
            // Implement multiple arguments  
            // iterate over args of current command to make an argument char* arr
            char ** args = new char*[tknr.commands.at(i)->args.size() + 1];

            for (size_t j = 0; j < tknr.commands.at(i)->args.size(); j++){
                args[j] = (char*)tknr.commands.at(i)->args[j].c_str();
            }

            args[tknr.commands.at(i)->args.size()] = nullptr;

            //char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};

            /* ------------------- TODO --------------------- */
            // if current command is redirected, then open file and dup2
            // redirect std(in/out) appropriately
            int inputFD;
            if (tknr.commands.at(i)->hasInput()){
                inputFD = open(tknr.commands.at(i)->in_file.c_str(), O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                if (inputFD < 0){
                    cerr << "File " << tknr.commands.at(i)->in_file << " cannot be open for reading." << endl;
                    delete [] args;
                    exit(2);
                }else{
                    dup2(inputFD, 0);
                }
            }
            if (tknr.commands.at(i)->hasOutput()){
                inputFD = open(tknr.commands.at(i)->out_file.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                dup2(inputFD, 1);
            }

            if (execvp(args[0], args) < 0) {  // error check
                perror("execvp");
                exit(2);
            }
        }
        else {  // if parent, wait for child to finish
            int status = 0;

            if (i == tknr.commands.size()-1) { // last argument
                if (tknr.commands.at(i)->isBackground()){
                    background.push_back(pid);
                }else{
                    waitpid(pid, &status, 0);
                }
            }else{
                close(pipeFds[1]);
                if (tknr.commands.at(i)->isBackground()){
                    background.push_back(pid);
                }else{
                    // We redirect parent's stdin to pipe's read side so future chidren can read the current children's output
                    dup2(pipeFds[0],STDIN_FILENO);
                    waitpid(pid,&status,0);
                }
            }
        }
    }

    /* ------------------- TODO --------------------- */
    // restore the original stdin/stdout
    dup2(oldIn, 0);
    dup2(oldOut, 1);
}

string replaceDollarSign(string str, size_t start, size_t end, string fileName){
    string newStr, s;

    for (size_t i = 0; i < start; i++){
        newStr.push_back(str.at(i));
    }

    ifstream inputFile;
    inputFile.open(fileName);

    while (!inputFile.eof()){
        getline(inputFile,s);
        newStr = newStr+s;
    }

    for (size_t i = end; i < str.size(); i++){
        newStr.push_back(str.at(i));
    }

    return newStr;
}

string commandsInArg(string& arg){
    bool insideQuotations = false;
    bool prevMoneySign = false;
    string runMe;
    
    for (size_t i = 0; i < arg.size(); i++){
        if (arg.at(i) == '\''){
            insideQuotations = !insideQuotations;
            continue;
        }
        if (arg.at(i) == '$'){
            prevMoneySign = true;
        }else{
            if (prevMoneySign){
                if (arg.at(i) == '('){
                    size_t j = i+1;

                    while (j < arg.size()){
                        if (arg.at(j) == ')'){
                            break;
                        }
                        runMe.push_back(arg.at(j));
                        j++;
                    }

                    runMe = runMe + " > output.txt";
                    runLine(runMe);

                    arg = replaceDollarSign(arg, i-1, j+1, "output.txt");

                    runLine("rm output.txt");

                    i = 0;
                    prevMoneySign = false;
                }else{
                    prevMoneySign = false;
                }
            }
        }
    }
    return arg;
}

int main () {

    // store the current directory
    getcwd(currPath,200);
    memcpy(prevPath,currPath,200);

    char * timeBuf = new char[50];

    // Store original file descriptors
    // Bonus content:
    map<string, string> variables;
    vector<string> prevInputs;

    for (;;) {

        /* ---------------- TODO --------------------- */
        // implement date/time
        time_t currTime = time(NULL);
        struct tm *local = localtime(&currTime);
        strftime(timeBuf, 50, "%b %e %T ", local);

        // implement username with getenv("USER")
        string username = getenv("USER");

        // implement currdir with getcwd()
        getcwd(currPath,200);

        cout << YELLOW << timeBuf << username << ':' << currPath << " reBASH$" << NC << " ";
        
        // get user inputted command
        string input;
        std::getline(cin, input);
        prevInputs.push_back(input);

        /* ---------------- TODO --------------------- */
        // implement iteration over vector of bg PID (vector should be declared outside loop)
        // Use waitpid() - using flag for non-blocking
        
        for (size_t i = 0; i < background.size(); i++){
            int status = 0;
            pid_t currProcess = background.at(i);
            pid_t s = waitpid(currProcess, &status, WNOHANG);
            
            if (s > 0){
                background.erase(background.begin()+i);
                i = i-1;
            }
        }

        if ( lowerCase(removeWhitespace(input)) == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        if (removeWhitespace(input).empty()){
            continue;
        }

        string runme = commandsInArg(input);
        
        if(runme != "") {
            runLine(runme);
        }
        prevInputs.push_back(runme);
    }
    // Check if any variables are declared/can be replaced on the input:
    delete [] currPath;
    delete [] timeBuf;
    delete [] prevPath;
}
