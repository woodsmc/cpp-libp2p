/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_HOST_INJECTOR_HPP
#define KAGOME_HOST_INJECTOR_HPP

#include <boost/di.hpp>
#include "injector/network_injector.hpp"

// implementations
#include "host/basic_host.hpp"
#include "peer/address_repository/inmem_address_repository.hpp"
#include "peer/impl/peer_repository_impl.hpp"
#include "peer/key_repository/inmem_key_repository.hpp"
#include "peer/protocol_repository/inmem_protocol_repository.hpp"

namespace libp2p::injector {

  template <typename... Ts>
  auto makeHostInjector(Ts &&... args) {
    using namespace boost;  // NOLINT

    // clang-format off
    return di::make_injector(
        makeNetworkInjector(),

        di::bind<Host>.template to<host::BasicHost>(),

        // repositories
        di::bind<peer::PeerRepository>.template to<peer::PeerRepositoryImpl>(),
        di::bind<peer::AddressRepository>.template to<peer::InmemAddressRepository>(),
        di::bind<peer::KeyRepository>.template to<peer::InmemKeyRepository>(),
        di::bind<peer::ProtocolRepository>.template to<peer::InmemProtocolRepository>(),

        // user-defined overrides...
        std::forward<decltype(args)>(args)...
    );
    // clang-format on
  }

}  // namespace libp2p::injector

#endif  // KAGOME_HOST_INJECTOR_HPP
