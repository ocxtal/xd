
/**
 * @file xd.c
 * @brief fast binary dump
 *
 * @date 2018/8/8
 * @license MIT
 */

/* make sure POSIX APIs are properly activated */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE		200112L
#endif
#if defined(__darwin__) && !defined(_DARWIN_C_FULL)
#  define _DARWIN_C_SOURCE		_DARWIN_C_FULL
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__x86_64__)
#  include <x86intrin.h>
#  define vec_t						__m128i
#  define _broadcast(_i)			( _mm_set1_epi8(_i) )
#  define _cast_to_vec(_i)			( _mm_cvtsi64_si128(_i) )
#  define _load(_p)					( _mm_loadu_si128((__m128i const *)(_p)) )
#  define _store(_p, _v)			{ _mm_storeu_si128((__m128i *)(_p), (_v)); }
#  define _unpack_nibble_fw(_x)		({ __m128i y = _mm_cvtepu8_epi16((_x)); y = _mm_and_si128(mv, _mm_or_si128(y, _mm_slli_epi16(y, 4))); y; })
#  define _unpack_nibble_low(_x)	({ __m128i y = _mm_cvtepu8_epi16((_x)); y = _mm_srli_epi16(_mm_or_si128(y, _mm_slli_epi16(y, 12)), 4); y; })
#  define _unpack_nibble_high(_x)	({ __m128i y = _mm_unpackhi_epi8((_x), _mm_setzero_si128()); y = _mm_srli_epi16(_mm_or_si128(y, _mm_slli_epi16(y, 12)), 4); y; })
#  define _asciibase(_x)			( _mm_shuffle_epi8(cv, (_x)) )
#  define _isascii(_x)				( _mm_cmpgt_epi8(_mm_add_epi8((_x), tv), sv) )
#  define _select(_m, _y, _x)		( _mm_blendv_epi8((_x), (_y), (_m)) )		/* m ? y : x */
#  define _interleave_low(_x)		( _mm_cvtepu16_epi32((_x)) )
#  define _interleave_high(_x)		( _mm_cvtepu16_epi32(_mm_srli_si128((_x), 8)) )
#  define _flip(_x, _d)				( _mm_shuffle_epi8((_x), _mm_sub_epi8(fv, _mm_set1_epi8(16 - (_d)))) )
#  define _finish(_x)				( _mm_add_epi8(sv, (_x)) )

#elif defined(__ARM_NEON__)
#  include <arm_neon.h>
#  define vec_t						uint8x16_t
#  define _broadcast(_i)			( vdupq_n_u8(_i) )
#  define _cast_to_vec(_i)			( vsetq_lane_u64((_i), vmovq_n_u64(0), 0) )
#  define _load(_p)					( vld1q_u8((uint8_t const *)(_p)) )
#  define _store(_p, _v)			{ vst1q_u8((uint8_t *)(_p), (_v)); }
#  define _unpack_nibble_fw(_x)		({ uint8x16_t y = vreinterpretq_u16_u8(vzip1q_u8((_x), vshrq_n_u8((_x), 4))); y = vandq_u8(mv, y); y; })
#  define _unpack_nibble_low(_x)	({ uint8x16_t y = vreinterpretq_u16_u8(vzip1q_u8(vshrq_n_u8((_x), 4), (_x))); y = vandq_u8(mv, y); y; })
#  define _unpack_nibble_high(_x)	({ uint8x16_t y = vreinterpretq_u16_u8(vzip2q_u8(vshrq_n_u8((_x), 4), (_x))); y = vandq_u8(mv, y); y; })
#  define _asciibase(_x)			( vqtbl1q_u8(cv, (_x)) )
#  define _isascii(_x)				( vreinterpretq_s8_u8(vcgtq_s8(vreinterpretq_u8_s8(vaddq_u8((_x), tv)), vreinterpretq_u8_s8(sv))) )
#  define _select(_m, _y, _x)		( vbslq_u8((_m), (_y), (_x)) )		/* m ? y : x */
#  define _interleave_low(_x)		( vreinterpretq_u32_u8(vzip1q_u16(vreinterpretq_u8_u16(_x), vmovq_n_u16(0))) )
#  define _interleave_high(_x)		( vreinterpretq_u32_u8(vzip2q_u16(vreinterpretq_u8_u16(_x), vmovq_n_u16(0))) )
#  define _flip(_x, _d)				( vqtbl1q_u8((_x), vsubq_u8(fv, vmovq_n_u8(16 - (_d)))) )
#  define _finish(_x)				( vaddq_u8(sv, (_x)) )

