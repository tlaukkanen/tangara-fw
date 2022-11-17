
FatfsAudioReader
 - input if a queue of filenames.
 - output is a cbor stream
    - 1 header, like "this is a new file! this is the file type!
    - followed by length-prefixed chunks of bytes
 - runs in a task, which prompts it to read/write one chunk, then returns.
 - task watches for kill signal, owns storage, etc.

AudioDecoder
 - input is the chunked bytes above.
 - output is also a cbor stream
    - 1 header, which is like a reconfiguration packet thing.
    - "data that follows is this depth, this sample rate"
    - also indicates whether the configuration is 'sudden' for soft muting?
    - then length-prefixed chunks of bytes

AudioOutput
  - input is the output of the decoder
  - outputs via writing to i2s_write, which copies data to a dma buffer
  - therefore, safe for us to consume any kind of reconfiguration here.
    - only issue is that we will need to wait for the dma buffers to drain before
      we can reconfigure the driver. (i2s_zero_dma_buffer)
 - this is important for i2s speed; we should avoid extra copy steps for the raw
 - pcm stream
 - input therefore needs to be two channels: one configuration channel, one bytes
   channel


How do things like seeking, and progress work?
 - Reader knows where we are in terms of file size and position
 - Decoder knows sample rate, frames, etc. for knowing how that maps into
 - the time progress
 - Output knows where we are as well in a sense, but only in terms of the PCM
  output. this doesn't correspond to anything very well.

  So, to seek:
   - come up with your position. this is likely "where we are plus 10", or a
   specific timecode. the decoder has what we need for the byte position of this
   - tell the reader "hey we need to be in this file at this byte position
   - reader clears its own output buffer (since it's been doing readahead) and
    starts again at the given location
  For current position, the decoder will need to track where in the file it's up
  to.

HEADERS + DATA:
 - cbor seems sensible for headers. allocate a little working buffer, encode the
  data, then send it out on the ringbuffer.
 - the data itself is harder, since tinycbor doesn't support writing chunked indefinite
  length stuff. this is a problem bc we need to give cbor the buffer up front, but
  we don't know exactly how long things will be, so it ends up being slightly awkward 
  and inefficient.
  - we could also just like... write the struct i guess? that might be okay.
  - gives us a format like <TYPE ENUM> <LENGTH> <DATA>
  - could be smart with the type, use like a 32 bit int, and encode the length
  - in there?
  - then from the reader's perspective, it's:
    - read 4 bytes, work out what's next
    - read the next X bytes
