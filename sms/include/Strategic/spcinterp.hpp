#ifndef STRATEGIC_SPCINTERP_HPP
#define STRATEGIC_SPCINTERP_HPP

#ifdef TARGET_PC
#include <hostendian.h>
#endif
#include <dolphin/types.h>
#include <string.h>
#include <types.h>

// Tracing is compiled out of the release build, so the body is empty. The map
// records the symbol as weak in several translation units, which is why it
// lives in this header rather than in spcinterp.cpp.
inline void SpcTrace(const char*, ...) { }

template <class T> class TSpcStack {
public:
	/* 0x0 */ s32 mCapacity;
	/* 0x4 */ s32 mSize;
	/* 0x8 */ T* mData;

public:
	TSpcStack(u32 capacity)
	    : mCapacity(capacity)
	    , mSize(0)
	    , mData(nullptr)
	{
		mData = new T[mCapacity];
	}

	virtual ~TSpcStack() { delete[] mData; }

	void push(const T& v)
	{
		if (mSize >= mCapacity) {
			SpcTrace("TSpcStack : stack overflow\n");
			return;
		}

		mData[mSize] = v;
		++mSize;
	}

	// fabricated
	T pop()
	{
		if (mSize <= 0) {
			SpcTrace("TSpcStack : stack underflow\n");
			return mData[mSize];
		}
		--mSize;
		return mData[mSize];
	}

	s32 size() const { return mSize; }
	T& getFromBottom(u32 idx) { return mData[idx]; }
	T& getFromTop(u32 idx) { return mData[mSize - 1 - idx]; }
	void setFromTop(u32 idx, const T& v) { mData[mSize - 1 - idx] = v; }
	void setFromBottom(u32 idx, const T& v) { mData[idx] = v; }
};

class TSpcSlice {
public:
	/* 0x0 */ u32 mType;
#ifdef TARGET_PC
	// Uses the host padding before mData; ordinary script ints stay 32-bit.
	bool mHostPointer;
#endif
	/* 0x4 */ union {
#ifdef TARGET_PC
		// Script ints are also how native calls hand object addresses to the
		// script and back (see getDataPtr), so the slot is pointer-wide. The
		// union already is, because of asString: the size is unchanged.
		intptr_t asInt;
#else
		int asInt;
#endif
		float asFloat;
		const char* asString;
	} mData;

public:
	enum { TYPE_INT, TYPE_FLOAT, TYPE_STRING };

	TSpcSlice()
	    : mType(TYPE_INT)
#ifdef TARGET_PC
	    , mHostPointer(false)
#endif
	{
#ifdef TARGET_PC
		mData.asInt = 0;
#else
		(int&)mData = 0;
#endif
	}

	TSpcSlice(int value)
	    : mType(TYPE_INT)
#ifdef TARGET_PC
	    , mHostPointer(false)
#endif
	{
#ifdef TARGET_PC
		mData.asInt = value;
#else
		(int&)mData = value;
#endif
	}

	TSpcSlice(f32 value)
	    : mType(TYPE_FLOAT)
#ifdef TARGET_PC
	    , mHostPointer(false)
#endif
	{
		(float&)mData = value;
	}

	TSpcSlice(const char* value)
	    : mType(TYPE_STRING)
#ifdef TARGET_PC
	    , mHostPointer(false)
#endif
	{
		if (!value)
			mData.asString = "";
		else
			mData.asString = value;
	}

	u32 typeof() const { return mType; }

	int getDataInt() const
	{
		switch (mType) {
		case TYPE_INT:
#ifdef TARGET_PC
			return (int)mData.asInt;
#else
			return mData.asInt;
#endif
		case TYPE_FLOAT:
			return mData.asFloat;
		default:
			(void)0;
			return 0;
		}
	}

#ifdef TARGET_PC
	intptr_t getDataIntForComparison() const
	{
		return mHostPointer ? mData.asInt : getDataInt();
	}

	// The address a native call pushed with TSpcInterp::push(void*).
	void* getDataPtr() const
	{
		if (mType == TYPE_INT)
			return (void*)mData.asInt;
		return nullptr;
	}
#endif

	operator int() const { return getDataInt(); }

	f32 getDataFloat() const
	{
		switch (mType) {
		case TYPE_INT:
			return mData.asInt;
		case TYPE_FLOAT:
			return mData.asFloat;
		default:
			(void)0;
			return 0.0f;
		}
	}

	operator float() const { return getDataFloat(); }

