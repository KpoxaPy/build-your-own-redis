#pragma once

#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

static constexpr std::size_t MAX_UNQUEUE_EVENTS_DEFAULT = -1;

class EventLoop;
using EventLoopPtr = std::shared_ptr<EventLoop>;

class EventLoop : public std::enable_shared_from_this<EventLoop> {
public:
  using Func = std::function<void()>;
  using Clock = std::chrono::steady_clock;
  using Timepoint = Clock::time_point;

private:
  template <typename Slot>
  struct SlotEventWrapper {
    using ProvidedFunc = Slot::ProvidedFunc;

    std::weak_ptr<Slot> slot_wptr;
    std::shared_ptr<EventLoop> loop;

    SlotEventWrapper(std::shared_ptr<EventLoop> loop, const std::shared_ptr<Slot>& slot_ptr) {
      this->loop = std::move(loop);
      this->slot_wptr = slot_ptr;
    }

    template <typename... Args>
    void call(Args&&... args) const {
      this->loop->_events.emplace([wptr = this->slot_wptr, args...]() {
        if (auto ptr = wptr.lock()) {
          ptr->call(std::move<Args>(args)...);
        }
      });
    }
  };

  struct JobWrapper {
    Func func;
    bool is_valid = false;
    std::optional<Timepoint> fire_time;

    JobWrapper(Func func) {
      this->func = std::move(func);
      this->is_valid = true;
    }

    void call() const {
      if (this->is_valid) {
        this->func();
      }
    }
  };
  using JobWrapperPtr = std::shared_ptr<JobWrapper>;

public:
  class JobHandle {
    friend EventLoop;

  public:
    JobHandle() = default;
    JobHandle(JobHandle&& other) = default;
    JobHandle& operator=(JobHandle&& other) = default;

    ~JobHandle() {
      this->invalidate();
    }

    void invalidate() {
      if (auto ptr = this->job.lock()) {
        ptr->is_valid = false;
      }
    }

  private:
    std::weak_ptr<JobWrapper> job;

    JobHandle(const JobWrapperPtr& ptr) {
      this->job = ptr;
    }
  };


  static EventLoopPtr make();

  EventLoop(std::size_t max_unqueue_events = MAX_UNQUEUE_EVENTS_DEFAULT);

  template <typename Signal, typename Slot>
  void connect(Signal& signal, const std::shared_ptr<Slot>& slot_ptr) {
    signal.connect(SlotEventWrapper(slot_ptr));
  }

  JobHandle post(Func);
  JobHandle repeat(Func);
  JobHandle set_timeout(std::size_t ms, Func);

  void start();

private:
  const std::size_t _max_unqueue_events;

  std::queue<JobWrapperPtr> _onetime_jobs;
  std::list<JobWrapperPtr> _repeated_jobs;
  std::list<JobWrapperPtr> _timeout_jobs; // stupid simple. better to have jobs sorted by fire time
  std::queue<Func> _events;
};
