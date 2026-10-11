#include <Strategic/spcinterp.hpp>
#include <macros.h>

static void spcYield(TSpcInterp* interp, u32 arg_count)
{
	interp->mStepsLeft = 0;
	for (int i = 0; i < (int)arg_count; ++i)
		interp->pop();
	interp->push();
}

static void spcExit(TSpcInterp* interp, u32 arg_count)
{
	interp->mStepsToDo = 0;
	interp->mStepsLeft = 0;
	for (int i = 0; i < (int)arg_count; ++i)
		interp->pop();
	interp->push();
}

static void spcLock(TSpcInterp* interp, u32 arg_count)
{
	interp->mLocked = TRUE;
	for (int i = 0; i < (int)arg_count; ++i)
		interp->pop();
	interp->push();
}

static void spcUnlock(TSpcInterp* interp, u32 arg_count)
{
	interp->mLocked = FALSE;
	for (int i = 0; i < (int)arg_count; ++i)
		interp->pop();
	interp->push();
}

static void spcPrint(TSpcInterp* interp, u32 arg_count)
{
	for (int i = 0; i < (int)arg_count; ++i) {
		const TSpcSlice& slice
		    = interp->mProcessStack
		          .mData[interp->mProcessStack.mSize - (arg_count - i)];
		switch (slice.mType) {
		case TSpcSlice::TYPE_INT:
			SpcTrace("%d", slice.getDataInt());
			break;
		case TSpcSlice::TYPE_FLOAT:
			SpcTrace("%f", slice.getDataFloat());
			break;
		case TSpcSlice::TYPE_STRING:
			SpcTrace("%s", slice.getDataString());
			break;
		}
	}

	for (int i = 0; i < (int)arg_count; ++i)
		interp->pop();
	interp->push();
}

static void spcDump(TSpcInterp* interp, u32 arg_count)
{
	interp->dump();

	for (int i = 0; i < (int)arg_count; ++i)
		interp->pop();
	interp->push();
}

// The three builtins below each keep the pushed value in a named local, but
// they build it three different ways, and the frames show that the differences
// are real. spcInt constructs the slice, so no default construction is emitted;
// spcFloat default-constructs and then assigns, so the zeroing stays; spcTypeof
// reads the type into a local first, which lets MWCC substitute the argument
// into push() instead of binding it to a temporary.
static void spcInt(TSpcInterp* interp, u32 arg_count)
{
	if (arg_count != 1)
		interp->verifyArgNum(1, &arg_count);

	TSpcSlice result(interp->pop().getDataInt());
	interp->mProcessStack.push(result);
}

static void spcFloat(TSpcInterp* interp, u32 arg_count)
{
	if (arg_count != 1)
		interp->verifyArgNum(1, &arg_count);

	TSpcSlice result;
	result.setDataFloat(interp->pop().getDataFloat());
	interp->mProcessStack.push(result);
}

static void spcTypeof(TSpcInterp* interp, u32 arg_count)
{
	if (arg_count != 1)
		interp->verifyArgNum(1, &arg_count);

	u32 type = interp->pop().typeof();
	interp->push((int)type);
}

#ifdef TARGET_PC
// SPCB binaries are big-endian on disc: a 0x1C header (magic, then six u32 fields), the text section (kept in disc
// order, see TSpcInterp::fetchU32), the data-offset table (u32 each), the string pool, the symbol table (TSpcSymbol,
// five u32 each) and the symbol names. Header, symbol table and data-offset table hold only u32 and match the disc
// layout on the host (sizeof(TSpcSymbol) == 0x14, no pointer members; mNativeCall is a handle, not an address), so
// they are swapped in place here, the first point where the bytes are available.
// Idempotency: the magic is swapped as a u32 together with the header, so "SPCB" on disc becomes "BCPS" in memory.
// The same buffer can be wrapped again (every TEventWatcher / TLiveManager of a scene re-uses its resource); a
// "BCPS" buffer is already host order and is left alone, and so is anything that is not an SPCB at all.
static_assert(sizeof(TSpcSymbol) == 0x14, "TSpcSymbol must match the disc record");
static_assert(sizeof(TSpcHeader) == 0x1C, "TSpcHeader must match the disc header");

