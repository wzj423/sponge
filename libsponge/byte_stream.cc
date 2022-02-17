#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _queue(capacity + 1)
    , _head(0)
    , _rear(0)
    , _written_size(0)
    , _read_size(0)
    , _buffer_size(0)
    , _capacity_size(capacity)
    , _end_input(false)
    , _error(false) {
    //_queue.reserve(capacity + 1);
}

size_t ByteStream::write(const string &data) {
    if (_end_input) {
        return 0;
    }
    size_t size_to_write = min(data.size(), _capacity_size - _buffer_size);

    for (size_t i = 0; i < size_to_write; ++i) {
        _rear = (_rear + 1) < _capacity_size ? _rear + 1 : 0;
        _queue[_rear] = data[i];
    }
    _written_size += size_to_write;
    _buffer_size += size_to_write;
    return size_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string output;
    size_t pop_size = min(len, _buffer_size), target = _head;
    for (size_t i = 0; i < pop_size; ++i) {
        // auto t = (_head + i + 1) % _capacity_size;
        target = target + 1 == _capacity_size ? 0 : target + 1;
        output += _queue[target];
    }
    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_size = min(len, _buffer_size);
    _head = (_head + pop_size) % _capacity_size;
    _buffer_size -= pop_size;
    _read_size += pop_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! If no "len" bytes available, then `read` will return every byte in stream.
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string data = this->peek_output(len);
    this->pop_output(len);
    return data;
}

// void ByteStream::end_input() {}

// bool ByteStream::input_ended() const { return {}; }

size_t ByteStream::buffer_size() const { return _buffer_size; }

bool ByteStream::buffer_empty() const { return 0 == _buffer_size; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _written_size; }

size_t ByteStream::bytes_read() const { return _read_size; }

size_t ByteStream::remaining_capacity() const { return _capacity_size - _buffer_size; }