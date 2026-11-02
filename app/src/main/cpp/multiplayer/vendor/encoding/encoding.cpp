#include "encoding.h"

constexpr Encoding::Letter Encoding::m_letters[];

static bool isStringValidUTF8(const std::string& s, bool& hasMultibyte)
{
    hasMultibyte = false;
    size_t i = 0;
    size_t len = s.length();
    while (i < len)
    {
        unsigned char c = (unsigned char)s[i];
        if (c <= 0x7F) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len) return false;
            if (((unsigned char)s[i + 1] & 0xC0) != 0x80) return false;
            hasMultibyte = true;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len) return false;
            if (((unsigned char)s[i + 1] & 0xC0) != 0x80) return false;
            if (((unsigned char)s[i + 2] & 0xC0) != 0x80) return false;
            hasMultibyte = true;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len) return false;
            if (((unsigned char)s[i + 1] & 0xC0) != 0x80) return false;
            if (((unsigned char)s[i + 2] & 0xC0) != 0x80) return false;
            if (((unsigned char)s[i + 3] & 0xC0) != 0x80) return false;
            hasMultibyte = true;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

std::string Encoding::trimIncompleteUtf8Tail(const std::string& s)
{
    if (s.empty()) return s;
    size_t len = s.size();

    // Look backwards up to 3 bytes from the end to find if an incomplete multibyte sequence starts there
    for (size_t back = 1; back <= 3 && back <= len; back++)
    {
        unsigned char c = (unsigned char)s[len - back];
        if (c <= 0x7F) {
            // ASCII byte - complete character, cannot be part of a multibyte sequence
            break;
        }
        if ((c & 0xC0) == 0xC0) {
            // Found a UTF-8 leading byte!
            size_t expected = 0;
            if ((c & 0xE0) == 0xC0) expected = 2;       // 2-byte sequence (110xxxxx)
            else if ((c & 0xF0) == 0xE0) expected = 3;  // 3-byte sequence (1110xxxx)
            else if ((c & 0xF8) == 0xF0) expected = 4;  // 4-byte sequence (11110xxx)
            else {
                // Invalid lead byte (>= 0xF8)
                break;
            }

            if (back < expected) {
                // Incomplete tail: expected 'expected' bytes but only 'back' bytes remain until end
                return s.substr(0, len - back);
            } else {
                // Sequence is complete
                break;
            }
        }
        // (c & 0xC0) == 0x80: continuation byte, keep scanning backwards to find its lead byte
    }
    return s;
}

