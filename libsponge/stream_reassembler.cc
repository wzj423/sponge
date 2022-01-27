#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _eof_idx(SIZE_MAX)
    , _is_eof_set(false)
    , _unassembled_byte_idx(0)
    , _unassembled_range_length(0)
    , _unassembled_bytes_num(0)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto [filtered_index, filtered_data] = filter_insertable_string(data, index);

    if (eof) {
        _eof_idx = index + data.size();
        _is_eof_set = true;
        update_eof_status();
    }
    if (!filtered_data.size()) {
        return;  // discard empty string slices.
    }
    insert_string_into_map(filtered_data, filtered_index);
    assemble_string();
}

inline size_t StreamReassembler::current_max_byte_idx() const {
    return _unassembled_byte_idx + _capacity - _output.buffer_size() - 1;
}

void StreamReassembler::update_unassembled_bytes_status() {
    if (_unassembled_strs.empty()) {
        _unassembled_range_length = 0;
    } else {
        auto last_slice = *_unassembled_strs.crbegin();
        size_t last_byte_idx = last_slice.first + last_slice.second.size() - 1;
        _unassembled_range_length = last_byte_idx - _unassembled_byte_idx + 1;
    }
}

inline void StreamReassembler::update_eof_status() {
    if (_is_eof_set && _unassembled_byte_idx == _eof_idx) {  // EOF has been set and there's nothing more to assemble.
        _output.end_input();
    }
}

std::pair<size_t, std::string> StreamReassembler::filter_insertable_string(std::string data, uint64_t index) const {
    if (index < _unassembled_byte_idx) {                     // cut head
        if (index + data.size() <= _unassembled_byte_idx) {  // strings already assembled.
            data = {};
            index = 0;
        } else {  // part of the string has not been assembled.
            data = data.substr(_unassembled_byte_idx - index);
            index = _unassembled_byte_idx;
        }
    }
    if (index + data.size() - 1 > current_max_byte_idx()) {  // cut tail
        data = data.substr(0, current_max_byte_idx() - index + 1);
    }
    return {index, data};
}

std::pair<size_t, std::string> StreamReassembler::merge_entry(const std::pair<size_t, std::string> &x,
                                                              const std::pair<size_t, std::string> &y) {
    if (x.first > y.first) {
        return merge_entry(y, x);
    }
    size_t l1 = x.first, r1 = x.first + x.second.size() - 1;
    size_t l2 = y.first, r2 = y.first + y.second.size() - 1;

    if (r1 < l2 - 1) {
        throw StringEntryUnmergeableException();
    }
    if (r1 >= r2)
        return x;

    size_t merged_index = l1;
    string merged_data = x.second + y.second.substr(r1 - l2 + 1);
    return std::make_pair(merged_index, merged_data);
}

/*Insert a string slice into `_unassembled_strs`, guaranteed that index >= _unassembled_byte_idx*/
void StreamReassembler::insert_string_into_map(const std::string &data, const uint64_t index) {
    auto right_end_of_entry = [](const std::pair<size_t, std::string> &p) { return p.first + p.second.size() - 1; };
    auto left_end_of_entry = [](const std::pair<size_t, std::string> &p) { return p.first; };

    if (_unassembled_strs.empty()) {
        _unassembled_strs[index] = data;
        _unassembled_bytes_num += data.size();
    } else {
        size_t left = index, right = index + data.size() - 1;

        auto left_iter = _unassembled_strs.lower_bound(left);
        auto right_iter = _unassembled_strs.upper_bound(right);

        if (left_iter != _unassembled_strs.begin() &&
            right_end_of_entry(*std::prev(left_iter)) >= index - 1) {  //要考虑到左侧恰好重合的情况
            --left_iter;
        }
        if (right_iter != _unassembled_strs.end() &&
            left_end_of_entry(*right_iter) == index + data.size()) {  //要考虑到右侧恰好重合的情况
            ++right_iter;
        }

        std::pair<size_t, std::string> merged_entry{index, data};
        for (auto iter = left_iter; iter != right_iter; ++iter) {
            merged_entry = merge_entry(merged_entry, *iter);

            _unassembled_bytes_num -= iter->second.size();
        }

        _unassembled_strs.erase(left_iter, right_iter);
        _unassembled_strs[merged_entry.first] = merged_entry.second;

        _unassembled_bytes_num += merged_entry.second.size();
    }
    update_unassembled_bytes_status();  // Maintain status
}

void StreamReassembler::assemble_string() {
    if (_unassembled_strs.empty() ||
        _unassembled_strs.begin()->first != _unassembled_byte_idx) {  // Nothing to assemble
        return;
    }
    auto first_slice = *_unassembled_strs.begin();
    auto first_str = first_slice.second;
    _unassembled_strs.erase(_unassembled_strs.begin());

    _output.write(first_str);
    _unassembled_byte_idx += first_str.size();
    _unassembled_bytes_num -= first_str.size();
    update_unassembled_bytes_status();
    update_eof_status();
}

// public functions
size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes_num; }

bool StreamReassembler::empty() const { return _unassembled_range_length == 0; }
