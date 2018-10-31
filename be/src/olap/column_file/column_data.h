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

#ifndef BDG_PALO_BE_SRC_OLAP_COLUMN_FILE_COLUMN_DATA_H
#define BDG_PALO_BE_SRC_OLAP_COLUMN_FILE_COLUMN_DATA_H

#include <string>

#include "olap/i_data.h"
#include "olap/row_block.h"
#include "olap/row_cursor.h"

namespace palo {

class OLAPTable;

namespace column_file {

class SegmentReader;

// This class is column data reader. this class will be used in two case.
class ColumnData : public IData {
public:
    explicit ColumnData(Rowset* olap_index);
    virtual ~ColumnData();

    virtual OLAPStatus init();

    OLAPStatus prepare_block_read(
            const RowCursor* start_key, bool find_start_key,
            const RowCursor* end_key, bool find_end_key,
            RowBlock** first_block) override;

    OLAPStatus get_next_block(RowBlock** row_block) override;

    virtual void set_read_params(
            const std::vector<uint32_t>& return_columns,
            const std::set<uint32_t>& load_bf_columns,
            const Conditions& conditions,
            const std::vector<ColumnPredicate*>& col_predicates,
            const std::vector<RowCursor*>& start_keys,
            const std::vector<RowCursor*>& end_keys,
            bool is_using_cache,
            RuntimeState* runtime_state);

    virtual OLAPStatus get_first_row_block(RowBlock** row_block);
    virtual OLAPStatus get_next_row_block(RowBlock** row_block);

    OLAPStatus pickle() override { return OLAP_SUCCESS; }
    OLAPStatus unpickle() override { return OLAP_SUCCESS; }

    // Only used to binary search in full-key find row
    const RowCursor* seek_and_get_current_row(const RowBlockPosition& position);

    virtual uint64_t get_filted_rows();

private:
    DISALLOW_COPY_AND_ASSIGN(ColumnData);

    // To compatable with schmea change read, use this function to init column data
    // for schema change read. Only called in get_first_row_block
    OLAPStatus _schema_change_init();

    // Try to seek to 'key'. If this funciton returned with OLAP_SUCCESS, current_row()
    // point to the first row meet the requirement.
    // If there is no such row, OLAP_ERR_DATA_EOF will return.
    // If error happend, other code will return
    OLAPStatus _seek_to_row(const RowCursor& key, bool find_key, bool is_end_key);

    // seek to block_pos without load that block, caller must call _get_block()
    // to load _read_block with data. If without_filter is false, this will seek to
    // other block. Because the seeked block may be filtered by condition or delete.
    OLAPStatus _seek_to_block(const RowBlockPosition &block_pos, bool without_filter);

    OLAPStatus _find_position_by_short_key(
            const RowCursor& key, bool find_last_key, RowBlockPosition *position);
    OLAPStatus _find_position_by_full_key(
            const RowCursor& key, bool find_last_key, RowBlockPosition *position);

    // Used in _seek_to_row, this function will goto next row that vaild for this
    // ColumnData
    OLAPStatus _next_row(const RowCursor** row, bool without_filter);

    // get block from reader, just read vector batch from _current_segment.
    // The read batch return by got_batch.
    OLAPStatus _get_block_from_reader(
        VectorizedRowBatch** got_batch, bool without_filter);

    // get block from segment reader. If this function returns OLAP_SUCCESS
    OLAPStatus _get_block(bool without_filter);

    const RowCursor* _current_row() {
        _read_block->get_row(_read_block->pos(), &_cursor);
        return &_cursor;
    }
private:
    OLAPTable* _table;
    // whether in normal read, use return columns to load block
    bool _is_normal_read = false;
    bool _end_key_is_set = false;
    bool _is_using_cache;
    bool _segment_eof = false;
    bool _need_eval_predicates = false;

    std::vector<uint32_t> _return_columns;
    std::vector<uint32_t> _seek_columns;
    std::set<uint32_t> _load_bf_columns;
    
    SegmentReader* _segment_reader;

    std::unique_ptr<VectorizedRowBatch> _seek_vector_batch;
    std::unique_ptr<VectorizedRowBatch> _read_vector_batch;

    std::unique_ptr<RowBlock> _read_block = nullptr;
    RowCursor _cursor;
    RowCursor _short_key_cursor;

    // Record when last key is found
    uint32_t _current_block = 0;
    uint32_t _current_segment;
    uint32_t _next_block;

    uint32_t _end_segment;
    uint32_t _end_block;
    int64_t _end_row_index = 0;

    size_t _num_rows_per_block;
};

class ColumnDataComparator {
public:
    ColumnDataComparator(
        RowBlockPosition position,
        ColumnData* olap_data,
        const Rowset* index)
            : _start_block_position(position),
            _olap_data(olap_data),
            _index(index) {}

    ~ColumnDataComparator() {}

    // less comparator function
    bool operator()(const iterator_offset_t& index, const RowCursor& key) const {
        return _compare(index, key, COMPARATOR_LESS);
    }
    // larger comparator function
    bool operator()(const RowCursor& key, const iterator_offset_t& index) const {
        return _compare(index, key, COMPARATOR_LARGER);
    }

private:
    bool _compare(
            const iterator_offset_t& index,
            const RowCursor& key,
            ComparatorEnum comparator_enum) const {
        OLAPStatus res = OLAP_SUCCESS;
        RowBlockPosition position = _start_block_position;
        if (OLAP_SUCCESS != (res = _index->advance_row_block(index, &position))) {
            OLAP_LOG_FATAL("fail to advance row block. [res=%d]", res);
            throw ComparatorException();
        }
        const RowCursor* helper_cursor = _olap_data->seek_and_get_current_row(position);
        if (helper_cursor == nullptr) {
            OLAP_LOG_WARNING("fail to seek and get current row.");
            throw ComparatorException();
        }

        if (COMPARATOR_LESS == comparator_enum) {
            return helper_cursor->cmp(key) < 0;
        } else {
            return helper_cursor->cmp(key) > 0;
        }
    }

    const RowBlockPosition _start_block_position;
    ColumnData* _olap_data;
    const Rowset* _index;
};

}  // namespace column_file
}  // namespace palo

#endif // BDG_PALO_BE_SRC_OLAP_COLUMN_FILE_COLUMN_DATA_H
