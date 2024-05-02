/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/env_esp.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

#include "spi.hpp"
#include "tasks.hpp"

namespace leveldb {

tasks::WorkerPool* sBackgroundThread = nullptr;

std::string ErrToStr(FRESULT err) {
  switch (err) {
    case FR_OK:
      return "FR_OK";
    case FR_DISK_ERR:
      return "FR_DISK_ERR";
    case FR_INT_ERR:
      return "FR_INT_ERR";
    case FR_NOT_READY:
      return "FR_NOT_READY";
    case FR_NO_FILE:
      return "FR_NO_FILE";
    case FR_NO_PATH:
      return "FR_NO_PATH";
    case FR_INVALID_NAME:
      return "FR_INVALID_NAME";
    case FR_DENIED:
      return "FR_DENIED";
    case FR_EXIST:
      return "FR_EXIST";
    case FR_INVALID_OBJECT:
      return "FR_INVALID_OBJECT";
    case FR_WRITE_PROTECTED:
      return "FR_WRITE_PROTECTED";
    case FR_INVALID_DRIVE:
      return "FR_INVALID_DRIVE";
    case FR_NOT_ENABLED:
      return "FR_NOT_ENABLED";
    case FR_NO_FILESYSTEM:
      return "FR_NO_FILESYSTEM";
    case FR_MKFS_ABORTED:
      return "FR_MKFS_ABORTED";
    case FR_TIMEOUT:
      return "FR_TIMEOUT";
    case FR_LOCKED:
      return "FR_LOCKED";
    case FR_NOT_ENOUGH_CORE:
      return "FR_NOT_ENOUGH_CORE";
    case FR_TOO_MANY_OPEN_FILES:
      return "FR_TOO_MANY_OPEN_FILES";
    case FR_INVALID_PARAMETER:
      return "FR_INVALID_PARAMETER";
    default:
      return "UNKNOWN";
  }
}

Status EspError(const std::string& context, FRESULT err) {
  if (err == FR_NO_FILE) {
    return Status::NotFound(context, ErrToStr(err));
  } else {
    return Status::IOError(context, ErrToStr(err));
  }
}

class EspSequentialFile final : public SequentialFile {
 public:
  EspSequentialFile(const std::string& filename, FIL file)
      : file_(file), filename_(filename) {}
  ~EspSequentialFile() override {
    auto lock = drivers::acquire_spi();
    f_close(&file_);
  }

  Status Read(size_t n, Slice* result, char* scratch) override {
    auto lock = drivers::acquire_spi();
    UINT read_size = 0;
    FRESULT res = f_read(&file_, scratch, n, &read_size);
    if (res != FR_OK) {  // Read error.
      return EspError(filename_, res);
    }
    *result = Slice(scratch, read_size);
    return Status::OK();
  }

  Status Skip(uint64_t n) override {
    auto lock = drivers::acquire_spi();
    DWORD current_pos = f_tell(&file_);
    FRESULT res = f_lseek(&file_, current_pos + n);
    if (res != FR_OK) {
      return EspError(filename_, res);
    }
    return Status::OK();
  }

 private:
  FIL file_;
  const std::string filename_;
};

// Implements random read access in a file using pread().
//
// Instances of this class are thread-safe, as required by the RandomAccessFile
// API. Instances are immutable and Read() only calls thread-safe library
// functions.
class EspRandomAccessFile final : public RandomAccessFile {
 public:
  // The new instance takes ownership of |fd|. |fd_limiter| must outlive this
  // instance, and will be used to determine if .
  explicit EspRandomAccessFile(const std::string& filename)
      : filename_(std::move(filename)) {}

  ~EspRandomAccessFile() override {}

  Status Read(uint64_t offset,
              size_t n,
              Slice* result,
              char* scratch) const override {
    auto lock = drivers::acquire_spi();
    FIL file;
    FRESULT res = f_open(&file, filename_.c_str(), FA_READ);
    if (res != FR_OK) {
      return EspError(filename_, res);
    }

    res = f_lseek(&file, offset);
    if (res != FR_OK) {
      return EspError(filename_, res);
    }

    Status status;
    UINT read_size = 0;
    res = f_read(&file, scratch, n, &read_size);
    if (res != FR_OK || read_size == 0) {
      return EspError(filename_, res);
    }
    *result = Slice(scratch, read_size);

    f_close(&file);

    return status;
  }

