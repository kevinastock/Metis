#ifndef ARRAY_BUCKET_MANAGER_HH_
#define ARRAY_BUCKET_MANAGER_HH_ 1
#include "mbktsmgr.hh"

struct arraybktmgr : public mbkts_mgr_t {
    void mbm_set_util(key_cmp_t kcmp);
    void mbm_mbks_init(int rows, int cols);
    void mbm_mbks_destroy(void);
    void mbm_mbks_bak(void);
    void mbm_rehash_bak(int row);
    void mbm_map_put(int row, void *key, void *val, size_t keylen,
			 unsigned hash);
    void mbm_map_prepare_merge(int row);
    xarray_base *mbm_map_get_output(int *n, bool *kvs);
    /* make sure the pairs of the reduce bucket is sorted by key, if
     * no out_cmp function is provided by application. */
    void mbm_do_reduce_task(int col);
};

#endif
