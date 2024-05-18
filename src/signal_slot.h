#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

template <typename... Args>
class Slot : public std::enable_shared_from_this<Slot<Args...>> {
public:
  using ProvidedFunc = std::function<void(Args...)>;

  template <typename F>
  explicit Slot(F func)
    : _func(std::move(func))
  {
  }

  void call(const Args&... args) {
    this->_func(args...);
  }

private:
  ProvidedFunc _func;
};

template <typename... Args>
using SlotPtr = std::shared_ptr<Slot<Args...>>;

template <typename... Args>
class Signal : public std::enable_shared_from_this<Signal<Args...>> {
  struct SlotContainerBase {
    virtual ~SlotContainerBase() = default;
    virtual void call(const Args&...) const = 0;
  };

  template <typename Slot>
  struct SlotContainer : public SlotContainerBase {
    Slot slot;

    SlotContainer(Slot slot)
      : slot(std::move(slot)) {
    }

    void call(const Args&... args) const override {
      this->slot.call(args...);
    }
  };

  template <typename Slot>
  struct SlotContainer<std::shared_ptr<Slot>> : public SlotContainerBase {
    std::weak_ptr<Slot> slot_wptr;

    SlotContainer(std::shared_ptr<Slot> slot_ptr)
      : slot_wptr(slot_ptr) {
    }

    void call(const Args&... args) const override {
      if (auto ptr = this->slot_wptr.lock()) {
        ptr->call(args...);
      }
    }
  };

  using SlotContainerBasePtr = std::unique_ptr<SlotContainerBase>;

public:
  using ExpectedFunc = std::function<void(Args...)>;

  template <typename Slot>
  void connect(Slot slot) {
    this->_slots.push_back(std::make_unique<SlotContainer<Slot>>(std::move(slot)));
  }

  void emit(const Args&... args) const {
    for (const auto& slot: this->_slots) {
      slot->call(args...);
    }
  }

  void call(const Args&... args) const {
    this->emit(args...);
  }

private:
  std::vector<SlotContainerBasePtr> _slots;
};

template <typename... Args>
using SignalPtr = std::shared_ptr<Signal<Args...>>;
