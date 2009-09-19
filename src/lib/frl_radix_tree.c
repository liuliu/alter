/*************************
 * Fotas Runtime Library *
 *************************
 * Author: Liu Liu
 */

#include "frl_radix_tree.h"

inline bool keycmp(apr_byte_t* key1, apr_byte_t* key2, apr_uint32_t k, apr_uint32_t size)
{
	for (; k < size; k++)
		if (key1[k] != key2[k])
			return 0;
	return 1;
}

inline void empty(frl_radix_tree_entry_t* elts)
{
	int i;
	for (i = 0; i < 256; i++)
		elts->children[i] = 0;
}

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_get_lock_free(frl_radix_tree_t* tree, apr_byte_t* key)
{
}

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_add_lock_free(frl_radix_tree_t* tree, apr_byte_t* key, void* pointer)
{
}

APR_DECLARE(apr_status_t) frl_radix_tree_remove_lock_free(frl_radix_tree_entry_t* elts)
{
}

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_get_lock_with(frl_radix_tree_t* tree, apr_byte_t* key)
{
	int i;
	apr_byte_t* kp = key;
#if APR_HAS_THREADS
	apr_thread_rwlock_rdlock(tree->rwlock);
#endif
	frl_radix_tree_entry_t* elts = tree->root;
	for (i = 0; i < tree->key_size; i++)
	{
		elts = elts->children[*kp];
		if (elts == 0)
		{
#if APR_HAS_THREADS
			apr_thread_rwlock_unlock(tree->rwlock);
#endif
			return 0;
		}
		if (keycmp(elts->key, key, i+1, tree->key_size))
		{
#if APR_HAS_THREADS
			apr_thread_rwlock_unlock(tree->rwlock);
#endif
			return elts;
		}
		kp++;
	}
#if APR_HAS_THREADS
		apr_thread_rwlock_unlock(tree->rwlock);
#endif
	return 0;
}

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_add_lock_with(frl_radix_tree_t* tree, apr_byte_t* key, void* pointer)
{
	int i;
	apr_byte_t* kp = key;
#if APR_HAS_THREADS
	apr_thread_rwlock_wrlock(tree->rwlock);
#endif
	frl_radix_tree_entry_t* elts = tree->root;
	for (i = 0; i < tree->key_size; i++)
	{
		if (elts->children[*kp] == 0)
		{
			frl_radix_tree_entry_t* leaf = (frl_radix_tree_entry_t*)frl_slab_palloc(tree->pool);
			empty(leaf);
			leaf->nelts = 0;
			memcpy(leaf->key, key, tree->key_size);
			leaf->pointer = pointer;
			leaf->tree = tree;
			leaf->parent = elts;
			leaf->depth = i;
			elts->children[*kp] = leaf;
			elts->nelts++;
			tree->nelts++;
#if APR_HAS_THREADS
			apr_thread_rwlock_unlock(tree->rwlock);
#endif
			return leaf;
		} else {
			if (keycmp(elts->key, key, i+1, tree->key_size))
				break;
			elts = elts->children[*kp];
		}
		kp++;
	}
#if APR_HAS_THREADS
	apr_thread_rwlock_unlock(tree->rwlock);
#endif
	return 0;
}

APR_DECLARE(apr_status_t) frl_radix_tree_remove_lock_with(frl_radix_tree_entry_t* elts)
{
	frl_radix_tree_t* tree = elts->tree;
#if APR_HAS_THREADS
	apr_thread_rwlock_wrlock(tree->rwlock);
#endif
	frl_radix_tree_entry_t* leaf = elts;
	frl_radix_tree_entry_t* newleaf = 0;
	while (leaf->nelts > 0)
	{
		newleaf = 0;
		int i;
		for (i = 0; i < 256; i++)
		{
			if (leaf->children[i] != 0)
			{
				if (leaf->children[i]->nelts == 0)
				{
					newleaf = leaf->children[i];
					break;
				}
				if (newleaf == 0)
					newleaf = leaf->children[i];
			}
		}
		leaf = newleaf;
	}
	leaf->parent->children[leaf->key[leaf->depth]] = 0;
	leaf->parent->nelts--;
	if (leaf != elts)
	{
		memcpy(elts->key, leaf->key, tree->key_size);
		elts->pointer = leaf->pointer;
	}
	frl_slab_pfree(leaf);
#if APR_HAS_THREADS
	apr_thread_rwlock_unlock(tree->rwlock);
#endif
	return APR_SUCCESS;
}

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_get(frl_radix_tree_t* tree, apr_byte_t* key)
{
	if (FRL_LOCK_FREE == tree->lock)
		return frl_radix_tree_get_lock_free(tree, key);
	else if (FRL_LOCK_WITH == tree->lock)
		return frl_radix_tree_get_lock_with(tree, key);
}

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_add(frl_radix_tree_t* tree, apr_byte_t* key, void* pointer)
{
	if (FRL_LOCK_FREE == tree->lock)
		return frl_radix_tree_add_lock_free(tree, key, pointer);
	else if (FRL_LOCK_WITH == tree->lock)
		return frl_radix_tree_add_lock_with(tree, key, pointer);
}

APR_DECLARE(apr_status_t) frl_radix_tree_remove(frl_radix_tree_entry_t* elts)
{
	frl_radix_tree_t* tree = elts->tree;
	if (FRL_LOCK_FREE == tree->lock)
		return frl_radix_tree_remove_lock_free(elts);
	else if (FRL_LOCK_WITH == tree->lock)
		return frl_radix_tree_remove_lock_with(elts);
}

APR_DECLARE(void*) frl_radix_tree_destroy(frl_radix_tree_t* tree)
{
#if APR_HAS_THREADS
	apr_thread_rwlock_destroy(tree->rwlock);
#endif
	if (FRL_MEMORY_SELF == tree->memory)
		frl_slab_pool_destroy(tree->pool);
	free(tree);
}

APR_DECLARE(apr_status_t) frl_radix_tree_create(frl_radix_tree_t** newtree, apr_pool_t* mempool, apr_uint32_t key_size, apr_size_t capacity, frl_lock_u lock, frl_slab_pool_t* pool)
{
	frl_radix_tree_t *tree = (frl_radix_tree_t*)malloc(SIZEOF_FRL_RADIX_TREE_T);
	if (tree == NULL)
		return APR_ENOMEM;
#if APR_HAS_THREADS
	apr_thread_rwlock_create(&tree->rwlock, mempool);
#endif
	if (pool == 0)
	{
		frl_slab_pool_create(&tree->pool, mempool, capacity, SIZEOF_FRL_RADIX_TREE_ENTRY_T, lock);
		tree->memory = FRL_MEMORY_SELF;
	} else {
		tree->pool = pool;
		tree->memory = FRL_MEMORY_GLOBAL;
	}
	tree->key_size = key_size;
	tree->lock = FRL_LOCK_WITH;
	tree->nelts = 0;
	tree->root = (frl_radix_tree_entry_t*)frl_slab_palloc(tree->pool);
	empty(tree->root);
	tree->root->depth = 0;
	tree->root->nelts = 0;
	tree->root->parent = 0;
	tree->root->pointer = 0;
	*newtree = tree;

	return APR_SUCCESS;
}