std::string Encoding::cp2utf(const std::string& s)
{
    if (s.empty()) return s;

    // 1. If string is already valid UTF-8 with multi-byte chars (Thai, Lao, emojis, etc.), preserve it!
    // Trimming incomplete tail handles Lao/Thai messages truncated by network length limits.
    std::string trimmed = trimIncompleteUtf8Tail(s);
    bool hasMultibyte = false;
    if (isStringValidUTF8(trimmed, hasMultibyte) && hasMultibyte)
    {
        return trimmed;
    }

    // 2. Check if string contains CP874 / TIS-620 Thai bytes (0xA1 .. 0xFB) or UTF-8 Thai/Lao
    bool hasCP874OrUtf8 = false;
    bool hasNonAscii = false;
    for (size_t k = 0; k < s.size(); k++)
    {
        unsigned char uc = (unsigned char)s[k];
        if (uc > 0x7F)
        {
            hasNonAscii = true;
            if ((uc >= 0xA1 && uc <= 0xFB) || uc == 0xE0)
            {
                hasCP874OrUtf8 = true;
                break;
            }
        }
    }

    if (!hasNonAscii) return s;

    // Mixed mode: decode sequence-by-sequence.
    // If a valid Thai/Lao UTF-8 sequence (U+0E00..U+0EFF -> 0xE0 0xB8..0xBB 0x80..0xBF)
    // or valid general UTF-8 sequence is encountered, keep it as UTF-8.
    // Single-byte CP874 characters (0xA1..0xFB) are converted to UTF-8.
    if (hasCP874OrUtf8)
    {
        std::string ns;
        ns.reserve(s.size() * 3);
        size_t i = 0;
        size_t len = s.size();

        while (i < len)
        {
            unsigned char c = (unsigned char)s[i];

            // A. ASCII byte
            if (c <= 0x7F)
            {
                ns.push_back((char)c);
                i++;
                continue;
            }

            // B. Valid 3-byte Thai/Lao UTF-8 sequence (U+0E00 .. U+0EFF):
            // Byte 1: 0xE0
            // Byte 2: 0xB8 .. 0xBB (Thai: 0xB8, 0xB9; Lao: 0xBA, 0xBB)
            // Byte 3: 0x80 .. 0xBF
            if (c == 0xE0)
            {
                if (i + 2 < len)
                {
                    unsigned char c2 = (unsigned char)s[i + 1];
                    unsigned char c3 = (unsigned char)s[i + 2];
                    if (c2 >= 0xB8 && c2 <= 0xBB && (c3 & 0xC0) == 0x80)
                    {
                        ns.push_back((char)c);
                        ns.push_back((char)c2);
                        ns.push_back((char)c3);
                        i += 3;
                        continue;
                    }
                }
                else
                {
                    // Incomplete Thai/Lao sequence at end of string:
                    // If followed by Lao/Thai byte 2 (0xB8..0xBB) but truncated before byte 3,
                    // discard incomplete tail to avoid corrupting into CP874 "เบ"
                    if (i + 1 < len)
                    {
                        unsigned char c2 = (unsigned char)s[i + 1];
                        if (c2 >= 0xB8 && c2 <= 0xBB)
                        {
                            i = len;
                            break;
                        }
                    }
                    else
                    {
                        // Lone 0xE0 at end
                        i = len;
                        break;
                    }
                }
            }

            // C. 4-byte multi-byte UTF-8 sequences (e.g. emojis)
            if ((c & 0xF8) == 0xF0 && i + 3 < len)
            {
                if (((unsigned char)s[i + 1] & 0xC0) == 0x80 &&
                    ((unsigned char)s[i + 2] & 0xC0) == 0x80 &&
                    ((unsigned char)s[i + 3] & 0xC0) == 0x80)
                {
                    ns.append(s, i, 4);
                    i += 4;
                    continue;
                }
            }

            // D. CP874 / TIS-620 Thai byte (0xA1 .. 0xFB)
            if (c >= 0xA1 && c <= 0xFB)
            {
                uint32_t unicode = 0x0E00 + (c - 0xA0);
                ns.push_back((char)0xE0);
                ns.push_back((char)(0x80 | ((unicode >> 6) & 0x3F)));
                ns.push_back((char)(0x80 | (unicode & 0x3F)));
                i++;
                continue;
            }

            // E. Fallback for other characters (e.g. Windows-1251)
            char buf[4];
            char in[2] = { (char)c, 0 };
            convert_windows1251_to_utf8(buf, in, strlen(in));
            ns += std::string(buf);
            i++;
        }
        return ns;
    }

    // 3. Fallback to Windows-1251 conversion
    int c, i;
    int len = s.size();
    std::string ns;
    for (i = 0; i < len; i++)
    {
        c = s[i];
        char buf[4];
        char in[2] = { 0, 0 };
        *in = c;
        convert_windows1251_to_utf8(buf, in, strlen(in));
        ns += std::string(buf);
    }
    return ns;
}

std::string Encoding::utf8_to_cp874(const std::string& s)
{
    if (s.empty()) return s;
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    size_t len = s.size();
    while (i < len)
    {
        unsigned char c1 = (unsigned char)s[i];
        if (c1 == 0xE0 && i + 2 < len)
        {
            unsigned char c2 = (unsigned char)s[i + 1];
            unsigned char c3 = (unsigned char)s[i + 2];
            uint32_t unicode = ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            if (unicode >= 0x0E01 && unicode <= 0x0E5B)
            {
                out.push_back((char)(0xA0 + (unicode - 0x0E00)));
                i += 3;
                continue;
            }
        }
        out.push_back(s[i]);
        i++;
    }
    return out;
}

