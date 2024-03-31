#include "core/templates/hash_set.h"
#ifndef HASH_SET_H_1
#define HASH_SET_H_1

template <typename TKey,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>>
class HashSet1 {
public:
	static constexpr uint32_t INITIAL_CAPACITY = 32;
	static constexpr float MAX_OCCUPANCY = 0.75;
	static constexpr uint32_t EMPTY_HASH = 0;

private:
	TKey *keys = nullptr;
	uint32_t *hash_to_key = nullptr;
	uint32_t *key_to_hash = nullptr;
	uint32_t *hashes = nullptr;

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
		if (unlikely(keys == nullptr)) {
			return false; // Failed lookups, no elements
		}

		uint32_t pos = p_hash & capacity;

		if (hashes[pos] == p_hash && Comparator::compare(keys[hash_to_key[pos]], p_key)) {
			r_pos = hash_to_key[pos];
			return true;
		}

		if (hashes[pos] == EMPTY_HASH) {
			return false;
		}

		// A collision occurred.
		pos = (pos + 1) & capacity;
		uint32_t distance = 1;
		while (true) {
			if (hashes[pos] == p_hash && Comparator::compare(keys[hash_to_key[pos]], p_key)) {
				r_pos = hash_to_key[pos];
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

	uint32_t _insert_with_hash(uint32_t p_hash, uint32_t p_index) {
		uint32_t hash = p_hash;
		uint32_t index = p_index;
		uint32_t distance = 0;
		uint32_t pos = p_hash & capacity;

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				hashes[pos] = hash;
				key_to_hash[index] = pos;
				hash_to_key[pos] = index;
				return pos;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos]);
			if (existing_probe_len < distance) {
				key_to_hash[index] = pos;
				SWAP(hash, hashes[pos]);
				SWAP(index, hash_to_key[pos]);
				distance = existing_probe_len;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity) {
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_new_capacity);
		capacity = next_power_of_2(capacity - 1) - 1;

		uint32_t *old_hashes = hashes;
		uint32_t *old_key_to_hash = key_to_hash;

		hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
		keys = reinterpret_cast<TKey *>(Memory::realloc_static(keys, sizeof(TKey) * (capacity + 1)));
		key_to_hash = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
		hash_to_key = reinterpret_cast<uint32_t *>(Memory::realloc_static(hash_to_key, sizeof(uint32_t) * (capacity + 1)));

		for (uint32_t i = 0; i < capacity + 1; i++) {
			hashes[i] = EMPTY_HASH;
		}

		for (uint32_t i = 0; i < num_elements; i++) {
			uint32_t h = old_hashes[old_key_to_hash[i]];
			_insert_with_hash(h, i);
		}

		Memory::free_static(old_hashes);
		Memory::free_static(old_key_to_hash);
	}

	_FORCE_INLINE_ int32_t _insert(const TKey &p_key) {
		if (unlikely(keys == nullptr)) {
			// Allocate on demand to save memory.

			hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
			keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * (capacity + 1)));
			key_to_hash = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
			hash_to_key = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));

			for (uint32_t i = 0; i < capacity + 1; i++) {
				hashes[i] = EMPTY_HASH;
			}
		}

		uint32_t pos = 0;
		uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, hash);

		if (exists) {
			return pos;
		} else {
			if (num_elements + 1 > MAX_OCCUPANCY * capacity) {
				_resize_and_rehash(capacity * 2);
			}

			if constexpr (!std::is_trivially_constructible_v<TKey>) {
				memnew_placement(&keys[num_elements], TKey(p_key));
			} else {
				TKey key = p_key;
				keys[num_elements] = key;
			}

			_insert_with_hash(hash, num_elements);
			num_elements++;
			return num_elements - 1;
		}
	}

	void _init_from(const HashSet1 &p_other) {
		capacity = p_other.capacity;
		num_elements = p_other.num_elements;

		if (p_other.num_elements == 0) {
			return;
		}

		hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
		keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * (capacity + 1)));
		key_to_hash = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));
		hash_to_key = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * (capacity + 1)));

		for (uint32_t i = 0; i < num_elements; i++) {
			if constexpr (!std::is_trivially_constructible_v<TKey>) {
				memnew_placement(&keys[i], TKey(p_other.keys[i]));
			} else {
				TKey key = p_other.keys[i];
				keys[i] = key;
			}
			key_to_hash[i] = p_other.key_to_hash[i];
		}

		for (uint32_t i = 0; i < capacity + 1; i++) {
			hashes[i] = p_other.hashes[i];
			hash_to_key[i] = p_other.hash_to_key[i];
		}
	}

