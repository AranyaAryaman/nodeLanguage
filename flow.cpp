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

struct Node
{
    string command;
    vector<string> args;
};

struct Pipe
{
    string from;
    string to;
};

struct StdErr
{
    string from;
};

map<string, Node> nodes;
map<string, Pipe> pipes;
map<string, vector<string> > concatenates;
map<string, StdErr> stdErrorNodes;

void concatenateNodes(const vector<string> &parts);

void parseFlowFile(const string &filename)
{
    ifstream file(filename);
    string line;

    while (getline(file, line))
    {
        istringstream iss(line);
        string key;
        iss >> key;
        string temp = key.substr(0, 4);

        if (temp == "node")
        {
            string nodeName, exec;
            size_t pos = key.find("=");
            nodeName = key.substr(pos + 1);
            getline(file, exec);
            pos = exec.find("=");
            string cmd = exec.substr(pos + 1);
            // cout<<cmd<<endl;
            stringstream cmdStream(cmd);
            string parsedCmd;
            char ch;
            while (cmdStream >> noskipws >> ch) // 'noskipws' to ensure whitespace is not skipped
            {
                if (ch != '\'' && ch != '"') // Ignore ' and "
                {
                    parsedCmd += ch;
                }
            }
            cout<<parsedCmd<<endl;
            nodes[nodeName].command = parsedCmd;
        }
        else if (temp == "pipe")
        {
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
        else if (temp == "conc")
        {
            string concatName, parts;
            vector<string> toPush;
            size_t pos = key.find("=");
            concatName = key.substr(pos + 1);
            getline(file, parts);
            int numParts = stoi(parts.substr(parts.find("=") + 1));
            string temp;
            for (int i = 0; i < numParts; i++)
            {
                getline(file, temp);
                pos = temp.find("=");
                toPush.push_back(temp.substr(pos + 1));
            }
            concatenates[concatName] = toPush;
        }
        else if (temp == "stde")
        {
            string stderrName, stderrFrom;
            size_t pos = key.find("=");
            stderrName = key.substr(pos + 1);
            getline(file, stderrFrom);
            pos = stderrFrom.find("=");
            stdErrorNodes[stderrName].from = stderrFrom.substr(pos + 1);
        }
    }
}

void executeNode(const Node &node, bool redirectStderr = false)
{
    vector<char *> args;
    istringstream iss(node.command);
    string arg;

    while (iss >> arg)
    {
        args.push_back(strdup(arg.c_str()));
    }
    args.push_back(nullptr);

    if (redirectStderr)
    {
        dup2(STDOUT_FILENO, STDERR_FILENO);
    }

    execvp(args[0], args.data());
    perror("execvp failed");
    exit(EXIT_FAILURE);
}

void runPipe(Pipe &pipe)
{
    int pipefds[2];
    ::pipe(pipefds);

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefds[0]);

        dup2(pipefds[1], STDOUT_FILENO);

        close(pipefds[1]);
        if (nodes.find(pipe.from) != nodes.end())
        {
            executeNode(nodes[pipe.from]);
        }
        else if (pipes.find(pipe.from) != pipes.end())
        {
            runPipe(pipes[pipe.from]);
        }
        else if (concatenates.find(pipe.from) != concatenates.end())
        {
            concatenateNodes(concatenates[pipe.from]);
        }
        else if (stdErrorNodes.find(pipe.from) != stdErrorNodes.end())
        {
            if (nodes.find(stdErrorNodes[pipe.from].from) != nodes.end())
            {
                executeNode(nodes[stdErrorNodes[pipe.from].from], true);
            }
        }
    }
    else
    {
        close(pipefds[1]);
        dup2(pipefds[0], STDIN_FILENO);
        close(pipefds[0]);
        if (nodes.find(pipe.to) != nodes.end())
        {
            executeNode(nodes[pipe.to]);
        }
        else if (pipes.find(pipe.to) != pipes.end())
        {
            runPipe(pipes[pipe.to]);
        }
        else if (concatenates.find(pipe.to) != concatenates.end())
        {
            concatenateNodes(concatenates[pipe.to]);
        }
        else if (stdErrorNodes.find(pipe.to) != stdErrorNodes.end())
        {
            if (nodes.find(stdErrorNodes[pipe.to].from) != nodes.end())
            {
                executeNode(nodes[stdErrorNodes[pipe.to].from], true);
            }
        }
        wait(nullptr);
    }
}

void concatenateNodes(const vector<string> &parts)
{
    for (const string &part : parts)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            if (nodes.find(part) != nodes.end())
            {
                executeNode(nodes[part]);
            }
            else if (pipes.find(part) != pipes.end())
            {
                runPipe(pipes[part]);
            }
            else if (concatenates.find(part) != concatenates.end())
            {
                concatenateNodes(concatenates[part]);
            }
            exit(0);
        }
        else
        {
            wait(nullptr);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Usage: " << argv[0] << " <flow file> <action>" << endl;
        return 1;
    }

    string flowFile = argv[1];
    string action = argv[2];

    parseFlowFile(flowFile);

    if (pipes.find(action) != pipes.end())
    {
        runPipe(pipes[action]);
    }
    else if (concatenates.find(action) != concatenates.end())
    {
        concatenateNodes(concatenates[action]);
    }
    else if (nodes.find(action) != nodes.end())
    {
        executeNode(nodes[action]);
    }
    else
    {
        cerr << "Unknown action: " << action << endl;
    }

    return 0;
}
