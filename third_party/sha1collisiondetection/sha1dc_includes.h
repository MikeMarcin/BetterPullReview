#ifndef BPR_THIRD_PARTY_SHA1COLLISIONDETECTION_SHA1DC_INCLUDES_H
#define BPR_THIRD_PARTY_SHA1COLLISIONDETECTION_SHA1DC_INCLUDES_H

/* [[BPR-1450]]. The only standard headers sha1collisiondetection is given:
   memory functions, fixed-width integers, and abort(), which its
   recompression switch calls for a step index that cannot occur. Git builds
   it the same way, with SHA1DC_NO_STANDARD_INCLUDES and its own compatibility
   header; nothing here does I/O. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#endif
