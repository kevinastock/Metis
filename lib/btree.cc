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
#include "btree.hh"
#include "appbase.hh"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifdef JOS_USER
#include <inc/compiler.h>
#endif

void btree_type::init() {
    nk_ = 0;
    nlevel_ = 0;
    root_ = NULL;
}

// left < key <= right. Right is the new sibling
void btree_type::insert_internal(void *key, btnode_base *left, btnode_base *right) {
    btnode_internal *parent = left->parent_;
    if (!parent) {
	btnode_internal *newroot = new btnode_internal;
	newroot->nk_ = 1;
        newroot->assign(0, left, key, right);
	root_ = newroot;
	left->parent_ = newroot;
	right->parent_ = newroot;
	++nlevel_;
    } else {
	int ikey = parent->upper_bound_pos(key);
	// insert newkey at ikey, values at ikey + 1
	for (int i = parent->nk_ - 1; i >= ikey; i--)
	    parent->e_[i + 1].k_ = parent->e_[i].k_;
	for (int i = parent->nk_; i >= ikey + 1; i--)
	    parent->e_[i + 1].v_ = parent->e_[i].v_;
        parent->assign_right(ikey, key, right);
	++parent->nk_;
	right->parent_ = parent;
	if (parent->need_split()) {
	    void *newkey = parent->e_[order].k_;
	    btnode_internal *newparent = parent->split();
	    // push up newkey
	    insert_internal(newkey, parent, newparent);
	    // fix parent pointers
	    for (int i = 0; i < newparent->nk_ + 1; ++i)
		newparent->e_[i].v_->parent_ = newparent;
	}
    }
}

btnode_leaf *btree_type::get_leaf(void *key) {
    if (!nlevel_) {
	root_ = new btnode_leaf;
	nlevel_ = 1;
	nk_ = 0;
	return static_cast<btnode_leaf *>(root_);
    }
    btnode_base *node = root_;
    for (int i = 0; i < nlevel_ - 1; ++i)
        node = static_cast<btnode_internal *>(node)->upper_bound(key);
    return static_cast<btnode_leaf *>(node);
}

// left < splitkey <= right. Right is the new sibling
int btree_type::map_insert_sorted_copy_on_new(void *k, void *v, size_t keylen, unsigned hash) {
    btnode_leaf *leaf = get_leaf(k);
    int pos;
    bool found;
    if (!(found = leaf->lower_bound(k, &pos))) {
        void *ik = static_appbase::key_copy(k, keylen);
        leaf->insert(pos, ik, hash);
        ++ nk_;
    }
    leaf->e_[pos].map_value_insert(v);
    if (leaf->need_split()) {
	btnode_leaf *right = leaf->split();
        insert_internal(right->e_[0].key, leaf, right);
    }
    return !found;
}

void btree_type::map_insert_sorted_new_and_raw(keyvals_t *p) {
    btnode_leaf *leaf = get_leaf(p->key);
    int pos;
    assert(!leaf->lower_bound(p->key, &pos));  // must be new key
    leaf->insert(pos, p->key, 0);  // do not copy key
    ++ nk_;
    leaf->e_[pos] = *p;
    if (leaf->need_split()) {
        btnode_leaf *right = leaf->split();
        insert_internal(right->e_[0].key, leaf, right);
    }
}

size_t btree_type::size() const {
    return nk_;
}

void btree_type::delete_level(btnode_base *node, int level) {
    for (int i = 0; level > 1 && i <= node->nk_; ++i)
        delete_level(static_cast<btnode_internal *>(node)->e_[i].v_, level - 1);
    delete node;
}

void btree_type::shallow_free() {
    if (!nlevel_)
        return;
    delete_level(root_, nlevel_);
    init();
}

btree_type::iterator btree_type::begin() {
    return iterator(first_leaf());
}

btree_type::iterator btree_type::end() {
    return btree_type::iterator(NULL);
}

uint64_t btree_type::copy(xarray<keyvals_t> *dst) {
    return copy_traverse(dst, false);
}

uint64_t btree_type::transfer(xarray<keyvals_t> *dst) {
    uint64_t n = copy_traverse(dst, true);
    shallow_free();
    return n;
}

uint64_t btree_type::copy_traverse(xarray<keyvals_t> *dst, bool clear_leaf) {
    assert(dst->size() == 0);
    if (!nlevel_)
	return 0;
    dst->resize(size());
    btnode_leaf *leaf = first_leaf();
    uint64_t n = 0;
    while (leaf) {
	memcpy(dst->at(n), leaf->e_, sizeof(keyvals_t) * leaf->nk_);
	n += leaf->nk_;
        if (clear_leaf)
            leaf->nk_ = 0;  // quickly delete all key/values from the leaf
        leaf = leaf->next_;
    }
    assert(n == nk_);
    return n;
}

btnode_leaf *btree_type::first_leaf() const {
    if (!nk_)
        return NULL;
    btnode_base *node = root_;
    for (int i = 0; i < nlevel_ - 1; ++i)
	node = static_cast<btnode_internal *>(node)->e_[0].v_;
    return static_cast<btnode_leaf *>(node);
}
