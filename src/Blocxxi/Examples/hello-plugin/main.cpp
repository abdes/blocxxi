//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <iostream>
#include <memory>

#include <Blocxxi/Node/node.h>

namespace {

class CountingPlugin final : public blocxxi::core::Plugin {
public:
  void OnEvent(blocxxi::core::ChainEvent const& event) override
  {
    ++events_;
    std::cout << "plugin-event=" << static_cast<int>(event.type) << '\n';
  }

  [[nodiscard]] auto Count() const -> std::size_t { return events_; }

private:
  std::size_t events_ { 0 };
};

} // namespace

auto main() -> int
{
  auto node = blocxxi::node::Node();
  auto plugin = std::make_shared<CountingPlugin>();
  node.RegisterPlugin(plugin);

  auto status = node.Start();
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  status = node.SubmitTransaction(
    blocxxi::core::Transaction::FromText("hello.plugin", "hello world", "sample"));
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  status = node.CommitPending("hello-plugin");
  if (!status.ok()) {
    std::cerr << status.message << '\n';
    return 1;
  }

  std::cout << "plugin-events=" << plugin->Count() << '\n';
  std::cout << "height=" << node.Snapshot().height << '\n';
  return 0;
}