static void spcSwapBinary(u8* data)
{
	if (data == nullptr || memcmp(data, "SPCB", 4) != 0)
		return;

	TSpcHeader* header = (TSpcHeader*)data;
	for (u32 i = 0; i < sizeof(TSpcHeader) / 4; ++i)
		swapInPlace32(data + i * 4);

	u8* dataOffsets = data + header->mDataOffset;
	for (u32 i = 0; i < header->mDataNum; ++i)
		swapInPlace32(dataOffsets + i * 4);

	u8* symbols = data + header->mSymbolOffset;
	for (u32 i = 0; i < header->mSymbolNum * (sizeof(TSpcSymbol) / 4); ++i)
		swapInPlace32(symbols + i * 4);
}

TSpcBinary::TSpcBinary(void* data)
    : mData((u8*)data)
{
	spcSwapBinary(mData);
}
#else
TSpcBinary::TSpcBinary(void* data)
    : mData((u8*)data)
{
}
#endif

TSpcBinary::~TSpcBinary() { }

void TSpcBinary::init()
{
	calcAndStoreKeys();
	initSystemBuiltin();
	initUserBuiltin();
	SpcTrace("TSpcBinary : system initialized\n");
}

u32 TSpcBinary::calcKey(const char* name)
{
	u32 result = 0;
	for (; *name != '\0'; ++name)
		result = *name + result * 3;
	return result;
}

void TSpcBinary::calcAndStoreKeys()
{
	for (int i = 0; i < getHeader()->mSymbolNum; ++i) {
		TSpcSymbol* symbol = getSymbol(i);
		const char* name   = getSymbolName(symbol);
		symbol->mNameHash  = calcKey(name);
	}
}

void TSpcBinary::initSystemBuiltin()
{
	bindSystemDataToSymbol("yield", PTR_U32(&spcYield));
	bindSystemDataToSymbol("exit", PTR_U32(&spcExit));
	bindSystemDataToSymbol("lock", PTR_U32(&spcLock));
	bindSystemDataToSymbol("unlock", PTR_U32(&spcUnlock));
	bindSystemDataToSymbol("print", PTR_U32(&spcPrint));
	bindSystemDataToSymbol("dump", PTR_U32(&spcDump));
	bindSystemDataToSymbol("int", PTR_U32(&spcInt));
	bindSystemDataToSymbol("float", PTR_U32(&spcFloat));
	bindSystemDataToSymbol("typeof", PTR_U32(&spcTypeof));
	SpcTrace("TSpcBinary : system symbol installed\n");
}

void TSpcBinary::initUserBuiltin() { }

TSpcSymbol* TSpcBinary::searchSymbol(const char* name)
{
	u32 wantHash = calcKey(name);

	for (int i = 0; i < getHeader()->mSymbolNum; ++i) {
		TSpcSymbol* candidate = getSymbol(i);
		if (wantHash == candidate->mNameHash) {
			if (strcmp(name, getSymbolName(candidate)) == 0) {
				return candidate;
			}
		}
	}

	return nullptr;
}

#ifdef TARGET_PC
// ponytail: fixed-size table with linear dedup; the game binds well under a
// hundred natives. An overflow leaves the symbol unbound (script call no-op).
static uintptr_t sNativeCalls[512];
static u32 sNativeCallNum;

uintptr_t TSpcBinary::getNativeCall(u32 handle)
{
	if (handle == 0 || handle > sNativeCallNum)
		return 0;
	return sNativeCalls[handle - 1];
}

