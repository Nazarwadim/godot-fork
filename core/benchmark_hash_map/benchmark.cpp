#include "benchmark.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/templates/hash_map.h"
#include "core/templates/oa_hash_map.h"
#include "core/templates/vmap.h"
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

HashMap1<Variant, long, VariantHasher> before_arr0[10];
HashMap<Variant, long, VariantHasher> after_arr0[10];

HashSet1<int> after_set_arr[10];
HashSet<int> before_set_arr[10];

VMap<Vector3, long> map1[10];
OAHashMap<Variant, long, VariantHasher> after_oa_arr[10];
OAHashMap1<Variant, long, VariantHasher> before_oa_arr[10];

Vector<int> rand_vec;
Vector<int> rand_vec2;
int found1;
int found2;
bool has1;
bool has2;
uint32_t count = 3;

void insert_random_variables_in_vec() {
	for (uint32_t i = 0; i < 100000000; i++) {
		rand_vec.push_back(rand());
	}
}

void insert_random_variables_in_vec2() {
	for (uint32_t i = 0; i < 2 * count; i++) {
		rand_vec2.push_back(rand());
	}
}

int a = 1;
int a2 = 1;

LocalVector<int> randoma;

__attribute__((noinline)) void test_1() {
	int num = a;
	int count_local = 2 * count;
	for (int i = 0; i < count_local; i++) {
		rand_vec.write[i] += num;
	}
}

__attribute__((noinline)) void test_2() {
	int num = a;
	for (const int value : rand_vec) {
		num += value;
	}
	a = num;
}

__attribute__((noinline)) void test_3() {
	int num = a;
	int count_local = 2 * count;
	for (int i = 0; i < count_local; i++) {
		num += rand_vec2[i];
	}
	a = num;
}

__attribute__((noinline)) void test_hash_map12() {
	insert_random_variables_in_vec2();
	uint64_t start1 = OS::get_singleton()->get_ticks_usec();
	test_2();
	uint64_t end1 = OS::get_singleton()->get_ticks_usec();
	print_line("Time set before:", end1 - start1);

	uint64_t start2 = OS::get_singleton()->get_ticks_usec();
	test_1();
	uint64_t end2 = OS::get_singleton()->get_ticks_usec();
	print_line("Time set after:", end2 - start2);

	uint64_t start3 = OS::get_singleton()->get_ticks_usec();
	test_3();
	uint64_t end3 = OS::get_singleton()->get_ticks_usec();
	print_line("Time set after:", end3 - start3);
}

__attribute__((noinline)) void test_hash_map_or() {
	// for (int i = 0; i < 10; i++) {
	// 	before_arr0[i].reserve(5 * pow(count, i + 1));
	// 	after_arr0[i].reserve(5 * pow(count, i + 1));
	// }

	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			before_arr0[ik][Vector3(rand_vec[i], rand_vec[i], rand_vec[i])] = rand_vec[i] * rand_vec[i];
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert before:", currentTime, "");
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			after_arr0[ik][Vector3(rand_vec[i], rand_vec[i], rand_vec[i])] = rand_vec[i] * rand_vec[i];
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert after:", currentTime);
	}
	print_line(" ");
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
			auto *e = before_arr0[ik].getptr(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]));
			if (e != nullptr) {
				found1 = *e;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find before:", currentTime);
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
			auto *e = after_arr0[ik].getptr(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]));
			if (e != nullptr) {
				found1 = *e;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find after:", currentTime);
	}
}

__attribute__((noinline)) void test_hash_map_set() {
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
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
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
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
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

__attribute__((noinline)) void test_oa_before() {
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			before_oa_arr[ik].set(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]), rand_vec[i] * rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert before:", currentTime, "");
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
			long data = 0;
			if (before_oa_arr[ik].lookup(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]), data)) {
				found1 = data;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find before:", currentTime);
	}
}

__attribute__((noinline)) void test_oa_after() {
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			after_oa_arr[ik].set(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]), rand_vec[i] * rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Time insert after:", currentTime);
	}
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
			long data = 0;
			if (after_oa_arr[ik].lookup(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]), data)) {
				found1 = data;
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find after:", currentTime);
	}
}

void test_hash_third() {
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start_insert = chrono::system_clock::now();
		for (uint32_t i = 0; i < c; i++) {
			map1[ik].insert(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]), rand_vec[i] * rand_vec[i]);
		}
		auto clock_now_insert = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::microseconds>(clock_now_insert - clock_start_insert).count());
		print_line("Size:", map1[ik].size());
		print_line("Time insert stl:", currentTime, "");
	}

	print_line(" ");
	for (int ik = 0; ik < 10; ik++) {
		uint32_t c = pow(count, ik + 1);
		auto clock_start = chrono::system_clock::now();
		for (uint32_t i = 0; i < 2 * 1000000; i++) {
			auto e = map1[ik].find(Vector3(rand_vec[i], rand_vec[i], rand_vec[i]));

			if (e != -1) {
				found1 = map1[ik].getv(e);
			}
		}
		auto clock_now = chrono::system_clock::now();
		auto currentTime = float(chrono::duration_cast<chrono::nanoseconds>(clock_now - clock_start).count());
		print_line("Time find stl:", currentTime);
	}
}

void main_benchmark() {
	insert_random_variables_in_vec();

	test_hash_map_or();
	print_line(" ");
	print_line(" ");
	test_oa_before();
	print_line(" ");
	test_oa_after();
	print_line(" ");
	print_line(" ");
	test_hash_third();
}
