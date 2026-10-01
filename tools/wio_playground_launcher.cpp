#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef WIO_PLAYGROUND_COMPILER_PATH
#error "WIO_PLAYGROUND_COMPILER_PATH must identify the configured Wio executable."
#endif

#ifndef WIO_PLAYGROUND_MANIFEST_PATH
#error "WIO_PLAYGROUND_MANIFEST_PATH must identify the playground manifest."
#endif

#ifndef WIO_PLAYGROUND_SOURCE_ROOT
#error "WIO_PLAYGROUND_SOURCE_ROOT must identify the Wio source root."
#endif

int main()
{
#ifdef _WIN32
    if (_putenv_s("WIO_ROOT", WIO_PLAYGROUND_SOURCE_ROOT) != 0)
#else
    if (setenv("WIO_ROOT", WIO_PLAYGROUND_SOURCE_ROOT, 1) != 0)
#endif
    {
        std::cerr << "Unable to configure WIO_ROOT for the playground.\n";
        return 1;
    }

    std::cout << "Running native Wio playground..." << std::endl;
#ifdef _WIN32
    const intptr_t result = _spawnl(_P_WAIT, WIO_PLAYGROUND_COMPILER_PATH, WIO_PLAYGROUND_COMPILER_PATH, "project",
                                    "run", "--project", WIO_PLAYGROUND_MANIFEST_PATH, nullptr);
    if (result == -1)
    {
        std::cerr << "Unable to launch the Wio playground process.\n";
        return 1;
    }
    return static_cast<int>(result);
#else
    const pid_t child = fork();
    if (child == 0)
    {
        execl(WIO_PLAYGROUND_COMPILER_PATH, WIO_PLAYGROUND_COMPILER_PATH, "project", "run", "--project",
              WIO_PLAYGROUND_MANIFEST_PATH, nullptr);
        _exit(127);
    }
    if (child < 0)
    {
        std::cerr << "Unable to launch the Wio playground process.\n";
        return 1;
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0)
        return 1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
