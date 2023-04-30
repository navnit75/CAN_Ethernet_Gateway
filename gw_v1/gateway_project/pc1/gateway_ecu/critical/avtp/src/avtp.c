#include <arpa/inet.h>
#include <stddef.h>

#include "avtp.h"
#include "util.h"

#define SHIFT_SUBTYPE			(31 - 7)
#define SHIFT_VERSION			(31 - 11)

#define MASK_SUBTYPE			(BITMASK(8) << SHIFT_SUBTYPE)
#define MASK_VERSION			(BITMASK(3) << SHIFT_VERSION)

int avtp_pdu_get(const struct avtp_common_pdu *pdu, enum avtp_field field,
                 uint32_t *val)
{
    uint32_t bitmap, mask;
    uint8_t shift;

    if (!pdu || !val)
        return -EINVAL;

    switch (field) {
        case AVTP_FIELD_SUBTYPE:
            mask = MASK_SUBTYPE;
            shift = SHIFT_SUBTYPE;
            break;
        case AVTP_FIELD_VERSION:
            mask = MASK_VERSION;
            shift = SHIFT_VERSION;
            break;
        default:
            return -EINVAL;
    }

    bitmap = ntohl(pdu->subtype_data);

    *val = BITMAP_GET_VALUE(bitmap, mask, shift);

    return 0;
}

int avtp_pdu_set(struct avtp_common_pdu *pdu, enum avtp_field field,
                 uint32_t value)
{
    uint32_t bitmap, mask;
    uint8_t shift;

    if (!pdu)
        return -EINVAL;

    switch (field) {
        case AVTP_FIELD_SUBTYPE:
            mask = MASK_SUBTYPE;
            shift = SHIFT_SUBTYPE;
            break;
        case AVTP_FIELD_VERSION:
            mask = MASK_VERSION;
            shift = SHIFT_VERSION;
            break;
        default:
            return -EINVAL;
    }

    bitmap = ntohl(pdu->subtype_data);

    BITMAP_SET_VALUE(bitmap, value, mask, shift);

    pdu->subtype_data = htonl(bitmap);

    return 0;
}
