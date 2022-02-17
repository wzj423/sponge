#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

#define PRTVAR(x) std::cerr << (#x) << " = " << x << std::endl;

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , ticker(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _flight_bytes_num; }

void TCPSender::fill_window() {
    size_t actual_window_size = _last_window_size ? _last_window_size : 1;
    while (_flight_bytes_num < actual_window_size) {  // still some room to send data
        TCPSegment seg;
        if (_is_syn_set == false) {   // "CLOSED", set SYN
            seg.header().syn = true;  // set SYN in header
            _is_syn_set = true;
        }

        seg.header().seqno = next_seqno();

        /* payload_max_size is the maximal number of bytes can be sent in a seg, it is ok for _stream not to have
         * sufficient bytes to read, that is to say, payload_max_size >= payload.size()*/
        const size_t payload_max_size = /* remember to leave space for SYN */
            min(TCPConfig::MAX_PAYLOAD_SIZE,
                actual_window_size - _flight_bytes_num - static_cast<size_t>(seg.header().syn));
        string payload = this->_stream.read(payload_max_size);

        if (!_is_fin_set && _stream.eof() && payload.size() + _flight_bytes_num < actual_window_size) {
            _is_fin_set = seg.header().fin = true;
        }
        seg.payload() = Buffer(std::move(payload));

        if (seg.length_in_sequence_space() == 0)
            break;

        if (_flight_seg.empty()) {  // `seg` is the first seg waiting, restarting retrans-counter for it
            ticker.reset_restart();
        }

        // send & update
        _segments_out.push(seg);
        _flight_bytes_num += seg.length_in_sequence_space();
        _flight_seg.insert(std::make_pair(next_seqno_absolute(), seg));
        _next_seqno += seg.length_in_sequence_space();

        if (_is_fin_set) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_seqno = unwrap(ackno, _isn, next_seqno_absolute());
    if (abs_seqno > next_seqno_absolute()) {  // unsuccessful ackno
        return;
    }
    for (auto iter = _flight_seg.begin(); iter != _flight_seg.end();) {
        const auto &seg = iter->second;
        auto seg_index = iter->first;

        if (seg_index + seg.length_in_sequence_space() <=
            abs_seqno) {  // at least one (always the first if any) seg has been successfully transmitted.
            // update status
            _flight_bytes_num -= seg.length_in_sequence_space();
            iter = _flight_seg.erase(iter);

            ticker.reset_restart();
        } else
            break;  // only part of the first outstanding seg were transmitted.
    }
    _consecutive_retransmissions_count = 0;

    _last_window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    ticker.tick(ms_since_last_tick);

    if (!_flight_seg.empty() && ticker.triggered()) {  // retransmission
        auto &first_segment = *_flight_seg.begin();

        /* non-zero window size means network conjestion, exp grow RTO;
         otherwise it simply means receiver cannot receive more data, so no RTO
          growth is needed.
          (Addition, one exception is that SYN set but not ACK received, which means no window
          size updated. However window size must be set 1 there. I choose to explicitly check this,
          another solution is to init _last_window_size as 1)*/
        if (_last_window_size > 0 || first_segment.second.header().syn)
            ticker.grow();

        // resend
        _segments_out.push(first_segment.second);

        // restart counter
        ticker.restart();  //_since_last_resend_time = 0;

        // record resend times
        ++_consecutive_retransmissions_count;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    // set correct sequence number
    empty_segment.header().seqno = next_seqno();
    // send & do not trace as "outstanding"
    _segments_out.push(empty_segment);
}

TCPSender::_SenderState TCPSender::_state() const {
    if (stream_in().error()) {
        return _SenderState::ERROR;
    } else if (next_seqno_absolute() == 0) {
        return _SenderState::CLOSED;
    } else if (next_seqno_absolute() == bytes_in_flight()) {
        return _SenderState::SYN_SENT;
    } else if (not stream_in().eof()) {
        return _SenderState::SYN_ACKED;
    } else if (next_seqno_absolute() < stream_in().bytes_written() + 2) {
        return _SenderState::SYN_ACKED_EOF;
    } else if (bytes_in_flight()) {
        return _SenderState::FIN_SENT;
    } else {
        return _SenderState::FIN_ACKED;
    }
}