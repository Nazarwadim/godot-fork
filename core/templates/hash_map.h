/**************************************************************************/
/*  hash_map.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef HASH_MAP_H
#define HASH_MAP_H

#include "core/math/math_funcs.h"
#include "core/os/memory.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/pair.h"

/**
 * A HashMap implementation that uses open addressing with Robin Hood hashing.
 * Robin Hood hashing swaps out entries that have a smaller probing distance
 * than the to-be-inserted entry, that evens out the average probing distance
 * and enables faster lookups. Backward shift deletion is employed to further
 * improve the performance and to avoid infinite loops in rare cases.
 *
 * Keys and values are stored in a double linked list by insertion order. This
 * has a slight performance overhead on lookup, which can be mostly compensated
 * using a paged allocator if required.
 *
 * The assignment operator copy the pairs from one map to the other.
 */

template <typename TKey, typename TValue>
struct HashMapElement {
	HashMapElement *next = nullptr;
	HashMapElement *prev = nullptr;
	KeyValue<TKey, TValue> data;
	HashMapElement() {}
	HashMapElement(const TKey &p_key, const TValue &p_value) :
			data(p_key, p_value) {}
};

template <typename TKey, typename TValue,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>,
		typename Allocator = DefaultTypedAllocator<HashMapElement<TKey, TValue>>>
class HashMap {
public:
	static constexpr uint32_t INITIAL_CAPACITY = 32;
	static constexpr float MAX_OCCUPANCY = 0.75;
	static constexpr uint32_t EMPTY_HASH = 0;

private:
	Allocator element_alloc;
	HashMapElement<TKey, TValue> **elements = nullptr;
	uint32_t *hashes = nullptr;
	HashMapElement<TKey, TValue> *head_element = nullptr;
	HashMapElement<TKey, TValue> *tail_element = nullptr;

	// Due to optimization, this is `capacity - 1`. Use + 1 to get normal capacity.
	uint32_t capacity = 0;
	uint32_t num_elements = 0;

	_FORCE_INLINE_ uint32_t _hash(const TKey &p_key) const {
		uint32_t hash = Hasher::hash(p_key);

		if (unlikely(hash == EMPTY_HASH)) {
			hash = EMPTY_HASH + 1;
		}

		return hash;
	}

	_FORCE_INLINE_ uint32_t _get_probe_length(uint32_t p_pos, uint32_t p_hash) const {
		const uint32_t original_pos = p_hash & capacity;
		if (unlikely(p_pos < original_pos)) {
			return capacity + 1 - original_pos + p_pos;
		}
		return p_pos - original_pos;
	}

	bool _lookup_pos(const TKey &p_key, uint32_t &r_pos) const {
		return _lookup_pos_with_hash(p_key, r_pos, _hash(p_key));
	}

