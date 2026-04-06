//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Blocxxi/Core/primitives.h>

namespace blocxxi::core {

TEST(CorePrimitivesTest, MakeIdIsDeterministic)
{
  auto const first = MakeId("blocxxi");
  auto const second = MakeId("blocxxi");
  auto const different = MakeId("different");

  EXPECT_EQ(first, second);
  EXPECT_NE(first, different);
}

TEST(CorePrimitivesTest, TransactionFromTextCapturesPayloadAndMetadata)
{
  auto const transaction = Transaction::FromText("demo.tx", "payload", "kind=demo");

  EXPECT_EQ(transaction.type, "demo.tx");
  EXPECT_EQ(transaction.PayloadText(), "payload");
  EXPECT_EQ(transaction.metadata, "kind=demo");
}

TEST(CorePrimitivesTest, BlockCompositionBuildsStableHeaderShape)
{
  auto const transaction = Transaction::FromText("demo.tx", "payload");
  auto const block = Block::MakeNext(BlockId {}, 7, { transaction }, "demo");

  EXPECT_EQ(block.header.height, 7);
  EXPECT_EQ(block.header.source, "demo");
  EXPECT_EQ(block.transactions.size(), 1U);
}

} // namespace blocxxi::core
