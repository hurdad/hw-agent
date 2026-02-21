#include "sensors/tegrastats.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <string>

namespace hw_agent::sensors {

namespace {
float parse_float(const std::string& value) noexcept {
  const char* begin = value.c_str();
  char* end = nullptr;
  const float parsed = std::strtof(begin, &end);
  if (end == begin || !std::isfinite(parsed)) {
    return 0.0F;
  }
  return parsed;
}

int parse_int_or_zero(const std::string& value) noexcept {
  int parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{}) {
    return 0;
  }
  return parsed;
}
}  // namespace

TegraStatsSensor::TegraStatsSensor(const std::uint32_t interval_ms) noexcept {
  enabled_ = launch(interval_ms);
}

TegraStatsSensor::~TegraStatsSensor() { disable(); }

void TegraStatsSensor::sample(model::signal_frame& frame) noexcept {
  (void)frame;

  if (!enabled_) {
    return;
  }

  char chunk[4096]{};
  bool parsed_at_least_one_line = false;

  while (true) {
    const ssize_t bytes_read = ::read(read_fd_, chunk, sizeof(chunk));
    if (bytes_read > 0) {
      read_buffer_.append(chunk, static_cast<std::size_t>(bytes_read));
      std::size_t newline_pos = read_buffer_.find('\n');
      while (newline_pos != std::string::npos) {
        std::string line = read_buffer_.substr(0, newline_pos);
        read_buffer_.erase(0, newline_pos + 1);
        parsed_at_least_one_line = parse_line(line) || parsed_at_least_one_line;
        newline_pos = read_buffer_.find('\n');
      }
      continue;
    }

    if (bytes_read == 0) {
      disable();
      return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }

    disable();
    return;
  }

  if (!parsed_at_least_one_line) {
    int status = 0;
    const pid_t wait_result = waitpid(child_pid_, &status, WNOHANG);
    if (wait_result == child_pid_) {
      disable();
    }
  }
}

bool TegraStatsSensor::enabled() const noexcept { return enabled_; }

const TegraStatsSensor::RawFields& TegraStatsSensor::raw() const noexcept { return raw_; }

bool TegraStatsSensor::launch(const std::uint32_t interval_ms) noexcept {
  int pipe_fds[2]{};
  if (pipe(pipe_fds) != 0) {
    return false;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return false;
  }

  if (pid == 0) {
    close(pipe_fds[0]);
    dup2(pipe_fds[1], STDOUT_FILENO);
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);

    const std::string interval = std::to_string(interval_ms);
    execlp("tegrastats", "tegrastats", "--interval", interval.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  close(pipe_fds[1]);

  const int flags = fcntl(pipe_fds[0], F_GETFL, 0);
  if (flags < 0 || fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) != 0) {
    close(pipe_fds[0]);
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, 0);
    return false;
  }

  read_fd_ = pipe_fds[0];
  child_pid_ = static_cast<int>(pid);
  return true;
}

void TegraStatsSensor::disable() noexcept {
  if (read_fd_ >= 0) {
    close(read_fd_);
    read_fd_ = -1;
  }

  if (child_pid_ > 0) {
    kill(child_pid_, SIGTERM);
    waitpid(child_pid_, nullptr, 0);
    child_pid_ = -1;
  }

  enabled_ = false;
}

bool TegraStatsSensor::parse_line(const std::string& line) noexcept {
  bool parsed_any = false;

  static const std::regex gpu_re(R"((?:GR3D_FREQ|GPU)\s+(\d+)%)");
  static const std::regex emc_re(R"(EMC_FREQ\s+(\d+)%)");
  static const std::regex temp_re(R"(([A-Za-z0-9_]+)@(-?\d+(?:\.\d+)?)C)");
  static const std::regex rail_re(R"((VDD_[A-Z0-9_]+)\s+(\d+)mW(?:\/(\d+)mW)?)");

  std::smatch match;
  if (std::regex_search(line, match, gpu_re) && match.size() >= 2) {
    raw_.gpu_util_pct = static_cast<float>(parse_int_or_zero(match[1].str()));
    parsed_any = true;
  }

  if (std::regex_search(line, match, emc_re) && match.size() >= 2) {
    raw_.emc_util_pct = static_cast<float>(parse_int_or_zero(match[1].str()));
    parsed_any = true;
  }

  float rail_sum = 0.0F;
  bool parsed_rail = false;
  for (std::sregex_iterator it(line.begin(), line.end(), rail_re), end; it != end; ++it) {
    if ((*it).size() < 3) {
      continue;
    }

    const std::string rail_name = (*it)[1].str();
    const std::string instantaneous = (*it)[2].str();
    const float rail_power = static_cast<float>(parse_int_or_zero(instantaneous));
    raw_.rail_power_mw[rail_name] = rail_power;
    rail_sum += rail_power;
    parsed_rail = true;
  }
  if (parsed_rail) {
    raw_.total_rail_power_mw = rail_sum;
    parsed_any = true;
  }

  bool parsed_temp = false;
  for (std::sregex_iterator it(line.begin(), line.end(), temp_re), end; it != end; ++it) {
    if ((*it).size() < 3) {
      continue;
    }

    raw_.temperatures_c[(*it)[1].str()] = parse_float((*it)[2].str());
    parsed_temp = true;
  }
  parsed_any = parsed_any || parsed_temp;

  return parsed_any;
}

}  // namespace hw_agent::sensors