	void setDataInt(int i)
	{
#ifdef TARGET_PC
		mHostPointer = false;
#endif
		mData.asInt = i;
		mType       = TYPE_INT;
	}

	void setDataFloat(f32 f)
	{
#ifdef TARGET_PC
		mHostPointer = false;
#endif
		mType         = TYPE_FLOAT;
		mData.asFloat = f;
	}

	void setDataString(const char* s)
	{
#ifdef TARGET_PC
		mHostPointer = false;
#endif
		mType = TYPE_STRING;
		if (!s)
			mData.asString = "";
		else
			mData.asString = s;
	}

	const char* getDataString() const
	{
		switch (mType) {
		case TYPE_STRING:
			return mData.asString;
		default:
			return "";
		}
	}

	TSpcSlice& operator++()
	{
		switch (mType) {
		case TYPE_INT:
			mData.asInt += 1;
			break;
		case TYPE_FLOAT:
			mData.asFloat += 1.0f;
			break;
		default:
			break;
		}
		return *this;
	}

	TSpcSlice& operator--()
	{
		switch (mType) {
		case TYPE_INT:
			mData.asInt -= 1;
			break;
		case TYPE_FLOAT:
			mData.asFloat -= 1.0f;
			break;
		default:
			break;
		}
		return *this;
	}

	// Returns *this so the caller can chain it into a push. The reference
	// return is load-bearing: it makes MWCC force-load the address into a
	// temporary, which is what the original frames show.
	TSpcSlice& negate()
	{
		switch (mType) {
		case TSpcSlice::TYPE_INT:
			mData.asInt = -mData.asInt;
			break;
		case TSpcSlice::TYPE_FLOAT:
			mData.asFloat = -mData.asFloat;
			break;
		default:
			break;
		}
		return *this;
	}

	BOOL operator==(const TSpcSlice& other) const
	{
		if (mType == TYPE_STRING && other.mType == TYPE_STRING) {
			if (strcmp(mData.asString, other.mData.asString) == 0)
				return true;
			else
				return false;
		} else if (mType == TYPE_FLOAT || other.mType == TYPE_FLOAT) {
			if (getDataFloat() == other.getDataFloat())
				return true;
			else
				return false;
		} else {
#ifdef TARGET_PC
			return getDataIntForComparison()
			       == other.getDataIntForComparison();
#else
			if (getDataInt() == other.getDataInt())
				return true;
			else
				return false;
#endif
		}
	}
	BOOL operator!=(const TSpcSlice& other) const
	{
		if (mType == TYPE_STRING && other.mType == TYPE_STRING) {
			if (strcmp(mData.asString, other.mData.asString) != 0)
				return true;
			else
				return false;
		} else if (mType == TYPE_FLOAT || other.mType == TYPE_FLOAT) {
			if (getDataFloat() != other.getDataFloat())
				return true;
			else
				return false;
		} else {
#ifdef TARGET_PC
			return getDataIntForComparison()
			       != other.getDataIntForComparison();
#else
			if (getDataInt() != other.getDataInt())
				return true;
			else
				return false;
#endif
		}
	}

	// The ternary is load-bearing twice over: it keeps the body at 6
	// statements, so the operator still expands at pass 1 when it sits in
	// argument position (the verbose if/else form is 12 and would be refused
	// there, forcing an out-of-line copy the ROM does not have), and returning
	// TRUE/FALSE rather than the bool literals avoids a bool-to-BOOL
	// zero-extend.
	friend BOOL operator>(const TSpcSlice& a, const TSpcSlice& b)
	{
		if (a.mType == TYPE_FLOAT || b.mType == TYPE_FLOAT)
			return (float)a > (float)b ? TRUE : FALSE;
		else
#ifdef TARGET_PC
			return a.getDataIntForComparison() > b.getDataIntForComparison()
			           ? TRUE : FALSE;
#else
			return (int)a > (int)b ? TRUE : FALSE;
#endif
	}

	friend BOOL operator<(const TSpcSlice& a, const TSpcSlice& b)
	{
		if (a.mType == TYPE_FLOAT || b.mType == TYPE_FLOAT)
			return (float)a < (float)b ? TRUE : FALSE;
		else
#ifdef TARGET_PC
			return a.getDataIntForComparison() < b.getDataIntForComparison()
			           ? TRUE : FALSE;
#else
			return (int)a < (int)b ? TRUE : FALSE;
#endif
	}