void TSpcBinary::bindSystemDataToSymbol(const char* name, uintptr_t param_2)
#else
void TSpcBinary::bindSystemDataToSymbol(const char* name, u32 param_2)
#endif
{
	TSpcSymbol* symbol = searchSymbol(name);
	if (symbol == nullptr) {
		SpcTrace("TSpcBinary : unknown symbol %s\n", name);
	} else {
#ifdef TARGET_PC
		u32 handle = 0;
		for (u32 i = 0; i < sNativeCallNum; ++i)
			if (sNativeCalls[i] == param_2)
				handle = i + 1;
		if (handle == 0 && param_2 != 0
		    && sNativeCallNum < ARRAY_COUNT(sNativeCalls)) {
			sNativeCalls[sNativeCallNum++] = param_2;
			handle                         = sNativeCallNum;
		}
		symbol->mNativeCall = handle;
#else
		symbol->mNativeCall = param_2;
#endif
	}
}

void TSpcInterp::execint() { mProcessStack.push((int)fetchS32()); }

void TSpcInterp::execflt()
{
	TSpcSlice slice;
	f32 data = fetchF32();
	slice.setDataFloat(data);
	mProcessStack.push(slice);
}

void TSpcInterp::execstr()
{
	TSpcSlice slice;
	slice.setDataString(fetchString());
	mProcessStack.push(slice);
}

void TSpcInterp::execadr() { mProcessStack.push((int)fetchU32()); }

void TSpcInterp::execvar()
{
	u32 arg1 = fetchU32();
	u32 arg2 = fetchU32();
	int idx  = mDisplay[arg1];
	mProcessStack.push(mStorageStack.getFromBottom(idx + arg2));
}

void TSpcInterp::execnop() { }

void TSpcInterp::execinc()
{
	u32 op   = fetchU8();
	int arg1 = fetchU32();
	int arg2 = fetchU32();

	u32 layer = mDisplay[arg1];
	++mStorageStack.getFromBottom(layer + arg2);

	mProcessStack.push(mStorageStack.getFromBottom(layer + arg2));
}

void TSpcInterp::execdec()
{
	u32 op   = fetchU8();
	int arg1 = fetchU32();
	int arg2 = fetchU32();

	u32 layer = mDisplay[arg1];
	--mStorageStack.getFromBottom(layer + arg2);

	mProcessStack.push(mStorageStack.getFromBottom(layer + arg2));
}

void TSpcInterp::execadd()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();

	if (arg1.typeof() == TSpcSlice::TYPE_FLOAT
	    || arg2.typeof() == TSpcSlice::TYPE_FLOAT) {
		TSpcSlice result;
		result.setDataFloat(arg1.getDataFloat() + arg2.getDataFloat());
		mProcessStack.push(result);
	} else {
		push(arg1.getDataInt() + arg2.getDataInt());
	}
}

void TSpcInterp::execsub()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();

	if (arg1.typeof() == TSpcSlice::TYPE_FLOAT
	    || arg2.typeof() == TSpcSlice::TYPE_FLOAT) {
		TSpcSlice result;
		result.setDataFloat(arg1.getDataFloat() - arg2.getDataFloat());
		mProcessStack.push(result);
	} else {
		push(arg1.getDataInt() - arg2.getDataInt());
	}
}

void TSpcInterp::execmul()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();

	if (arg1.typeof() == TSpcSlice::TYPE_FLOAT
	    || arg2.typeof() == TSpcSlice::TYPE_FLOAT) {
		TSpcSlice result;
		result.setDataFloat(arg1.getDataFloat() * arg2.getDataFloat());
		mProcessStack.push(result);
	} else {
		push(arg1.getDataInt() * arg2.getDataInt());
	}
}

void TSpcInterp::execdiv()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();

	if (arg1.typeof() == TSpcSlice::TYPE_FLOAT
	    || arg2.typeof() == TSpcSlice::TYPE_FLOAT) {
		TSpcSlice result;
		result.setDataFloat(arg1.getDataFloat() / arg2.getDataFloat());
		mProcessStack.push(result);
	} else {
		push(arg1.getDataInt() / arg2.getDataInt());
	}
}

void TSpcInterp::execmod()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();

	push(arg1.getDataInt() % arg2.getDataInt());
}

void TSpcInterp::execass()
{
	TSpcSlice slice = mProcessStack.pop();
	u32 op          = fetchU8();
	int arg1        = fetchU32();
	int arg2        = fetchU32();

	u32 layer                                 = mDisplay[arg1];
	mStorageStack.getFromBottom(layer + arg2) = slice;
}