std::string Encoding::utf2cp(const std::string& s)
{
    size_t len = s.size();
    const char* buff = s.c_str();
    char* output = new char[len];
    convert_utf8_to_windows1251(buff, output, len);
    std::string ns(output);
    return ns;
}

bool Encoding::convert_utf8_to_windows1251(const char* utf8, char* windows1251, size_t n)
{
    int i = 0;
    int j = 0;
    for (; i < (int)n && utf8[i] != 0; ++i) {
        char prefix = utf8[i];
        char suffix = utf8[i + 1];
        if ((prefix & 0x80) == 0) {
            windows1251[j] = (char)prefix;
            ++j;
        }
        else if ((~prefix) & 0x20) {
            int first5bit = prefix & 0x1F;
            first5bit <<= 6;
            int sec6bit = suffix & 0x3F;
            int unicode_char = first5bit + sec6bit;

            if (unicode_char >= 0x410 && unicode_char <= 0x44F) {
                windows1251[j] = (char)(unicode_char - 0x350);
            }
            else if (unicode_char >= 0x80 && unicode_char <= 0xFF) {
                windows1251[j] = (char)(unicode_char);
            }
            else if (unicode_char >= 0x402 && unicode_char <= 0x403) {
                windows1251[j] = (char)(unicode_char - 0x382);
            }
            else {
                int count = sizeof(m_letters) / sizeof(Letter);
                for (int k = 0; k < count; ++k) {
                    if (unicode_char == m_letters[k].unicode) {
                        windows1251[j] = m_letters[k].win1251;
                        goto NEXT_LETTER;
                    }
                }
                // can't convert this char
                return false;
            }
            NEXT_LETTER:
            ++i;
            ++j;
        }
        else {
            // can't convert this chars
            return false;
        }
    }
    windows1251[j] = 0;
    return true;
}