	friend BOOL operator>=(const TSpcSlice& a, const TSpcSlice& b)
	{
		if (a.mType == TYPE_FLOAT || b.mType == TYPE_FLOAT)
			return (float)a >= (float)b ? TRUE : FALSE;
		else
#ifdef TARGET_PC
			return a.getDataIntForComparison() >= b.getDataIntForComparison()
			           ? TRUE : FALSE;
#else
			return (int)a >= (int)b ? TRUE : FALSE;
#endif
	}

	friend BOOL operator<=(const TSpcSlice& a, const TSpcSlice& b)
	{
		if (a.mType == TYPE_FLOAT || b.mType == TYPE_FLOAT)
			return (float)a <= (float)b ? TRUE : FALSE;
		else
#ifdef TARGET_PC
			return a.getDataIntForComparison() <= b.getDataIntForComparison()
			           ? TRUE : FALSE;
#else
			return (int)a <= (int)b ? TRUE : FALSE;
#endif
	}
};

class TSpcInterp;

struct TSpcSymbol {
	/* 0x0 */ u32 mType;
	/* 0x4 */ u32 mNameOffset;
	/* 0x8 */ u32 mData;
	/* 0xC */ u32 mNameHash;
	/* 0x10 */ u32 mNativeCall;
};

struct TSpcHeader {
	/* 0x0 */ char mMagic[4]; // SPCB
	/* 0x4 */ u32 mTextOffset;
	/* 0x8 */ u32 mDataOffset;
	/* 0xC */ u32 mDataNum;
	/* 0x10 */ u32 mSymbolOffset;
	/* 0x14 */ u32 mSymbolNum;
	/* 0x18 */ s32 unk18;
};

// Native calls pass object addresses through script ints and register their
// function addresses in 32-bit symbol records. On the console these expand to
// the original int/u32 code; on PC they use the pointer-wide paths above.
//   SPC_GET_PTR(T, slice): pointer of type T carried by a popped slice
//   SPC_PUSH_PTR(interp, p): push address p as a script value
//   SPC_NATIVE_CALL(sym): the bound function address of a TSpcSymbol*
#ifdef TARGET_PC
#define SPC_GET_PTR(T, slice) ((T)(slice).getDataPtr())
#define SPC_PUSH_PTR(interp, p) ((interp)->push((void*)(p)))
#define SPC_NATIVE_CALL(sym) (TSpcBinary::getNativeCall((sym)->mNativeCall))
#else
#define SPC_GET_PTR(T, slice) ((T)(slice).getDataInt())
#define SPC_PUSH_PTR(interp, p) ((interp)->push((int)(p)))
#define SPC_NATIVE_CALL(sym) ((sym)->mNativeCall)
#endif

class TSpcBinary {
	/* 0x0 */ u8* mData;

public:
	TSpcBinary(void*);

	virtual ~TSpcBinary();
	virtual void initUserBuiltin();

	void init();
	u32 calcKey(const char*);
	void calcAndStoreKeys();
	void initSystemBuiltin();
	TSpcSymbol* searchSymbol(const char*);
#ifdef TARGET_PC
	// mNativeCall sits in the 32-bit on-disc symbol record, so on PC it holds
	// a 1-based index into a host table of function addresses instead of the
	// address itself (0 = unbound, like the console).
	void bindSystemDataToSymbol(const char*, uintptr_t);
	static uintptr_t getNativeCall(u32 handle);
#else
	void bindSystemDataToSymbol(const char*, u32);
#endif

	TSpcHeader* getHeader() const { return (TSpcHeader*)mData; }

	// fabricated
	TSpcSymbol* getSymbol(u32 idx)
	{
		return &((TSpcSymbol*)(mData + getHeader()->mSymbolOffset))[idx];
	}
	// The const on the parameter is load-bearing. It stops MWCC from merging
	// this read of mNameOffset with the caller's own read of the same field,
	// which is what dump() needs: the original loads the field twice.
	const char* getSymbolName(const TSpcSymbol* symbol)
	{
		return (const char*)(mData + getHeader()->mSymbolOffset
		                     + getHeader()->mSymbolNum * sizeof(TSpcSymbol)
		                     + symbol->mNameOffset);
	}
	u32 getDataOffset(u32 idx)
	{
#ifdef TARGET_PC
		// Swapped in place at construction. The table follows the unaligned
		// text section, so it is not necessarily 4-aligned.
		u32 offset;
		memcpy(&offset, mData + getHeader()->mDataOffset + idx * sizeof(u32),
		       sizeof(offset));
		return offset;
#else
		return *(u32*)(mData + getHeader()->mDataOffset + idx * sizeof(u32));
#endif
	}
	void* getData(u32 idx)
	{
		return mData + getHeader()->mDataOffset + getHeader()->mDataNum * 4
		       + getDataOffset(idx);
	}
	// Returns void*, like getData above. The cast every caller then writes is
	// load-bearing: the conversion node lets MWCC propagate the pointer into a
	// compiler temporary, so the fetches below keep `result` as their only
	// named local. See the comment on fetchF32.
	void* getText(u32 offset)
	{
		return mData + getHeader()->mTextOffset + offset;
	}
};

