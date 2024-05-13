#include "events.h"

#include <iostream>

EventLoopPtr EventLoop::make() {
  return std::make_shared<EventLoop>();
}

EventLoop::EventLoop(std::size_t max_unqueue_events)
  : _max_unqueue_events(max_unqueue_events)
{
}

EventLoop::JobHandle EventLoop::post(Func func) {
  auto job = std::make_shared<JobWrapper>(std::move(func));
  this->_onetime_jobs.push(job);
  return job;
}

EventLoop::JobHandle EventLoop::repeat(Func func) {
  auto job = std::make_shared<JobWrapper>(std::move(func));
  this->_repeated_jobs.push_back(job);
  return job;
}

EventLoop::JobHandle EventLoop::set_timeout(std::size_t ms, Func func) {
  auto job = std::make_shared<JobWrapper>(std::move(func));
  job->fire_time = Clock::now() + std::chrono::milliseconds{ms};
  this->_timeout_jobs.push_back(job);
  return job;
}

void EventLoop::start() {
  while (true) {
    while (this->_onetime_jobs.size() > 0) {
      try {
        auto job = std::move(this->_onetime_jobs.front());
        this->_onetime_jobs.pop();
        job->call();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while onetime job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    for (const auto& job: this->_repeated_jobs) {
      try {
        job->call();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while repeating job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    for (auto it = this->_timeout_jobs.begin(); it != this->_timeout_jobs.end();) {
      try {
        auto job = *it;
        if (Clock::now() >= job->fire_time.value()) {
          it = this->_timeout_jobs.erase(it);
          job->call();
        } else {
          ++it;
        }
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while timeout job: " << std::endl
          << exception.what() << std::endl;
      }
    }

    std::size_t unqueue_size = this->_max_unqueue_events;
    if (unqueue_size == 0 || unqueue_size > this->_events.size()) {
      unqueue_size = this->_events.size();
    }

    for (std::size_t i = 0; i < unqueue_size; ++i) {
      try {
        auto event = std::move(this->_events.front());
        this->_events.pop();
        event();
      } catch (const std::exception& exception) {
        std::cerr << "EventLoop caught exception while processing event: " << std::endl
                  << exception.what() << std::endl;
      }
    }
  }
}
