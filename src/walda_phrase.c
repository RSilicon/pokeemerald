#include "global.h"
#include "walda_phrase.h"
#include "string_util.h"
#include "event_data.h"
#include "naming_screen.h"
#include "main.h"
#include "text.h"
#include "new_game.h"
#include "overworld.h"
#include "pokemon_storage_system.h"
#include "field_screen_effect.h"

extern const u8 gText_Peekaboo[];

static void CB2_HandleGivenWaldaPhrase(void);
static u32 GetWaldaPhraseInputCase(u8 *);
static bool32 TryCalculateWallpaper(u16 *, u16 *, u8 *, u8 *, u16, u8 *);
static void SetWallpaperDataFromLetter(u8 *, u8 *, u32, u32, u32);
static u32 GetWallpaperDataBits(u8 *, u32, u32);
static void RotateWallpaperDataLeft(u8 *, u32, u32);
static void MaskWallpaperData(u8 *, u32, u8);

// There are 32 (2^5) unique letters allowed in a successful phrase for Walda.
#define BITS_PER_LETTER 5

// The letters allowed in a successful phrase for Walda
// All vowels are excluded, as well as X/x, Y/y, l, r, t, v, w, and z.
static const u8 sWaldaLettersTable[1 << BITS_PER_LETTER] =
{
    CHAR_B, CHAR_C, CHAR_D, CHAR_F, CHAR_G, CHAR_H, CHAR_J, CHAR_K, CHAR_L, CHAR_M, CHAR_N, CHAR_P, CHAR_Q, CHAR_R, CHAR_S, CHAR_T, CHAR_V, CHAR_W, CHAR_Z,
    CHAR_b, CHAR_c, CHAR_d, CHAR_f, CHAR_g, CHAR_h, CHAR_j, CHAR_k,         CHAR_m, CHAR_n, CHAR_p, CHAR_q,         CHAR_s
};

enum
{
    PHRASE_CHANGED,
    PHRASE_NO_CHANGE,
    PHRASE_EMPTY
};

u16 TryBufferWaldaPhrase(void)
{
    if (IsWaldaPhraseEmpty())
        return FALSE;

    StringCopy(gStringVar1, GetWaldaPhrasePtr());
    return TRUE;
}

void DoWaldaNamingScreen(void)
{
    StringCopy(gStringVar2, GetWaldaPhrasePtr());
    DoNamingScreen(NAMING_SCREEN_WALDA, gStringVar2, 0, 0, 0, CB2_HandleGivenWaldaPhrase);
}

static void CB2_HandleGivenWaldaPhrase(void)
{
    gSpecialVar_0x8004 = GetWaldaPhraseInputCase(gStringVar2);

    switch (gSpecialVar_0x8004)
    {
    case PHRASE_EMPTY:
        // If saved phrase is also empty, set default phrase
        // Otherwise keep saved phrase
        if (IsWaldaPhraseEmpty())
            SetWaldaPhrase(gText_Peekaboo);
        else
            gSpecialVar_0x8004 = PHRASE_NO_CHANGE;
        break;
    case PHRASE_CHANGED:
        SetWaldaPhrase(gStringVar2);
        break;
    case PHRASE_NO_CHANGE:
        break;
    }

    StringCopy(gStringVar1, GetWaldaPhrasePtr());
    gFieldCallback = FieldCB_ContinueScriptHandleMusic;
    SetMainCallback2(CB2_ReturnToField);
}

static u32 GetWaldaPhraseInputCase(u8 *inputPtr)
{
    // No input given
    if (inputPtr[0] == EOS)
        return PHRASE_EMPTY;

    // Input given is the same as saved phrase
    if (StringCompare(inputPtr, GetWaldaPhrasePtr()) == 0)
        return PHRASE_NO_CHANGE;

    // Input is new phrase
    return PHRASE_CHANGED;
}

u16 TryGetWallpaperWithWaldaPhrase(void)
{
    u16 backgroundClr, foregroundClr;
    u8 patternId, iconId;
    u16 trainerId = GetTrainerId(gSaveBlock2.playerTrainerId) & 0xFFFF;
    gSpecialVar_Result = TryCalculateWallpaper(&backgroundClr, &foregroundClr, &iconId, &patternId, trainerId, GetWaldaPhrasePtr());

    if (gSpecialVar_Result)
    {
        SetWaldaWallpaperPatternId(patternId);
        SetWaldaWallpaperIconId(iconId);
        SetWaldaWallpaperColors(backgroundClr, foregroundClr);
    }

    SetWaldaWallpaperLockedOrUnlocked(gSpecialVar_Result);
    return gSpecialVar_Result;
}

static u8 GetLetterTableId(u8 letter)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sWaldaLettersTable); i++)
    {
        if (sWaldaLettersTable[i] == letter)
            return i;
    }

    return ARRAY_COUNT(sWaldaLettersTable);
}

