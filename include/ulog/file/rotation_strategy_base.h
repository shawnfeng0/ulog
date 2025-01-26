#pragma once

#include "ulog/status.h"

namespace ulog::file {

class RotationStrategyBase {
 public:
  RotationStrategyBase() = default;
  virtual ~RotationStrategyBase() = default;

  virtual Status Rotate() = 0;
  virtual std::string LatestFilename() = 0;
};

}  // namespace ulog::file