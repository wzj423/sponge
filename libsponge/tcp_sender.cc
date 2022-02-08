#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _rto(retx_timeout)
    , _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    DUMMY_CODE(_initial_retransmission_timeout, _is_fin_set, _is_syn_set, _flight_seg, _rto, _last_window_size);
}

uint64_t TCPSender::bytes_in_flight() const { return _flight_bytes_num; }

void TCPSender::fill_window() {
    size_t actual_window_size = _last_window_size ? _last_window_size : 1;
    while (_flight_bytes_num < actual_window_size) {  // still some room to send data
        TCPSegment seg;
        if (_is_syn_set == false) {   // set SYN
            seg.header().syn = true;  // set SYN in header
            _is_syn_set = true;
        }

        seg.header().seqno = next_seqno();

        const size_t payload_size = /* remember to leave space for SYN */
            min(TCPConfig::MAX_PAYLOAD_SIZE, actual_window_size - _flight_bytes_num - static_cast<size_t>(_is_syn_set));
        string payload=this->_stream.read(payload_size);

    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    DUMMY_CODE(ackno, window_size);
    uint64_t abs_seqno = unwrap(ackno, _isn, next_seqno_absolute());
    if (abs_seqno > next_seqno_absolute()) {  // unsuccessful ackno
        return;
    }
    for (auto iter = _flight_seg.begin(); iter != _flight_seg.end();) {
        const auto &seg = iter->second;
        auto seg_index = iter->first;

        if (seg_index + seg.length_in_sequence_space() <= abs_seqno) {
            _flight_bytes_num -= seg.length_in_sequence_space();
            iter = _flight_seg.erase(iter);

            _rto = _initial_retransmission_timeout; // reset retrans-counter
            _since_last_resend_time = 0;
        } else
            break;  // only part of the first outstanding seg were transmitted.
    }
    _consecutive_retransmissions_count = 0;

    _last_window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _total_lifetime += ms_since_last_tick;
    _since_last_resend_time += ms_since_last_tick;

    if (!_flight_seg.empty() && _since_last_resend_time > _rto) {  // retransmission
        auto &first_segment = *_flight_seg.begin();

        /* non-zero window size means network conjestion, exp grow RTO;
         otherwise it simply means receiver cannot receive more data, so no RTO
          growth is needed.*/
        if (_last_window_size > 0)
            _rto *= 2;

        // resend
        _segments_out.push(first_segment.second);

        // reset counter
        _since_last_resend_time = 0;

        // record resend times
        ++_consecutive_retransmissions_count;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    // set correct sequence number
    empty_segment.header().seqno = next_seqno();

    // send & no trace as "outstanding"
    _segments_out.push(empty_segment);
}