#else
#  error "unknown architecture."

#endif

static inline
size_t conv(
	size_t ofs,
	uint8_t const *src,
	uint8_t *dst,
	size_t addr_digits)								/* smaller than 16 */
{
	#define z(_x)						( (_x) - ' ' )							/* offsetted */
	static uint8_t const conv_table[16] __attribute__(( aligned(16) )) = {
		z('0'), z('1'), z('2'), z('3'), z('4'), z('5'), z('6'), z('7'), z('8'), z('9'), z('a'), z('b'), z('c'), z('d'), z('e'), z('f')
	};
	#undef z
	static uint8_t const flip_table[16] __attribute__(( aligned(16) )) = {
		15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
	};
	vec_t const cv = _load(conv_table);
	vec_t const fv = _load(flip_table);
	vec_t const tv = _broadcast(1),   mv = _broadcast(0x0f);
	vec_t const sv = _broadcast(' '), dv = _broadcast('.');

	uint8_t *p = dst;

	/* print offset */
	vec_t const ofsv = _finish(_flip(
		_asciibase(_unpack_nibble_fw(_cast_to_vec(ofs))),
		addr_digits
	));
	_store(p, ofsv);
	p += addr_digits + 2;							/* spaced by two */

	/* print values */
	vec_t const ar = _load(src);
	vec_t const all = _finish(_interleave_low(_asciibase(_unpack_nibble_low(ar))));
	vec_t const alh = _finish(_interleave_high(_asciibase(_unpack_nibble_low(ar))));
	vec_t const ahl = _finish(_interleave_low(_asciibase(_unpack_nibble_high(ar))));
	vec_t const ahh = _finish(_interleave_high(_asciibase(_unpack_nibble_high(ar))));

	_store(p, all); p += 16;
	_store(p, alh); p += 16;
	_store(p, ahl); p += 16;
	_store(p, ahh); p += 16;

	/* print ascii */
	vec_t const pv = _select(_isascii(ar), ar, dv);
	_store(p, pv); p += 16;
	*p++ = '\n';
	return(p - dst);
}

static inline
void patch(
	size_t rem,
	uint8_t *tail,
	size_t addr_digits)
{
	(void)(addr_digits);		/* unused */

	uint8_t *base = &tail[-5LL * 16 - 1];
	for(size_t i = rem; i < 16; i++) {
		base[4 * i     ] = ' ';
		base[4 * i + 1 ] = ' ';
		base[4 * 16 + i] = ' ';
	}
	return;
}

int main(int argc, char const *argv[])
{
	size_t const size = 2ULL * 1024 * 1024, margin = 256, cols = 12;
	uint8_t *in = malloc(size + margin);
	uint8_t *out = malloc(8 * size + margin);

	FILE *fp = argc == 1 ? fdopen(0, "rb") : fopen(argv[1], "rb");
	size_t ofs = 0;
	while(feof(fp) == 0) {
		size_t len = fread(in, 1, size, fp);
		uint8_t *p = out;
		for(size_t i = 0; i < len; i += 16) {
			p += conv(ofs, &in[i], p, cols); ofs += 16;
		}

		if((len % 16) != 0) {
			patch(len % 16, p, cols);
		}

		fwrite(out, 1, p - out, stdout);
	}
	fflush(stdout);
	fclose(fp);
	return(0);
}


/**
 * end of xd.c
 */
