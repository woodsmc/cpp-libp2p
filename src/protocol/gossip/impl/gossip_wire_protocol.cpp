/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/protocol/gossip/impl/gossip_wire_protocol.hpp>

#include <array>
#include <cassert>

#include <libp2p/multi/multihash.hpp>
#include <libp2p/multi/uvarint.hpp>

#include <generated/protocol/gossip/protobuf/rpc.pb.h>

namespace libp2p::protocol::gossip {

  namespace {

    inline const char *toString(const ByteArray &bytes) {
      // NOLINTNEXTLINE
      return reinterpret_cast<const char *>(bytes.data());
    }

    class RPCMessageDeserializer : public MessageReceiver {
     public:
      explicit RPCMessageDeserializer(RPCMessage &msg) : msg_(msg) {}

      bool parse(gsl::span<const uint8_t> bytes) {
        return parser_.parse(bytes);
      }

      void dispatch() {
        parser_.dispatch(getEmptyPeer(), *this);
      }

     private:
      RPCMessage &msg_;
      MessageParser parser_;

      void onSubscription(const peer::PeerId &, bool subscribe,
                          const TopicId &topic) override {
        msg_.subscriptions.push_back({subscribe, topic});
      }

      void onIHave(const peer::PeerId &, const TopicId &topic,
                   MessageId &&msg_id) override {
        msg_.i_have[topic].push_back(std::move(msg_id));
      }

      void onIWant(const peer::PeerId &, MessageId &&msg_id) override {
        msg_.i_want.push_back(std::move(msg_id));
      }

      void onGraft(const peer::PeerId &, const TopicId &topic) override {
        msg_.graft.push_back(topic);
      }

      void onPrune(const peer::PeerId &, const TopicId &topic) override {
        msg_.prune.push_back(topic);
      }

      void onMessage(const peer::PeerId &, TopicMessage::Ptr msg) override {
        msg_.publish.push_back(std::move(msg));
      }
    };

  }  // namespace

  bool RPCMessage::empty() const {
    return subscriptions.empty() && publish.empty() && i_have.empty()
        && i_want.empty() && graft.empty() && prune.empty();
  }

  void RPCMessage::clear() {
    subscriptions.clear();
    publish.clear();
    i_have.clear();
    i_want.clear();
    graft.clear();
    prune.clear();
  }

  bool RPCMessage::deserialize(gsl::span<const uint8_t> bytes) {
    clear();
    RPCMessageDeserializer des(*this);
    if (!des.parse(bytes)) return false;
    des.dispatch();
    return !empty();
  }

  bool RPCMessage::serialize(std::vector<uint8_t> &buffer) const {
    MessageBuilder builder;

    for (auto &sub : subscriptions) {
      builder.addSubscription(sub.subscribe, sub.topic_id);
    }

    for (auto &pub : publish) {
      builder.addMessage(*pub);
    }

    for (auto &[topic, message_ids] : i_have) {
      for (auto &mid : message_ids) {
        builder.addIHave(topic, mid);
      }
    }

    for (auto &mid : i_want) {
      builder.addIWant(mid);
    }

    for (auto &gr : graft) {
      builder.addGraft(gr);
    }

    for (auto &pr : prune) {
      builder.addPrune(pr);
    }

    return builder.serialize(buffer);
  }

  MessageBuilder::MessageBuilder()
      : pb_msg_(std::make_unique<pubsub::pb::RPC>()),
        control_pb_msg_(std::make_unique<pubsub::pb::ControlMessage>()),
        control_not_empty_(false) {}

  MessageBuilder::~MessageBuilder() = default;

  void MessageBuilder::clear() {
    pb_msg_->Clear();
    control_pb_msg_->Clear();
    control_not_empty_ = false;
    ihaves_.clear();
    iwant_.clear();
  }

  bool MessageBuilder::serialize(std::vector<uint8_t> &buffer) {
    for (auto &[topic, message_ids] : ihaves_) {
      auto *ih = control_pb_msg_->add_ihave();
      ih->set_topicid(topic);
      for (auto &mid : message_ids) {
        ih->add_messageids(toString(mid), mid.size());
      }
    }

    if (!iwant_.empty()) {
      auto *iw = control_pb_msg_->add_iwant();
      for (auto &mid : iwant_) {
        iw->add_messageids(toString(mid), mid.size());
      }
    }

    if (control_not_empty_) {
      pb_msg_->set_allocated_control(control_pb_msg_.get());
    }

    size_t msg_sz = pb_msg_->ByteSizeLong();

    auto varint_len = multi::UVarint{msg_sz};
    auto varint_vec = varint_len.toVector();
    size_t prefix_sz = varint_vec.size();

    size_t old_sz = buffer.size();
    buffer.resize(old_sz + prefix_sz + msg_sz);
    memcpy(&buffer[old_sz], varint_vec.data(), prefix_sz);

    bool success =
        pb_msg_->SerializeToArray(&buffer[old_sz + prefix_sz], msg_sz);

    if (control_not_empty_) {
      pb_msg_->release_control();
    }

    clear();
    return success;
  }

