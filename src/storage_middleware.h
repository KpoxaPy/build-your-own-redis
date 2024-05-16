#pragma once

#include "command.h"
#include "message.h"
#include "events.h"
#include "rdb_parser.h"
#include "signal_slot.h"
#include "storage.h"

#include <list>
#include <memory>
#include <unordered_map>

using ReplicaId = std::size_t;

class IReplicasManager {
public:
  enum class ReplState {
    MET,
    RESYNC,
    WRITE,
  };

  virtual ~IReplicasManager() = default;
  virtual ReplicaId add_replica(SlotPtr<Message> slot_command) = 0;
  virtual void remove_replica(ReplicaId) = 0;
  virtual bool replica_process_conf(ReplicaId, CommandPtr) = 0;
  virtual void replica_set_state(ReplicaId, ReplState) = 0;
  virtual std::size_t count_replicas() = 0;
  virtual void wait_for(std::size_t count, std::size_t timeout_ms, SlotPtr<Message> slot_message) = 0;
};
using IReplicasManagerPtr = std::shared_ptr<IReplicasManager>;

class StorageMiddleware : public IStorage, public IReplicasManager {
  struct ReplicaHandle {
    StorageMiddleware& parent;

    ReplicaId id;
    ReplState state;
    SlotPtr<Message> slot_message;

    std::size_t bytes_pushed = 0;
    std::size_t bytes_ack = 0;

    ReplicaHandle(StorageMiddleware&, ReplicaId id, ReplState state, SlotPtr<Message> slot_message);

    bool process_conf(CommandPtr);
    void set_state(ReplState);

    void push(const Message&);
  };

  struct WaitHandle;
  using WaitHandlePtr = std::shared_ptr<WaitHandle>;
  using WaitList = std::list<WaitHandlePtr>;

  struct WaitHandle {
    WaitHandle(StorageMiddleware&, std::size_t count, std::size_t timeout_ms, SlotPtr<Message> slot_message);

    StorageMiddleware& parent;

    std::size_t replicas_expected;
    std::size_t replicas_ready;

    std::size_t timeout_ms;
    EventLoop::JobHandle timeout;

    SlotPtr<Message> slot_message;

    WaitList::iterator it;

    std::unordered_map<ReplicaId, std::size_t> replica_ack_waitlist;

    void update_replica_ack(ReplicaHandle&);
    void setup();
    void reply();
  };

public:
  StorageMiddleware(EventLoopPtr event_loop);

  void set_storage(IStoragePtr);

  void restore(std::string key, std::string value, std::optional<Timepoint> expire_time) override;

  void set(std::string key, std::string value, std::optional<int> expire_ms) override;
  std::optional<std::string> get(std::string key) override;

  std::tuple<StreamId, StreamErrorType> xadd(std::string key, InputStreamId id, StreamPartValue values) override;
  StreamRange xrange(std::string key, BoundStreamId left_id, BoundStreamId right_id) override;

  StorageType type(std::string key) override;

  std::vector<std::string> keys(std::string_view selector) const override;

  ReplicaId add_replica(SlotPtr<Message> slot_message) override;
  void remove_replica(ReplicaId) override;
  bool replica_process_conf(ReplicaId, CommandPtr) override;
  void replica_set_state(ReplicaId, ReplState) override;

  std::size_t count_replicas() override;
  void wait_for(std::size_t count, std::size_t timeout_ms, SlotPtr<Message> slot_message) override;

private:
  EventLoopPtr _event_loop;

  IStoragePtr _storage;

  ReplicaId _next_replica_id = 0;
  std::unordered_map<ReplicaId, ReplicaHandle> _replicas;

  WaitList _waits;

  void push(const Message&);
};
