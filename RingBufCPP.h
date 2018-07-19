#ifndef EM_RINGBUF_CPP_H
#define EM_RINGBUF_CPP_H

#include "RingBufHelpers.h"

/**
 * A simple ring (FIFO) buffer with concurrency protection built in, thus being
 * safe to perform operations on the buffer inside of ISR's. All memory is
 * statically allocated at compile time, so no heap memory is used. It can
 * buffer any fixed size object (ints, floats, structs, objects, etc...).
 *
 * @tparam T    Type of the elements.
 *              Aliased as member type RingBufCPP::value_type.
 * @tparam Size The maximum number of elements buffer can hold as content.
 *              Note that the allocated memory size will be at least
 *              `Size * sizeof(T)`.
 */
template<typename T, size_t Size>
class RingBufCPP {
public:
    /** Type of the elements (the first template parameter, `T`). */
    typedef T value_type;
    /** An unsigned integral type that can represent any non-negative value. */
    typedef size_t size_type;

    /** Construct the ring/circular buffer. */
    RingBufCPP() {
        clear();
    }

    // Prevent copying and moving instances by deleting copy/move
    // constructors and assignment operators.

    /** Copy constructor. */
    RingBufCPP(const RingBufCPP &other) noexcept = delete;

    /** Move constructor. */
    RingBufCPP(RingBufCPP &&other) noexcept = delete;

    /** Copy assignment operator. */
    RingBufCPP &operator=(const RingBufCPP &other) noexcept = delete;

    /** Move assignment operator. */
    RingBufCPP &operator=(RingBufCPP &&other) noexcept = delete;

    // Disable heap (dynamic) allocation by deleting `new`/`delete` operators.

    void *operator new(size_t) = delete;

    void *operator new[](size_t) = delete;

    void operator delete(void *) = delete;

    void operator delete[](void *) = delete;


    // Capacity (size, max_size, [resize], empty, [shrink_to_fit])

    /**
     * @return The maximum number of elements this buffer can hold at the same
     *         time.
     */
    static constexpr size_type max_size() noexcept {
        return Size;
    }

    /**
     * @return The number of elements currently in the buffer.
     */
    constexpr size_type size() const noexcept {
        return _numElements;
    }

    /**
     * Test whether this buffer is empty.
     *
     * @return true where are no elements in the buffer (`size() == 0`),
     *         false otherwise.
     */
    constexpr bool empty() const noexcept {
        return size() == 0;
    }

    /**
     * Test whether this buffer is full.
     *
     * @return true when the buffer is full (`size() == max_size()`).
     */
    constexpr bool full() const noexcept {
        return size() >= max_size();
    }

    // Element access (operator[], at, front, back)
    // Not implemented as they return references (which cannot be nullptr)
    // or throw exceptions (not supported).

    // Modifiers (assign, push, pop, insert, erase, swap, clear)

    /**
     * Removes all elements from the buffer, effectively setting its size to 0.
     * The buffer will be empty after this call returns.
     */
    void clear() noexcept {
        RB_ATOMIC_START
            {
                _numElements = 0;
                _head = 0;
            }
        RB_ATOMIC_END
    }

    /**
     *  Add a new element to the buffer via copy/move, optionally removing the
     *  oldest if the buffer is full.
     *
     *  This effectively increases the buffer size by one.
     *
     *  @param obj   The element to add.
     *  @param force If true, then the new object is always added to the
     *               buffer even if the buffer is full - by discarding the
     *               oldest (first added) object.
     *
     *  @return true on success.
     */
    bool push(const value_type &obj, bool force = false) noexcept {
        bool ret = false;

        RB_ATOMIC_START
            {
                const bool space = !full();

                if (space || force) {
                    _buf[_head++] = obj;
                    // Wrap around if the end of the array was reached.
                    if (_head >= max_size()) {
                        _head = 0;
                    }
                    if (space) {
                        _numElements++;
                    }

                    ret = true;
                }
            }
        RB_ATOMIC_END

        return ret;
    }

    /**
     * @see push(obj, force)
     */
    bool add(const value_type &obj, bool force = false) noexcept {
        return push(obj, force);
    }

    /**
     * Retrieves, but does not remove, the n'th element in the buffer.
     *
     * @param index Index of the element to peek at. As this is FIFO buffer,
     *              the oldest element in the buffer is always at index 0 and
     *              the last added one is at the index `size() - 1`.
     *
     * @return A pointer to the index'th element or `nullptr` if there is less
     *         elements currently in the buffer than provided index.
     */
    value_type *peek(size_type index = 0) noexcept {
        value_type *ret = nullptr;

        RB_ATOMIC_START
            {
                // Make sure not out of bounds.
                if (index < size()) {
                    // TODO: Replace modulus operator with decision logic as in tail_index()
                    ret = &_buf[(tail_index() + index) % max_size()];
                }
            }
        RB_ATOMIC_END

        return ret;
    }

    /**
     * Retrieves and removes the oldest element in the buffer and copies it to
     * the destination.
     *
     * @param destination[out] Allocated object to which removed element will
     *                         be copied. If the buffer is empty (false is
     *                         returned), this object will not be altered.
     *
     * @return true on success, false if buffer is empty.
     */
    bool poll(value_type &destination) noexcept {
        bool ret = false;

        RB_ATOMIC_START
            {
                if (!empty()) {
                    destination = _buf[tail_index()];
                    _numElements--;

                    ret = true;
                }
            }
        RB_ATOMIC_END

        return ret;
    }

    /**
     * The same functionality as poll() except the object is simply discarded.
     *
     * @see poll(destination)
     */
    bool poll() noexcept {
        bool ret = false;

        RB_ATOMIC_START
            {
                if (!empty()) {
                    _numElements--;

                    ret = true;
                }
            }
        RB_ATOMIC_END

        return ret;
    }

    /**
     * Retrieves and removes the oldest element in the buffer and copies it to
     * the destination.
     *
     * @param destination[out] Pointer on the allocated object to which removed
     *                         element will be copied. If the buffer is empty
     *                         (false is returned), this object will not be
     *                         altered. If `nullptr` then the element is simply
     *                         discarded.
     *
     * @return true on success, false if buffer is empty.
     */
    bool pull(value_type *const destination = nullptr) noexcept {
        return (destination ? poll(*destination) : poll());
    }

protected:

    /**
     * Calculates the index of the oldest element in the array.
     *
     * @return index of the oldest element in the array.
     */
    constexpr size_t tail_index() const {
        return (full() ? _head
                       : (_head >= _numElements ? _head - _numElements
                                                : max_size() + _head - _numElements));
    }


    /** Underlying array. */
    value_type _buf[Size];

    /** Index of the next element slot in the underlying buffer. */
    size_type _head;
    /** @see size() */
    size_type _numElements;

};

#endif
