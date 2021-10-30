#ifndef UNITY_ENDIAN_HELPER_H_
#define UNITY_ENDIAN_HELPER_H_


enum EndianMode {
	kEndianDirect	= 0,
	kEndianConvert	= 1,
#if UNITY_BIG_ENDIAN
	kHostEndian		= kEndianDirect,
	kTargetEndian	= kEndianDirect,
#else
	kHostEndian		= kEndianConvert,
	kTargetEndian	= kEndianConvert,
#endif
};

	template<EndianMode Endian>
	struct EndianHelper;

	template<>
	struct EndianHelper<kEndianDirect>
	{
		static UInt16 Load( UInt16 const* p ) {
			return *p;
		}

		static UInt32 Load( UInt32 const* p ) {
			return *p;
		}

		template<int Offset>
		static void Store( UInt16* p, UInt16 arg ) {
			*reinterpret_cast<UInt16*>( ((intptr_t)p + Offset) ) = arg;
		}

		template<int Offset>
		static void Store( UInt32* p, UInt32 arg ) {
			*reinterpret_cast<UInt32*>( ((intptr_t)p + Offset) ) = arg;
		}

	};

	template<>
	struct EndianHelper<kEndianConvert>
	{
		static UInt16 Load( register UInt16 const* p ) {
//#if defined(__native_client__) || (defined(__GNUC__) && !defined(__APPLE__))
			return ((*p&0xff00U)>>8) | ((*p&0x00ffU)<<8);
//#elif defined(__ppc__)
//			register UInt16 temp;
//			__asm {
//				lhbrx temp, 0, p
//			}
//			return temp;
//#elif defined(__i386) || defined(_M_IX86)
//			register UInt16 temp = *p;
//			__asm {
//				mov ax, temp
//				xchg ah, al
//				mov temp, ax
//			}
//			return temp;
//#else
//#error "Unsupported architecture"
//#endif
		}

		static UInt32 Load( register UInt32 const* p ) {
//#if defined(__native_client__) || (defined(__GNUC__) && !defined(__APPLE__))
			return
				((*p&0xff000000U)>>24) |
				((*p&0x00ff0000U)>>8) |
				((*p&0x0000ff00U)<<8) |
				((*p&0x000000ffU)<<24)
			;
//#elif defined(__ppc__)
//			register UInt32 temp;
//			__asm {
//				lwbrx r4, 0, p
//			}
//			return temp;
//#elif defined(__i386) || defined(_M_IX86)
//			register UInt32 temp = *p;
//			__asm {
//				mov eax, temp
//				bswap eax
//				mov temp, eax
//			}
//			return temp;
//#else
//#error "Unsupported architecture"
//#endif
		}

		// Offset is in bytes
		template<int Offset>
		static void Store( register UInt16* p, register UInt16 arg ) {
//#if defined(__native_client__) || (defined(__GNUC__) && !defined(__APPLE__))
			UInt16 temp = ((arg&0xff00U)>>8) | ((arg&0x00ffU)<<8);
			*(UInt16*)((UInt8*)p + Offset) = temp;
//#elif defined(__ppc__)
//			register short int ofs = Offset;
//			__asm {
//				sthbrx arg, ofs, p
//			}
//#elif defined(__i386) || defined(_M_IX86)
//			register void* pp = (void*)((uintptr_t)p + Offset);
//			__asm {
//				mov ax, arg
//				xchg ah, al
//				mov ecx, dword ptr [pp]
//				mov word ptr [ecx], ax
//			}
//#else
//#error "Unsupported architecture"
//#endif
		}

		template<int Offset>
		static void Store( register UInt32* p, register UInt32 arg ) {
//#if defined(__native_client__) || (defined(__GNUC__) && !defined(__APPLE__))
			UInt32 temp =
				((arg&0xff000000U)>>24) |
				((arg&0x00ff0000U)>>8) |
				((arg&0x0000ff00U)<<8) |
				((arg&0x000000ffU)<<24)
			;
			*(UInt32*)((UInt8*)p + Offset) = temp;
//#elif defined(__ppc__)
//			register short int ofs = Offset;
//			__asm {
//				stwbrx arg, ofs, p
//			}
//#elif defined(__i386) || defined(_M_IX86)
//			register void* pp = (void*)((uintptr_t)p + Offset);
//			__asm {
//				mov eax, arg
//				bswap eax
//				mov ecx, dword ptr [pp]
//				mov word ptr [ecx], eax
//			}
//#else
//#error "Unsupported architecture"
//#endif
		}
	};

#endif // UNITY_ENDIAN_HELPER_H_
