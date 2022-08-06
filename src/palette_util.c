#include "global.h"
#include "palette.h"
#include "palette_util.h"
#include "util.h"

// "RouletteFlash" is more accurately a general flashing/fading util
// this file handles fading the palettes for the color/icon selections on the Roulette wheel
// but it also handles the "pulse blend" effect of Mirage Tower

void RouletteFlash_Reset(struct RouletteFlashUtil *flash)
{
    #if !MODERN
    flash->enabled = 0;
    flash->flags = 0;
    memset(flash->palettes, 0, sizeof(flash->palettes));
    #else
    memset(flash, 0, sizeof(*flash));
    #endif
}

u8 RouletteFlash_Add(struct RouletteFlashUtil *flash, u8 id, const struct RouletteFlashSettings *settings)
{
    if (id >= ARRAY_COUNT(flash->palettes) || flash->palettes[id].available)
        return 0xFF;

    flash->palettes[id].settings.color = settings->color;
    flash->palettes[id].settings.paletteOffset = settings->paletteOffset;
    flash->palettes[id].settings.numColors = settings->numColors;
    flash->palettes[id].settings.delay = settings->delay;
    flash->palettes[id].settings.unk6 = settings->unk6;
    flash->palettes[id].settings.numFadeCycles = settings->numFadeCycles;
    flash->palettes[id].settings.unk7_5 = settings->unk7_5;
    flash->palettes[id].settings.colorDeltaDir = settings->colorDeltaDir;
    flash->palettes[id].state = 0;
    flash->palettes[id].available = TRUE;
    #if !MODERN
    flash->palettes[id].fadeCycleCounter = 0;
    flash->palettes[id].delayCounter = 0;
    #else
    flash->palettes[id].delayCounter = 0;
    flash->palettes[id].fadeCycleCounter = 0;
    #endif
    if (flash->palettes[id].settings.colorDeltaDir < 0)
        flash->palettes[id].colorDelta = -1;
    else
        flash->palettes[id].colorDelta = 1;

    return id;
}

// Unused
static u8 RouletteFlash_Remove(struct RouletteFlashUtil *flash, u8 id)
{
    if (id >= ARRAY_COUNT(flash->palettes))
        return 0xFF;
    if (!flash->palettes[id].available)
        return 0xFF;

    memset(&flash->palettes[id], 0, sizeof(flash->palettes[id]));
    return id;
}

static u8 RouletteFlash_FadePalette(struct RouletteFlashPalette *pal)
{
    u8 i;
    u8 returnval;

    for (i = 0; i < pal->settings.numColors; i++)
    {
        struct PlttData *faded =   (struct PlttData *)&gPlttBufferFaded[pal->settings.paletteOffset + i];
        struct PlttData *unfaded = (struct PlttData *)&gPlttBufferUnfaded[pal->settings.paletteOffset + i];

        switch (pal->state)
        {
        case 1:
            // Fade color
            if (faded->r + pal->colorDelta < 32 && faded->r + pal->colorDelta >= 0)
                faded->r += pal->colorDelta;
            if (faded->g + pal->colorDelta < 32 && faded->g + pal->colorDelta >= 0)
                faded->g += pal->colorDelta;
            if (faded->b + pal->colorDelta < 32 && faded->b + pal->colorDelta >= 0)
                faded->b += pal->colorDelta;
            break;
        case 2:
            // Fade back to original color
            if (pal->colorDelta < 0)
            {
                if (faded->r + pal->colorDelta >= unfaded->r)
                    faded->r += pal->colorDelta;
                if (faded->g + pal->colorDelta >= unfaded->g)
                    faded->g += pal->colorDelta;
                if (faded->b + pal->colorDelta >= unfaded->b)
                    faded->b += pal->colorDelta;
            }
            else
            {
                if (faded->r + pal->colorDelta <= unfaded->r)
                    faded->r += pal->colorDelta;
                if (faded->g + pal->colorDelta <= unfaded->g)
                    faded->g += pal->colorDelta;
                if (faded->b + pal->colorDelta <= unfaded->b)
                    faded->b += pal->colorDelta;
            }
            break;
        }
    }
    // Why have the u32 cast or u8 cast?
    // TODO: check which is better
    if (pal->fadeCycleCounter++ != (u8)pal->settings.numFadeCycles)
    {
        return 0;
    }

    pal->fadeCycleCounter = 0;
    pal->colorDelta *= -1;
    if (pal->state == 1)
        pal->state++;
    else
        pal->state--;
    returnval = 1;
    return 1;
}

