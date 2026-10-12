#pragma once

#include "std_async.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace vm_task_inputs
{
    struct Point
    {
        std::int32_t x;
    };
    inline wio::runtime::AsyncTask<std::int32_t>
    RelayTask(std::function<wio::runtime::AsyncTask<std::int32_t>(wio::runtime::AsyncTask<std::int32_t>)> callback,
              wio::runtime::AsyncTask<std::int32_t> task)
    {
        return callback(std::move(task));
    }

    inline std::optional<wio::runtime::AsyncTask<std::int32_t>> kept;
    inline void Keep(wio::runtime::AsyncTask<std::int32_t> task)
    {
        kept = std::move(task);
    }
    inline wio::runtime::AsyncTask<std::int32_t> ReturnKept()
    {
        return kept.value();
    }
    inline bool KeptCancelled()
    {
        return kept && kept->IsCancelled() && kept->IsReady();
    }
    inline void DropKept()
    {
        kept.reset();
    }

    template <class T> wio::runtime::AsyncTask<T> Forward(wio::runtime::AsyncTask<T> task)
    {
        if constexpr (std::is_void_v<T>)
        {
            co_await task;
            co_return;
        }
        else
            co_return co_await task;
    }

    inline std::vector<wio::runtime::AsyncTask<std::int32_t>>
    EchoTasks(std::vector<wio::runtime::AsyncTask<std::int32_t>> tasks)
    {
        return tasks;
    }

    inline void Print(std::string message)
    {
        std::cout << message << std::endl;
    }

    inline std::int32_t BlockForward(wio::runtime::AsyncTask<std::int32_t> task)
    {
        // Wait on a composed C++ task, not directly on the decoded host task.
        return Forward(std::move(task)).Get();
    }
} // namespace vm_task_inputs
