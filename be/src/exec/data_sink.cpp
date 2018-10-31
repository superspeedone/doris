// Modifications copyright (C) 2017, Baidu.com, Inc.
// Copyright 2017 The Apache Software Foundation

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "exec/data_sink.h"

#include <string>
#include <map>
#include <memory>

#include "exec/exec_node.h"
#include "exec/olap_table_sink.h"
#include "exprs/expr.h"
#include "gen_cpp/PaloInternalService_types.h"
#include "runtime/data_stream_sender.h"
#include "runtime/result_sink.h"
#include "runtime/mysql_table_sink.h"
#include "runtime/data_spliter.h"
#include "runtime/export_sink.h"
#include "runtime/runtime_state.h"
#include "util/logging.h"

namespace palo {

Status DataSink::create_data_sink(
        ObjectPool* pool,
        const TDataSink& thrift_sink,
        const std::vector<TExpr>& output_exprs,
        const TPlanFragmentExecParams& params,
        const RowDescriptor& row_desc,
        boost::scoped_ptr<DataSink>* sink) {
    DataSink* tmp_sink = NULL;

    switch (thrift_sink.type) {
    case TDataSinkType::DATA_STREAM_SINK: {
        if (!thrift_sink.__isset.stream_sink) {
            return Status("Missing data stream sink.");
        }

        // TODO: figure out good buffer size based on size of output row
        tmp_sink = new DataStreamSender(
                pool, params.sender_id, row_desc,
                thrift_sink.stream_sink, params.destinations, 16 * 1024);
        // RETURN_IF_ERROR(sender->prepare(state->obj_pool(), thrift_sink.stream_sink));
        sink->reset(tmp_sink);
        break;
    }
    case TDataSinkType::RESULT_SINK:
        if (!thrift_sink.__isset.result_sink) {
            return Status("Missing data buffer sink.");
        }

        // TODO: figure out good buffer size based on size of output row
        tmp_sink = new ResultSink(row_desc, output_exprs, thrift_sink.result_sink, 1024);
        sink->reset(tmp_sink);
        break;

    case TDataSinkType::MYSQL_TABLE_SINK: {
        if (!thrift_sink.__isset.mysql_table_sink) {
            return Status("Missing data buffer sink.");
        }

        // TODO: figure out good buffer size based on size of output row
        MysqlTableSink* mysql_tbl_sink = new MysqlTableSink(
            pool, row_desc, output_exprs);
        sink->reset(mysql_tbl_sink);
        break;
    }

    case TDataSinkType::DATA_SPLIT_SINK: {
        if (!thrift_sink.__isset.split_sink) {
            return Status("Missing data split buffer sink.");
        }

        // TODO: figure out good buffer size based on size of output row
        std::unique_ptr<DataSpliter> data_spliter(new DataSpliter(row_desc));
        RETURN_IF_ERROR(DataSpliter::from_thrift(pool,
                                                 thrift_sink.split_sink,
                                                 data_spliter.get()));
        sink->reset(data_spliter.release());
        break;
    }

    case TDataSinkType::EXPORT_SINK: {
        if (!thrift_sink.__isset.export_sink) {
            return Status("Missing export sink sink.");
        }

        std::unique_ptr<ExportSink> export_sink(new ExportSink(pool, row_desc, output_exprs));
        sink->reset(export_sink.release());
        break;
    }
    case TDataSinkType::OLAP_TABLE_SINK: {
        Status status;
        DCHECK(thrift_sink.__isset.olap_table_sink);
        sink->reset(new stream_load::OlapTableSink(pool, row_desc, output_exprs, &status));
        RETURN_IF_ERROR(status);
        break;
    }

    default:
        std::stringstream error_msg;
        std::map<int, const char*>::const_iterator i =
            _TDataSinkType_VALUES_TO_NAMES.find(thrift_sink.type);
        const char* str = "Unknown data sink type ";

        if (i != _TDataSinkType_VALUES_TO_NAMES.end()) {
            str = i->second;
        }

        error_msg << str << " not implemented.";
        return Status(error_msg.str());
    }

    if (sink->get() != NULL) {
        RETURN_IF_ERROR((*sink)->init(thrift_sink));
    }

    return Status::OK;
}

Status DataSink::init(const TDataSink& thrift_sink) {
    return Status::OK;
}

Status DataSink::prepare(RuntimeState* state) {
    _expr_mem_tracker.reset(new MemTracker(-1, "Data sink", state->instance_mem_tracker()));
    return Status::OK;
}

}  // namespace palo
