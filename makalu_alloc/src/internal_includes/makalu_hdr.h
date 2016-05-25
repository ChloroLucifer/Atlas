#ifndef _MAKALU_CORE_MD_H
#define _MAKALU_CORE_MD_H

#include "makalu_config.h"


typedef struct hblkhdr {

    size_t hb_sz;  /* If in use, size in bytes, of objects in the block. */
                   /* if free, the size in bytes of the whole block      */
                   /* We assume that this is convertible to signed_word  */
                   /* without generating a negative result.  We avoid    */
                   /* generating free blocks larger than that.           */
    word hb_descr;              /* object descriptor for marking.  See  */
                                /* mark.h.                              */

    struct hblk * hb_block;     /* The corresponding block.             */
    short * hb_map;
    unsigned char hb_flags;
    unsigned char hb_large_block;
    char page_reclaim_state;
    word dummy_for_cache_alignment[3];
    counter_t hb_n_marks;       /* Number of set mark bits, excluding   */
                                /* the one always set at the end.       */
                                /* Currently it is concurrently         */
                                /* updated and hence only approximate.  */
                                /* But a zero value does guarantee that */
                                /* the block contains no marked         */
                                /* objects.                             */
                                /* Ensuring this property means that we */
                                /* never decrement it to zero during a  */
                                /* collection, and hence the count may  */
                                /* be one too high.  Due to concurrent  */
                                /* updates, an arbitrary number of      */
                                /* increments, but not all of them (!)  */
                                /* may be lost, hence it may in theory  */
                                /* be much too low.                     */
                                /* The count may also be too high if    */
                                /* multiple mark threads mark the       */
                                /* same object due to a race.           */
                                /* Without parallel marking, the count  */
                                /* is accurate.                         */
    word hb_marks[MARK_BITS_SZ];
    struct hblk * hb_next;      /* Link field for hblk free list         */
                                /* and for lists of chunks waiting to be */
                                /* reclaimed.                            */
    struct hblk * hb_prev;      /* Backwards link for free list.        */
} hdr;


typedef struct bi {
    hdr * index[BOTTOM_SZ];
        /*
         * The bottom level index contains one of three kinds of values:
         * 0 means we're not responsible for this block,
         *   or this is a block other than the first one in a free block.
         * 1 < (long)X <= MAX_JUMP means the block starts at least
         *        X * HBLKSIZE bytes before the current address.
         * A valid pointer points to a hdr structure. (The above can't be
         * valid pointers due to the GET_MEM return convention.)
         */
    struct bi * asc_link;       /* All indices are linked in    */
                                /* ascending order...           */
    struct bi * desc_link;      /* ... and in descending order. */
    word key;                   /* high order address bits.     */
    struct bi * hash_link;      /* Hash chain link.             */
    word dummy_for_cache_align[4];
} bottom_index;

#define HDR_FROM_BI(bi, p) \
                ((bi)->index[((word)(p) >> LOG_HBLKSIZE) & (BOTTOM_SZ - 1)])

  /* Hash function for tree top level */
# define TL_HASH(hi) ((hi) & (TOP_SZ - 1))
  /* Hash function for tree top level */
# define TL_HASH(hi) ((hi) & (TOP_SZ - 1))
  /* Set bottom_indx to point to the bottom index for address p */
# define GET_BI(p, bottom_indx) \
      { \
          register word hi = \
              (word)(p) >> (LOG_BOTTOM_SZ + LOG_HBLKSIZE); \
          register bottom_index * _bi = MAK_top_index[TL_HASH(hi)]; \
          while (_bi -> key != hi && _bi != MAK_all_nils) \
              _bi = _bi -> hash_link; \
          (bottom_indx) = _bi; \
      }
# define GET_HDR_ADDR(p, ha) \
      { \
          register bottom_index * bi; \
          GET_BI(p, bi); \
          (ha) = &(HDR_FROM_BI(bi, p)); \
      }
# define GET_HDR(p, hhdr) { register hdr ** _ha; GET_HDR_ADDR(p, _ha); \
                            (hhdr) = *_ha; }
# define SET_HDR(p, hhdr) { register hdr ** _ha; GET_HDR_ADDR(p, _ha); \
                            MAK_STORE_NVM_PTR_ASYNC(_ha, (hhdr)); }
# define SET_HDR_NO_LOG(p, hhdr) { register hdr ** _ha; GET_HDR_ADDR(p, _ha); \
                            MAK_NO_LOG_STORE_NVM(*_ha, (hhdr)); }
# define HDR(p) MAK_find_header((ptr_t)(p))

/* Get an HBLKSIZE aligned address closer to the beginning of the block */
/* h.  Assumes hhdr == HDR(h) and IS_FORWARDING_ADDR(hhdr).             */
#define FORWARDED_ADDR(h, hhdr) ((struct hblk *)(h) - (size_t)(hhdr))

// caching headers

typedef struct hce {
  word block_addr;    /* right shifted by LOG_HBLKSIZE */
  hdr * hce_hdr;
} hdr_cache_entry;


#define HCE_VALID_FOR(hce,h) ((hce) -> block_addr == \
                                ((word)(h) >> LOG_HBLKSIZE))

#define HCE_HDR(h) ((hce) -> hce_hdr)


#define HCE(h, hdr_cache, hc_sz) hdr_cache + (((word)(h) >> LOG_HBLKSIZE) \
               & (((word) hc_sz)-1))


//the hdr cache below only used for mark phase //////////////////////

#define DECLARE_HDR_CACHE \
        hdr_cache_entry hdr_cache[HDR_CACHE_SIZE]

#define INIT_HDR_CACHE BZERO(hdr_cache, sizeof(hdr_cache))

MAK_INNER hdr * MAK_header_cache_miss(ptr_t p, hdr_cache_entry *hce);
# define HEADER_CACHE_MISS(p, hce, source) MAK_header_cache_miss(p, hce)


/* Set hhdr to the header for p.  Analogous to GET_HDR below,           */
/* except that in the case of large objects, it                         */
/* gets the header for the object beginning, if MAK_all_interior_ptrs    */
/* is set.                                                              */
/* Returns zero if p points to somewhere other than the first page      */
/* of an object, and it is not a valid pointer to the object.           */
#define HC_GET_HDR(p, hhdr, source, exit_label) \
        { \
          hdr_cache_entry * hce = HCE(p, hdr_cache, HDR_CACHE_SIZE); \
          if (EXPECT(HCE_VALID_FOR(hce, p), TRUE)) { \
            HC_HIT(); \
            hhdr = hce -> hce_hdr; \
          } else { \
            hhdr = HEADER_CACHE_MISS(p, hce, source); \
            if (0 == hhdr) goto exit_label; \
          } \
        }

/* allocation time header cache */
//TODO: unify the allocation time and gc time hdr cache
extern hdr_cache_entry MAK_hdr_cache[HDR_CACHE_SIZE];

MAK_INNER void MAK_update_hc(ptr_t p, hdr* hhdr, hdr_cache_entry* hc, word hc_sz);
MAK_INNER hdr* MAK_get_hdr_and_update_hc(ptr_t p, hdr_cache_entry* hc, word hc_sz);
MAK_INNER hdr* MAK_get_hdr_no_update(ptr_t p, hdr_cache_entry* hc, word hc_sz);


#define ENSURE_64_BIT_COPY(dest, src) \
{ \
   asm volatile("" ::: "memory"); \
   __asm__ __volatile__ ("movq %1, %0;\n" \
       :"=r"(dest) \
       :"r" (src) \
       :); \
   asm volatile("" ::: "memory"); \
}

#endif
