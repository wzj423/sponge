#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    std::map<size_t, std::string> _unassembled_strs;  //
    size_t
        _eof_idx;  //!< Index of the eof byte flag,namely the next byte after the last byte and is default set to SIZE_MAX).
    bool _is_eof_set;

    size_t
        _unassembled_byte_idx;  //!< Index of the first unassembled byte ,which starts from zero and is needless to be accpted
    size_t _unassembled_range_length;
    size_t
        _unassembled_bytes_num;  //!< Number of unassembled bytes. The different between it and `_unassembled_range_length` is that it will not count blanks between two data slices.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    inline size_t current_max_byte_idx() const;

    inline void update_unassembled_bytes_status();
    inline void update_eof_status();

    std::pair<size_t, std::string> filter_insertable_string(std::string data, uint64_t index) const;

    void insert_string_into_map(const std::string &data, const uint64_t index);

    /* Merge two (index,data) slice entries, return the merged new entry, assuming that index_x <= index_y and the two
     * entries can be merged. */
    std::pair<size_t, std::string> merge_entry(const std::pair<size_t, std::string> &x,
                                               const std::pair<size_t, std::string> &y);

    //! \brief Assemble string from slices in `_unassembled_strs` and push it into `_output` .
    //! \note This function will also update `_unassembled_bytes_idx`and `_unassembled_range_length`.
    //!  Nothing will happen if strings in `_unassembled_strs` cannot be assembled.
    void assemble_string();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

class CapacityOverflowException : public std::exception {
  public:
    const char *what() const noexcept override { return "Exceeded Maximum Capacity."; }
};
class StringEntryUnmergeableException : public std::exception {
  public:
    const char *what() const noexcept override { return "The given string entries are not mergeable."; }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
