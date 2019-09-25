/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_MULTIBASE_IMPL_HPP
#define KAGOME_MULTIBASE_IMPL_HPP

#include "multi/multibase_codec.hpp"

namespace libp2p::multi {
  /**
   * Implementation of the MultibaseCodec with fair codecs
   */
  class MultibaseCodecImpl : public MultibaseCodec {
   public:
    ~MultibaseCodecImpl() override = default;

    enum class Error { UNSUPPORTED_BASE = 1, INPUT_TOO_SHORT };

    std::string encode(const ByteBuffer &bytes,
                       Encoding encoding) const override;

    outcome::result<ByteBuffer> decode(
        std::string_view string) const override;
  };
}  // namespace libp2p::multi

OUTCOME_HPP_DECLARE_ERROR(libp2p::multi, MultibaseCodecImpl::Error)

#endif  // KAGOME_MULTIBASE_IMPL_HPP
