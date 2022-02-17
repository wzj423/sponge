#include "tcp_connection.hh"

#include <cassert>
#include <iostream>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;

    bool ack_needed = seg.length_in_sequence_space();  // if the incoming segment occupied any sequence numbers, the
                                                       // TCPConnection makes sure that at least one segment is sent in
                                                       // reply, to reflect an update in the ackno and window size.

    if (seg.header().rst) {
        reset(false);
        return;
    }

    _receiver.segment_received(seg);

    assert(_sender.segments_out().empty());

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        /* ack_received() will call fill_window, thus sending a packet.
        The ACK hustle has been taken by TCPSender and TCPReceiver, so calling ack_received here is enough and we dont
        have to worry about the FSM state transition caused by this ack.

        Besides, things are almost the same when it comes to FIN except there we need to maintain _is_active and
        `_linger_after_streams_finish` as they are not contained in sender or receiver. It is a little bit more
        complicated for SYN because sender and receiver only consider SYN in one direction, so to make a complete 3-way
        handshake we need to tell sender to send a SYN if one received. */

        if (_sender.segments_out().empty() == false &&
            ack_needed) {  // if ack_received() ALREADY sends a packet, then no more empty acks needed.
            ack_needed = false;
        }

        // send_segments_from_sender(); // delay sending possible packet until end of the function.
    }

    // sender and receiver are robust enough so we donnot worry about ACK, however in the three-way-handshake there is
    // SYN left to be tackled, that is, when moving from LISTEN to SYN, another SYN should be manually added.
    // but where is the ACK in the SYNACK? Well, send_segment_from_sender() will detect changes caused by
    // _receiver.segment_received() and automatically add that ACK.

    // if received a SYN:
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        /* state described beneath doesn't match any state in TCP FSM, that's because only the _receiver will undergo a
         * transition if SYN is received. So we call connnect() to send the SYN, as stated beneath ACK will be added
         * later.*/
        connect();
        // here sender state should be SYN_SENT, connection in SYN_SENT.
        return;
    }
    /* that's all about 3-way handshake, moving on to clean closing.
        According to section 5.1 of the tutorial, TIME_WAIT is only needed for active shutdown, therefore,
       `_linger_after_streams_finish` should be set to false if CLOSE_WAIT(passive shutdown) is detected.
    */

    /* First we cope with lingering, if CLOSE_WAIT or LAST_ACK, no lingering needed. However, we only detect CLOSE_WAIT
     * here because it is hard to tell LAST_ACK and CLOSING apart, mistaking active close as passive close.*/
    if ((TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
         TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)) {
        _linger_after_streams_finish = false;
    }

    /* do the actual shutdown. It is worth noting that active closing side always exit in procedure tick(), so here only
     * copes with passive closing and closing before ESTABLISHD. From tcp_state.cc we can know that here _linger... must
     * be false.  */
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish == false) {
        _is_active = false;
        return;  // closed
    }

    // empty-ack keep-alive
    if (ack_needed) {
        _sender.send_empty_segment();
    } else if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
               seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        assert(false);
    }
    send_segments_from_sender();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    auto actual_bytes_written = _sender.stream_in().write(data);
    // send data through sender
    _sender.fill_window();
    send_segments_from_sender();
    return actual_bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    send_segments_from_sender();
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        reset(true);
        return;
    }

    /* 5.1 TIME WAIT */
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // FIN is needed to be send immediately. (TEST 36#,37# active_close)
    _sender.fill_window();
    send_segments_from_sender();
}

void TCPConnection::connect() {
    _sender.fill_window();  // send a SYN
    send_segments_from_sender();

    _is_active = true;  // connected, become active.
}

void TCPConnection::send_segments_from_sender() {
    while (!_sender.segments_out().empty()) {
        // extract segment from _sender
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = *_receiver.ackno();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::reset(bool send_reset_segment = false) {  // unclean shutdown
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
    _linger_after_streams_finish = false;  // If the inbound stream ends before the TCPConnection has reached EOF on its
                                           // outbound stream, this variable needs to be set to false.
    if (send_reset_segment) {
        while (!_sender.segments_out().empty())
            _sender.segments_out().pop();
        while (!_segments_out.empty())
            _segments_out.pop();
        // clean up outgoing segment queues, making sure previous pushed segments don't interrupt RST segment.

        _sender.send_empty_segment();
        _sender.segments_out().front().header().rst = true;

        send_segments_from_sender();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            reset(true);  // true to send a RST.
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
