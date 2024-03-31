#include "benchmark.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/templates/hash_map.h"
#include "core/templates/oa_hash_map.h"
#include "core/templates/rb_set.h"
#include "core/templates/vmap.h"
#include "fast_mod_hash_map.h"
#include "hash_map_version_before.h"
#include "new_hash_set.h"
#include "old_hash_map.h"
#include "old_oa_hash_map.h"
#include <chrono>
#include <map>

template <>
struct std::hash<Variant> {
	_FORCE_INLINE_ std::size_t operator()(const Variant &k) const {
		return VariantHasher::hash(k);
	}
};

struct Hasher {
	static _FORCE_INLINE_ uint32_t hash(const uint32_t p_int) { return p_int; }
	static _FORCE_INLINE_ uint32_t hash(const int32_t p_int) { return p_int; }
	static _FORCE_INLINE_ uint32_t hash(const uint64_t p_int) {
		union {
			uint64_t ui;
			uint32_t i1;
			uint32_t i2;
		} conv = { p_int };
		uint32_t i1 = conv.i1;
		conv.i1 |= conv.i2;
		conv.i1 *= i1;
		return conv.i1;
	}
};

using namespace std;

HashMap4<int, long> before_arr0[10]; // Fastmod.
HashMapVB<int, long> after_arr0[10]; // The same, but without fastmod.

HashSet1<int> after_set_arr[10];
HashSet<int> before_set_arr[10];

OAHashMap<Variant, int, VariantHasher, VariantComparator> after_oa_arr[10];
OAHashMap1<Variant, int, VariantHasher, VariantComparator> before_oa_arr[10];

Vector<int> rand_vec;
int found1;
int found2;
bool has1;
bool has2;
uint32_t count = 5; // Exponent.

void insert_random_numbers_in_vec() {
	for (uint32_t i = 1000000000; i < 1000000000 + 200000000; i += 10) {
		rand_vec.push_back(rand());
	}
}

void test_hash_map_or_before() {
	uint64_t general_time = OS::get_singleton()->get_ticks_msec();
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			before_arr0[ik][rand_vec[i]] = rand_vec[i] * rand_vec[i];
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert before:", currentTime, "");
	}

	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * c; i++) {
			auto *e = before_arr0[ik].getptr(rand_vec[i]);
			if (e != nullptr) {
				found1 = *e;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find before:", currentTime);
	}
	general_time = OS::get_singleton()->get_ticks_msec() - general_time;
	print_line("General_time msec:", general_time);
}

void test_hash_map_or_after() {
	uint64_t general_time = OS::get_singleton()->get_ticks_msec();
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			after_arr0[ik][rand_vec[i]] = rand_vec[i] * rand_vec[i];
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert after:", currentTime);
	}

	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * c; i++) {
			auto *e = after_arr0[ik].getptr(rand_vec[i]);
			if (e != nullptr) {
				found1 = *e;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find after:", currentTime);
	}
	general_time = OS::get_singleton()->get_ticks_msec() - general_time;
	print_line("General_time msec:", general_time);
}

void test_hash_map_or() {
	test_hash_map_or_before();
	print_line(" ");
	test_hash_map_or_after();
}

void test_hash_map_set() {
	for (int i = 0; i < 10; i++) {
		before_set_arr[i].reserve(5 * pow(count, i + 1));
		after_set_arr[i].reserve(5 * pow(count, i + 1));
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			before_set_arr[ik].insert(rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert before:", currentTime, "");
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			after_set_arr[ik].insert(rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert after:", currentTime);
	}
	print_line(" ");
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * c; i++) {
			auto it = before_set_arr[ik].find(rand_vec[i]);
			if (it != before_set_arr[ik].end()) {
				found1 = rand_vec[i];
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find before:", currentTime);
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * c; i++) {
			auto it = after_set_arr[ik].find(rand_vec[i]);
			if (it != after_set_arr[ik].end()) {
				found1 = rand_vec[i];
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find after:", currentTime);
	}
}

void test_oa_before() {
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			before_oa_arr[ik].set(rand_vec[i], rand_vec[i] * rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert before:", currentTime, "");
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * c; i++) {
			int data = 0;
			if (before_oa_arr[ik].lookup(rand_vec[i], data)) {
				found1 = data;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find before:", currentTime);
	}
}

void test_oa_after() {
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			after_oa_arr[ik].set(rand_vec[i], rand_vec[i] * rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert after:", currentTime);
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * c; i++) {
			int data = 0;
			if (after_oa_arr[ik].lookup(rand_vec[i], data)) {
				found1 = data;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find after:", currentTime);
	}
}

void main_benchmark() {
	insert_random_numbers_in_vec();
	test_hash_map_or();
}
