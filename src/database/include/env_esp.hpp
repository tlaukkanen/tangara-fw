/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <mutex>
#include <set>
#include <string>

#include "leveldb/env.h"
#include "leveldb/status.h"

namespace leveldb {

// Tracks the files locked by EspEnv::LockFile().
//
// We maintain a separate set instead of relying on fcntl(F_SETLK) because
// fcntl(F_SETLK) does not provide any protection against multiple uses from the
// same process.
//
// Instances are thread-safe because all member data is guarded by a mutex.
class InMemoryLockTable {
 public:
  bool Insert(const std::string& fname) {
    mu_.lock();
    bool succeeded = locked_files_.insert(fname).second;
    mu_.unlock();
    return succeeded;
  }
  void Remove(const std::string& fname) {
    mu_.lock();
    locked_files_.erase(fname);
    mu_.unlock();
  }

 private:
  std::mutex mu_;
  std::set<std::string> locked_files_;
};

class EspEnv : public leveldb::Env {
 public:
  EspEnv();
  ~EspEnv() override;

  Status NewSequentialFile(const std::string& filename,
                           SequentialFile** result) override;

  Status NewRandomAccessFile(const std::string& filename,
                             RandomAccessFile** result) override;

  Status NewWritableFile(const std::string& filename,
                         WritableFile** result) override;

  Status NewAppendableFile(const std::string& filename,
                           WritableFile** result) override;

  bool FileExists(const std::string& filename) override;

  Status GetChildren(const std::string& directory_path,
                     std::vector<std::string>* result) override;

  Status RemoveFile(const std::string& filename) override;

  Status CreateDir(const std::string& dirname) override;

  Status RemoveDir(const std::string& dirname) override;

  Status GetFileSize(const std::string& filename, uint64_t* size) override;

  Status RenameFile(const std::string& from, const std::string& to) override;

  Status LockFile(const std::string& filename, FileLock** lock) override;

  Status UnlockFile(FileLock* lock) override;

  void Schedule(void (*background_work_function)(void* background_work_arg),
                void* background_work_arg) override;

  void StartThread(void (*thread_main)(void* thread_main_arg),
                   void* thread_main_arg) override;

  Status GetTestDirectory(std::string* result) override;

  Status NewLogger(const std::string& filename, Logger** result) override;

  uint64_t NowMicros() override;

  void SleepForMicroseconds(int micros) override;

  void BackgroundThreadMain();

 private:
  InMemoryLockTable locks_;  // Thread-safe.
};

}  // namespace leveldb

namespace database {

// Wraps an Env instance whose destructor is never created.
//
// Intended usage:
//   using PlatformSingletonEnv = SingletonEnv<PlatformEnv>;
//   void ConfigurePosixEnv(int param) {
//     PlatformSingletonEnv::AssertEnvNotInitialized();
//     // set global configuration flags.
//   }
//   Env* Env::Default() {
//     static PlatformSingletonEnv default_env;
//     return default_env.env();
//   }
template <typename EnvType>
class SingletonEnv {
 public:
  SingletonEnv() {
    static_assert(sizeof(env_storage_) >= sizeof(EnvType),
                  "env_storage_ will not fit the Env");
    static_assert(alignof(decltype(env_storage_)) >= alignof(EnvType),
                  "env_storage_ does not meet the Env's alignment needs");
    new (&env_storage_) EnvType();
  }
  ~SingletonEnv() = default;

  SingletonEnv(const SingletonEnv&) = delete;
  SingletonEnv& operator=(const SingletonEnv&) = delete;

  leveldb::Env* env() { return reinterpret_cast<leveldb::Env*>(&env_storage_); }

 private:
  typename std::aligned_storage<sizeof(EnvType), alignof(EnvType)>::type
      env_storage_;
};

}  // namespace database
