#pragma once

#include "gpio-expander.hpp"

namespace gay_ipod {

/*
 * Display driver for LVGL.
 */
class Display {
  public:
    enum Error {};
    static auto create(GpioExpander* expander)
	-> cpp::result<std::unique_ptr<Display>, Error>;

    Display(GpioExpander *gpio);
    ~Display();

    void WriteData();

  private:
    GpioExpander *gpio_;
};

} // namespace gay_ipod
