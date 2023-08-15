/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef PMAP_H
#define PMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

struct pmap;

typedef void (*pmap_value_inc_ref)(void *value);
typedef void (*pmap_value_dec_ref)(void *value);

struct pmap *pmap_create(void);

void pmap_destroy(struct pmap *map);
void pmap_destroy_cnted(struct pmap *map, pmap_value_dec_ref value_dec_ref);

void pmap_add(struct pmap *map, uint64_t key, void *value);
void pmap_add_cnted(struct pmap *map, pmap_value_inc_ref value_inc_ref,
		    uint64_t key, void *value);
bool pmap_has_key(const struct pmap *map, uint64_t key);

void *pmap_get(const struct pmap *map, uint64_t key);

void pmap_del(struct pmap *map, uint64_t key);
void pmap_del_cnted(struct pmap *map, pmap_value_dec_ref value_dec_ref,
		    uint64_t key);

void pmap_clear(struct pmap *map);
void pmap_clear_cnted(struct pmap *map, pmap_value_dec_ref value_dec_ref);

size_t pmap_size(const struct pmap *map);

typedef bool (*pmap_foreach_cb)(uint64_t key, void *elem, void *cb_data);

void pmap_foreach(const struct pmap *pmap, pmap_foreach_cb cb,
		  void *cb_data);

#define PMAP_GEN_WRAPPER_DECL(map_name, map_type, key_type, value_type, \
			      fun_attrs)				\
    fun_attrs map_type *map_name ## _create(void);			\
    fun_attrs void map_name ## _destroy(map_type *map);			\
    fun_attrs void map_name ## _add(map_type *map, key_type key,	\
				    value_type *value);			\
    fun_attrs bool map_name ## _has_key(const map_type *map, key_type key); \
    fun_attrs value_type *map_name ## _get(const map_type *map,		\
					   key_type key);		\
    fun_attrs void map_name ## _del(map_type *map, key_type key);	\
    fun_attrs void map_name ## _clear(map_type *map);			\
    fun_attrs size_t map_name ## _size(const map_type *map);		\
									\
    typedef bool (*map_name ## _foreach_cb)(key_type key, value_type *elem, \
					    void *cb_data);		\
									\
    fun_attrs void map_name ## _foreach(const map_type *map,		\
					map_name ## _foreach_cb cb,	\
					void *cb_data);

#define _PMAP_GEN_GENERIC_WRAPPER_DEF(map_name, map_type, key_type,	\
				      value_type, fun_attrs)		\
    fun_attrs map_type *map_name ## _create(void)			\
    {									\
	return (map_type *)pmap_create();				\
    }									\
    									\
    fun_attrs bool map_name ## _has_key(const map_type *map, key_type key) \
    {									\
	return pmap_has_key((const struct pmap *)map, (uint64_t)key);	\
    }									\
									\
    fun_attrs value_type *map_name ## _get(const map_type *map, key_type key) \
    {									\
	return (value_type *)pmap_get((const struct pmap *)map,		\
				      (uint64_t)key);			\
    }									\
									\
    fun_attrs size_t map_name ## _size(const map_type *map)		\
    {									\
	return pmap_size((const struct pmap *)map);			\
    }									\
									\
    typedef bool (*map_name ## _foreach_cb)(key_type key, value_type *elem, \
					    void *cb_data);		\
									\
    fun_attrs void map_name ## _foreach(const map_type *map,		\
					map_name ## _foreach_cb cb,	\
					void *cb_data)			\
    {									\
	pmap_foreach((const struct pmap *)map, (pmap_foreach_cb)cb,	\
		     cb_data);						\
    }

#define PMAP_GEN_WRAPPER_DEF(map_name, map_type, key_type, value_type,	\
			     fun_attrs)					\
    _PMAP_GEN_GENERIC_WRAPPER_DEF(map_name, map_type, key_type, value_type, \
				  fun_attrs)				\
    fun_attrs void map_name ## _destroy(map_type *map)			\
    {									\
	pmap_destroy((struct pmap *)map);				\
    }									\
    									\
    fun_attrs void map_name ## _add(map_type *map, key_type key,	\
				    value_type *value)			\
    {									\
	pmap_add((struct pmap*)map, (uint64_t)key, value);		\
    }									\
									\
    fun_attrs void map_name ## _del(map_type *map, key_type key)	\
    {									\
	pmap_del((struct pmap *)map, (uint64_t)key);			\
    }									\
    									\
    fun_attrs void map_name ## _clear(map_type *map)			\
    {									\
	pmap_clear((struct pmap *)map);					\
    }

#define PMAP_GEN_WRAPPER(map_name, map_type, key_type, value_type,	\
			 fun_attrs)					\
    PMAP_GEN_WRAPPER_DECL(map_name, map_type, key_type, value_type,	\
				  fun_attrs)				\
    PMAP_GEN_WRAPPER_DEF(map_name, map_type, key_type, value_type, \
				  fun_attrs)				\

#define PMAP_GEN_REF_CNT_WRAPPER_DEF(map_name, map_type, key_type,	\
				     value_type, value_inc_ref,		\
				     value_dec_ref, fun_attrs)		\
    _PMAP_GEN_GENERIC_WRAPPER_DEF(map_name, map_type, key_type, value_type, \
				  fun_attrs)				\
    fun_attrs void map_name ## _destroy(map_type *map)			\
    {									\
	pmap_destroy_cnted((struct pmap *)map,				\
			   (pmap_value_dec_ref)value_dec_ref);	\
    }									\
    									\
    fun_attrs void map_name ## _add(map_type *map, key_type key,	\
				    value_type *value)			\
    {									\
	pmap_add_cnted((struct pmap*)map, (pmap_value_inc_ref)value_inc_ref, \
		       (uint64_t)key, value);				\
    }									\
									\
    fun_attrs void map_name ## _del(map_type *map, key_type key)	\
    {									\
	pmap_del_cnted((struct pmap *)map, (pmap_value_dec_ref)value_dec_ref, \
		       (uint64_t)key);					\
    }									\
    									\
    fun_attrs void map_name ## _clear(map_type *map)			\
    {									\
	pmap_clear_cnted((struct pmap *)map,				\
			 (pmap_value_dec_ref)value_dec_ref);		\
    }

#define PMAP_GEN_REF_CNT_WRAPPER(map_name, map_type, key_type, value_type, \
				 value_inc_ref,	value_dec_ref, fun_attrs) \
    PMAP_GEN_WRAPPER_DECL(map_name, map_type, key_type, value_type,	\
			  fun_attrs)					\
    PMAP_GEN_REF_CNT_WRAPPER_DEF(map_name, map_type, key_type, value_type, \
				 value_inc_ref,	value_dec_ref, fun_attrs)

#endif
