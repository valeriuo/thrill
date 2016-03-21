/*******************************************************************************
 * thrill/common/schedule_thread.cpp
 *
 * A thread running a set of tasks scheduled at regular time intervals. Used in
 * Thrill for creating profiles of CPU usage, memory, etc.
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2016 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <thrill/common/schedule_thread.hpp>

namespace thrill {
namespace common {

/******************************************************************************/
// ScheduleThread

ScheduleThread::ScheduleThread() {
    thread_ = std::thread(&ScheduleThread::Worker, this);
}

ScheduleThread::~ScheduleThread() {
    std::unique_lock<std::timed_mutex> lock(mutex_);
    terminate_ = true;
    cv_.notify_one();
    lock.unlock();
    thread_.join();

    for (Timer& t : tasks_.container()) {
        if (t.own_task)
            delete t.task;
    }
}

void ScheduleThread::Worker() {
    std::unique_lock<std::timed_mutex> lock(mutex_);

    steady_clock::time_point tm = steady_clock::now();

    while (!terminate_)
    {
        if (tasks_.empty()) {
            cv_.wait(mutex_, [this]() { return !tasks_.empty(); });
            continue;
        }

        while (tasks_.top().next_timeout <= tm) {
            const Timer& top = tasks_.top();
            top.task->RunTask(tm);

            // requeue timeout event again.
            tasks_.emplace(top.next_timeout + top.period,
                           top.period, top.task, top.own_task);
            tasks_.pop();
        }

        cv_.wait_until(mutex_, tasks_.top().next_timeout);
        tm = steady_clock::now();
    }
}

/******************************************************************************/
// ScheduleThread::Timer

ScheduleThread::Timer::Timer(const steady_clock::time_point& _next_timeout,
                             const milliseconds& _period,
                             ScheduleTask* _task, bool _own_task)
    : next_timeout(_next_timeout), period(_period),
      task(_task), own_task(_own_task) { }

bool ScheduleThread::Timer::operator < (const Timer& b) const {
    return next_timeout > b.next_timeout;
}

} // namespace common
} // namespace thrill

/******************************************************************************/