public:
	_FORCE_INLINE_ uint32_t get_capacity() const { return capacity + 1; }
	_FORCE_INLINE_ uint32_t size() const { return num_elements; }

	/* Standard Godot Container API */

	_FORCE_INLINE_ bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		if (keys == nullptr || num_elements == 0) {
			return;
		}
		for (uint32_t i = 0; i < capacity + 1; i++) {
			hashes[i] = EMPTY_HASH;
		}
		if constexpr (!std::is_trivially_destructible_v<TKey>) {
			for (uint32_t i = 0; i < num_elements; i++) {
				keys[i].~TKey();
			}
		}

		num_elements = 0;
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

		uint32_t key_pos = pos;
		pos = key_to_hash[pos]; //make hash pos

		uint32_t next_pos = (pos + 1) & capacity;
		while (hashes[next_pos] != EMPTY_HASH && _get_probe_length(next_pos, hashes[next_pos]) != 0) {
			uint32_t kpos = hash_to_key[pos];
			uint32_t kpos_next = hash_to_key[next_pos];
			SWAP(key_to_hash[kpos], key_to_hash[kpos_next]);
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(hash_to_key[next_pos], hash_to_key[pos]);

			pos = next_pos;
			next_pos = (next_pos + 1) & capacity;
		}

		hashes[pos] = EMPTY_HASH;
		if constexpr (!std::is_trivially_destructible_v<TKey>) {
			keys[key_pos].~TKey();
		}
		num_elements--;
		if (key_pos < num_elements) {
			// Not the last key, move the last one here to keep keys lineal
			if constexpr (!std::is_trivially_constructible_v<TKey>) {
				memnew_placement(&keys[key_pos], TKey(keys[num_elements]));
			} else {
				TKey key = keys[num_elements];
				keys[key_pos] = key;
			}
			if constexpr (!std::is_trivially_destructible_v<TKey>) {
				keys[num_elements].~TKey();
			}
			key_to_hash[key_pos] = key_to_hash[num_elements];
			hash_to_key[key_to_hash[num_elements]] = key_pos;
		}

		return true;
	}

	// Reserves space for a number of elements, useful to avoid many resizes and rehashes.
	// If adding a known (possibly large) number of elements at once, must be larger than old capacity.
	void reserve(uint32_t p_new_capacity) {
		ERR_FAIL_COND(p_new_capacity < capacity + 1);
		if (keys == nullptr) {
			capacity = MAX(4u, p_new_capacity);
			capacity = next_power_of_2(capacity - 1) - 1;
			return; // Unallocated yet.
		}
		_resize_and_rehash(p_new_capacity);
	}

	/** Iterator API **/

	struct Iterator {
		_FORCE_INLINE_ const TKey &operator*() const {
			return keys[index];
		}
		_FORCE_INLINE_ const TKey *operator->() const {
			return &keys[index];
		}
		_FORCE_INLINE_ Iterator &operator++() {
			index++;
			if (index >= (int32_t)num_keys) {
				index = -1;
				keys = nullptr;
				num_keys = 0;
			}
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			index--;
			if (index < 0) {
				index = -1;
				keys = nullptr;
				num_keys = 0;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return keys == b.keys && index == b.index; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return keys != b.keys || index != b.index; }

		_FORCE_INLINE_ explicit operator bool() const {
			return keys != nullptr;
		}

		_FORCE_INLINE_ Iterator(const TKey *p_keys, uint32_t p_num_keys, int32_t p_index = -1) {
			keys = p_keys;
			num_keys = p_num_keys;
			index = p_index;
		}
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_it) {
			keys = p_it.keys;
			num_keys = p_it.num_keys;
			index = p_it.index;
		}
		_FORCE_INLINE_ void operator=(const Iterator &p_it) {
			keys = p_it.keys;
			num_keys = p_it.num_keys;
			index = p_it.index;
		}

	private:
		const TKey *keys = nullptr;
		uint32_t num_keys = 0;
		int32_t index = -1;
	};

	_FORCE_INLINE_ Iterator begin() const {
		return num_elements ? Iterator(keys, num_elements, 0) : Iterator();
	}
	_FORCE_INLINE_ Iterator end() const {
		return Iterator();
	}
	_FORCE_INLINE_ Iterator last() const {
		if (num_elements == 0) {
			return Iterator();
		}
		return Iterator(keys, num_elements, num_elements - 1);
	}

	_FORCE_INLINE_ Iterator find(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return end();
		}
		return Iterator(keys, num_elements, pos);
	}

	_FORCE_INLINE_ void remove(const Iterator &p_iter) {
		if (p_iter) {
			erase(*p_iter);
		}
	}

	/* Insert */

	Iterator insert(const TKey &p_key) {
		uint32_t pos = _insert(p_key);
		return Iterator(keys, num_elements, pos);
	}

	/* Constructors */

	HashSet1(const HashSet1 &p_other) {
		_init_from(p_other);
	}

	void operator=(const HashSet1 &p_other) {
		if (this == &p_other) {
			return; // Ignore self assignment.
		}

		clear();

		if (keys != nullptr) {
			Memory::free_static(keys);
			Memory::free_static(key_to_hash);
			Memory::free_static(hash_to_key);
			Memory::free_static(hashes);
			keys = nullptr;
			hashes = nullptr;
			hash_to_key = nullptr;
			key_to_hash = nullptr;
		}

		_init_from(p_other);
	}

	HashSet1(uint32_t p_initial_capacity) {
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_initial_capacity);
		capacity = next_power_of_2(capacity - 1) - 1;
	}
	HashSet1() {
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, INITIAL_CAPACITY);
		capacity = next_power_of_2(capacity - 1) - 1;
	}

	void reset() {
		clear();

		if (keys != nullptr) {
			Memory::free_static(keys);
			Memory::free_static(key_to_hash);
			Memory::free_static(hash_to_key);
			Memory::free_static(hashes);
			keys = nullptr;
			hashes = nullptr;
			hash_to_key = nullptr;
			key_to_hash = nullptr;
		}
		capacity = 0;
	}

	~HashSet1() {
		reset();
	}
};

#endif // HASH_SET_H