	bool _lookup_pos_with_hash(const TKey &p_key, uint32_t &r_pos, uint32_t p_hash) const {
		uint32_t pos = p_hash & capacity;

		if (hashes[pos] == p_hash && Comparator::compare(elements[pos]->data.key, p_key)) {
			r_pos = pos;
			return true;
		}

		if (hashes[pos] == EMPTY_HASH) {
			return false;
		}

		// A collision occurred.
		pos = (pos + 1) & capacity;
		uint32_t distance = 1;
		while (true) {
			if (hashes[pos] == p_hash && Comparator::compare(elements[pos]->data.key, p_key)) {
				r_pos = pos;
				return true;
			}

			if (hashes[pos] == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_length(pos, hashes[pos])) {
				return false;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	void _insert_with_hash_and_element(uint32_t p_hash, HashMapElement<TKey, TValue> *p_value) {
		uint32_t hash = p_hash;
		HashMapElement<TKey, TValue> *value = p_value;
		uint32_t distance = 0;
		uint32_t pos = p_hash & capacity;

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				elements[pos] = value;
				hashes[pos] = hash;

				num_elements++;

				return;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos]);
			if (existing_probe_len < distance) {
				SWAP(hash, hashes[pos]);
				SWAP(value, elements[pos]);
				distance = existing_probe_len;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity) {
		uint32_t old_capacity = capacity;

		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_new_capacity);
		capacity = next_power_of_2(capacity - 1) - 1;

		HashMapElement<TKey, TValue> **old_elements = elements;
		uint32_t *old_hashes = hashes;

		num_elements = 0;
		hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
		elements = reinterpret_cast<HashMapElement<TKey, TValue> **>(Memory::alloc_static(sizeof(HashMapElement<TKey, TValue> *) * (capacity + 1)));

		for (uint32_t i = 0; i < capacity + 1; i++) {
			hashes[i] = 0;
			elements[i] = nullptr;
		}

		if (old_elements == nullptr) {
			// Nothing to do.
			return;
		}

		for (uint32_t i = 0; i < old_capacity + 1; i++) {
			if (old_hashes[i] == EMPTY_HASH) {
				continue;
			}

			_insert_with_hash_and_element(old_hashes[i], old_elements[i]);
		}

		Memory::free_static(old_elements);
		Memory::free_static(old_hashes);
	}

	// This method does not check that the key is in the table.
	// Use it if you know for sure that the key is new.
	_FORCE_INLINE_ HashMapElement<TKey, TValue> *_insert_with_hash(const TKey &p_key, const TValue &p_value, uint32_t p_hash, bool p_front_insert = false) {
		if (num_elements + 1 > MAX_OCCUPANCY * capacity) {
			_resize_and_rehash(capacity * 2);
		}

		HashMapElement<TKey, TValue> *elem = element_alloc.new_allocation(HashMapElement<TKey, TValue>(p_key, p_value));

		if (tail_element == nullptr) {
			head_element = elem;
			tail_element = elem;
		} else if (p_front_insert) {
			head_element->prev = elem;
			elem->next = head_element;
			head_element = elem;
		} else {
			tail_element->next = elem;
			elem->prev = tail_element;
			tail_element = elem;
		}

		_insert_with_hash_and_element(p_hash, elem);
		return elem;
	}

public:
	_FORCE_INLINE_ uint32_t get_capacity() const { return capacity + 1; }
	_FORCE_INLINE_ uint32_t size() const { return num_elements; }

	/* Standard Godot Container API */

	_FORCE_INLINE_ bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		if (num_elements == 0) {
			return;
		}
		for (uint32_t i = 0; i < capacity + 1; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}

			hashes[i] = EMPTY_HASH;
			element_alloc.delete_allocation(elements[i]);
			elements[i] = nullptr;
		}

