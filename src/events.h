#pragma once

#include "events_int.h"

#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>

static constexpr std::size_t MAX_UNQUEUE_EVENTS_DEFAULT = -1;

class EventLoop;
using EventLoopPtr = std::shared_ptr<EventLoop>;

template <typename Result>
class SlotHolder;

template <typename Result>
class SlotDescriptor {
  friend SlotHolder<Result>;

public:
  using ResultType = Result;

  SlotDescriptor() = default;

  template <typename... Args>
  _NS_Events::Thenable<Result>& emit(Args&&... args);

private:
  SlotDescriptor(EventLoopPtr event_loop, _NS_Events::EventDescriptor descriptor)
    : _event_loop(std::move(event_loop)), _descriptor(descriptor) {
  }

  EventLoopPtr _event_loop;
  _NS_Events::EventDescriptor _descriptor = _NS_Events::UNDEFINED_EVENT;
};

template <typename Result>
class SlotHolder {
  friend EventLoop;

public:
  SlotHolder(EventLoopPtr event_loop, _NS_Events::EventDescriptor descriptor, _NS_Events::SlotId id)
      : _event_loop(std::move(event_loop)), _descriptor(descriptor), _id(id) {
  }

  ~SlotHolder();

  SlotDescriptor<Result> descriptor() const {
    return {this->_event_loop, this->_descriptor};
  }

private:
  EventLoopPtr _event_loop;
  _NS_Events::EventDescriptor _descriptor;
  _NS_Events::SlotId _id;
};

template <typename Result>
using SlotHolderPtr = std::unique_ptr<SlotHolder<Result>>;

class EventLoop : public std::enable_shared_from_this<EventLoop> {
  template <typename Result>
  friend class SlotDescriptor;

  template <typename Result>
  friend class SlotHolder;

  using Slot = _NS_Events::Slot;
  using Signal = _NS_Events::Signal;

  using SlotId = _NS_Events::SlotId;
  using EventDescriptor = _NS_Events::EventDescriptor;

public:
  static EventLoopPtr make();

  EventLoop(std::size_t max_unqueue_events = MAX_UNQUEUE_EVENTS_DEFAULT);

  using Func = std::function<void()>;
  void repeat(Func);
  void post(Func);

  template <typename F>
  auto listen(F func) -> SlotHolderPtr<typename decltype(std::function(func))::result_type> {
    using Result = typename decltype(std::function(func))::result_type;
    auto [descriptor, slot_id] = this->listen(Slot(std::move(func)));
    return std::make_unique<SlotHolder<Result>>(this->shared_from_this(), descriptor, slot_id);
  }

  template <typename Result>
  void unlisten(const SlotHolderPtr<Result>& holder) {
    this->unlisten(holder->_id);
  }

  void start();

private:

  const std::size_t _max_unqueue_events;

  std::list<Func> _repeated_jobs;
  std::queue<Func> _onetime_jobs;

  EventDescriptor _next_descriptor = _NS_Events::UNDEFINED_EVENT + 1;
  SlotId _next_slot_id = 0;
  std::unordered_map<SlotId, _NS_Events::Slot> _slot_registry;
  std::unordered_map<EventDescriptor, SlotId> _slots;

  std::queue<_NS_Events::Signal> _events;

  void emit(Signal);

  std::pair<EventDescriptor, SlotId> listen(Slot slot);
  void unlisten(SlotId);
};

template <typename Result>
template <typename... Args>
_NS_Events::Thenable<Result>& SlotDescriptor<Result>::emit(Args&&... args) {
  if (this->_descriptor == _NS_Events::UNDEFINED_EVENT) {
    throw std::runtime_error("Emit unconnected signal");
    // TODO make meaningful return, signals should be able emitted nowhere
  }

  auto [signal, thenable] = _NS_Events::Signal::make<Result>(this->_descriptor, std::forward<Args>(args)...);
  this->_event_loop->emit(std::move(signal));
  return *thenable;
}

template <typename Result>
SlotHolder<Result>::~SlotHolder() {
  this->_event_loop->unlisten(this->_id);
}