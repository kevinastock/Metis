/* Metis
 * Yandong Mao, Robert Morris, Frans Kaashoek
 * Copyright (c) 2012 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, subject to the conditions listed
 * in the Metis LICENSE file. These conditions include: you must preserve this
 * copyright notice, and you cannot mention the copyright holders in
 * advertising related to the Software without their permission.  The Software
 * is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Metis LICENSE file; the license in that file is legally
 * binding.
 */
#ifndef BTREE_HH_
#define BTREE_HH_ 1

#include "bsearch.hh"
#include "appbase.hh"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

enum { order = 3 };

struct btnode_internal;

struct btnode_base {
    btnode_internal *parent_;
    short nk_;
    btnode_base() : parent_(NULL), nk_(0) {}
    virtual ~btnode_base() {}
};

struct btnode_leaf : public btnode_base {
    static const int fanout = 2 * order + 2;
    keyvals_t e_[fanout];
    btnode_leaf *next_;
    ~btnode_leaf() {
        for (int i = 0; i < nk_; ++i)
            e_[i].reset();
        for (int i = nk_; i < fanout; ++i)
            e_[i].init();
    }

    btnode_leaf() : btnode_base(), next_(NULL) {
        for (int i = 0; i < fanout; ++i)
            e_[i].init();
    }
    btnode_leaf *split() {
        btnode_leaf *right = new btnode_leaf;
        memcpy(right->e_, &e_[order + 1], sizeof(e_[0]) * (1 + order));
        right->nk_ = order + 1;
        nk_ = order + 1;
        btnode_leaf *next = next_;
        next_ = right;
        right->next_ = next;
        return right;
    }

    bool lower_bound(void *key, int *p) {
        bool found = false;
        keyvals_t tmp;
        tmp.key = key;
        *p = xsearch::lower_bound(&tmp, e_, nk_,
                                  static_appbase::pair_comp<keyvals_t>, &found);
        return found;
    }

    void insert(int pos, void *key, unsigned hash) {
        if (pos < nk_)
            memmove(&e_[pos + 1], &e_[pos], sizeof(e_[0]) * (nk_ - pos));
        ++ nk_;
        e_[pos].init();
        e_[pos].key = key;
        e_[pos].hash = hash;
    }

    bool need_split() const {
        return nk_ == fanout;
    }
};


struct btnode_internal : public btnode_base {
    static const int fanout = 2 * order + 2;
    struct xpair {
        xpair(void *k) : k_(k) {} 
        xpair() : k_(), v_() {} 
        union {
            void *k_;
            void *key;
        };
        btnode_base *v_;
    };

    xpair e_[fanout];
    btnode_internal() : btnode_base() {
        bzero(e_, sizeof(e_));
    }
    virtual ~btnode_internal() {}

    btnode_internal *split() {
        btnode_internal *nn = new btnode_internal;
        nn->nk_ = order;
        memcpy(nn->e_, &e_[order + 1], sizeof(e_[0]) * (order + 1));
        nk_ = order;
        return nn;
    }
    void assign(int p, btnode_base *left, void *key, btnode_base *right) {
        e_[p].v_ = left;
        e_[p].k_ = key;
        e_[p + 1].v_ = right;
    }
    void assign_right(int p, void *key, btnode_base *right) {
        e_[p].k_ = key;
        e_[p + 1].v_ = right;
    }
    btnode_base *upper_bound(void *key) {
        int pos = upper_bound_pos(key);
        return e_[pos].v_;
    }
    int upper_bound_pos(void *key) {
        xpair tmp(key);
        return xsearch::upper_bound(&tmp, e_, nk_, static_appbase::pair_comp<xpair>);
    }
    bool need_split() const {
        return nk_ == fanout - 1;
    }
};

struct btree_type {
    typedef keyvals_t element_type;

    void init();
    /* @brief: free the tree, but not the values */
    void shallow_free();
    void map_insert_sorted_new_and_raw(keyvals_t *kvs);

    /* @brief: insert key/val pair into the tree
       @return true if it is a new key */
    int map_insert_sorted_copy_on_new(void *key, void *val, size_t keylen, unsigned hash);
    size_t size() const;
    uint64_t transfer(xarray<keyvals_t> *dst);
    uint64_t copy(xarray<keyvals_t> *dst);

    /* @brief: return the number of values in the tree */
    uint64_t test_get_nvalue() {
        iterator i = begin();
        uint64_t n = 0;
        while (i != end()) {
            n += i->size();
            ++ i;
        }
        return n;
    }

    struct iterator {
        iterator() : c_(NULL), i_(0) {}
        explicit iterator(btnode_leaf *c) : c_(c), i_(0) {}
        iterator &operator=(const iterator &a) {
            c_ = a.c_;
            i_ = a.i_;
            return *this;
        }
        void operator++() {
            if (c_ && i_ + 1 == c_->nk_) {
                c_ = c_->next_;
                i_ = 0;
            } else if (c_)
                ++i_;
            else
                assert(0);
        }
        void operator++(int) {
            ++(*this);
        }
        bool operator==(const iterator &a) {
            return (!c_ && !a.c_) || (c_ == a.c_ && i_ == a.i_);
        }
        bool operator!=(const iterator &a) {
            return !(*this == a);
        }
        keyvals_t *operator->() {
            return &c_->e_[i_];
        }
        keyvals_t &operator*() {
            return c_->e_[i_];
        }
      private:
        btnode_leaf *c_;
        int i_;
    };

    iterator begin();
    iterator end();

  private:
    size_t nk_;
    short nlevel_;
    btnode_base *root_;
    uint64_t copy_traverse(xarray<keyvals_t> *dst, bool clear_leaf);

    /* @brief: insert @key at position @pos into leaf node @leaf,
     * and set the value of that key to empty */
    static void insert_leaf(btnode_leaf *leaf, void *key, int pos, int keylen);
    static void delete_level(btnode_base *node, int level);

    btnode_leaf *first_leaf() const;

    /* @brief: insert (@key, @right) into left's parent */
    void insert_internal(void *key, btnode_base *left, btnode_base *right);
    btnode_leaf *get_leaf(void *key);
};

#endif
