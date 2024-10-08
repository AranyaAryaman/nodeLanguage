#include<iostream>
#include<unordered_map>
#include<unordered_set>
#include<vector>
#include<list>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

using namespace std;

struct Node {
    string command;
    vector<string> args;
};

struct Pipe {
    string from;
    string to;
};

map<string, Node> nodes;
map<string, Pipe> pipes;

void parseFlowFile(const string& filename) {
    ifstream file(filename);
    string line;
    
    while (getline(file, line)) {
        istringstream iss(line);
        string key;
        iss >> key;
        string temp = key.substr(0,4);
        if (temp == "node") {
            string nodeName, exec;
            size_t pos = key.find("=");
            nodeName = key.substr(pos+1);
            getline(file, exec); 
            pos = exec.find("="); 
            nodes[nodeName].command = exec.substr(pos+1);  
        } 
        else if (temp == "pipe") {
            string pipeName, pipeFrom, pipeTo;
            Pipe pipe;
            size_t pos = key.find("=");
            pipeName = key.substr(pos+1);
            getline(file,pipeFrom);
            pos = pipeFrom.find("=");
            pipes[pipeName].from = pipeFrom.substr(pos+1);
            getline(file,pipeTo);
            pos = pipeTo.find("=");
            pipes[pipeName].to = pipeTo.substr(pos+1);
        }
    }
}


void executeNode(const Node& node) {
    // Prepare the command and arguments for execvp
    vector<char*> args;
    istringstream iss(node.command);
    string arg;
    while (iss >> arg) {
        args.push_back(strdup(arg.c_str()));
    }
    args.push_back(nullptr);

    execvp(args[0], args.data());  // Replace current process with command
    perror("execvp failed");
}

void runFlow(const string& action) {
    // Pipe creation
    int pipefds[2];
    pipe(pipefds);  // Create a pipe

    pid_t pid = fork();
    if (pid == 0) {
        // In child process: execute the first node
        close(pipefds[0]);  // Close reading end
        dup2(pipefds[1], STDOUT_FILENO);  // Redirect stdout to pipe
        close(pipefds[1]);
        executeNode(nodes[pipes[0].from]);
    } else {
        // In parent process: execute the second node
        close(pipefds[1]);  // Close writing end
        dup2(pipefds[0], STDIN_FILENO);  // Redirect stdin to pipe
        close(pipefds[0]);
        executeNode(nodes[pipes[0].to]);
        wait(nullptr);  // Wait for child process to finish
    }
}

void concatenateNodes(const vector<string>& parts) {
    int pipefds[2];
    pipe(pipefds);

    for (size_t i = 0; i < parts.size(); ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(pipefds[0], STDIN_FILENO);  // Redirect input from previous process
            }
            if (i < parts.size() - 1) {
                dup2(pipefds[1], STDOUT_FILENO);  // Redirect output to next process
            }
            close(pipefds[0]);
            close(pipefds[1]);

            executeNode(nodes[parts[i]]);
        } else {
            // Parent process waits for the child
            wait(nullptr);
        }
    }
}

int main(int argc, char* argv[]){
    
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <flow file> <action>" << endl;
        return 1;
    }

    string flowFile = argv[1];
    string action = argv[2];

    parseFlowFile(flowFile);

    // for(auto& node:nodes){
    //     cout<<node.first<<" "<<node.second.command<<"\n";
    //     char* cmd = (char*) malloc(sizeof(char) * node.second.command.length());
    //     for(int i=0;i<node.second.command.length();i++){
    //         cmd[i]=node.second.command[i];
    //     }
    //     system(cmd);
    // }

    // for(auto& pipe:pipes){
    //     cout<<pipe.first<<" "<<pipe.second.from<<" "<<pipe.second.to<<"\n";
    // }
    cout<<pipes.find(action)->second.from<<" "<<pipes.find(action)->second.to;
    string str = nodes.find(pipes.find(action)->second.from)->second.command + " | " +  nodes.find(pipes.find(action)->second.to)->second.command;
    char* cmd = (char*) malloc(sizeof(char) * str.length());
    for(int i=0;i<str.length();i++){
        cmd[i]=str[i];
    }
    // cout<<cmd;
    system(cmd);
    
    // if (action == "doit") {
    //     system("")
    // } 
    // else if (action == "concatenate") {
    //     vector<string> parts;
    //     parts.push_back("cat_foo");
    //     parts.push_back("foo_to_fuu");  
    //     concatenateNodes(parts);
    // }

    return 0;

}