class TSpcInterp {
public:
	/* 0x0 */ TSpcBinary* mBinary;
	/* 0x4 */ s32 mStepsToDo;
	/* 0x8 */ u32 mProgramCounter;
	/* 0xC */ s32 mStepsLeft;
	/* 0x10 */ void* unk10;
	/* 0x14 */ s32 mLocked;
	/* 0x18 */ TSpcStack<TSpcSlice> mProcessStack;
	/* 0x28 */ TSpcStack<TSpcSlice> mStorageStack;
	/* 0x38 */ TSpcStack<u32> mContextStack;
	/* 0x48 */ u32 mDisplay[4];
	/* 0x58 */ const char* mCurrentlyExecutingBuiltinName;

public:
#ifdef TARGET_PC
	// The text section keeps its disc (big-endian) byte order: opcodes are
	// single bytes with unaligned 4-byte immediates interleaved, so swapping it
	// in place would need a full decode pass whose desync would corrupt the
	// stream silently, and it is read only through these fetchers. Reading each
	// immediate with rdbe32 is exact and trivially idempotent. See
	// TSpcBinary::TSpcBinary for the header, symbol and data-offset tables.
	u32 fetchU32()
	{
		u32 result = rdbe32(mBinary->getText(mProgramCounter));
		mProgramCounter += sizeof(u32);
		return result;
	}
	s32 fetchS32() { return (s32)fetchU32(); }
	f32 fetchF32()
	{
		u32 bits = fetchU32();
		f32 result;
		memcpy(&result, &bits, sizeof(result));
		return result;
	}
#else
	// The text section is not aligned, so the four bytes are copied one at a
	// time. The bound is unsigned (sizeof), which is what earns the `lbzu`
	// peephole. The loop also keeps these three over the inliner's budget from
	// pass 2 down, which is why execint, execadr and execstr call them instead
	// of expanding them.
	//
	// `result` must be the only named local here, which is why `src` is written
	// as a cast of a void* rather than kept in a u8* variable of its own. MWCC
	// numbers a function's locals in declaration order when it compiles the
	// body, but in *reverse* declaration order when it inlines it, so any two
	// named locals swap places between the out-of-line copy and every inlined
	// one. With only `result` needing a stack home there is nothing left to
	// swap, and both agree.
	f32 fetchF32()
	{
		u8* src = (u8*)mBinary->getText(mProgramCounter);
		f32 result;
		u8* dst = (u8*)&result;
		for (int i = 0; i < sizeof(f32); ++i)
			dst[i] = src[i];
		mProgramCounter += sizeof(f32);
		return result;
	}
	s32 fetchS32()
	{
		u8* src = (u8*)mBinary->getText(mProgramCounter);
		s32 result;
		u8* dst = (u8*)&result;
		for (int i = 0; i < sizeof(s32); ++i)
			dst[i] = src[i];
		mProgramCounter += sizeof(s32);
		return result;
	}

	u32 fetchU32()
	{
		u8* src = (u8*)mBinary->getText(mProgramCounter);
		u32 result;
		u8* dst = (u8*)&result;
		for (int i = 0; i < sizeof(u32); ++i)
			dst[i] = src[i];
		mProgramCounter += sizeof(u32);
		return result;
	}
#endif

	u8 fetchU8()
	{
		u8 result = *(u8*)mBinary->getText(mProgramCounter);
		++mProgramCounter;
		return result;
	}

	const char* fetchString()
	{
		return (const char*)mBinary->getData(fetchU32());
	}

