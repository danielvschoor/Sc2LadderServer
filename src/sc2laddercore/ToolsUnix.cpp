#if defined(__unix__) || defined(__APPLE__)

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Tools.h"
#include "Types.h"

namespace {

int RedirectOutput(const BotConfig &Agent, int SrcFD, const char *LogFile)
{
    int logFD = open(LogFile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (logFD < 0) {
        std::cerr << Agent.BotName +
            ": Failed to create a log file, error: " +
            strerror(errno) << std::endl;
            return logFD;
    }

    int ret = dup2(logFD, SrcFD);
    if (ret < 0) {
        std::cerr << Agent.BotName +
            ": Can't redirect output to a log file, error: " +
            strerror(errno) << std::endl;
        return ret;
    }

    close(logFD);
    return ret;
}

} // namespace

void StartBotProcess(const BotConfig &Agent, const std::string &CommandLine, unsigned long *ProcessId)
{
    pid_t pID = fork();

    if (pID < 0)
    {
        std::cerr << Agent.BotName + ": Can't fork the bot process, error: " +
            strerror(errno) << std::endl;
        return;
    }

    if (pID == 0) // child
    {
        int ret = chdir(Agent.RootPath.c_str());
        if (ret < 0) {
            std::cerr << Agent.BotName +
                ": Can't change working directory to " + Agent.RootPath +
                ", error: " + strerror(errno) << std::endl;
            exit(errno);
        }

        if (RedirectOutput(Agent, STDERR_FILENO, "stderr.log") < 0)
            exit(errno);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);

        std::vector<char*> unix_cmd;
        std::istringstream stream(CommandLine);
        std::istream_iterator<std::string> begin(stream), end;
        std::vector<std::string> tokens(begin, end);
        for (const auto& i : tokens)
            unix_cmd.push_back(const_cast<char*>(i.c_str()));

        // FIXME (alkurbatov): Unfortunately, the cmdline uses relative path.
        // This hack is needed because we have to change the working directory
        // before calling to exec.
        if (Agent.Type == BinaryCpp)
            unix_cmd[0] = const_cast<char*>(Agent.FileName.c_str());

        unix_cmd.push_back(NULL);

        // NOTE (alkurbatov): For the Python bots we need to search in the PATH
        // for the interpreter.
        if (Agent.Type == Python || Agent.Type == Wine || Agent.Type == Mono || Agent.Type == DotNetCore)
            ret = execvp(unix_cmd.front(), &unix_cmd.front());
        else
            ret = execv(unix_cmd.front(), &unix_cmd.front());

        if (ret < 0)
        {
            std::cerr << Agent.BotName + ": Failed to execute '" + CommandLine +
                "', error: " + strerror(errno) << std::endl;
            exit(errno);
        }

        exit(0);
    }

    // parent
    *ProcessId = pID;

    int exit_status = 0;
    int ret = waitpid(pID, &exit_status, 0);
    if (ret < 0) {
        std::cerr << Agent.BotName +
            ": Can't wait for the child process, error:" +
            strerror(errno) << std::endl;
    }
}

void StartExternalProcess(const std::string CommandLine)
{
	execve(CommandLine.c_str(), NULL, NULL);
}

void SleepFor(int seconds)
{
    sleep(seconds);
}

void KillSc2Process(unsigned long pid)
{
    int ret = kill(pid, SIGKILL);
    if (ret < 0)
    {
        std::cerr << std::string("Failed to send SIGKILL, error:") +
            strerror(errno) << std::endl;
    }
}

bool MoveReplayFile(const char* lpExistingFileName, const  char* lpNewFileName)
{
    int ret = rename(lpExistingFileName, lpNewFileName);
    if (ret < 0)
    {
        std::cerr << std::string("Failed to move a replay file, error:") +
            strerror(errno) << std::endl;
    }

    return ret == 0;
}

#endif