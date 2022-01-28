#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    DUMMY_CODE(n, isn);
    /*constexpr uint64_t WrappingInt32Mod = (uint64_t(1)) + std::numeric_limits<uint32_t>::max();
    return WrappingInt32((n % WrappingInt32Mod + isn.raw_value()) % WrappingInt32Mod);*/

    // or we can exploit overflow
    //std::cerr<<"Wrap:"<<static_cast<decltype(isn.raw_value())>(n) + isn.raw_value()<<endl;
    return WrappingInt32(static_cast<decltype(isn.raw_value())>(n) + isn.raw_value());
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    constexpr uint64_t INT32_RANGE = 1ll + std::numeric_limits<uint32_t>::max();
    constexpr uint64_t INT32_HALF_RANGE = INT32_RANGE>>1;
    constexpr uint64_t INT32_MASK=~static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());

    uint32_t min_abs_seqno = n - isn; /* unsigned substraction overflow == modular */

    if (checkpoint < min_abs_seqno)
        return min_abs_seqno;
    else {
        uint64_t less_abs_seqno = ((checkpoint - min_abs_seqno) &INT32_MASK) + min_abs_seqno;
        uint64_t greater_abs_seqno = less_abs_seqno + INT32_RANGE;
        if (static_cast<uint32_t>(checkpoint - min_abs_seqno) < INT32_HALF_RANGE) {
            return less_abs_seqno;
        } else {
            return greater_abs_seqno;
        }
    }
}