static u8 RouletteFlash_FlashPalette(struct RouletteFlashPalette *pal)
{
    #if !MODERN
    u8 i = 0;
    #else
    u32 i;
    #endif
    switch (pal->state)
    {
    case 1:
        // Flash to color
        #if !MODERN
        for (; i < pal->settings.numColors; i++)
            gPlttBufferFaded[pal->settings.paletteOffset + i] = pal->settings.color;
        #else
        for (i = 0; i < pal->settings.numColors; i++)
            gPlttBufferFaded[pal->settings.paletteOffset + i] = pal->settings.color;
        #endif
        pal->state++;
        break;
    case 2:
        // Restore to original color
        #if !MODERN
        for (; i < pal->settings.numColors; i++)
            gPlttBufferFaded[pal->settings.paletteOffset + i] = gPlttBufferUnfaded[pal->settings.paletteOffset + i];
        #else
        for (i = 0; i < pal->settings.numColors; i++)
            gPlttBufferFaded[pal->settings.paletteOffset + i] = gPlttBufferUnfaded[pal->settings.paletteOffset + i];
        #endif
        pal->state--;
        break;
    }
    return 1;
}

void RouletteFlash_Run(struct RouletteFlashUtil *flash)
{
    m8 i = 0;

    if (flash->enabled)
    {
        for (i = 0; i < ARRAY_COUNT(flash->palettes); i++)
        {
            if ((flash->flags >> i) & 1)
            {
                // Why not have the delayCounter go down when this is FALSE
                #if !MODERN
                if (flash->palettes[i].delayCounter-- == 0)
                {
                    if (flash->palettes[i].settings.color & FLASHUTIL_USE_EXISTING_COLOR)
                        RouletteFlash_FadePalette(&flash->palettes[i]);
                    else
                        RouletteFlash_FlashPalette(&flash->palettes[i]);

                    flash->palettes[i].delayCounter = flash->palettes[i].settings.delay;
                }
                #else
                if (flash->palettes[i].delayCounter == 0)
                {
                    if (flash->palettes[i].settings.color & FLASHUTIL_USE_EXISTING_COLOR)
                        RouletteFlash_FadePalette(&flash->palettes[i]);
                    else
                        RouletteFlash_FlashPalette(&flash->palettes[i]);

                    flash->palettes[i].delayCounter = flash->palettes[i].settings.delay;
                }
                else
                    flash->palettes[i].delayCounter--;
                #endif
            }
        }
    }
}

void RouletteFlash_Enable(struct RouletteFlashUtil *flash, u16 flags)
{
    #if !MODERN
    u8 i = 0;
    flash->enabled++;
    #else
    u16 i;
    flash->enabled = TRUE;
    #endif

    for (i = 0; i < ARRAY_COUNT(flash->palettes); i++)
    {
        if ((flags >> i) & 1)
        {
            if (flash->palettes[i].available)
            {
                flash->flags |= 1 << i;
                flash->palettes[i].state = 1;
            }
        }
    }
}

void RouletteFlash_Stop(struct RouletteFlashUtil *flash, u16 flags)
{
    m8 i;

    for (i = 0; i < ARRAY_COUNT(flash->palettes); i++)
    {
        if ((flash->flags >> i) & 1)
        {
            if (flash->palettes[i].available)
            {
                if ((flags >> i) & 1)
                {
                    #if !MODERN
                    u32 offset = flash->palettes[i].settings.paletteOffset;
                    u16 *faded = &gPlttBufferFaded[offset];
                    u16 *unfaded = &gPlttBufferUnfaded[offset];
                    memcpy(faded, unfaded, flash->palettes[i].settings.numColors * 2);
                    #else
                    u16 offset = flash->palettes[i].settings.paletteOffset;
                    memcpy(&gPlttBufferFaded[offset],  &gPlttBufferUnfaded[offset], flash->palettes[i].settings.numColors * 2);
                    #endif
                    flash->palettes[i].state = 0;
                    #if !MODERN
                    flash->palettes[i].fadeCycleCounter = 0;
                    flash->palettes[i].delayCounter = 0;
                    #else
                    flash->palettes[i].delayCounter = 0;
                    flash->palettes[i].fadeCycleCounter = 0;
                    #endif
                    if (flash->palettes[i].settings.colorDeltaDir)
                        flash->palettes[i].colorDelta = -1;
                    else
                        flash->palettes[i].colorDelta = 1;
                }
            }
        }
    }

    if (flags == 0xFFFF)
    {
        // Stopped all
        flash->enabled = 0;
        flash->flags = 0;
    }
    else
    {
        flash->flags &= ~flags;
    }
}

