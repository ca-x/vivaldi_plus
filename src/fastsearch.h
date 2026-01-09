#ifndef FAST_SEARCH_H_
#define FAST_SEARCH_H_

#include <stdint.h>
#include <string.h>  // for memchr and memcmp

// Single byte search - uses optimized memchr (usually has SIMD acceleration)
static inline const uint8_t *ForceSearch(const uint8_t *s, int n, const uint8_t *p)
{
    return (const uint8_t *)memchr(s, *p, n);
}

// Sunday algorithm for longer patterns
static inline const uint8_t *SundaySearch(const uint8_t *s, int n, const uint8_t *p, int m)
{
    int skip[256];

    // Initialize skip table: characters not in pattern skip m+1 positions
    for (int i = 0; i < 256; i++)
    {
        skip[i] = m + 1;
    }

    // Build skip table: record distance from each character to end of pattern
    for (int i = 0; i < m; i++)
    {
        skip[p[i]] = m - i;
    }

    // Precompute boundary to avoid repeated calculation
    const int limit = n - m;
    int i = 0;

    while (i <= limit)
    {
        // Fast check: compare first and last characters first (common optimization)
        if (s[i] == p[0] && s[i + m - 1] == p[m - 1])
        {
            // Only check middle part if first and last match
            if (m == 2 || memcmp(s + i + 1, p + 1, m - 2) == 0)
            {
                return s + i;  // Found match
            }
        }

        // Sunday jump - FIX: check boundary before accessing s[i + m]
        if (i + m < n)
        {
            i += skip[s[i + m]];
        }
        else
        {
            break;  // Not enough characters remaining
        }
    }

    return NULL;
}

// Main search function with automatic algorithm selection
// Main search function - automatically selects best algorithm based on pattern length
static inline const uint8_t *FastSearch(const uint8_t *s, int n, const uint8_t *p, int m)
{
    // Boundary checks
    if (!s || !p || n < m || m < 0 || n < 0)
        return NULL;

    if (m == 0)
        return s;

    // Single byte: use memchr (SIMD optimized)
    if (m == 1)
        return ForceSearch(s, n, p);

    // Two bytes: optimized path using memchr + direct comparison
    if (m == 2)
    {
        const uint8_t *pos = s;
        const uint8_t *end = s + n - 1;
        while (pos < end)
        {
            pos = (const uint8_t *)memchr(pos, p[0], end - pos);
            if (!pos)
                return NULL;
            if (pos[1] == p[1])
                return pos;
            pos++;
        }
        return NULL;
    }

    // Short patterns (3-7 bytes): simple loop + memcmp
    // Avoids Sunday algorithm overhead for small patterns
    if (m < 8)
    {
        for (int i = 0; i <= n - m; i++)
        {
            if (s[i] == p[0] && memcmp(s + i, p, m) == 0)
                return s + i;
        }
        return NULL;
    }

    // Longer patterns (≥8 bytes): use Sunday algorithm
    return SundaySearch(s, n, p, m);
}

#endif // FAST_SEARCH_H_