  void MessageBuilder::addSubscription(bool subscribe, const TopicId &topic) {
    auto *dst = pb_msg_->add_subscriptions();
    dst->set_subscribe(subscribe);
    dst->set_topicid(topic);
  }

  void MessageBuilder::addIHave(const TopicId &topic, const MessageId &msg_id) {
    ihaves_[topic].push_back(msg_id);
    control_not_empty_ = true;
  }

  void MessageBuilder::addIWant(const MessageId &msg_id) {
    iwant_.push_back(msg_id);
    control_not_empty_ = true;
  }

  void MessageBuilder::addGraft(const TopicId &topic) {
    control_pb_msg_->add_graft()->set_topicid(topic);
    control_not_empty_ = true;
  }

  void MessageBuilder::addPrune(const TopicId &topic) {
    control_pb_msg_->add_prune()->set_topicid(topic);
    control_not_empty_ = true;
  }

  void MessageBuilder::addMessage(const TopicMessage &msg) {
    auto *dst = pb_msg_->add_publish();
    dst->set_from(msg.from.data(), msg.from.size());
    dst->set_data(msg.data.data(), msg.data.size());
    dst->set_seqno(msg.seq_no.data(), msg.seq_no.size());
    for (auto &id : msg.topic_ids) {
      *dst->add_topicids() = id;
    }
    if (msg.signature) {
      dst->set_signature(msg.signature.value().data(),
                         msg.signature.value().size());
    }
    if (msg.key) {
      dst->set_key(msg.key.value().data(), msg.key.value().size());
    }
  }

  MessageParser::MessageParser() = default;

  MessageParser::~MessageParser() = default;

  bool MessageParser::parse(gsl::span<const uint8_t> bytes) {
    pb_msg_ = std::make_unique<pubsub::pb::RPC>();
    return pb_msg_->ParseFromArray(bytes.data(), bytes.size());
  }

  void MessageParser::dispatch(const peer::PeerId &from,
                               MessageReceiver &receiver) {
    if (!pb_msg_) {
      return;
    }

    for (auto &s : pb_msg_->subscriptions()) {
      if (!s.has_subscribe() || !s.has_topicid()) {
        continue;
      }
      receiver.onSubscription(from, s.subscribe(), s.topicid());
    }

    if (pb_msg_->has_control()) {
      auto &c = pb_msg_->control();

      for (auto &h : c.ihave()) {
        if (!h.has_topicid() || h.messageids_size() == 0) {
          continue;
        }
        const TopicId &topic = h.topicid();
        for (auto &msg_id : h.messageids()) {
          if (msg_id.empty()) {
            continue;
          }
          receiver.onIHave(from, topic, fromString(msg_id));
        }
      }

      for (auto &w : c.iwant()) {
        if (w.messageids_size() == 0) {
          continue;
        }
        for (auto &msg_id : w.messageids()) {
          if (msg_id.empty()) {
            continue;
          }
          receiver.onIWant(from, fromString(msg_id));
        }
      }

      for (auto &gr : c.graft()) {
        if (!gr.has_topicid()) {
          continue;
        }
        receiver.onGraft(from, gr.topicid());
      }

      for (auto &pr : c.prune()) {
        if (!pr.has_topicid()) {
          continue;
        }
        receiver.onPrune(from, pr.topicid());
      }
    }

    for (auto &m : pb_msg_->publish()) {
      if (!m.has_from() || !m.has_data() || !m.has_seqno()
          || m.topicids_size() == 0) {
        continue;
      }
      auto message = TopicMessage::fromWire(
          fromString(m.from()), fromString(m.seqno()), fromString(m.data()));
      for (auto &tid : m.topicids()) {
        message->topic_ids.push_back(tid);
      }
      if (m.has_signature()) {
        message->signature = fromString(m.signature());
      }
      if (m.has_key()) {
        message->key = fromString(m.key());
      }
      receiver.onMessage(from, std::move(message));
    }
  }

}  // namespace libp2p::protocol::gossip