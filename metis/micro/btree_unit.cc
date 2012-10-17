#include "btree.hh"
#include <assert.h>
#include <iostream>
using namespace std;

template <typename T1, typename T2>
void CHECK_EQ(const T1 &expected, const T2 &actual) {
    if (expected != actual) {
        cerr << "Actual: " << actual << "\nExpected: " << expected << endl;
        assert(0);
    }
}

static int intcmp(const void *k1, const void *k2) {
    int64_t i1 = int64_t(k1);
    int64_t i2 = int64_t(k2);
    return i1 - i2;
}

void check_tree(btree_type &bt) {
    int64_t i = 1;
    btree_type::iterator it = bt.begin();
    while (it != bt.end()) {
        CHECK_EQ(i, int64_t(it->key));
        CHECK_EQ(1, int64_t(it->len));
        CHECK_EQ(i + 1, int64_t(it->vals[0]));
        ++it;
        ++i;
    }
    assert(size_t(i) == (bt.size() + 1));
}

void check_tree_copy(btree_type &bt) {
    keyvals_t *dst = new keyvals_t[bt.size()];
    memset(dst, 0, sizeof(keyvals_t) * bt.size());
    bt.copy_kvs(dst);
    for (int64_t i = 1; i <= int64_t(bt.size()); ++i) {
        CHECK_EQ(i, int64_t(dst[i - 1].key));
        CHECK_EQ(1, int64_t(dst[i - 1].len));
        CHECK_EQ(i + 1, int64_t(dst[i - 1].vals[0]));
        ++i;
    }
}

void check_tree_copy_and_free(btree_type &bt) {
    keyvals_t *dst = new keyvals_t[bt.size()];
    memset(dst, 0, sizeof(keyvals_t) * bt.size());
    bt.copy_kvs(dst);
    bt.shallow_free();
    for (int64_t i = 1; i <= int64_t(bt.size()); ++i) {
        CHECK_EQ(i, int64_t(dst[i - 1].key));
        CHECK_EQ(1, int64_t(dst[i - 1].len));
        CHECK_EQ(i + 1, int64_t(dst[i - 1].vals[0]));
        ++i;
    }
}

void test1() {
    btree_type bt;
    bt.set_key_compare(intcmp);
    bt.init();
    check_tree(bt);
    check_tree_copy(bt);
    for (int64_t i = 1; i < 1000; ++i) {
        bt.insert_kv((void *)i, (void *)(i + 1), 4, 0);
        check_tree(bt);
        check_tree_copy(bt);
    }
    assert(bt.size() == 999);
}

void test2() {
    btree_type bt;
    bt.set_key_compare(intcmp);
    bt.init();
    check_tree(bt);
    check_tree_copy(bt);
    for (int64_t i = 1; i < 1000; ++i) {
        keyvals_t kvs;
        kvs.key = (void *)i;
        kvs.len = 1;
        kvs.alloc_len = 1;
        kvs.vals = new void *[1];
        kvs.vals[0] = (void *)(i + 1);
        bt.insert_kvs(&kvs);
        check_tree(bt);
        check_tree_copy(bt);
        CHECK_EQ(size_t(i), bt.size());
    }
    assert(bt.size() == 999);
    check_tree_copy_and_free(bt);
}

int main(int argc, char *argv[]) {
    test1();
    test2();
    cerr << "PASS" << endl;
    return 0;
}