void InitPulseBlend(struct PulseBlend *pulseBlend)
{
    u8 i = 0;
    pulseBlend->usedPulseBlendPalettes = 0;
    memset(&pulseBlend->pulseBlendPalettes, 0, sizeof(pulseBlend->pulseBlendPalettes));
    for (; i < 16; i++)
        pulseBlend->pulseBlendPalettes[i].paletteSelector = i;
}

u32 InitPulseBlendPaletteSettings(struct PulseBlend *pulseBlend, const struct PulseBlendSettings *settings)
{
    #if !MODERN
    u8 i = 0;
    #else
    u32 i;
    #endif
    struct PulseBlendPalette *pulseBlendPalette = NULL;
    #if !MODERN
    for (; i < 16; i++)
    #else
    for (i = 0; i < 16; i++)
    #endif
    {
        if (!pulseBlend->pulseBlendPalettes[i].inUse)
        {
            pulseBlendPalette = &pulseBlend->pulseBlendPalettes[i];
            break;
        }
    }

    if (pulseBlendPalette == NULL)
        return 0xFF;

    pulseBlendPalette->blendCoeff = 0;
    pulseBlendPalette->fadeDirection = 0;
    pulseBlendPalette->available = 1;
    pulseBlendPalette->inUse = 1;
    pulseBlendPalette->delayCounter = 0;
    pulseBlendPalette->fadeCycleCounter = 0;
    memcpy(&pulseBlendPalette->pulseBlendSettings, settings, sizeof(*settings));
    return i;
}

static void ClearPulseBlendPalettesSettings(struct PulseBlendPalette *pulseBlendPalette)
{
    u16 i;

    if (!pulseBlendPalette->available && pulseBlendPalette->pulseBlendSettings.restorePaletteOnUnload)
    {
        for (i = pulseBlendPalette->pulseBlendSettings.paletteOffset; i < pulseBlendPalette->pulseBlendSettings.paletteOffset + pulseBlendPalette->pulseBlendSettings.numColors; i++)
            gPlttBufferFaded[i] = gPlttBufferUnfaded[i];
    }

    memset(&pulseBlendPalette->pulseBlendSettings, 0, sizeof(pulseBlendPalette->pulseBlendSettings));
    pulseBlendPalette->blendCoeff = 0;
    pulseBlendPalette->fadeDirection = 0;
    pulseBlendPalette->unk1_5 = 0;
    pulseBlendPalette->available = 1;
    pulseBlendPalette->inUse = 0;
    pulseBlendPalette->fadeCycleCounter = 0;
    pulseBlendPalette->delayCounter = 0;
}

void UnloadUsedPulseBlendPalettes(struct PulseBlend *pulseBlend, u16 pulseBlendPaletteSelector, u8 multiSelection)
{
    #if !MODERN
    u16 i = 0;
    #else
    u32 i;
    #endif

    if (!multiSelection)
    {
        ClearPulseBlendPalettesSettings(&pulseBlend->pulseBlendPalettes[pulseBlendPaletteSelector & 0xF]);
        return;
    }

    for (i = 0; i < 16; i++)
    {
        if ((pulseBlendPaletteSelector & 1) && pulseBlend->pulseBlendPalettes[i].inUse)
            ClearPulseBlendPalettesSettings(&pulseBlend->pulseBlendPalettes[i]);

        pulseBlendPaletteSelector >>= 1;
    }
}

void MarkUsedPulseBlendPalettes(struct PulseBlend *pulseBlend, u16 pulseBlendPaletteSelector, u8 multiSelection)
{
    #if !MODERN
    u8 i = 0;
    #else
    u32 i;
    #endif

    if (!multiSelection)
    {
        i = pulseBlendPaletteSelector & 0xF;
        pulseBlend->pulseBlendPalettes[i].available = 0;
        pulseBlend->usedPulseBlendPalettes |= 1 << i;
        return;
    }

    for (i = 0; i < 16; i++)
    {
        if (!(pulseBlendPaletteSelector & 1) || !pulseBlend->pulseBlendPalettes[i].inUse || !pulseBlend->pulseBlendPalettes[i].available)
        {
            pulseBlendPaletteSelector <<= 1;
            continue;
        }

        pulseBlend->pulseBlendPalettes[i].available = 0;
        pulseBlend->usedPulseBlendPalettes |= 1 << i;
    }
}

