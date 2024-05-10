#pragma once

#include "utils.h"

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

class EventLoop;

namespace _NS_Events {

using SlotId = std::size_t;
using EventDescriptor = std::size_t;

static constexpr EventDescriptor UNDEFINED_EVENT = 0;

class Slot {
  struct Interface { 
    virtual ~Interface() = default;
    std::string demangled_slot_type;
  };

  template <typename F>
  struct Container : public Interface {
    F func;

    Container(F func) : func(func) {
      this->demangled_slot_type = demangled<Container<F>>();
    }
  };

  template <typename F>
  static auto makeContainer(F func) -> std::shared_ptr<Container<decltype(std::function(func))>> {
    return std::make_shared<Container<decltype(std::function(func))>>(func);
  }

public:
  template <typename F>
  Slot(F func)
    : ptr(_NS_Events::Slot::makeContainer(std::move(func)))
  {
  }

  template <typename R, typename... Args>
  R call(Args... args) {
    using ContainerType = Container<std::function<R(Args...)>>;

    if (ContainerType* f_ptr = dynamic_cast<ContainerType*>(this->ptr.get())) {
      return f_ptr->func(std::forward<Args>(args)...);
    } else {
      std::ostringstream ss;
      ss << "Mismatching types for slot:" << std::endl;
      ss << "expect: " << this->ptr->demangled_slot_type << std::endl;
      ss << "taken:  " << demangled<ContainerType>() << std::endl;
      throw std::runtime_error(ss.str());
    }
    return R{};
  }

private:
  std::shared_ptr<Interface> ptr;
};

template <typename Arg>
class Thenable;

namespace _NS_Thenable {
  template <typename _Arg>
  struct Interface {
    virtual ~Interface() = default;
    virtual void call(_Arg&& arg) = 0;
  };

  template <>
  struct Interface<void> {
    virtual ~Interface() = default;
    virtual void call() = 0;
  };

  template <typename Result, typename _Arg>
  struct ContainerBase {
    std::function<Result(_Arg)> func;
    std::shared_ptr<Thenable<Result>> next;
  };

  template <typename Result, typename _Arg>
  struct Container : public Interface<_Arg>, public ContainerBase<Result, _Arg> {
    void call(_Arg&& arg) override {
      if constexpr (std::is_void_v<Result>) {
        this->func(std::forward<_Arg>(arg));
        this->next->call();
      } else {
        this->next->call(this->func(std::forward<_Arg>(arg)));
      }
    }
  };

  template <typename Result>
  struct Container<Result, void> : public Interface<void>, public ContainerBase<Result, void> {
    void call() override {
      if constexpr (std::is_void_v<Result>) {
        this->func();
        this->next->call();
      } else {
        this->next->call(this->func());
      }
    }
  };
} // _NS_Thenable

template <typename Arg>
class ThenableBase {
public:
  template <typename F>
  auto then(F func) -> Thenable<typename decltype(std::function(func))::result_type>& {
    using Result = typename decltype(std::function(func))::result_type;
    auto ptr = std::make_shared<_NS_Thenable::Container<Result, Arg>>();
    ptr->func = func;
    ptr->next = std::make_shared<Thenable<Result>>();
    this->_ptr = ptr;
    return *(ptr->next);
  }

protected:
  std::shared_ptr<_NS_Thenable::Interface<Arg>> _ptr;
};

template <typename Arg>
class Thenable : public ThenableBase<Arg> {
public:
  void call(Arg&& arg) {
    if (!this->_ptr) {
      return;
    }
    this->_ptr->call(std::forward<Arg>(arg));
  }
};

template <>
class Thenable<void> : public ThenableBase<void> {
public:
  void call() {
    if (!this->_ptr) {
      return;
    }
    this->_ptr->call();
  }
};

class Signal {
  friend EventLoop;

public:
  template <typename R = void, typename... Args>
  static std::pair<Signal, std::shared_ptr<Thenable<R>>> make(EventDescriptor desc, Args&&... args) {
    auto ptr = std::make_shared<Container<R>>(std::forward<Args>(args)...);

    Signal signal;
    signal._descriptor = desc;
    signal._ptr = ptr;
    return {std::move(signal), ptr->thenable};
  }

  void deliver(Slot& slot) {
    this->_ptr->deliver(slot);
  }

private:
  struct Interface { 
    virtual ~Interface() = default;
    virtual void deliver(Slot& slot) = 0;
  };

  template <typename R>
  struct Container : public Interface {
    std::function<void(Slot&)> func;
    std::shared_ptr<Thenable<R>> thenable;

    template <typename... Args>
    Container(Args&&... args) {
      this->func = [this, args...](Slot& slot) {
        if constexpr (std::is_void_v<R>) {
          slot.call<R>(args...);
          this->thenable->call();
        } else {
          this->thenable->call(slot.call<R>(args...));
        }
      };
      this->thenable = std::make_shared<Thenable<R>>();
    }

    void deliver(Slot& slot) override {
      func(slot);
    }
  };

  EventDescriptor _descriptor = UNDEFINED_EVENT;
  std::shared_ptr<Interface> _ptr;
};

} // _NS_Events