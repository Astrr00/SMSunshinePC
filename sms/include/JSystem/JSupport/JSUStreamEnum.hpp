#ifndef JSU_STREAM_ENUM_H
#define JSU_STREAM_ENUM_H

enum JSUStreamSeekFrom {
	JSUStreamSeekFrom_SET = 0,
	JSUStreamSeekFrom_CUR = 1,
	JSUStreamSeekFrom_END = 2
};

#ifdef TARGET_PC
// The CRT defines EOF as the macro (-1); the enumerator below must keep its value of 1.
#undef EOF
#endif
enum EIoState { GOOD = 0, EOF = 1 };

#endif