void TSpcInterp::execeq()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	mProcessStack.push(arg1 == arg2);
}

void TSpcInterp::execne()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	mProcessStack.push(arg1 != arg2);
}

void TSpcInterp::execgt()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1 > arg2);
}

void TSpcInterp::execlt()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1 < arg2);
}

void TSpcInterp::execge()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1 >= arg2);
}

void TSpcInterp::execle()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1 <= arg2);
}

void TSpcInterp::execneg()
{
	TSpcSlice slice = mProcessStack.pop();
	mProcessStack.push(slice.negate());
}

void TSpcInterp::execnot()
{
	TSpcSlice slice = mProcessStack.pop();
	push(slice.getDataInt() == 0 ? 1 : 0);
}

void TSpcInterp::execand()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1.getDataInt() != 0 && arg2.getDataInt() != 0 ? 1 : 0);
}

void TSpcInterp::execor()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1.getDataInt() != 0 || arg2.getDataInt() != 0 ? 1 : 0);
}

void TSpcInterp::execband()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1.getDataInt() & arg2.getDataInt());
}

void TSpcInterp::execbor()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1.getDataInt() | arg2.getDataInt());
}

void TSpcInterp::execshl()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1.getDataInt() << arg2.getDataInt());
}

void TSpcInterp::execshr()
{
	TSpcSlice arg2 = mProcessStack.pop();
	TSpcSlice arg1 = mProcessStack.pop();
	push(arg1.getDataInt() >> arg2.getDataInt());
}

void TSpcInterp::execcall()
{
	char trash[0x20];
	u32 address = fetchU32();
	s32 argNum  = fetchS32();
	u32 counter = mProgramCounter;

	mContextStack.push(counter);
	mContextStack.push(mStorageStack.size());

	for (int i = 0; i < argNum; ++i)
		mStorageStack.push(TSpcSlice());
	for (int i = 0; i < argNum; ++i)
		mStorageStack.setFromTop(i, mProcessStack.pop());
	mProgramCounter = address;
}

void TSpcInterp::execfunc()
{
	u32 builtinIdx = fetchU32();
	u32 argNum     = fetchU32();
	dispatchBuiltin(builtinIdx, argNum);
}

void TSpcInterp::execmkfr()
{
	s32 arg1 = fetchS32();
	if (mContextStack.size() > 0)
		for (int i = 0; i < arg1; ++i)
			mStorageStack.push(TSpcSlice());
}

void TSpcInterp::execmkds()
{
	s32 displayIdx       = fetchS32();
	u32 display          = mDisplay[displayIdx];
	mDisplay[displayIdx] = mContextStack.getFromTop(0);
	mContextStack.push(display);
	mContextStack.push(displayIdx);
}

void TSpcInterp::execret()
{
	u32 restoredDisplayIdx       = mContextStack.pop();
	u32 restoredDisplay          = mContextStack.pop();
	u32 restoredStorageStackSize = mContextStack.pop();

	for (int i = mStorageStack.size(); i > restoredStorageStackSize; --i)
		mStorageStack.pop();
	mDisplay[restoredDisplayIdx] = restoredDisplay;

	mProgramCounter = mContextStack.pop();
}

void TSpcInterp::execret0()
{
	mProcessStack.push(0);

	execret();
}

void TSpcInterp::execjne()
{
	u32 arg1 = fetchU32();
	if (!pop().getDataInt())
		mProgramCounter = arg1;
}

void TSpcInterp::execjmp() { mProgramCounter = fetchU32(); }

void TSpcInterp::execpop() { mProcessStack.pop(); }

void TSpcInterp::execint0() { mProcessStack.push(0); }

void TSpcInterp::execint1() { mProcessStack.push(1); }

void TSpcInterp::execend()
{
	mStepsToDo = 0;
	mStepsLeft = 0;
	SpcTrace("TSpcInterp : script finished\n");
}

