#include <comet/RAND.hh>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <cassert>
#include <cstring>

namespace comet
{

#define MD_DIGEST_LENGTH        SHA_DIGEST_LENGTH

#define MD_Update(a,b,c)        EVP_DigestUpdate(a,b,c)
#define MD_Final(a,b)           EVP_DigestFinal_ex(a,b,NULL)
#define MD_Init(a)              EVP_DigestInit_ex(a,EVP_sha1(), NULL)
#define MD(a,b,c)               EVP_Digest(a,b,c,NULL,EVP_sha1(), NULL)

#define ENTROPY_NEEDED          32  /* require 256 bits = 32 bytes of randomness */

#define STATE_SIZE              1023

  static unsigned int crypto_lock_rand = 0; /* may be set only when a thread
                                             * holds CRYPTO_LOCK_RAND
                                             * (to prevent double locking) */
  /* access to lockin_thread is synchronized by CRYPTO_LOCK_RAND2 */
  static CRYPTO_THREADID locking_threadid; /* valid iff crypto_lock_rand is set */

  static int state_num=0,state_index=0;
  static unsigned char state[STATE_SIZE+MD_DIGEST_LENGTH];
  static unsigned char md[MD_DIGEST_LENGTH];
  static long md_count[2]={0,0};
  static double entropy=0;
  static int initialized=0;

  void RAND_cleanup(void)
  {
    OPENSSL_cleanse(state,sizeof(state));
    state_num=0;
    state_index=0;
    OPENSSL_cleanse(md,MD_DIGEST_LENGTH);
    md_count[0]=0;
    md_count[1]=0;
    entropy=0;
    initialized=0;

    /// this section has been taken from ssleay_rand_bytes() in
    /// order to have enough 'md' to perform the operations
    {
      /* In the output function only half of 'md' remains secret,
       * so we better make sure that the required entropy gets
       * 'evenly distributed' through 'state', our randomness pool.
       * The input function (ssleay_rand_add) chains all of 'md',
       * which makes it more suitable for this purpose.
       */

      int n = STATE_SIZE; /* so that the complete pool gets accessed */
      while (n > 0)
        {
#if MD_DIGEST_LENGTH > 20
# error "Please adjust DUMMY_SEED."
#endif
#define DUMMY_SEED "...................." /* at least MD_DIGEST_LENGTH */
          /* Note that the seed does not matter, it's just that
           * ssleay_rand_add expects to have something to hash. */
          /// the following line is a fix replacing ssleay_rand_add()
          RAND_add(DUMMY_SEED, MD_DIGEST_LENGTH, 0.0);
          n -= MD_DIGEST_LENGTH;
        }
    }
  }