		tail_element = nullptr;
		head_element = nullptr;
		num_elements = 0;
	}

	_FORCE_INLINE_ TValue &get(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		CRASH_COND_MSG(!exists, "HashMap key not found.");
		return elements[pos]->data.value;
	}

	_FORCE_INLINE_ const TValue &get(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		CRASH_COND_MSG(!exists, "HashMap key not found.");
		return elements[pos]->data.value;
	}

	_FORCE_INLINE_ const TValue *getptr(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &elements[pos]->data.value;
		}
		return nullptr;
	}

	_FORCE_INLINE_ TValue *getptr(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &elements[pos]->data.value;
		}
		return nullptr;
	}

	_FORCE_INLINE_ bool has(const TKey &p_key) const {
		uint32_t _pos = 0;
		return _lookup_pos(p_key, _pos);
	}

	bool erase(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (!exists) {
			return false;
		}

		uint32_t next_pos = (pos + 1) & capacity;
		while (hashes[next_pos] != EMPTY_HASH && _get_probe_length(next_pos, hashes[next_pos]) != 0) {
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(elements[next_pos], elements[pos]);
			pos = next_pos;
			next_pos = (next_pos + 1) & capacity;
		}

		hashes[pos] = EMPTY_HASH;

		if (head_element == elements[pos]) {
			head_element = elements[pos]->next;
		}

		if (tail_element == elements[pos]) {
			tail_element = elements[pos]->prev;
		}

		if (elements[pos]->prev) {
			elements[pos]->prev->next = elements[pos]->next;
		}

		if (elements[pos]->next) {
			elements[pos]->next->prev = elements[pos]->prev;
		}

		element_alloc.delete_allocation(elements[pos]);
		elements[pos] = nullptr;

		num_elements--;
		return true;
	}

	// Replace the key of an entry in-place, without invalidating iterators or changing the entries position during iteration.
	// p_old_key must exist in the map and p_new_key must not, unless it is equal to p_old_key.
	bool replace_key(const TKey &p_old_key, const TKey &p_new_key) {
		if (p_old_key == p_new_key) {
			return true;
		}
		uint32_t pos = 0;
		ERR_FAIL_COND_V(_lookup_pos(p_new_key, pos), false);
		ERR_FAIL_COND_V(!_lookup_pos(p_old_key, pos), false);
		HashMapElement<TKey, TValue> *element = elements[pos];

		// Delete the old entries in hashes and elements.
		uint32_t next_pos = (pos + 1) & capacity;
		while (hashes[next_pos] != EMPTY_HASH && _get_probe_length(next_pos, hashes[next_pos]) != 0) {
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(elements[next_pos], elements[pos]);
			pos = next_pos;
			next_pos = (next_pos + 1) & capacity;
		}
		hashes[pos] = EMPTY_HASH;
		elements[pos] = nullptr;
		// _insert_with_hash_and_element will increment this again.
		num_elements--;

		// Update the HashMapElement with the new key and reinsert it.
		const_cast<TKey &>(element->data.key) = p_new_key;
		uint32_t hash = _hash(p_new_key);
		_insert_with_hash_and_element(hash, element);

		return true;
	}

	// Reserves space for a number of elements, useful to avoid many resizes and rehashes.
	// If adding a known (possibly large) number of elements at once, must be larger than old capacity.
	void reserve(uint32_t p_new_capacity) {
		ERR_FAIL_COND_MSG(p_new_capacity < get_capacity(), "It is impossible to reserve less capacity than is currently available.");
		_resize_and_rehash(p_new_capacity);
	}

	/** Iterator API **/

	struct ConstIterator {
		_FORCE_INLINE_ const KeyValue<TKey, TValue> &operator*() const {
			return E->data;
		}
		_FORCE_INLINE_ const KeyValue<TKey, TValue> *operator->() const { return &E->data; }
		_FORCE_INLINE_ ConstIterator &operator++() {
			if (E) {
				E = E->next;
			}
			return *this;
		}
		_FORCE_INLINE_ ConstIterator &operator--() {
			if (E) {
				E = E->prev;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const ConstIterator &b) const { return E == b.E; }
		_FORCE_INLINE_ bool operator!=(const ConstIterator &b) const { return E != b.E; }

		_FORCE_INLINE_ explicit operator bool() const {
			return E != nullptr;
		}

		_FORCE_INLINE_ ConstIterator(const HashMapElement<TKey, TValue> *p_E) { E = p_E; }
		_FORCE_INLINE_ ConstIterator() {}
		_FORCE_INLINE_ ConstIterator(const ConstIterator &p_it) { E = p_it.E; }
		_FORCE_INLINE_ void operator=(const ConstIterator &p_it) {
			E = p_it.E;
		}

	private:
		const HashMapElement<TKey, TValue> *E = nullptr;
	};

	struct Iterator {
		_FORCE_INLINE_ KeyValue<TKey, TValue> &operator*() const {
			return E->data;
		}
		_FORCE_INLINE_ KeyValue<TKey, TValue> *operator->() const { return &E->data; }
		_FORCE_INLINE_ Iterator &operator++() {
			if (E) {
				E = E->next;
			}
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			if (E) {
				E = E->prev;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return E == b.E; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return E != b.E; }

		_FORCE_INLINE_ explicit operator bool() const {
			return E != nullptr;
		}

		_FORCE_INLINE_ Iterator(HashMapElement<TKey, TValue> *p_E) { E = p_E; }
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_it) { E = p_it.E; }
		_FORCE_INLINE_ void operator=(const Iterator &p_it) {
			E = p_it.E;
		}

		operator ConstIterator() const {
			return ConstIterator(E);
		}

	private:
		HashMapElement<TKey, TValue> *E = nullptr;
	};

	_FORCE_INLINE_ Iterator begin() {
		return Iterator(head_element);
	}
	_FORCE_INLINE_ Iterator end() {
		return Iterator(nullptr);
	}
	_FORCE_INLINE_ Iterator last() {
		return Iterator(tail_element);
	}

	_FORCE_INLINE_ Iterator find(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return end();
		}
		return Iterator(elements[pos]);
	}

	_FORCE_INLINE_ void remove(const Iterator &p_iter) {
		if (p_iter) {
			erase(p_iter->key);
		}
	}

	_FORCE_INLINE_ ConstIterator begin() const {
		return ConstIterator(head_element);
	}
	_FORCE_INLINE_ ConstIterator end() const {
		return ConstIterator(nullptr);
	}
	_FORCE_INLINE_ ConstIterator last() const {
		return ConstIterator(tail_element);
	}

	_FORCE_INLINE_ ConstIterator find(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return end();
		}
		return ConstIterator(elements[pos]);
	}

	/* Indexing */

	const TValue &operator[](const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		CRASH_COND(!exists);
		return elements[pos]->data.value;
	}

	TValue &operator[](const TKey &p_key) {
		uint32_t pos = 0;
		const uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, hash);
		if (exists) {
			return elements[pos]->data.value;
		} else {
			return _insert_with_hash(p_key, TValue(), hash)->data.value;
		}
	}

	/* Insert */

	Iterator insert(const TKey &p_key, const TValue &p_value, bool p_front_insert = false) {
		uint32_t pos = 0;
		const uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, hash);
		if (!exists) {
			return Iterator(_insert_with_hash(p_key, p_value, hash, p_front_insert));
		} else {
			elements[pos]->data.value = p_value;
			return Iterator(elements[pos]);
		}
	}

	/* Constructors */

	HashMap(const HashMap &p_other) {
		_resize_and_rehash(p_other.capacity);

		if (p_other.num_elements == 0) {
			return;
		}

		for (const KeyValue<TKey, TValue> &E : p_other) {
			insert(E.key, E.value);
		}
	}

	void operator=(const HashMap &p_other) {
		if (this == &p_other) {
			return; // Ignore self assignment.
		}
		if (num_elements != 0) {
			clear();
		}

		if (p_other.capacity != capacity) {
			_resize_and_rehash(p_other.capacity);
		}

		for (const KeyValue<TKey, TValue> &E : p_other) {
			insert(E.key, E.value);
		}
	}

	HashMap(uint32_t p_initial_capacity) {
		_resize_and_rehash(p_initial_capacity);
	}
	HashMap() {
		_resize_and_rehash(INITIAL_CAPACITY);
	}

	uint32_t debug_get_hash(uint32_t p_index) {
		if (num_elements == 0) {
			return 0;
		}
		ERR_FAIL_INDEX_V(p_index, get_capacity(), 0);
		return hashes[p_index];
	}
	Iterator debug_get_element(uint32_t p_index) {
		if (num_elements == 0) {
			return Iterator();
		}
		ERR_FAIL_INDEX_V(p_index, get_capacity(), Iterator());
		return Iterator(elements[p_index]);
	}

	~HashMap() {
		clear();

		Memory::free_static(elements);
		Memory::free_static(hashes);
	}
};

#endif // HASH_MAP_H