 private:
  const std::string filename_;
};

// TODO(jacqueline): LevelDB expects writes to this class to be buffered in
// memory. FatFs already does in-memory buffering, but we should think about
// whether to layer more on top.
class EspWritableFile final : public WritableFile {
 public:
  EspWritableFile(std::string filename, FIL file)
      : filename_(std::move(filename)), file_(file), is_open_(true) {}

  ~EspWritableFile() override {
    if (is_open_) {
      // Ignoring any potential errors
      Close();
    }
  }

  Status Append(const Slice& data) override {
    if (!is_open_) {
      return EspError(filename_, FR_NOT_ENABLED);
    }

    auto lock = drivers::acquire_spi();
    size_t write_size = data.size();
    const char* write_data = data.data();

    UINT bytes_written = 0;
    FRESULT res = f_write(&file_, write_data, write_size, &bytes_written);
    if (res != FR_OK) {
      return EspError(filename_, res);
    }

    return Status::OK();
  }

  Status Close() override {
    auto lock = drivers::acquire_spi();
    is_open_ = false;
    FRESULT res = f_close(&file_);
    if (res != FR_OK) {
      return EspError(filename_, res);
    }
    return Status::OK();
  }

  Status Flush() override { return Sync(); }

  Status Sync() override {
    if (!is_open_) {
      return EspError(filename_, FR_NOT_ENABLED);
    }
    auto lock = drivers::acquire_spi();
    FRESULT res = f_sync(&file_);
    if (res != FR_OK) {
      return EspError(filename_, res);
    }
    return Status::OK();
  }

 private:
  const std::string filename_;
  FIL file_;
  bool is_open_;
};

class EspFileLock : public FileLock {
 public:
  explicit EspFileLock(const std::string& filename) : filename_(filename) {}
  const std::string& filename() { return filename_; }

 private:
  const std::string filename_;
};

class EspLogger final : public Logger {
 public:
  explicit EspLogger(FIL file) : file_(file) {}
  ~EspLogger() override { f_close(&file_); }

  void Logv(const char* format, std::va_list ap) override {
    /*
    std::va_list args_copy;
    va_copy(args_copy, ap);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    std::size_t bytes_needed = snprintf(NULL, 0, format, args_copy);
    char* output = reinterpret_cast<char*>(
        heap_caps_calloc(bytes_needed, 1, MALLOC_CAP_SPIRAM));
    snprintf(output, bytes_needed, format, args_copy);
#pragma GCC diagnostic pop
    va_end(args_copy);
    ESP_LOGI("LEVELDB", "%s", output);
    // f_puts(output, &file_);
    free(reinterpret_cast<void*>(output));
    */
  }