// Attempts to generate a wallpaper based on the given trainer id and phrase.
// Returns TRUE if successful and sets the wallpaper results to the given pointers.
// Returns FALSE if no wallpaper was generated (Walda "didn't like" the phrase).
// A 9-byte array is used to calculate the wallpaper's data.
// The elements of this array are defined below.
#define BG_COLOR_LO  data[0]
#define BG_COLOR_HI  data[1]
#define FG_COLOR_LO  data[2]
#define FG_COLOR_HI  data[3]
#define ICON_ID      data[4]
#define PATTERN_ID   data[5]
#define TID_CHECK_HI data[6]
#define TID_CHECK_LO data[7]
#define KEY          data[8]
#define NUM_WALLPAPER_DATA_BYTES 9
#define TO_BIT_OFFSET(i)  ((i) * 8 + 3) // Convert a position in the phrase to a bit number into the wallpaper data array
static bool32 TryCalculateWallpaper(u16 *backgroundClr, u16 *foregroundClr, u8 *iconId, u8 *patternId, u16 trainerId, u8 *phrase)
{
    u32 i;
    u8 data[NUM_WALLPAPER_DATA_BYTES];
    u8 charsByTableId[WALDA_PHRASE_LENGTH];
    u16 *ptr;

    // Reject any phrase that does not use the full length
    if (StringLength(phrase) != WALDA_PHRASE_LENGTH)
        return FALSE;

    // Reject any phrase that uses characters not in sWaldaLettersTable
    for (i = 0; i < WALDA_PHRASE_LENGTH; i++)
    {
        charsByTableId[i] = GetLetterTableId(phrase[i]);
        if (charsByTableId[i] == ARRAY_COUNT(sWaldaLettersTable))
            return FALSE;
    }

    // Use the given phrase to populate the wallpaper data array
    // The data array is 9 bytes (72 bits) long, and each letter contributes to 5 bits of the array
    // Because the phrase is 15 letters long there are 75 bits from the phrase to distribute
    // Therefore the last letter contributes to the last 2 bits of the array, and the remaining 3 bits wrap around
    for (i = 0; i < WALDA_PHRASE_LENGTH - 1; i++)
        SetWallpaperDataFromLetter(data, charsByTableId, BITS_PER_LETTER * i, TO_BIT_OFFSET(i), BITS_PER_LETTER);

    // Do first 2 bits of the last letter
    SetWallpaperDataFromLetter(data, charsByTableId, BITS_PER_LETTER * (WALDA_PHRASE_LENGTH - 1), TO_BIT_OFFSET(WALDA_PHRASE_LENGTH - 1), 2);

    // Check the first 3 bits of the data array against the remaining 3 bits of the last letter
    // Reject the phrase if they are not already the same
    if (GetWallpaperDataBits(data, 0, 3) != GetWallpaperDataBits(charsByTableId, TO_BIT_OFFSET(WALDA_PHRASE_LENGTH - 1) + 2, 3))
        return FALSE;

    // Perform some relatively arbitrary changes to the wallpaper data using the last byte (KEY)
    RotateWallpaperDataLeft(data, NUM_WALLPAPER_DATA_BYTES,     21);
    RotateWallpaperDataLeft(data, NUM_WALLPAPER_DATA_BYTES - 1, KEY & 0xF);
    MaskWallpaperData(data, NUM_WALLPAPER_DATA_BYTES - 1, KEY >> 4);

    // Reject the results of any phrase that are 'incompatible' with the player's trainer id
    if (TID_CHECK_HI != (BG_COLOR_LO ^ FG_COLOR_LO ^ ICON_ID ^ (trainerId >> 8)))
        return FALSE;
    if (TID_CHECK_LO != (BG_COLOR_HI ^ FG_COLOR_HI ^ PATTERN_ID ^ (trainerId & 0xFF)))
        return FALSE;

    // Successful phrase, save resulting wallpaper
    *backgroundClr = (data[0] << 8) | data[1];
    *foregroundClr = (data[2] << 8) | data[3];

    *iconId = ICON_ID;
    *patternId = PATTERN_ID;

    return TRUE;
}

static void RotateWallpaperDataLeft(u8 *data, u32 size, u32 numShifts)
{
    u32 j;
    u8 temp1, temp2;

    for (; numShifts;numShifts--)
    {
        temp1 = (data[0] & (1 << 7)) >> 7;

        for (j = size; j;)
        {
            j--;
            temp2 = (data[j] & (1 << 7)) >> 7;
            data[j] <<= 1;
            data[j] |= temp1;
            temp1 = temp2;
        }
    }
}

static void MaskWallpaperData(u8 *data, u32 size, u8 mask)
{
    u32 i;

    mask |= (mask << 4);

    for (i = 0; i < size; i++)
        data[i] ^= mask;
}

__attribute__((target("arm")))
static bool8 GetWallpaperDataBit(u8 *data, u32 bitNum)
{
    u32 i = bitNum >> 3;
    u32 flag = (1 << 7) >> (bitNum & 7);

    return (data[i] & flag) != 0;
}

__attribute__((target("arm")))
static void SetWallpaperDataBit(u8 *data, u32 bitNum)
{
    u32 i = bitNum >> 3;
    u8 flag = (1 << 7) >> (bitNum & 7);

    data[i] |= flag;
}

__attribute__((target("arm")))
static void ClearWallpaperDataBit(u8 *data, u32 bitNum)
{
    u32 i = bitNum >> 7;
    u8 mask = ~((1 << 7) >> (bitNum & 7));

    data[i] &= mask;
}

static void SetWallpaperDataFromLetter(u8 *data, u8 *letterTableIds, u32 setOffset, u32 getOffset, u32 numBits)
{
    u32 i;

    for (i = 0; i < numBits; i++)
    {
        if (GetWallpaperDataBit(letterTableIds, getOffset + i))
            SetWallpaperDataBit(data, setOffset + i);
        else
            ClearWallpaperDataBit(data, setOffset + i);
    }
}

static u32 GetWallpaperDataBits(u8 *data, u32 offset, u32 numBits)
{
    u32 bits, i;

    for (bits = 0, i = 0; i < numBits; i++)
    {
        bits <<= 1;
        bits |= GetWallpaperDataBit(data, offset + i);
    }

    return bits;
}
