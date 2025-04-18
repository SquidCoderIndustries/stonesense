#pragma once

#include <cstdint>

//Mainly walls and rivers
//Byte values are used because walls can have either 1 or 2 in any given direction.
const int TileDirectionCount = 4;
union TileDirection
{
    uint32_t whole;
    unsigned char b[TileDirectionCount];
    struct
    {
        //Maybe should add 'up' and 'down' for Z-levels?
        unsigned char north, south, west, east;
    };

    inline TileDirection()
    {
        whole = 0;
    }
    TileDirection(uint32_t whole_bits)
    {
        whole = whole_bits;
    }
    bool operator== (const TileDirection &other) const
    {
        return whole == other.whole;
    }
    bool operator!= (const TileDirection &other) const
    {
        return whole != other.whole;
    }
    operator bool() const
    {
        return whole != 0;
    }
    TileDirection(unsigned char North, unsigned char South, unsigned char West, unsigned char East)
    {
        north = North; south = South; east = East; west = West;
    }
    TileDirection(const char *dir)
    {
        //This one just made for fun.
        //Supports N S E W
        const char *p = dir;
        unsigned char *l = 0;
        north = south = east = west = 0;
        if (!dir) return;

        for (; *p; ++p)
        {
            switch (*p)
            {
            case 'N': //North / Up
            case 'n':
                ++north; l = &north; break;
            case 'S': //South / Down
            case 's':
                ++south; l = &south; break;
            case 'E': //East / Right
            case 'e':
                ++east; l = &east; break;
            case 'W': //West / Left
            case 'w':
                ++west; l = &west; break;
            case '-':
            case ' ':
                //Explicitly ensure dash and space are ignored.
                //Other characters/symbols may be assigned in the future.
                break;
            default:
                if (l && '0' <= *p && '9' >= *p)
                    *l += *p - '0';
                break;
            }
        }
    }

    //may be useful for some situations
    inline uint32_t sum() const
    {
        return 0L + north + south + east + west;
    }

};