 private:
  FIL file_;
};

EspEnv::~EspEnv() {
  ESP_LOGE("LEVELDB", "EspEnv singleton destroyed. Unsupported behavior!");
}

Status EspEnv::NewSequentialFile(const std::string& filename,
                                 SequentialFile** result) {
  auto lock = drivers::acquire_spi();
  FIL file;
  FRESULT res = f_open(&file, filename.c_str(), FA_READ);
  if (res != FR_OK) {
    *result = nullptr;
    return EspError(filename, res);
  }

  *result = new EspSequentialFile(filename, file);
  return Status::OK();
}

Status EspEnv::NewRandomAccessFile(const std::string& filename,
                                   RandomAccessFile** result) {
  auto lock = drivers::acquire_spi();
  // EspRandomAccessFile doesn't try to open the file until it's needed, so
  // we need to first ensure the file exists to handle the NotFound case
  // correctly.
  FILINFO info;
  FRESULT res = f_stat(filename.c_str(), &info);
  if (res != FR_OK) {
    *result = nullptr;
    return EspError(filename, res);
  }

  *result = new EspRandomAccessFile(filename);
  return Status::OK();
}

Status EspEnv::NewWritableFile(const std::string& filename,
                               WritableFile** result) {
  auto lock = drivers::acquire_spi();
  FIL file;
  FRESULT res = f_open(&file, filename.c_str(), FA_WRITE | FA_CREATE_ALWAYS);
  if (res != FR_OK) {
    *result = nullptr;
    return EspError(filename, res);
  }

  *result = new EspWritableFile(filename, file);
  return Status::OK();
}

Status EspEnv::NewAppendableFile(const std::string& filename,
                                 WritableFile** result) {
  auto lock = drivers::acquire_spi();
  FIL file;
  FRESULT res = f_open(&file, filename.c_str(), FA_WRITE | FA_OPEN_APPEND);
  if (res != FR_OK) {
    *result = nullptr;
    return EspError(filename, res);
  }

  *result = new EspWritableFile(filename, file);
  return Status::OK();
}

bool EspEnv::FileExists(const std::string& filename) {
  auto lock = drivers::acquire_spi();
  FILINFO info;
  return f_stat(filename.c_str(), &info) == FR_OK;
}

Status EspEnv::GetChildren(const std::string& directory_path,
                           std::vector<std::string>* result) {
  result->clear();

  auto lock = drivers::acquire_spi();
  FF_DIR dir;
  FRESULT res = f_opendir(&dir, directory_path.c_str());
  if (res != FR_OK) {
    return EspError(directory_path, res);
  }

  FILINFO info;
  for (;;) {
    res = f_readdir(&dir, &info);
    if (res != FR_OK) {
      return EspError(directory_path, res);
    }
    if (info.fname[0] == 0) {
      break;
    }
    result->emplace_back(info.fname);
  }

  res = f_closedir(&dir);
  if (res != FR_OK) {
    return EspError(directory_path, res);
  }

  return Status::OK();
}

Status EspEnv::RemoveFile(const std::string& filename) {
  auto lock = drivers::acquire_spi();
  FRESULT res = f_unlink(filename.c_str());
  if (res != FR_OK) {
    return EspError(filename, res);
  }
  return Status::OK();
}

Status EspEnv::CreateDir(const std::string& dirname) {
  auto lock = drivers::acquire_spi();
  FRESULT res = f_mkdir(dirname.c_str());
  if (res != FR_OK) {
    return EspError(dirname, res);
  }
  return Status::OK();
}

Status EspEnv::RemoveDir(const std::string& dirname) {
  return RemoveFile(dirname);
}

Status EspEnv::GetFileSize(const std::string& filename, uint64_t* size) {
  auto lock = drivers::acquire_spi();
  FILINFO info;
  FRESULT res = f_stat(filename.c_str(), &info);
  if (res != FR_OK) {
    *size = 0;
    return EspError(filename, res);
  }
  *size = info.fsize;
  return Status::OK();
}

Status EspEnv::RenameFile(const std::string& from, const std::string& to) {
  // Match the POSIX behaviour of replacing any existing file.
  if (FileExists(to)) {
    Status s = RemoveFile(to);
    if (!s.ok()) {
      return s;
    }
  }
  auto lock = drivers::acquire_spi();
  FRESULT res = f_rename(from.c_str(), to.c_str());
  if (res != FR_OK) {
    return EspError(from, res);
  }
  return Status::OK();
}

Status EspEnv::LockFile(const std::string& filename, FileLock** lock) {
  *lock = nullptr;

  if (!locks_.Insert(filename)) {
    return Status::IOError("lock " + filename, "already held by process");
  }

  *lock = new EspFileLock(filename);
  return Status::OK();
}

Status EspEnv::UnlockFile(FileLock* lock) {
  EspFileLock* posix_file_lock = static_cast<EspFileLock*>(lock);
  locks_.Remove(posix_file_lock->filename());
  delete posix_file_lock;
  return Status::OK();
}

void EspEnv::StartThread(void (*thread_main)(void* thread_main_arg),
                         void* thread_main_arg) {
  std::thread new_thread(thread_main, thread_main_arg);
  new_thread.detach();
}

Status EspEnv::GetTestDirectory(std::string* result) {
  CreateDir("/tmp");
  *result = "/tmp";
  return Status::OK();
}

Status EspEnv::NewLogger(const std::string& filename, Logger** result) {
  auto lock = drivers::acquire_spi();
  FIL file;
  FRESULT res = f_open(&file, filename.c_str(), FA_WRITE | FA_OPEN_APPEND);
  if (res != FR_OK) {
    *result = nullptr;
    return EspError(filename, res);
  }

  *result = new EspLogger(file);
  return Status::OK();
}

uint64_t EspEnv::NowMicros() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
}

void EspEnv::SleepForMicroseconds(int micros) {
  vTaskDelay(pdMS_TO_TICKS(micros / 1000));
}

EspEnv::EspEnv() {}

void EspEnv::Schedule(
    void (*background_work_function)(void* background_work_arg),
    void* background_work_arg) {
  auto worker = sBackgroundThread;
  if (worker) {
    worker->Dispatch<void>(
        [=]() { std::invoke(background_work_function, background_work_arg); });
  }
}

}  // namespace leveldb
