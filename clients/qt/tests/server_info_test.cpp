#include "server_info.hpp"

#include <iostream>

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
  constexpr auto response = R"xml(<?xml version="1.0" encoding="utf-8"?>
<root status_code="200">
  <hostname>Gaming PC</hostname>
  <appversion>7.1.431.0</appversion>
  <GfeVersion>3.23.0.74</GfeVersion>
  <uniqueid>0123456789ABCDEF</uniqueid>
  <HttpsPort>47984</HttpsPort>
  <ServerCodecModeSupport>65793</ServerCodecModeSupport>
  <PairStatus>1</PairStatus>
  <currentgame>42</currentgame>
  <state>SUNSHINE_SERVER_BUSY</state>
</root>)xml";

  QString error;
  const auto info = parseServerInfo(response, &error);
  failures += expect(info.has_value(), "valid serverinfo should parse");
  failures += expect(error.isEmpty(), "valid serverinfo should not set an error");
  failures += expect(info && info->stable_id == QStringLiteral("0123456789ABCDEF"), "unique ID should parse");
  failures += expect(info && info->display_name == QStringLiteral("Gaming PC"), "hostname should parse");
  failures += expect(info && info->paired, "pairing status should parse");
  failures += expect(info && info->https_port == 47984, "HTTPS port should parse");
  failures += expect(info && info->codec_mode_support == 65793, "codec support should parse");
  failures += expect(info && info->current_game_id == 42, "running application should parse while the host is busy");

  const auto stale_game = parseServerInfo(
      R"xml(<root status_code="200"><uniqueid>host</uniqueid><currentgame>42</currentgame><state>SUNSHINE_SERVER_FREE</state></root>)xml",
      &error);
  failures +=
      expect(stale_game && stale_game->current_game_id == 0, "stale game ID should be ignored while the host is free");

  const auto rejected = parseServerInfo("<root status_code=\"503\" status_message=\"Busy\"/>", &error);
  failures += expect(!rejected.has_value() && error == QStringLiteral("Busy"), "error response should be rejected");

  const auto missing_id = parseServerInfo("<root status_code=\"200\"><hostname>PC</hostname></root>", &error);
  failures += expect(!missing_id.has_value(), "serverinfo without unique ID should be rejected");
  int status = 0;
  failures += expect(!parseServerInfo("<root status_code=\"401\"/>", nullptr, &status) && status == 401,
                     "a complete authorization rejection must expose its status");
  failures += expect(!parseServerInfo("<root status_code=\"401\">", nullptr, &status) && status == 0,
                     "malformed XML must not expose a trusted rejection status");
  return failures == 0 ? 0 : 1;
}