TSpcInterp::ExecFunction TSpcInterp::chooseExecFunction(u8 cmd)
{
	static ExecFunction table[] = {
		&TSpcInterp::execint,  &TSpcInterp::execflt,  &TSpcInterp::execstr,
		&TSpcInterp::execadr,  &TSpcInterp::execvar,  &TSpcInterp::execnop,
		&TSpcInterp::execinc,  &TSpcInterp::execdec,  &TSpcInterp::execadd,
		&TSpcInterp::execsub,  &TSpcInterp::execmul,  &TSpcInterp::execdiv,
		&TSpcInterp::execmod,  &TSpcInterp::execass,  &TSpcInterp::execeq,
		&TSpcInterp::execne,   &TSpcInterp::execgt,   &TSpcInterp::execlt,
		&TSpcInterp::execge,   &TSpcInterp::execle,   &TSpcInterp::execneg,
		&TSpcInterp::execnot,  &TSpcInterp::execand,  &TSpcInterp::execor,
		&TSpcInterp::execband, &TSpcInterp::execbor,  &TSpcInterp::execshl,
		&TSpcInterp::execshr,  &TSpcInterp::execcall, &TSpcInterp::execfunc,
		&TSpcInterp::execmkfr, &TSpcInterp::execmkds, &TSpcInterp::execret,
		&TSpcInterp::execret0, &TSpcInterp::execjne,  &TSpcInterp::execjmp,
		&TSpcInterp::execpop,  &TSpcInterp::execint0, &TSpcInterp::execint1,
		&TSpcInterp::execend,
	};

	if (cmd >= ARRAY_COUNT(table)) {
		SpcTrace("TSpcInterp : unknown command 0x%x\n", cmd);
		return nullptr;
	}

	return table[cmd];
}

void TSpcInterp::dispatchBuiltinDefault(u32 sym_index, u32 arg_count)
{
	TSpcSymbol* sym = mBinary->getSymbol(sym_index);
	if (sym == nullptr) {
		SpcTrace("TSpcInterp : null symbol\n");
		for (int i = 0; i < (int)arg_count; ++i)
			pop();
		push();
		return;
	}

	typedef void (*Call)(TSpcInterp*, u32);
	Call call = (Call)SPC_NATIVE_CALL(sym);
	if (call == nullptr) {
		SpcTrace("TSpcInterp : unknown builtin function %s\n",
		         mBinary->getSymbolName(sym));
		for (int i = 0; i < (int)arg_count; ++i)
			pop();
		push();
		return;
	}

	mCurrentlyExecutingBuiltinName = mBinary->getSymbolName(sym);
	call(this, arg_count);
}

void TSpcInterp::dispatchBuiltin(u32 sym_index, u32 arg_count)
{
	dispatchBuiltinDefault(sym_index, arg_count);
}

TSpcInterp::TSpcInterp(TSpcBinary* binary, void* owner, int steps,
                       int process_stack_size, int storage_stack_size,
                       int context_stack_size)
    : mBinary(binary)
    , mStepsToDo(steps)
    , mProgramCounter(0)
    , mStepsLeft(steps)
    , unk10(owner)
    , mProcessStack(process_stack_size)
    , mStorageStack(storage_stack_size)
    , mContextStack(context_stack_size)
{
	if (mBinary == nullptr || mBinary->getHeader() == nullptr) {
		SpcTrace("TSpcInterp : null binary\n");
		return;
	}

#ifdef TARGET_PC
	// TSpcBinary swapped the header in place, which reversed the magic bytes.
	if (memcmp(mBinary->getHeader()->mMagic, "BCPS", 4) != 0) {
		SpcTrace("TSpcInterp : invalid magic\n");
	}
#else
	if (mBinary->getHeader()->mMagic[0] != 'S'
	    || mBinary->getHeader()->mMagic[1] != 'P'
	    || mBinary->getHeader()->mMagic[2] != 'C'
	    || mBinary->getHeader()->mMagic[3] != 'B') {
		SpcTrace("TSpcInterp : invalid magic\n");
	}
#endif

	for (int i = 0; i < ARRAY_COUNT(mDisplay); ++i)
		mDisplay[i] = 0;

	for (int i = 0; i < mBinary->getHeader()->unk18; ++i) {
		mStorageStack.push(TSpcSlice(0));
	}
}