	// The scalar overloads matter for codegen, they are not sugar: passing the
	// value through a parameter binds it to a temporary, which is what puts the
	// result of an opcode in the frame's temporary area instead of giving it a
	// named local slot.
	void push(const TSpcSlice& slice) { mProcessStack.push(slice); }
	void push(int v) { mProcessStack.push(TSpcSlice(v)); }
	void push(f32 v) { mProcessStack.push(TSpcSlice(v)); }
#ifdef TARGET_PC
	// Native calls hand object addresses to the script as an int slice; the
	// slot is pointer-wide here, read back with TSpcSlice::getDataPtr.
	void push(void* p)
	{
		TSpcSlice slice;
		slice.mData.asInt = (intptr_t)p;
		slice.mHostPointer = true;
		mProcessStack.push(slice);
	}
#endif
	// Unlike the other scalar overloads this one builds the slice with the
	// default constructor and setDataString, not with TSpcSlice(const char*).
	// The callers show it: the null test inside setDataString survives even
	// when the argument is a string literal, which only happens when the
	// pointer arrives through this parameter.
	void push(const char* v)
	{
		TSpcSlice slice;
		slice.setDataString(v);
		mProcessStack.push(slice);
	}
	void push() { push(TSpcSlice()); }
	TSpcSlice pop() { return mProcessStack.pop(); }

public:
	/// Pushes an immediate int constant C to the process stack
	/// Immediate arguments: C
	/// Returns: C
	void execint();
	/// Pushes an immediate float constant C to the process stack
	/// Immediate arguments: C
	/// Returns: C
	void execflt();
	/// Pushes a string constant from the binary's data section with index C to
	/// the process stack
	/// Immediate arguments: C
	/// Returns: C
	void execstr();
	/// Pushes an immediate address constant C to the process stack
	/// Immediate arguments: C
	/// Returns: C
	void execadr();
	/// Reads the variable V from layer L and pushes it to the process stack
	/// Immediate arguments: L, V
	/// Returns: the current value of the variable
	void execvar();
	/// No-op
	void execnop();
	/// Increments the variable V from layer L and pushes it's new value to the
	/// process stack
	/// Immediate arguments: L, V
	/// Returns: new value of the variable
	void execinc();
	/// Decrements the variable V from layer L and pushes it's new value to the
	/// process stack
	/// Immediate arguments: L, V
	/// Returns: new value of the variable
	void execdec();
	/// Pops two values A, B from the process stack, adds them and pushes the
	/// result back to it
	/// Arguments: A, B
	/// Returns: A + B
	void execadd();
	/// Pops two values A, B from the process stack, subtracts them and pushes
	/// the result back to it
	/// Arguments: A, B
	/// Returns: A - B
	void execsub();
	/// Pops two values A, B from the process stack, multiplies them and pushes
	/// the result back to it
	/// Arguments: A, B
	/// Returns: A * B
	void execmul();
	/// Pops two values A, B from the process stack, divides them and pushes
	/// the result back to it
	/// Arguments: A, B
	/// Returns: A / B
	void execdiv();
	/// Pops two values A, B from the process stack, modulos them and pushes
	/// the result back to it
	/// Arguments: A, B
	/// Returns: A % B
	void execmod();
	/// Pops value A from the process stack and assigns it to variable V in
	/// layer L
	/// Immediate arguments: L, V
	/// Arguments: A
	void execass();
	/// Pops two values A, B from the process stack, compares them for equality
	/// and pushes the result back to it
	/// Arguments: A, B
	/// Returns: A == B
	void execeq();
	/// Pops two values A, B from the process stack, compares them for
	/// inequality and pushes the result back to it
	/// Arguments: A, B
	/// Returns: A != B
	void execne();
	/// Pops two values A, B from the process stack, compares them for
	/// being greater then and pushes the result back to it
	/// Arguments: A, B
	/// Returns: A > B
	void execgt();
	/// Pops two values A, B from the process stack, compares them for
	/// being less then and pushes the result back to it
	/// Arguments: A, B
	/// Returns: A < B
	void execlt();
	/// Pops two values A, B from the process stack, compares them for
	/// being greater then or equal and pushes the result back to it
	/// Arguments: A, B
	/// Returns: A >= B
	void execge();
	/// Pops two values A, B from the process stack, compares them for
	/// being less then or equal and pushes the result back to it
	/// Arguments: A, B
	/// Returns: A <= B
	void execle();
	/// Pops value A from the stack, negates it and pushes -A back
	/// Arguments: A
	/// Returns: -A
	void execneg();
	/// Pops value A from the stack, logically negates it and pushes the result
	/// back to the stack
	/// Arguments: A
	/// Returns: 1 if A != 0, 0 otherwise
	void execnot();
	/// Pops values A, B from the stack, checks whether both are non-zero and
	/// pushes the result back
	/// Arguments: A, B
	/// Returns: 1 if A != 0 && B != 0, 0 otherwise
	void execand();
	/// Pops values A, B from the stack, checks whether at least one is non-zero
	/// and pushes the result
	/// Arguments: A, B
	/// Returns: 1 if A != 0 || B != 0, 0 otherwise
	void execor();
	/// Pops two values A, B from the process stack, bitwise ANDs them and
	/// pushes the result back to it
	/// Arguments: A, B
	/// Returns: A & B
	void execband();
	/// Pops two values A, B from the process stack, bitwise ORs them and
	/// pushes the result back to it
	/// Arguments: A, B
	/// Returns: A | B
	void execbor();
	/// Pops two values A, B from the process stack, bitwise shifts one of them
	/// left by the other and pushes the result back
	/// Arguments: A, B
	/// Returns: A << B
	void execshl();
	/// Pops two values A, B from the process stack, bitwise shifts one of them
	/// right by the other and pushes the result back
	/// Arguments: A, B
	/// Returns: A >> B
	void execshr();
	/// Calls an SPC function present in the binary at offset A with N topmost
	/// values from the process stack as arguments
	/// Immediate arguments: A, N
	/// Arguments: depends
	/// Result: depends
	void execcall();
	/// Calls a native function with id I bound to SPC with the topmost N values
	/// from the process stack as arguments
	/// Immediate arguments: I, N
	/// Arguments: depends
	/// Result: depends
	void execfunc();
	/// Allocates N new variables in the current function's stack frame with
	/// integral value of 0
	/// Immediate arguments: N
	void execmkfr();
	/// Makes a "stack frame" for a function in layer L. Expected to be called
	/// at the start of any SPC function
	/// Immediate arguments: L
	void execmkds();
	/// Returns from an SPC function. If call and mkds were not called before it
	/// in that exact order, everything will blow up in flames
	void execret();
	/// Pushes 0 to the process stack and returns from an SPC function,
	// equivalent to int 0, ret.
	void execret0();
	/// Pops value B from the process stack and if it is zero, jumps to the
	/// immediate address A
	/// Immediate arguments: A
	/// Arguments: B
	void execjne();
	/// Jumps to the immediate address A
	/// Immediate arguments: A
	void execjmp();
	/// Pops a value from the process stack
	void execpop();
	/// Pushes an int 0 to the process stack
	void execint0();
	/// Pushes an int 1 to the process stack
	void execint1();
	/// Concludes execution of an SPC script
	void execend();

