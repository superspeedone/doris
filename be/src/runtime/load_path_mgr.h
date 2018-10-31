// Copyright (c) 2017, Baidu.com, Inc. All Rights Reserved

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef BDG_PALO_BE_SRC_RUNTIME_LOAD_PATH_MGR_H
#define BDG_PALO_BE_SRC_RUNTIME_LOAD_PATH_MGR_H

#include <pthread.h>
#include <string>
#include <vector>
#include <mutex>
#include "common/status.h"

namespace palo {

class TUniqueId;
class ExecEnv;

// In every directory, '.trash' directory is used to save data need to delete
// daemon thread is check no used directory to delete
class LoadPathMgr {
public:
    LoadPathMgr(ExecEnv* env);

    ~LoadPathMgr() {
    }

    Status init();

    Status allocate_dir(const std::string& db, const std::string& label, std::string* prefix);

    void get_load_data_path(std::vector<std::string>* data_paths);

    Status get_load_error_file_name(
            const std::string& db,
            const std::string&label,
            const TUniqueId& fragment_instance_id,
            std::string* error_path);
    std::string get_load_error_absolute_path(const std::string& file_path);
    const std::string& get_load_error_file_dir() const {
        return _error_log_dir;
    }

private:
    bool is_too_old(time_t cur_time, const std::string& label_dir);
    void clean_one_path(const std::string& path);
    void clean_error_log();
    void clean();
    void process_path(time_t now, const std::string& path);

    static void* cleaner(void* param);

    ExecEnv* _exec_env;
    std::mutex _lock;
    std::vector<std::string> _path_vec;
    int _idx;
    int _reserved_hours;
    pthread_t _cleaner_id;
    std::string _error_log_dir;
    uint32_t _next_shard;
    uint32_t _error_path_next_shard;
};

}

#endif
