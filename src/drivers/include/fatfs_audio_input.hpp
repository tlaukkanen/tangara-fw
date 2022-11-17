#pragma once

namespace drivers {

  class FatfsAudioInput {
    public:
      FatfsAudioInput(std::shared_ptr<SdStorage> storage);
      ~FatfsAudioInput();

      enum Status {
        /*
         * Successfully read data into the output buffer, and there is still
         * data remaining in the file.
         */
        OKAY,

        /*
         * The ringbuffer was full. No data was read.
         */
        RINGBUF_FULL,

        /*
         * Some data may have been read into the output buffer, but the file is
         * now empty.
         */
        FILE_EMPTY,
      };
      auto Process() -> Status;

      auto GetOutputBuffer() -> RingbufHandle_t;

    private:
      std::shared_ptr<SdStorage> storage_;
      RingbufHandle_t output_;

      std::string path_;

  };
 
} // namespace drivers
