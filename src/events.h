#pragma once

#include <functional>
#include <list>
#include <memory>
#include <queue>

static constexpr std::size_t MAX_UNQUEUE_EVENTS_DEFAULT = -1;

class EventLoop;
using EventLoopPtr = std::shared_ptr<EventLoop>;

class EventLoop : public std::enable_shared_from_this<EventLoop> {
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

public:
  using Func = std::function<void()>;

  static EventLoopPtr make();

  EventLoop(std::size_t max_unqueue_events = MAX_UNQUEUE_EVENTS_DEFAULT);

  template <typename Signal, typename Slot>
  void connect(Signal& signal, const std::shared_ptr<Slot>& slot_ptr) {
    signal.connect(SlotEventWrapper(slot_ptr));
  }

  void post(Func);
  void repeat(Func);

  void start();

private:
  const std::size_t _max_unqueue_events;

  std::queue<Func> _onetime_jobs;
  std::list<Func> _repeated_jobs;
  std::queue<Func> _events;
};
