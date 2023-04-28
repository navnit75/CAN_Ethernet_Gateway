
#pragma once
#define BIT(n)				(1ULL << n)
#define BITMASK(len)		(BIT(len) - 1)

/* Get value from the bits within 'bitmap' represented by 'mask'. The 'mask'
 * parameter must be a continuous bit mask (e.g. 0b00111000). This macro
 * doesn't work with non-continuous bit masks (e.g. 0b00101001).
 */
#define BITMAP_GET_VALUE(bitmap, mask, shift) \
			((bitmap & mask) >> shift)

/* Set the value 'val' in the 'bitmap' variable at the position represented by
 * 'mask'.
 */
#define BITMAP_SET_VALUE(bitmap, val, mask, shift) \
			(bitmap = (bitmap & ~mask) | ((val << shift) & mask))

struct __una_u32 { uint32_t x; } __attribute__((packed));

static inline uint32_t get_unaligned_be32(const void *p)
{
    const struct __una_u32 *ptr = (const struct __una_u32 *)p;
    return ntohl(ptr->x);
}

static inline void put_unaligned_be32(uint32_t val, void *p)
{
    struct __una_u32 *ptr = (struct __una_u32 *)p;
    ptr->x = htonl(val);
}