void UnmarkUsedPulseBlendPalettes(struct PulseBlend *pulseBlend, u16 pulseBlendPaletteSelector, u8 multiSelection)
{
    m16 i;
    struct PulseBlendPalette *pulseBlendPalette;
    #if !MODERN
    u8 j = 0;
    #else
    u32 j;
    #endif

    if (!multiSelection)
    {
        pulseBlendPalette = &pulseBlend->pulseBlendPalettes[pulseBlendPaletteSelector & 0xF];
        if (!pulseBlendPalette->available && pulseBlendPalette->inUse)
        {
            if (pulseBlendPalette->pulseBlendSettings.restorePaletteOnUnload)
            {
                for (i = pulseBlendPalette->pulseBlendSettings.paletteOffset; i < pulseBlendPalette->pulseBlendSettings.paletteOffset + pulseBlendPalette->pulseBlendSettings.numColors; i++)
                    gPlttBufferFaded[i] = gPlttBufferUnfaded[i];
            }

            pulseBlendPalette->available = 1;
            pulseBlend->usedPulseBlendPalettes &= ~(1 << j);
        }
        return;
    }

    for (j = 0; j < 16; j++)
    {
        pulseBlendPalette = &pulseBlend->pulseBlendPalettes[j];
        if (!(pulseBlendPaletteSelector & 1) || pulseBlendPalette->available || !pulseBlendPalette->inUse)
        {
            pulseBlendPaletteSelector <<= 1;
            continue;
        }

        if (pulseBlendPalette->pulseBlendSettings.restorePaletteOnUnload)
        {
            for (i = pulseBlendPalette->pulseBlendSettings.paletteOffset; i < pulseBlendPalette->pulseBlendSettings.paletteOffset + pulseBlendPalette->pulseBlendSettings.numColors; i++)
                gPlttBufferFaded[i] = gPlttBufferUnfaded[i];
        }

        pulseBlendPalette->available = 1;
        pulseBlend->usedPulseBlendPalettes &= ~(1 << j);
    }
}

