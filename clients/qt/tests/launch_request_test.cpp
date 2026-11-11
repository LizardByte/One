#include <iostream>

#include "launch_request.hpp"

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

  one::protocol::StreamConfiguration configuration({
      .width = 2560,
      .height = 1440,
      .frames_per_second = 120,
      .audio_layout = one::protocol::AudioLayout::surround_5_1,
  });
  const auto parameters = configuration.launch_parameters();
  const LaunchRequestOptions sunshine_options {
      .application_id = 42,
      .gamepad_mask = 5,
  };
  const auto sunshine_query = buildLaunchQuery(parameters, sunshine_options);
  failures += expect(sunshine_query.queryItemValue(QStringLiteral("appid")) == QStringLiteral("42"), "app ID should be sent");
  failures += expect(
      sunshine_query.queryItemValue(QStringLiteral("mode")) == QStringLiteral("2560x1440x120"),
      "Sunshine should receive the requested high frame rate");
  failures += expect(
      sunshine_query.queryItemValue(QStringLiteral("rikey")).size() == 32,
      "launch request should contain the stream key");
  failures += expect(
      sunshine_query.queryItemValue(QStringLiteral("remoteControllersBitmap")) == QStringLiteral("5") &&
          sunshine_query.queryItemValue(QStringLiteral("gcmap")) == QStringLiteral("5"),
      "controller mask should be sent in both protocol fields");
  failures += expect(sunshine_query.queryItemValue(QStringLiteral("hdrMode")) == QStringLiteral("1"), "HDR fields should be sent");
  failures += expect(sunshine_query.queryItemValue(QStringLiteral("corever")) == QStringLiteral("1"), "core extension should be sent");

  auto nvidia_options = sunshine_options;
  nvidia_options.is_nvidia_server = true;
  failures += expect(
      buildLaunchQuery(parameters, nvidia_options).queryItemValue(QStringLiteral("mode")) == QStringLiteral("2560x1440x0"),
      "NVIDIA hosts should receive the compatibility FPS value above 60 FPS");

  QString error;
  const auto response = parseLaunchResponse(
      R"xml(<root status_code="200"><gamesession>1</gamesession><sessionUrl0>rtsp://192.168.1.2:48010</sessionUrl0></root>)xml",
      &error);
  failures += expect(response.has_value(), "successful launch response should parse");
  failures += expect(
      response.has_value() && response->rtsp_session_url == QStringLiteral("rtsp://192.168.1.2:48010"),
      "RTSP session URL should parse");

  const auto without_rtsp = parseLaunchResponse(R"xml(<root status_code="200"><gamesession>1</gamesession></root>)xml", &error);
  failures += expect(without_rtsp.has_value(), "RTSP URL should remain optional for older hosts");

  const auto rejected = parseLaunchResponse(
      R"xml(<root status_code="503" status_message="A game is already running"/>)xml",
      &error);
  failures += expect(!rejected.has_value(), "server launch error should be rejected");
  failures += expect(error == QStringLiteral("A game is already running"), "server launch error should be preserved");

  const auto malformed = parseLaunchResponse(R"xml(<root status_code="200"><gamesession>)xml", &error);
  failures += expect(!malformed.has_value(), "malformed launch response should be rejected");
  return failures == 0 ? 0 : 1;
}
