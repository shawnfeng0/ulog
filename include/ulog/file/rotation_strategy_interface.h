#pragma once

#include "ulog/status.h"

namespace ulog::file {

class RotationStrategyInterface {
 public:
  RotationStrategyInterface() = default;
  virtual ~RotationStrategyInterface() = default;

  virtual Status Rotate() = 0;
  virtual std::string LatestFilename() = 0;
};

}  // namespace ulog::file