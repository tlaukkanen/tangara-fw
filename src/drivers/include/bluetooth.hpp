
#pragma once

#include <vector>

namespace drivers {

class Bluetooth {
    public:
    static auto Enable() -> Bluetooth*;
    Bluetooth();
    ~Bluetooth();

    struct Device {};
    auto Scan() -> std::vector<Device>;
    private:
  };

}
