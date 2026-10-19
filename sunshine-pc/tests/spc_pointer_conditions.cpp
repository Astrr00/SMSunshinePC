// Real SPC slices, stacks and opcodes; allocated Windows addresses share low32 bits.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Strategic/SpcInterp.hpp>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>

static void putBE(u8* p, u32 value)
{
	for (int i = 0; i < 4; ++i)
		p[i] = (u8)(value >> (24 - i * 8));
}

int main()
{
	static_assert(sizeof(TSpcSlice) == 16 && offsetof(TSpcSlice, mData) == 8);
	void* first = nullptr;
	void* second = nullptr;
	for (uintptr_t slot = 1; slot < 32; slot += 2) {
		first = VirtualAlloc((void*)(slot << 32), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!first)
			continue;
		second = VirtualAlloc((void*)((slot + 1) << 32), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (second)
			break;
		VirtualFree(first, 0, MEM_RELEASE);
		first = nullptr;
	}
	if (!first || !second) {
		std::puts("FAIL: could not allocate the two required real addresses");
		return 2;
	}
	*(int*)first = 17;
	*(int*)second = 29;
	std::printf("real distinct allocations: %p / %p, low32=%08X / %08X\n", first, second,
	            (u32)(uintptr_t)first, (u32)(uintptr_t)second);
	u8 image[64] = {};
	std::memcpy(image, "SPCB", 4);
	putBE(image + 4, 28);
	putBE(image + 8, 32);
	putBE(image + 16, 32);
	TSpcBinary binary(image);
	TSpcInterp vm(&binary, nullptr, 100, 16, 16, 8);
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char* name) {
		++checks;
		if (!ok) { ++failures; std::printf("FAIL: %s\n", name); }
	};
	vm.push(first);
	TSpcSlice a = vm.pop();
	vm.push(second);
	TSpcSlice b = vm.pop();
	vm.push((void*)nullptr);
	TSpcSlice nullHandle = vm.pop();
	check(a.getDataPtr() == first && b.getDataPtr() == second, "native pointer push/pop retains addresses");
	check(*(int*)a.getDataPtr() == 17 && *(int*)b.getDataPtr() == 29, "the two handles address real distinct storage");
	check(!(a == b) && a != b, "different handles sharing low32 are unequal");
	check(a == a && !(a != a), "same handle is equal");
	check(!(a == TSpcSlice(0)) && a != TSpcSlice(0), "nonnull handle with zero low32 differs from null integer");
	check(nullHandle == TSpcSlice(0), "null handle equals integer zero");
	check(b > a && a < b && b >= a && a <= b, "integer handle ordering uses the complete address");
	check(!(a == TSpcSlice("name")) && a != TSpcSlice("name"), "nonnull handle is not a string's legacy numeric zero");
	for (const TSpcSlice& handle : {a, b}) {
		vm.push(handle); vm.push(0); vm.execeq();
		check(vm.pop().getDataInt() == 0, "EQ does not confuse a live handle with null");
		vm.push(handle); vm.push(0); vm.execne();
		check(vm.pop().getDataInt() == 1, "NE recognizes a live handle");
		vm.push(handle); vm.execnot();
		check(vm.pop().getDataInt() == 0, "NOT recognizes a live handle");
		vm.push(handle); vm.push(1); vm.execand();
		check(vm.pop().getDataInt() == 1, "AND recognizes a live handle");
		vm.push(handle); vm.push(0); vm.execor();
		check(vm.pop().getDataInt() == 1, "OR recognizes a live handle");
		putBE(image + 28, 0x1234); vm.mProgramCounter = 0;
		vm.push(handle); vm.execjne();
		check(vm.mProgramCounter == 4, "conditional branch does not take the null path for a live handle");
	}
	vm.push(a); vm.push(b); vm.execeq();
	check(vm.pop().getDataInt() == 0, "EQ distinguishes full pointer payloads");
	putBE(image + 28, 0x1234); vm.mProgramCounter = 0;
	vm.push(nullHandle); vm.execjne();
	check(vm.mProgramCounter == 0x1234, "null handle still takes the conditional branch");
	for (int value : {INT_MIN, -1, 0, 1, INT_MAX}) {
		TSpcSlice n(value);
		check(n == TSpcSlice(value), "ordinary integer equality is unchanged");
		check((n < TSpcSlice(0)) == (value < 0), "signed integer order is unchanged");
		vm.push(n); vm.execnot();
		check(vm.pop().getDataInt() == (value == 0), "integer truth remains unchanged");
	}
	TSpcSlice wrapped(INT_MAX);
	++wrapped;
	check(wrapped == TSpcSlice(INT_MIN) && wrapped < TSpcSlice(0), "ordinary integer comparisons retain 32-bit conversion after increment");
	for (float value : {0.0f, 0.25f, -1.0f, 1.5f}) {
		check(TSpcSlice(value) == TSpcSlice(value), "float equality remains unchanged");
		vm.push(value); vm.execnot();
		check(vm.pop().getDataInt() == ((int)value == 0), "legacy float-to-int truth remains unchanged");
	}
	check(TSpcSlice(1) == TSpcSlice(1.0f), "mixed numeric equality remains unchanged");
	check(TSpcSlice("same") == TSpcSlice("same"), "string equality remains unchanged");
	a.setDataInt(0);
	check(a == TSpcSlice(0), "replacing a handle with an integer clears pointer provenance");
	b.setDataFloat(0.25f);
	vm.push(b); vm.execnot();
	check(vm.pop().getDataInt() == 1, "replacing a handle with float restores legacy truth");
	b.setDataString("name");
	vm.push(b); vm.execnot();
	check(vm.pop().getDataInt() == 1, "replacing a handle with string restores legacy truth");
	VirtualFree(first, 0, MEM_RELEASE);
	VirtualFree(second, 0, MEM_RELEASE);
	std::printf("%d SPC pointer-condition checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