	typedef void (TSpcInterp::*ExecFunction)();
	ExecFunction chooseExecFunction(u8);
	void dispatchBuiltinDefault(u32 sym_index, u32 arg_count);

	virtual void dispatchBuiltin(u32 sym_index, u32 arg_count);

	TSpcInterp(TSpcBinary* binary, void* owner, int steps,
	           int process_stack_size, int storage_stack_size,
	           int context_stack_size);
	virtual ~TSpcInterp();

	void dump();
	void verifyArgNum(u32, u32*);
	void callByAddress(u32, u32);
	void callByName(const char*, u32);
	void invokeByAddress(u32, u32);
	void invokeByName(const char*, u32);
	void referByIndex(u32);
	void referByName(const char*);

	virtual void update();
};

template <class T> class TSpcTypedBinary : public TSpcBinary {
public:
	TSpcTypedBinary(void* data)
	    : TSpcBinary(data)
	{
	}

	virtual ~TSpcTypedBinary() { }
	virtual void initUserBuiltin();
};

template <class T> class TSpcTypedInterp : public TSpcInterp {
public:
	TSpcTypedInterp(TSpcBinary* binary, void* owner, int steps,
	                int process_stack_size, int storage_stack_size,
	                int context_stack_size)
	    : TSpcInterp(binary, owner, steps, process_stack_size,
	                 storage_stack_size, context_stack_size)
	{
	}

	virtual void dispatchBuiltin(u32 sym_index, u32 arg_count)
	{
		typedef void (*TypedNativeCall)(TSpcTypedInterp<T>*, u32);
		TSpcSymbol* sym = mBinary->getSymbol(sym_index);

		if (sym && sym->mNativeCall) {
			TypedNativeCall call = (TypedNativeCall)SPC_NATIVE_CALL(sym);

			mCurrentlyExecutingBuiltinName = mBinary->getSymbolName(sym);
			call(this, arg_count);
		} else {
			TSpcInterp::dispatchBuiltin(sym_index, arg_count);
		}
	}

	// fabricated
	T* getOwner() const { return (T*)unk10; }
};

#endif
