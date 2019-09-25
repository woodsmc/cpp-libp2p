/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_UPGRADER_SESSION_HPP
#define KAGOME_UPGRADER_SESSION_HPP

#include <memory>

#include "connection/capable_connection.hpp"
#include "transport/upgrader.hpp"

namespace libp2p::transport {

  /**
   * @brief Class, which reduces callback hell in transport upgrader.
   */
  struct UpgraderSession
      : public std::enable_shared_from_this<UpgraderSession> {
    using ConnectionCallback =
        void(outcome::result<std::shared_ptr<connection::CapableConnection>>);
    using HandlerFunc = std::function<ConnectionCallback>;

    UpgraderSession(std::shared_ptr<transport::Upgrader> upgrader,
                    std::shared_ptr<connection::RawConnection> raw,
                    HandlerFunc handler);

    void secureOutbound(const peer::PeerId &remoteId);

    void secureInbound();

   private:
    std::shared_ptr<transport::Upgrader> upgrader_;
    std::shared_ptr<connection::RawConnection> raw_;
    HandlerFunc handler_;

    void onSecured(
        outcome::result<std::shared_ptr<connection::SecureConnection>> rsecure);
  };

}  // namespace libp2p::transport

#endif  // KAGOME_UPGRADER_SESSION_HPP
