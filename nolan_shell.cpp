#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>

using namespace std;

// Global variables for job management
map<int, pid_t> jobs;   // Job ID to process ID (PID) mapping
map<int, string> jobNames; // Job ID to Command mapping
int job_counter = 1;    // Job ID counter
pid_t current_foreground_pid = -1; // PID of the current foreground job

void signalHandler(int signum);
void parseCommand(const string& input, vector<string>& args);
void executeCommand(const vector<string>& args);
void launchJob(const vector<string>& args, bool inBackground);
void listJobs();
void handleFg(int jobID);
void handleBg(int jobID);

int main() {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTSTP, signalHandler);

    while (true) {
        cout << "shell> ";
        string input;
        getline(cin, input);

        vector<string> args;
        parseCommand(input, args);

        if (args.empty()) continue;
        if (args[0] == "exit") break; // Exit the shell
        if (args[0] == "jobs") {
            listJobs();
            continue;
        }
        if (args[0] == "fg") {
            handleFg(stoi(args[1]));
            continue;
        }
        if (args[0] == "bg") {
            handleBg(stoi(args[1]));
            continue;
        }

        bool inBackground = (args.back() == "&");
        if (inBackground) args.pop_back(); // Remove '&' from command

        launchJob(args, inBackground);
    }

    return 0;
}

void parseCommand(const string& input, vector<string>& args) {
    istringstream iss(input);
    string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
}

void launchJob(const vector<string>& args, bool inBackground) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        vector<char*> execArgs;
        for (const auto& arg : args) 
            execArgs.push_back(const_cast<char*>(arg.c_str()));
        execArgs.push_back(nullptr);

        execvp(execArgs[0], execArgs.data());
        cerr << "Error: command not found\n";
        exit(1);
    } else if (pid > 0) {
        // Parent process
        if (inBackground) {
            jobs[job_counter] = pid; // Track the job by its PID
            jobNames[job_counter] = args[0]; // Store the job name
            cout << "Job [" << job_counter << "] started in background: " << args[0] << "\n";
            job_counter++;
        } else {
            current_foreground_pid = pid;
            waitpid(pid, nullptr, 0); // Wait for foreground job to finish
            current_foreground_pid = -1; // Reset after the job is finished
        }
    } else {
        cerr << "Fork failed.\n";
    }
}

void signalHandler(int signum) {
    if (current_foreground_pid > 0) {
        if (signum == SIGINT) {
            cout << "\nInterrupt signal received (Ctrl+C). Stopping foreground job.\n";
            kill(current_foreground_pid, SIGINT); // Send SIGINT to the foreground job
        } else if (signum == SIGTSTP) {
            cout << "\nStop signal received (Ctrl+Z). Suspending foreground job.\n";
            kill(current_foreground_pid, SIGTSTP); // Send SIGTSTP to the foreground job
        }
    }
}

void listJobs() {
    cout << "Current jobs:\n";
    for (const auto& job : jobs) {
        cout << "[" << job.first << "] " << jobNames[job.first] << " (PID: " << job.second << ")\n";
    }
}

void handleFg(int jobID) {
    if (jobs.count(jobID)) {
        cout << "Bringing job [" << jobID << "] to the foreground: " << jobNames[jobID] << "\n";
        current_foreground_pid = jobs[jobID]; // Set the job as the current foreground process
        kill(jobs[jobID], SIGCONT); // Send a continue signal to the job
        waitpid(jobs[jobID], nullptr, 0); // Wait for the job to complete
        jobs.erase(jobID); // Remove from jobs list after completion
        jobNames.erase(jobID);
        current_foreground_pid = -1; // Reset after the job is finished
    } else {
        cerr << "Job ID not found.\n";
    }
}

void handleBg(int jobID) {
    if (jobs.count(jobID)) {
        cout << "Running job [" << jobID << "] in the background: " << jobNames[jobID] << "\n";
        kill(jobs[jobID], SIGCONT); // Send a continue signal to the job
    } else {
        cerr << "Job ID not found.\n";
    }
}