void UpdatePulseBlend(struct PulseBlend *pulseBlend)
{
    struct PulseBlendPalette *pulseBlendPalette;
    m8 i = 0;

    if (pulseBlend->usedPulseBlendPalettes)
    {
        for (i = 0; i < 16; i++)
        {
            pulseBlendPalette = &pulseBlend->pulseBlendPalettes[i];
            if ((!pulseBlendPalette->available && pulseBlendPalette->inUse) && (!gPaletteFade.active || !pulseBlendPalette->pulseBlendSettings.unk7_7))
            {
                if (pulseBlendPalette->delayCounter-- == 0)
                {
                    pulseBlendPalette->delayCounter = pulseBlendPalette->pulseBlendSettings.delay;
                    BlendPalette(pulseBlendPalette->pulseBlendSettings.paletteOffset, pulseBlendPalette->pulseBlendSettings.numColors, pulseBlendPalette->blendCoeff, pulseBlendPalette->pulseBlendSettings.blendColor);
                    switch (pulseBlendPalette->pulseBlendSettings.fadeType)
                    {
                    case 0: // Fade all the way to the max blend amount, then wrap around
                        // BUG: This comparison will never be true for maxBlendCoeff values that are >= 8. This is because
                        // maxBlendCoeff is a signed 4-bit field, but blendCoeff is an unsigned 4-bit field. This code is never
                        // reached, anyway, so the bug is not observable in vanilla gameplay.
#if !UBFIX
                        if (pulseBlendPalette->blendCoeff++ == pulseBlendPalette->pulseBlendSettings.maxBlendCoeff)
                        {
                            pulseBlendPalette->fadeCycleCounter++;
                            pulseBlendPalette->blendCoeff = 0;
                        }
#else
                        if (pulseBlendPalette->blendCoeff == (u8)pulseBlendPalette->pulseBlendSettings.maxBlendCoeff)
                        {
                            pulseBlendPalette->fadeCycleCounter++;
                            pulseBlendPalette->blendCoeff = 0;
                        }
                        else
                            pulseBlendPalette->blendCoeff++;
#endif
                        break;
                    case 1: // Fade in and out
                        if (pulseBlendPalette->fadeDirection)
                        {
                            if (pulseBlendPalette->blendCoeff-- == 1)
                            {
                                pulseBlendPalette->fadeCycleCounter++;
                                #if !MODERN
                                pulseBlendPalette->fadeDirection ^= 1;
                                #else
                                pulseBlendPalette->fadeDirection = FALSE;
                                #endif
                            }
                        }
                        else
                        {
                            u8 max = (u8)((pulseBlendPalette->pulseBlendSettings.maxBlendCoeff - 1) & 0xF);
                            if (pulseBlendPalette->blendCoeff++ == max)
                            {
                                pulseBlendPalette->fadeCycleCounter++;
                                #if !MODERN
                                pulseBlendPalette->fadeDirection ^= 1;
                                #else
                                pulseBlendPalette->fadeDirection = FALSE;
                                #endif
                            }
                        }
                        break;
                    case (MODERN ? -2 : 2): // Flip back and forth
                        // This code is never reached in vanilla
                        if (pulseBlendPalette->fadeDirection)
                            pulseBlendPalette->blendCoeff = 0;
                        else
                            pulseBlendPalette->blendCoeff = pulseBlendPalette->pulseBlendSettings.maxBlendCoeff & 0xF;

                        pulseBlendPalette->fadeDirection ^= 1;
                        pulseBlendPalette->fadeCycleCounter++;
                        break;
                    }

                    if (pulseBlendPalette->pulseBlendSettings.numFadeCycles != 0xFF
                     && pulseBlendPalette->fadeCycleCounter == pulseBlendPalette->pulseBlendSettings.numFadeCycles)
                        UnmarkUsedPulseBlendPalettes(pulseBlend, pulseBlendPalette->paletteSelector, FALSE);
                }
            }
        }
    }
}

// Below used for the Roulette grid
void FillTilemapRect(u16 *dest, u16 value, u8 left, u8 top, u8 width, u8 height)
{
    #if !MODERN
    u16 *_dest;
    u8 i;
    u8 j;
    i = 0;
    dest += top * 32 + left;
    for (; i < height; i++)
    #else
    u16 *_dest;
    u32 i, j;
    for (i = 0; i < height; i++)
    #endif
    {
        _dest = dest + i * 32;
        for (j = 0; j < width; j++)
            *_dest++ = value;
    }
}

#if !MODERN
void SetTilemapRect(u16 *dest, const u16 *src, u8 left, u8 top, u8 width, u8 height)
{
    u16 *_dest;
    const u16 *_src = src;
    u8 i;
    u8 j;
    i = 0;
    dest += top * 32 + left;
    for (; i < height; i++)
    {
        _dest = dest + i * 32;
        for (j = 0; j < width; j++)
            *_dest++ = *_src++;
    }
}
#else
void SetTilemapRect(u16 *dest, const u16 *src, u8 left, u8 top, u8 width, u8 height)
{
    u16 *_dest;
    u32 i, j;
    dest += top * 32 + left;
    for (i = 0; i < height; i++)
    {
        _dest = dest + i * 32;
        for (j = 0; j < width; j++)
            *_dest++ = *src++;
    }
}
#endif
static void FillTilemapRect_Unused(void *dest, u16 value, u8 left, u8 top, u8 width, u8 height)
{
    u8 i, j;
    u8 x, y;

    for (i = 0, y = top; i < height; i++)
    {
        for (x = left, j = 0; j < width; j++)
        {
            *(u16 *)((dest) + (y * 64 + x * 2)) = value;
            x = (x + 1) % 32;
        }
        y = (y + 1) % 32;
    }
}

static void SetTilemapRect_Unused(void *dest, const u16 *src, u8 left, u8 top, u8 width, u8 height)
{
    u8 i, j;
    u8 x, y;
    const u16 *_src;

    for (i = 0, _src = src, y = top; i < height; i++)
    {
        for (x = left, j = 0; j < width; j++)
        {
            *(u16 *)((dest) + (y * 64 + x * 2)) = *(_src++);
            x = (x + 1) % 32;
        }
        y = (y + 1) % 32;
    }
}
