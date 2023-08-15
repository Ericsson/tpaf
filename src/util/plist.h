/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef PLIST_H
#define PLIST_H

#include <stdbool.h>
#include <sys/types.h>

struct plist;

typedef void *(*plist_elem_clone)(const void *original);
typedef void (*plist_elem_destroy)(void *original);

typedef void (*plist_elem_inc_ref)(void *original);
typedef void (*plist_elem_dec_ref)(void *original);

typedef bool (*plist_elem_equal)(const void *a, const void *b);

struct plist *plist_create(void);
struct plist *plist_clone(const struct plist *plist);
struct plist *plist_clone_deep(const struct plist *plist,
			       plist_elem_clone elem_clone);
struct plist *plist_clone_cnted(const struct plist *plist,
				plist_elem_inc_ref elem_inc_ref);

struct plist *plist_clone_non_null(const struct plist *plist);
struct plist *plist_clone_non_null_deep(const struct plist *plist,
					plist_elem_clone elem_clone);
struct plist *plist_clone_non_null_cnted(const struct plist *plist,
					plist_elem_inc_ref elem_inc_ref);

void plist_destroy(struct plist *plist);
void plist_destroy_deep(struct plist *plist, plist_elem_destroy elem_destroy);
void plist_destroy_cnted(struct plist *plist, plist_elem_dec_ref elem_dec_ref);

void plist_append(struct plist *plist, void *elem);
void plist_append_deep(struct plist *plist, plist_elem_clone elem_clone,
		       const void *elem);
void plist_append_cnted(struct plist *plist, plist_elem_inc_ref elem_inc_ref,
			void *elem);

void *plist_get(const struct plist *plist, size_t index);

void plist_del(struct plist *plist, size_t index);
void plist_del_deep(struct plist *plist, plist_elem_destroy elem_destroy,
		    size_t index);
void plist_del_cnted(struct plist *plist, plist_elem_dec_ref elem_dec_ref,
		     size_t index);

void plist_clear(struct plist *plist);
void plist_clear_deep(struct plist *plist, plist_elem_destroy elem_destroy);
void plist_clear_cnted(struct plist *plist, plist_elem_dec_ref elem_dec_ref);

size_t plist_len(const struct plist *plist);

bool plist_has(const struct plist *plist, plist_elem_equal elem_equal,
	       const void *elem);
ssize_t plist_index_of(const struct plist *plist, plist_elem_equal elem_equal,
		       const void *elem);

typedef bool (*plist_foreach_cb)(void *elem, void *cb_data);

void plist_foreach(const struct plist *plist, plist_foreach_cb cb,
		   void *cb_data);

#define _PLIST_GEN_GENERIC_WRAPPER_DEF(list_name, list_type, elem_type, \
				       elem_equal, fun_attrs)		\
    list_type;								\
									\
    fun_attrs list_type *list_name ## _create(void)			\
    {									\
	return (list_type *)plist_create();				\
    }									\
    fun_attrs elem_type *list_name ## _get(const list_type *list,	\
					   size_t index)		\
    {									\
	return plist_get((const struct plist *)list, index);		\
    }									\
									\
    fun_attrs size_t list_name ## _len(const list_type *list)		\
    {									\
	return plist_len((const struct plist *)list);			\
    }									\
									\
    fun_attrs bool list_name ## _has(const list_type *list,		\
				     const elem_type *elem)		\
    {									\
	return plist_has((const struct plist *)list, elem_equal, elem); \
    }									\
    fun_attrs ssize_t list_name ## _index_of(const list_type *list,	\
					     const elem_type *elem)	\
    {									\
	return plist_index_of((const struct plist *)list, elem_equal,	\
			      elem);					\
    }									\
									\
    typedef bool (*list_name ## _foreach_cb)(elem_type *elem,		\
					     void *cb_data);		\
									\
    fun_attrs void list_name ## _foreach(const list_type *list,		\
					 list_name ## _foreach_cb cb,	\
					 void *cb_data)			\
    {									\
	plist_foreach((const struct plist *)list, (plist_foreach_cb)cb, \
		      cb_data);						\
    }

#define PLIST_GEN_BY_VALUE_WRAPPER_DEF(list_name, list_type, elem_type, \
				       elem_clone, elem_destroy,	\
				       elem_equal, fun_attrs)		\
									\
    _PLIST_GEN_GENERIC_WRAPPER_DEF(list_name, list_type, elem_type,	\
				   elem_equal, fun_attrs)		\
									\
    fun_attrs list_type *list_name ## _clone(const list_type *list)	\
    {									\
	return (list_type *)plist_clone_deep((const struct plist *)list, \
					     elem_clone);		\
    }									\
									\
    fun_attrs list_type *list_name ## _clone_non_null(const list_type *list) \
    {									\
	return (list_type *)plist_clone_non_null_deep(			\
	    (const struct plist *)list, elem_clone			\
	    );								\
    }									\
									\
    fun_attrs void list_name ## _destroy(list_type *list)		\
    {									\
	plist_destroy_deep((struct plist *)list, elem_destroy);		\
    }									\
									\
    fun_attrs void list_name ## _append(list_type *list,		\
					const elem_type *elem)		\
    {									\
	plist_append_deep((struct plist *)list, elem_clone, elem);	\
    }									\
									\
    fun_attrs void list_name ## _del(list_type *list, size_t index)	\
    {									\
	plist_del_deep((struct plist *)list, elem_destroy,		\
		       index);						\
    }									\
    fun_attrs void list_name ## _clear(list_type *list)			\
    {									\
	plist_clear_deep((struct plist *)list, elem_destroy);		\
    }

#define PLIST_GEN_BY_REF_WRAPPER_DEF(list_name, list_type, elem_type,	\
				     elem_equal, fun_attrs)		\
									\
    _PLIST_GEN_GENERIC_WRAPPER_DEF(list_name, list_type, elem_type,	\
				   elem_equal, fun_attrs)		\
									\
    fun_attrs list_type *list_name ## _clone(const list_type *list)	\
    {									\
	return (list_type *)plist_clone((const struct plist *)list);	\
    }									\
									\
    fun_attrs list_type *list_name ## _clone_non_null(const list_type *list) \
    {									\
	return (list_type *)plist_clone_non_null((const struct plist *)list); \
    }									\
									\
    fun_attrs void list_name ## _destroy(list_type *list)		\
    {									\
	plist_destroy((struct plist *)list);				\
    }									\
									\
    fun_attrs void list_name ## _append(list_type *list, elem_type *elem) \
    {									\
	plist_append((struct plist *)list, elem);			\
    }									\
									\
    fun_attrs void list_name ## _del(const list_type *list, size_t index) \
    {									\
	plist_del((struct plist *)list, index);				\
    }									\
    fun_attrs void list_name ## _clear(const list_type *list)		\
    {									\
	plist_clear((struct plist *)list);				\
    }

#endif


    