TSpcInterp::~TSpcInterp() { }

void TSpcInterp::dump()
{
	SpcTrace("TSpcInterp : internal status\n");
	SpcTrace("textofs = %u\n", mBinary->getHeader()->mTextOffset);
	SpcTrace("dataofs = %u\n", mBinary->getHeader()->mDataOffset);
	SpcTrace("datanum = %u\n", mBinary->getHeader()->mDataNum);
	SpcTrace("symofs  = %u\n", mBinary->getHeader()->mSymbolOffset);
	SpcTrace("symnum  = %u\n", mBinary->getHeader()->mSymbolNum);

	for (int i = 0; i < 4; ++i)
		SpcTrace("mDisplay[%d] = %d\n", i, mDisplay[i]);

	for (int i = 0; i < mProcessStack.size(); ++i)
		SpcTrace("mProcessStack[%d] = %d\n", i,
		         mProcessStack.getFromTop(i).getDataInt());

	for (int i = 0; i < mContextStack.size(); ++i)
		SpcTrace("mContextStack[%d] = %d\n", i, mContextStack.getFromTop(i));

	for (int i = 0; i < mStorageStack.size(); ++i)
		SpcTrace("mStorageStack[%d] = %d\n", i,
		         mStorageStack.getFromTop(i).getDataInt());

	for (int i = 0; i < mBinary->getHeader()->mSymbolNum; ++i) {
		TSpcSymbol* symbol = mBinary->getSymbol(i);
		SpcTrace("symbol [%03u] %s : type = %u, ofs = %u, data = %u\n", i,
		         mBinary->getSymbolName(symbol), symbol->mType,
		         symbol->mNameOffset, symbol->mData);
	}
}

void TSpcInterp::verifyArgNum(u32 param_1, u32* param_2)
{
	if (param_1 == *param_2)
		return;

	SpcTrace("TSpcInterp : argument number mismatch (%s, %d -> %d)\n",
	         mCurrentlyExecutingBuiltinName, *param_2, param_1);

	while (param_1 < *param_2) {
		pop();
		--*param_2;
	}

	while (param_1 > *param_2) {
		push();
		++*param_2;
	}
}

void TSpcInterp::callByAddress(u32, u32) { }

void TSpcInterp::callByName(const char*, u32)
{
	SpcTrace("TSpcInterp : cannot call variable symbol %s\n");
}

void TSpcInterp::invokeByAddress(u32, u32) { }

static void dummy()
{
	SpcTrace("TSpcStack : stack overflow\n");
	SpcTrace("TSpcStack : stack underflow\n");
}

void TSpcInterp::invokeByName(const char*, u32)
{
	SpcTrace("TSpcInterp : unknown function, %s\n", "");
	SpcTrace("TSpcInterp : cannot invoke variable symbol %s\n", "");
	SpcTrace("TSpcInterp : cannot invoke builtin, %s\n", "");
}

void TSpcInterp::referByIndex(u32) { }

void TSpcInterp::referByName(const char*)
{
	SpcTrace("TSpcInterp : unknown variable, %s\n", "");
	SpcTrace("TSpcInterp : cannot refer function, %s\n", "");
	SpcTrace("TSpcInterp : cannot refer variable\n");
}

// NOTE: the stuff above is supposed to be referencing these float constants
static void dummy_7(f32& x) { x = 1.0f; }
static void dummy_6(f32& x, int y) { x = y; }
static void dummy_5(f32& x) { x = 0.0f; }

void TSpcInterp::update()
{
	if (mBinary == nullptr || mBinary->getHeader() == nullptr) {
		SpcTrace("TSpcInterp : null binary\n");
		return;
	}

	while (mStepsLeft > 0) {
		if (!mLocked)
			--mStepsLeft;

		u8 cmd = fetchU8();

		ExecFunction function;
		function = chooseExecFunction(cmd);

		if (function)
			(*this.*function)();
	}
	mStepsLeft = mStepsToDo;
}
