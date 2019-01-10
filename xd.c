
/**
 * @file bd.c
 * @brief fast binary dump
 *
 * @date 2018/8/8
 * @license MIT

 * bd -c largefile.txt | grep "string" | vipe | bd -pi largefile.txt
 */

#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

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
	__m128i const cv = _mm_load_si128((__m128i const *)conv_table);
	__m128i const fv = _mm_load_si128((__m128i const *)flip_table);
	__m128i const tv = _mm_set1_epi8(1), mv = _mm_set1_epi8(0x0f);
	__m128i const sv = _mm_set1_epi8(' '), dv = _mm_set1_epi8('.');

	#define _load(_p)					( _mm_loadu_si128((__m128i const *)(_p)) )
	#define _store(_p, _v)				{ _mm_storeu_si128((__m128i *)(_p), (_v)); }
	#define _unpack_nibble_fw(_x)		({ __m128i y = _mm_cvtepu8_epi16((_x)); y = _mm_and_si128(mv, _mm_or_si128(y, _mm_slli_epi16(y, 4))); y; })
	#define _unpack_nibble_low(_x)		({ __m128i y = _mm_cvtepu8_epi16((_x)); y = _mm_srli_epi16(_mm_or_si128(y, _mm_slli_epi16(y, 12)), 4); y; })
	#define _unpack_nibble_high(_x)		({ __m128i y = _mm_unpackhi_epi8((_x), _mm_setzero_si128()); y = _mm_srli_epi16(_mm_or_si128(y, _mm_slli_epi16(y, 12)), 4); y; })
	#define _asciibase(_x)				( _mm_shuffle_epi8(cv, (_x)) )
	#define _isascii(_x)				( _mm_cmpgt_epi8(_mm_add_epi8((_x), tv), sv) )
	#define _select(_m, _y, _x)			( _mm_blendv_epi8((_x), (_y), (_m)) )		/* m ? y : x */
	#define _interleave_low(_x)			( _mm_cvtepu16_epi32((_x)) )
	#define _interleave_high(_x)		( _mm_cvtepu16_epi32(_mm_srli_si128((_x), 8)) )
	#define _flip(_x, _d)				( _mm_shuffle_epi8((_x), _mm_sub_epi8(fv, _mm_set1_epi8(16 - (_d)))) )
	#define _finish(_x)					( _mm_add_epi8(sv, (_x)) )

	uint8_t *p = dst;

	/* print offset */
	__m128i ofsv = _finish(_flip(
		_asciibase(_unpack_nibble_fw(_mm_cvtsi64_si128(ofs))),
		addr_digits
	));
	_store(p, ofsv);
	p += addr_digits + 2;							/* spaced by two */

	/* print values */
	__m128i ar = _load(src);
	__m128i all = _finish(_interleave_low(_asciibase(_unpack_nibble_low(ar))));
	__m128i alh = _finish(_interleave_high(_asciibase(_unpack_nibble_low(ar))));
	__m128i ahl = _finish(_interleave_low(_asciibase(_unpack_nibble_high(ar))));
	__m128i ahh = _finish(_interleave_high(_asciibase(_unpack_nibble_high(ar))));

	_store(p, all); p += 16;
	_store(p, alh); p += 16;
	_store(p, ahl); p += 16;
	_store(p, ahh); p += 16;

	/* print ascii */
	__m128i pv = _select(_isascii(ar), ar, dv);
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
	fprintf(stderr, "rem(%zu)\n", rem);
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
 * end of bd.c
 */
