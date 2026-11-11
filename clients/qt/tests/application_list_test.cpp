#include <iostream>

#include "application.hpp"

namespace {

int expect(const bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << message << '\n';
  return 1;
}

}  // namespace

int main() {
  int failures = 0;
  QString error;
  const auto applications = parseApplicationList(R"xml(<?xml version="1.0" encoding="utf-8"?>
    <root status_code="200">
      <App>
        <AppTitle>Desktop</AppTitle>
        <ID>1</ID>
        <IsHdrSupported>1</IsHdrSupported>
        <IsAppCollectorGame>0</IsAppCollectorGame>
      </App>
      <App>
        <AppTitle/>
        <ID>42</ID>
        <IsHdrSupported>0</IsHdrSupported>
        <IsAppCollectorGame>1</IsAppCollectorGame>
      </App>
    </root>
  )xml", &error);

  failures += expect(applications.has_value(), "valid application list should parse");
  failures += expect(applications.has_value() && applications->size() == 2, "all applications should be returned");
  if (applications.has_value() && applications->size() == 2) {
    failures += expect((*applications)[0].title == QStringLiteral("Desktop"), "application title should parse");
    failures += expect((*applications)[0].id == 1, "application ID should parse");
    failures += expect((*applications)[0].hdr_supported, "HDR support should parse");
    failures += expect((*applications)[1].title.isEmpty(), "empty application title should remain valid");
    failures += expect((*applications)[1].app_collector_game, "collector flag should parse");
  }

  const auto rejected_status = parseApplicationList(
      R"xml(<root status_code="401" status_message="Client is not authorized"/>)xml", &error);
  failures += expect(!rejected_status.has_value(), "server error status should be rejected");
  failures += expect(error == QStringLiteral("Client is not authorized"), "server error should be preserved");

  const auto incomplete = parseApplicationList(
      R"xml(<root status_code="200"><App><AppTitle>Missing ID</AppTitle></App></root>)xml", &error);
  failures += expect(!incomplete.has_value(), "incomplete application should be rejected");

  const auto malformed = parseApplicationList(
      R"xml(<root status_code="200"><App><AppTitle>Broken</root>)xml", &error);
  failures += expect(!malformed.has_value(), "malformed XML should be rejected");
  return failures == 0 ? 0 : 1;
}