  void RAND_add(const void *buf, int num, double add)
  {
    int i,j,k,st_idx;
    long md_c[2];
    unsigned char local_md[MD_DIGEST_LENGTH];
    EVP_MD_CTX m;
    int do_not_lock;

    /*
     * (Based on the rand(3) manpage)
     *
     * The input is chopped up into units of 20 bytes (or less for
     * the last block).  Each of these blocks is run through the hash
     * function as follows:  The data passed to the hash function
     * is the current 'md', the same number of bytes from the 'state'
     * (the location determined by in incremented looping index) as
     * the current 'block', the new key data 'block', and 'count'
     * (which is incremented after each use).
     * The result of this is kept in 'md' and also xored into the
     * 'state' at the same locations that were used as input into the
     * hash function.
     */

    /* check if we already have the lock */
    if (crypto_lock_rand)
      {
        CRYPTO_THREADID cur;
        CRYPTO_THREADID_current(&cur);
        CRYPTO_r_lock(CRYPTO_LOCK_RAND2);
        do_not_lock = !CRYPTO_THREADID_cmp(&locking_threadid, &cur);
        CRYPTO_r_unlock(CRYPTO_LOCK_RAND2);
      }
    else
      do_not_lock = 0;

    if (!do_not_lock) CRYPTO_w_lock(CRYPTO_LOCK_RAND);
    st_idx=state_index;

    /* use our own copies of the counters so that even
     * if a concurrent thread seeds with exactly the
     * same data and uses the same subarray there's _some_
     * difference */
    md_c[0] = md_count[0];
    md_c[1] = md_count[1];

    memcpy(local_md, md, sizeof md);

    /* state_index <= state_num <= STATE_SIZE */
    state_index += num;
    if (state_index >= STATE_SIZE)
      {
        state_index%=STATE_SIZE;
        state_num=STATE_SIZE;
      }
    else if (state_num < STATE_SIZE)
      {
        if (state_index > state_num)
          state_num=state_index;
      }
    /* state_index <= state_num <= STATE_SIZE */

    /* state[st_idx], ..., state[(st_idx + num - 1) % STATE_SIZE]
     * are what we will use now, but other threads may use them
     * as well */

    md_count[1] += (num / MD_DIGEST_LENGTH) + (num % MD_DIGEST_LENGTH > 0);

    if (!do_not_lock) CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

    EVP_MD_CTX_init(&m);
    for (i=0; i<num; i+=MD_DIGEST_LENGTH)
      {
        j=(num-i);
        j=(j > MD_DIGEST_LENGTH)?MD_DIGEST_LENGTH:j;

        MD_Init(&m);
        MD_Update(&m,local_md,MD_DIGEST_LENGTH);
        k=(st_idx+j)-STATE_SIZE;
        if (k > 0)
          {
            MD_Update(&m,&(state[st_idx]),j-k);
            MD_Update(&m,&(state[0]),k);
          }
        else
          MD_Update(&m,&(state[st_idx]),j);

        /* DO NOT REMOVE THE FOLLOWING CALL TO MD_Update()! */
        MD_Update(&m,buf,j);
        /* We know that line may cause programs such as
           purify and valgrind to complain about use of
           uninitialized data.  The problem is not, it's
           with the caller.  Removing that line will make
           sure you get really bad randomness and thereby
           other problems such as very insecure keys. */

        MD_Update(&m,(unsigned char *)&(md_c[0]),sizeof(md_c));
        MD_Final(&m,local_md);
        md_c[1]++;

        buf=(const char *)buf + j;

        for (k=0; k<j; k++)
          {
            /* Parallel threads may interfere with this,
             * but always each byte of the new state is
             * the XOR of some previous value of its
             * and local_md (itermediate values may be lost).
             * Alway using locking could hurt performance more
             * than necessary given that conflicts occur only
             * when the total seeding is longer than the random
             * state. */
            state[st_idx++]^=local_md[k];
            if (st_idx >= STATE_SIZE)
              st_idx=0;
          }
      }
    EVP_MD_CTX_cleanup(&m);

    if (!do_not_lock) CRYPTO_w_lock(CRYPTO_LOCK_RAND);
    /* Don't just copy back local_md into md -- this could mean that
     * other thread's seeding remains without effect (except for
     * the incremented counter).  By XORing it we keep at least as
     * much entropy as fits into md. */
    for (k = 0; k < (int)sizeof(md); k++)
      {
        md[k] ^= local_md[k];
      }
    if (entropy < ENTROPY_NEEDED) /* stop counting when we have enough */
      entropy += add;
    if (!do_not_lock) CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

#if !defined(OPENSSL_THREADS) && !defined(OPENSSL_SYS_WIN32)
    assert(md_c[1] == md_count[1]);
#endif
  }

  void RAND_seed(const void *buf, int num)
  {
    /// this line is a fix of ssleay_rand_add().
    RAND_add(buf, num, (double)num);
  }