void Encoding::convert_windows1251_to_utf8(char* windows1251, const char* utf8, unsigned int len)
{
    static const int table[128] = {
            /*
            0x82D0,		0x83D0,		0x9A80E2,	0x93D1,		0x9E80E2,	0xA680E2,	0xA080E2,	0xA180E2,
            0xAC82E2,	0xB080E2,	0x89D0,		0xB980E2,	0x8AD0,		0x8CD0,		0x8BD0,		0x8FD0,
            0x92D1,		0x9880E2,	0x9980E2,	0x9C80E2,	0x9D80E2,	0xA280E2,	0x9380E2,	0x9480E2,
            0,			0xA284E2,	0x99D1,		0xBA80E2,	0x9AD1,		0x9CD1,		0x9BD1,		0x9FD1,
            0xA0C2,		0x8ED0,		0x9ED1,		0x88D0,		0xA4C2,		0x90D2,		0xA6C2,		0xA7C2,
            0x81D0,		0xA9C2,		0x84D0,		0xABC2,		0xACC2,		0xADC2,		0xAEC2,		0x87D0,
            0xB0C2,		0xB1C2,		0x86D0,		0x96D1,		0x91D2,		0xB5C2,		0xB6C2,		0xB7C2,
            0x91D1,		0x9684E2,	0x94D1,		0xBBC2,		0x98D1,		0x85D0,		0x95D1,		0x97D1,
            0x90D0,		0x91D0,		0x92D0,		0x93D0,		0x94D0,		0x95D0,		0x96D0,		0x97D0,
            0x98D0,		0x99D0,		0x9AD0,		0x9BD0,		0x9CD0,		0x9DD0,		0x9ED0,		0x9FD0,
            0xA0D0,		0xA1D0,		0xA2D0,		0xA3D0,		0xA4D0,		0xA5D0,		0xA6D0,		0xA7D0,
            0xA8D0,		0xA9D0,		0xAAD0,		0xABD0,		0xACD0,		0xADD0,		0xAED0,		0xAFD0,
            0xB0D0,		0xB1D0,		0xB2D0,		0xB3D0,		0xB4D0,		0xB5D0,		0xB6D0,		0xB7D0,
            0xB8D0,		0xB9D0,		0xBAD0,		0xBBD0,		0xBCD0,		0xBDD0,		0xBED0,		0xBFD0,
            0x80D1,		0x81D1,		0x82D1,		0x83D1,		0x84D1,		0x85D1,		0x86D1,		0x87D1,
            0x88D1,		0x89D1,		0x8AD1,		0x8BD1,		0x8CD1,		0x8DD1,		0x8ED1,		0x8FD1
            */
            // 80
            0x82D0,     0x83D0,     0x9A80E2,   0x93D1,     0x9E80E2,   0xA680E2,   0xA080E2,   0xA180E2,
            0xAC82E2,   0xB080E2,   0x89D0,     0xB980E2,   0x8AD0,     0x8CD0,     0x8BD0,     0x8FD0,
            // 90
            0x92D1,     0x9880E2,   0x9980E2,   0x9C80E2,   0x9D80E2,   0xA280E2,   0x9380E2,   0x9480E2,
            0,          0xA284E2,   0x99D1,     0xBA80E2,   0x9AD1,     0x9CD1,     0x9BD1,     0x9FD1,
            // A0
            0x80b8e0,0x81b8e0,0x82b8e0,0x83b8e0,0x84b8e0,0x85b8e0,0x86b8e0,0x87b8e0,
            0x88b8e0,0x89b8e0,0x8ab8e0,0x8bb8e0,0x8cb8e0,0x8db8e0,0x8eb8e0,0x8fb8e0,
            // B0
            0x90b8e0,0x91b8e0,0x92b8e0,0x93b8e0,0x94b8e0,0x95b8e0,0x96b8e0,0x97b8e0,
            0x98b8e0,0x99b8e0,0x9ab8e0,0x9bb8e0,0x9cb8e0,0x9db8e0,0x9eb8e0,0x9fb8e0,
            // C0
            0xa0b8e0,0xa1b8e0,0xa2b8e0,0xa3b8e0,0xa4b8e0,0xa5b8e0,0xa6b8e0,0xa7b8e0,
            0xa8b8e0,0xa9b8e0,0xaab8e0,0xabb8e0,0xacb8e0,0xadb8e0,0xaeb8e0,0xafb8e0,
            // D0
            0xb0b8e0,0xb1b8e0,0xb2b8e0,0xb3b8e0,0xb4b8e0,0xb5b8e0,0xb6b8e0,0xb7b8e0,
            0xb8b8e0,0xb9b8e0,0xbab8e0,0       ,0       ,0       ,0       ,0xbfb8e0,
            // E0
            0x80b9e0,0x81b9e0,0x82b9e0,0x83b9e0,0x84b9e0,0x85b9e0,0x86b9e0,0x87b9e0,
            0x88b9e0,0x89b9e0,0x8ab9e0,0x8bb9e0,0x8cb9e0,0x8db9e0,0x8eb9e0,0x8fb9e0,
            // F0
            0x90b9e0,0x91b9e0,0x92b9e0,0x93b9e0,0x94b9e0,0x95b9e0,0x96b9e0,0x97b9e0,
            0x98b9e0,0x99b9e0,0x9ab9e0,0x9bb9e0,//128
    };

    int count = 0;

    while (*utf8)
    {
        if(len && (count >= len)) break;

        if (*utf8 & 0x80)
        {
            int v = table[(int)(0x7f & *utf8++)];
            if (!v) continue;
            *windows1251++ = (char)v;
            *windows1251++ = (char)(v >> 8);
            if (v >>= 16) {
                *windows1251++ = (char)v;
            }
        }
        else
        {
            *windows1251++ = *utf8++;
        }

        count++;
    }

    *windows1251 = 0;
}