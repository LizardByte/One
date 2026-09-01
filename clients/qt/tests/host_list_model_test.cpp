#include <iostream>

#include <QCoreApplication>

#include "host_list_model.hpp"

namespace {

int expect(const bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << message << '\n';
  return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  HostListModel model;
  int failures = 0;

  failures += expect(model.rowCount() == 0, "host list should begin empty");
  failures += expect(!model.addManualHost("   "), "blank manual host should be rejected");
  failures += expect(!model.addManualHost("example.test", 65536), "invalid port should be rejected");
  failures += expect(model.addManualHost("  example.test  "), "valid manual host should be accepted");
  failures += expect(model.rowCount() == 1, "manual host should appear in the model");

  const auto first = model.index(0);
  failures += expect(
      model.data(first, HostListModel::AddressRole).toString() == QStringLiteral("example.test"),
      "manual host address should be normalized");
  failures += expect(
      model.data(first, HostListModel::PortRole).toInt() == 47989,
      "manual host should use the GameStream default port");
  failures += expect(
      model.data(first, HostListModel::ManuallyAddedRole).toBool(),
      "manual host role should be exposed");

  failures += expect(model.addManualHost("example.test"), "existing manual host should be refreshable");
  failures += expect(model.rowCount() == 1, "refreshing a host must not duplicate it");
  return failures == 0 ? 0 : 1;
}
