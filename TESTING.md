# Running tests

Our test suite currently must be run on an actual device. A subset of our tests may run correctly on a bare ESP32-WROVER module, but in general they do rely on the real Tangara hardware being available.

Tests are implemented as a separate application build, located in the `test`
directory. We use Catch2 as our test framework.

To run them, navigate to the test directory, then build and flash as normal. e.g.

```
idf.py -p /dev/serial/by-id/usb-cool_tech_zone_Tangara_* app-flash
```

Connect to your device via serial, and you will be presented with 
 variant of our standard dev console. To run all tests, simply execute `catch` in the console.

The `catch` command accepts additional arguments as if it were a standard Catch2 test runner binary. You can therefore see a brief guide to the available options with `catch -?`.

# Writing tests

Tests live within the `test` subcomponent of the component that the tests are written for. In practice, this means that device driver tests should live in `src/drivers/test`, whilst most other tests live in `src/tangara/test`.

## Tags

Catch2 has a flexible system of test tags that can be used to categorise different test cases. Feel free to add new tags as-needed. In general, most tests should be tagged with one of:

- `[integration]`, for tests that rely on the hardware being in a specific state
- `[unit]`, for tests that operate purely in-memory, either without any additional device drivers needed, or by using test doubles rather than real drivers.
