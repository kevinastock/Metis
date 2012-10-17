#include "value_helper.hh"
//#include "values.hh"
#include "pch_kvsbtree.hh"
#include "pch_kvsarray.hh"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifdef JOS_USER
#include <inc/compiler.h>
#endif

key_cmp_t JSHARED_ATTR btree_type::keycmp_ = NULL;

void btree_type::set_key_compare(key_cmp_t kcmp)
{
    keycmp_ = kcmp;
}

void btree_type::init() {
    nk_ = 0;
    nlevel_ = 0;
    root_ = NULL;
}

// left < key <= right. Right is the new sibling
void btree_type::insert_internal(void *key, btnode_base *left, btnode_base *right)
{
    btnode_internal *parent = left->parent_;
    if (!parent) {
	btnode_internal *newroot = new btnode_internal;
	newroot->nk_ = 1;
	newroot->e_[0].k_ = key;
	newroot->e_[0].v_ = left;
	newroot->e_[1].v_ = right;
	root_ = newroot;
	left->parent_ = newroot;
	right->parent_ = newroot;
	nlevel_++;
    } else {
	int ikey = parent->upper_bound_pos(key, keycmp_);
	// insert newkey at ikey, values at ikey + 1
	for (int i = parent->nk_ - 1; i >= ikey; i--)
	    parent->e_[i + 1].k_ = parent->e_[i].k_;
	for (int i = parent->nk_; i >= ikey + 1; i--)
	    parent->e_[i + 1].v_ = parent->e_[i].v_;
	parent->e_[ikey].k_ = key;
	parent->e_[ikey + 1].v_ = right;
	parent->nk_++;
	right->parent_ = parent;
	if (parent->nk_ == 2 * order + 1) {
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

btnode_leaf *btree_type::get_leaf(void *key)
{
    if (!nlevel_) {
	root_ = new btnode_leaf;
	nlevel_ = 1;
	nk_ = 0;
	return static_cast<btnode_leaf *>(root_);
    }
    btnode_base *node = root_;
    for (int i = 0; i < nlevel_ - 1; ++i)
        node = static_cast<btnode_internal *>(node)->upper_bound(key, keycmp_);
    return static_cast<btnode_leaf *>(node);
}

// left < splitkey <= right. Right is the new sibling
int btree_type::insert_kv(void *key, void *val, size_t keylen, unsigned hash)
{
    btnode_leaf *leaf = get_leaf(key);
    bool bfound = false;
    int pos = leaf->lower_bound(key, keycmp_, &bfound);
    if (!bfound) {
        void *ik = (keylen && mrkeycopy) ? mrkeycopy(key, keylen) : key;
        leaf->insert(pos, ik, hash);
        ++ nk_;
    }
    values_insert(&leaf->e_[pos], val);
    if (leaf->need_split()) {
	btnode_leaf *right = leaf->split();
        insert_internal(right->e_[0].key, leaf, right);
    }
    return !bfound;
}

void btree_type::insert_kvs(const keyvals_t *k)
{
    btnode_leaf *leaf = get_leaf(k->key);
    bool bfound = false;
    int pos = leaf->lower_bound(k->key, keycmp_, &bfound);
    assert(!bfound);
    leaf->insert(pos, k->key, 0);  // do not copy key
    ++ nk_;
    leaf->e_[pos] = *k;
    if (leaf->need_split()) {
        btnode_leaf *right = leaf->split();
        insert_internal(right->e_[0].key, leaf, right);
    }
}

size_t btree_type::size() const {
    return nk_;
}

void btree_type::delete_level(btnode_base *node, int level)
{
    for (int i = 0; level > 1 && i < node->nk_; ++i)
        delete_level(static_cast<btnode_internal *>(node)->e_[i].v_, level - 1);
    delete node;
}

void btree_type::shallow_free()
{
    if (nlevel_) {
	delete_level(root_, nlevel_);
	nlevel_ = 0;
        root_ = NULL;
    }
}

btree_type::iterator btree_type::begin() {
    return iterator(first_leaf());
}

btree_type::iterator btree_type::end() {
    return btree_type::iterator(NULL);
}

uint64_t btree_type::copy_kvs(keyvals_t *dst)
{
    if (!nlevel_)
	return 0;
    btnode_leaf *leaf = first_leaf();
    uint64_t n = 0;
    while (leaf) {
	memcpy(&dst[n], leaf->e_, sizeof(keyvals_t) * leaf->nk_);
	n += leaf->nk_;
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
