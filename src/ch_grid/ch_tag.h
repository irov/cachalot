#ifndef CH_TAG_H_
#define CH_TAG_H_

#include "ch_config.h"

#include "hb_utils/hb_time.h"
#include "hb_utils/hb_ring.h"

#define CH_TAG_VALUE_MAX 32

typedef struct ch_tag_t
{
    HB_RING_DECLARE( ch_tag_t );

    hb_time_t created_timestamp;

    char value[CH_TAG_VALUE_MAX];

} ch_tag_t;

#endif