  int RAND_bytes(unsigned char *buf, int num)
  {
    int i,j,k,st_num,st_idx;
    int num_ceil;
    int ok;
    long md_c[2];
    unsigned char local_md[MD_DIGEST_LENGTH];
    EVP_MD_CTX m;

    if (num <= 0)
      return 1;

    EVP_MD_CTX_init(&m);
    /* round upwards to multiple of MD_DIGEST_LENGTH/2 */
    num_ceil = (1 + (num-1)/(MD_DIGEST_LENGTH/2)) * (MD_DIGEST_LENGTH/2);

    /*
     * (Based on the rand(3) manpage:)
     *
     * For each group of 10 bytes (or less), we do the following:
     *
     * Input into the hash function the local 'md' (which is initialized from
     * the global 'md' before any bytes are generated), the bytes that are to
     * be overwritten by the random bytes, and bytes from the 'state'
     * (incrementing looping index). From this digest output (which is kept
     * in 'md'), the top (up to) 10 bytes are returned to the caller and the
     * bottom 10 bytes are xored into the 'state'.
     *
     * Finally, after we have finished 'num' random bytes for the
     * caller, 'count' (which is incremented) and the local and global 'md'
     * are fed into the hash function and the results are kept in the
     * global 'md'.
     */

    CRYPTO_w_lock(CRYPTO_LOCK_RAND);

    /* prevent ssleay_rand_bytes() from trying to obtain the lock again */
    CRYPTO_w_lock(CRYPTO_LOCK_RAND2);
    CRYPTO_THREADID_current(&locking_threadid);
    CRYPTO_w_unlock(CRYPTO_LOCK_RAND2);
    crypto_lock_rand = 1;

    if (!initialized)
      {
        /// the call to RAND_poll() has been removed here because
        /// injecting unwanted entropy.
        initialized = 1;
      }

    ok = (entropy >= ENTROPY_NEEDED);
    if (!ok)
      {
        /* If the PRNG state is not yet unpredictable, then seeing
         * the PRNG output may help attackers to determine the new
         * state; thus we have to decrease the entropy estimate.
         * Once we've had enough initial seeding we don't bother to
         * adjust the entropy count, though, because we're not ambitious
         * to provide *information-theoretic* randomness.
         *
         * NOTE: This approach fails if the program forks before
         * we have enough entropy. Entropy should be collected
         * in a separate input pool and be transferred to the
         * output pool only when the entropy limit has been reached.
         */
        entropy -= num;
        if (entropy < 0)
          entropy = 0;
      }

    /// here the section filling the 'md' has been moved to RAND_cleanup().

    st_idx=state_index;
    st_num=state_num;
    md_c[0] = md_count[0];
    md_c[1] = md_count[1];
    memcpy(local_md, md, sizeof md);

    state_index+=num_ceil;
    if (state_index > state_num)
      state_index %= state_num;

    /* state[st_idx], ..., state[(st_idx + num_ceil - 1) % st_num]
     * are now ours (but other threads may use them too) */

    md_count[0] += 1;

    /* before unlocking, we must clear 'crypto_lock_rand' */
    crypto_lock_rand = 0;
    CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

    while (num > 0)
      {
        /* num_ceil -= MD_DIGEST_LENGTH/2 */
        j=(num >= MD_DIGEST_LENGTH/2)?MD_DIGEST_LENGTH/2:num;
        num-=j;
        MD_Init(&m);
        MD_Update(&m,local_md,MD_DIGEST_LENGTH);
        MD_Update(&m,(unsigned char *)&(md_c[0]),sizeof(md_c));

        /* The following line uses the supplied buffer as a small
         * source of entropy: since this buffer is often uninitialised
         * it may cause programs such as purify or valgrind to
         * complain. So for those builds it is not used: the removal
         * of such a small source of entropy has negligible impact on
         * security.
         */
        /// the following line adds undeterministic entropy and has therefore
        /// been removed
        // MD_Update(&m,buf,j);

        k=(st_idx+MD_DIGEST_LENGTH/2)-st_num;
        if (k > 0)
          {
            MD_Update(&m,&(state[st_idx]),MD_DIGEST_LENGTH/2-k);
            MD_Update(&m,&(state[0]),k);
          }
        else
          MD_Update(&m,&(state[st_idx]),MD_DIGEST_LENGTH/2);
        MD_Final(&m,local_md);

        for (i=0; i<MD_DIGEST_LENGTH/2; i++)
          {
            state[st_idx++]^=local_md[i]; /* may compete with other threads */
            if (st_idx >= st_num)
              st_idx=0;
            if (i < j)
              *(buf++)=local_md[i+MD_DIGEST_LENGTH/2];
          }
      }

    MD_Init(&m);
    MD_Update(&m,(unsigned char *)&(md_c[0]),sizeof(md_c));
    MD_Update(&m,local_md,MD_DIGEST_LENGTH);
    CRYPTO_w_lock(CRYPTO_LOCK_RAND);
    MD_Update(&m,md,MD_DIGEST_LENGTH);
    MD_Final(&m,md);
    CRYPTO_w_unlock(CRYPTO_LOCK_RAND);

    EVP_MD_CTX_cleanup(&m);
    if (ok)
      return(1);
    else
      {
        RANDerr(RAND_F_SSLEAY_RAND_BYTES,RAND_R_PRNG_NOT_SEEDED);
        ERR_add_error_data(1, "You need to read the OpenSSL FAQ, "
                           "http://www.openssl.org/support/faq.html");
        return(0);
      }
  }

  int RAND_pseudorand(unsigned char *buf, int num)
  {
    int ret;
    unsigned long err;

    ret = RAND_bytes(buf, num);
    if (ret == 0)
      {
        err = ERR_peek_error();
        if (ERR_GET_LIB(err) == ERR_LIB_RAND &&
            ERR_GET_REASON(err) == RAND_R_PRNG_NOT_SEEDED)
          ERR_clear_error();
      }

    return (ret);
  }

  int RAND_status(void)
  {
    CRYPTO_THREADID cur;
    int ret;
    int do_not_lock;

    CRYPTO_THREADID_current(&cur);
    /* check if we already have the lock
     * (could happen if a RAND_poll() implementation calls RAND_status()) */
    if (crypto_lock_rand)
      {
        CRYPTO_r_lock(CRYPTO_LOCK_RAND2);
        do_not_lock = !CRYPTO_THREADID_cmp(&locking_threadid, &cur);
        CRYPTO_r_unlock(CRYPTO_LOCK_RAND2);
      }
    else
      do_not_lock = 0;

    if (!do_not_lock)
      {
        CRYPTO_w_lock(CRYPTO_LOCK_RAND);

        /* prevent ssleay_rand_bytes() from trying to obtain the lock again */
        CRYPTO_w_lock(CRYPTO_LOCK_RAND2);
        CRYPTO_THREADID_cpy(&locking_threadid, &cur);
        CRYPTO_w_unlock(CRYPTO_LOCK_RAND2);
        crypto_lock_rand = 1;
      }

    if (!initialized)
      {
        /// the call to RAND_poll() has been removed here because
        /// injecting additional entropy.
        initialized = 1;
      }

    ret = entropy >= ENTROPY_NEEDED;

    if (!do_not_lock)
      {
        /* before unlocking, we must clear 'crypto_lock_rand' */
        crypto_lock_rand = 0;

        CRYPTO_w_unlock(CRYPTO_LOCK_RAND);
      }

    return ret;
  }

}
