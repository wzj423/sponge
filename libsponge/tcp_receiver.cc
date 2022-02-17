#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();

    if (!_isn) {  // _isn not set, in LISTEN state, waiting for SYN
        if (header.syn) {
            _isn = header.seqno;  // seqno starts at SYN
            /* we cannot return here because SYN might also contains useful data */
        } else {  // discard any packets ahead of SYN
            return;
        }
    }

    /* process data */

    /* The abs seqno of first byte unknown */
    uint64_t abs_ack_no = _reassembler.stream_out().bytes_written() + 2;  // taking SYN into consideration.

    /* current packet's absolute sequence number */
    uint64_t cur_abs_seqno = unwrap(header.seqno, *_isn, abs_ack_no - 2);
    /* In your TCP implementation, you’ll use the index of the last reassembled byte as the checkpoint. -- Lab2
        that's why we substract 2 here on the checkpoint.*/

    /* the stream index of the first byte in the packet */
    uint64_t stream_index =
        (cur_abs_seqno - 1) +
        header.syn;  // note that we should skip the logical start SYN, that's why we're adding `header.syn`

    /* Hacked case fsm_ack_rst_relaxed(Lab4) : if `cur_abs_seqno` and `header.syn` all equals zero, i.e. a malicious
     * packet,then overflow might occur. So a patch here is needed.*/
    if (cur_abs_seqno + header.syn == 0)
        return;

    cerr << "Receiver received segment: payload=|" << seg.payload().copy() << "|len=" << seg.payload().copy().length()
         << " \n\t" << abs_ack_no << " " << cur_abs_seqno << " " << stream_index << endl;
    cerr << "\tSYN=" << seg.header().syn << " ACK=" << seg.header().ack << " FIN=" << seg.header().fin << endl;
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
    cerr << "After push_string, _reassembler has " << _reassembler.unassembled_bytes() << "unassembled bytes" << endl;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn) {
        return std::nullopt;
    }
    uint64_t abs_ack_no = _reassembler.stream_out().bytes_written() + 1;  // taking SYN into consideration.
    if (_reassembler.stream_out().input_ended())
        ++abs_ack_no;  // taking FIN into consideration.

    return *_isn + abs_ack_no;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
