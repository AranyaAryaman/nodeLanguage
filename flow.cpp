#include <iostream>
#include <unordered_map>
#include <vector>
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

void concatenateNodes(const vector<string>& parts);

map<string, Node> nodes;
map<string, Pipe> pipes;
map<string, vector<string> > concatenates;

// Function to parse the flow file
void parseFlowFile(const string& filename) {
    ifstream file(filename);
    string line;
    
    while (getline(file, line)) {
        istringstream iss(line);
        string key;
        iss >> key;
        string temp = key.substr(0, 4);

        if (temp == "node") {
            string nodeName, exec;
            size_t pos = key.find("=");
            nodeName = key.substr(pos + 1);
            getline(file, exec); 
            pos = exec.find("="); 
            nodes[nodeName].command = exec.substr(pos + 1);  
        } 
        else if (temp == "pipe") {
            string pipeName, pipeFrom, pipeTo;
            size_t pos = key.find("=");
            pipeName = key.substr(pos + 1);
            getline(file, pipeFrom);
            pos = pipeFrom.find("=");
            pipes[pipeName].from = pipeFrom.substr(pos + 1);
            getline(file, pipeTo);
            pos = pipeTo.find("=");
            pipes[pipeName].to = pipeTo.substr(pos + 1);
        }
        else if (temp == "conc") {
            string concatName, parts;
            vector<string> toPush;
            size_t pos = key.find("=");
            concatName = key.substr(pos + 1);
            getline(file, parts);
            int numParts = stoi(parts.substr(parts.find("=") + 1));
            string temp;
            for (int i = 0; i < numParts; i++) {
                getline(file, temp);
                pos = temp.find("=");
                toPush.push_back(temp.substr(pos + 1));
            }
            concatenates[concatName] = toPush;
        }
    }
}


void executeNode(const Node& node) {
    vector<char*> args;
    istringstream iss(node.command);
    string arg;

    while (iss >> arg) {
        args.push_back(strdup(arg.c_str()));
    }
    args.push_back(nullptr);

    execvp(args[0], args.data());
    perror("execvp failed");
    exit(EXIT_FAILURE); 
}


void runPipe(Pipe& pipe) {
    int pipefds[2];
    ::pipe(pipefds);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefds[0]);
        dup2(pipefds[1], STDOUT_FILENO);  
        close(pipefds[1]);
        if (nodes.find(pipe.from) != nodes.end()) {
            executeNode(nodes[pipe.from]); 
        } else if (pipes.find(pipe.from) != pipes.end()) {
            runPipe(pipes[pipe.from]);  
        } else if (concatenates.find(pipe.from) != concatenates.end()) {
            concatenateNodes(concatenates[pipe.from]);  
        }
    } else {
        close(pipefds[1]);
        dup2(pipefds[0], STDIN_FILENO);  
        close(pipefds[0]);
        if (nodes.find(pipe.to) != nodes.end()) {
            executeNode(nodes[pipe.to]);  
        } else if (pipes.find(pipe.to) != pipes.end()) {
            runPipe(pipes[pipe.to]);  
        } else if (concatenates.find(pipe.to) != concatenates.end()) {
            concatenateNodes(concatenates[pipe.to]);  
        }
        wait(nullptr);  
    }
}

// Function to handle the execution of a concatenate
void concatenateNodes(const vector<string>& parts) {
    int pipefds[2];
    pid_t pid;

    for (size_t i = 0; i < parts.size(); ++i) {
        if (i < parts.size() - 1) pipe(pipefds);

        pid = fork();
        if (pid == 0) {
            // Child process: execute the current part
            if (i > 0) dup2(pipefds[0], STDIN_FILENO);  // Get input from previous process
            if (i < parts.size() - 1) dup2(pipefds[1], STDOUT_FILENO);  // Output to next process

            close(pipefds[0]);
            close(pipefds[1]);

            if (nodes.find(parts[i]) != nodes.end()) {
                executeNode(nodes[parts[i]]);  // It's a node
            } else if (pipes.find(parts[i]) != pipes.end()) {
                runPipe(pipes[parts[i]]);  // It's a pipe
            } else if (concatenates.find(parts[i]) != concatenates.end()) {
                concatenateNodes(concatenates[parts[i]]);  // It's a concatenate
            }
        } else {
            // Parent process: close pipes and wait
            if (i > 0) close(pipefds[0]);
            if (i < parts.size() - 1) close(pipefds[1]);
            wait(nullptr);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <flow file> <action>" << endl;
        return 1;
    }

    string flowFile = argv[1];
    string action = argv[2];

    parseFlowFile(flowFile);

    if (pipes.find(action) != pipes.end()) {
        runPipe(pipes[action]);
    } else if (concatenates.find(action) != concatenates.end()) {
        concatenateNodes(concatenates[action]);
    } else if (nodes.find(action) != nodes.end()) {
        executeNode(nodes[action]);
    } else {
        cerr << "Unknown action: " << action << endl;
    }

    return 0;
}
