#include "graph.h"

Graph::Graph(uint16_t _height, uint16_t _width)
{
    height = _height;
    width = _width;

    dots = new float[width - 1];

    Reset();
}

Graph::~Graph()
{
    delete[] dots;
}

void Graph::shiftGraphToLeft()
{
    // skip 0, start with 1!
    for (uint16_t dot = 1; dot < MaxDotCount(); ++dot)
    {
        dots[dot - 1] = dots[dot];
    }
}

void Graph::AddPoint(float val)
{
    dots[idx] = val;
    //Serial.printf("new dot(%03d/%03d): %03.1f\r\n", idx, MaxDotCount(), dots[idx]);

    if (!autoShiftToLeft)
    {
        idx = (idx + 1) % MaxDotCount();
        if (idx == 0)
        {
            overflown = true;
        }
    }
    else
    {
        idx += 1;
        if (idx >= MaxDotCount())
        {
            shiftGraphToLeft();
            idx -= 1;
        }
    }
}

void Graph::Reset()
{
    idx = 0;
    overflown = false;

    minValue = infinity();
    maxValue = infinity();
}

void Graph::resetSprite(TFT_eSprite &spr)
{
    uint16_t cmap[16];

    cmap[0] = TFT_BLACK; // We will keep this as black
    cmap[1] = TFT_NAVY;
    cmap[2] = TFT_DARKGREEN;
    cmap[3] = TFT_DARKCYAN;
    cmap[4] = TFT_MAROON;
    cmap[5] = TFT_PURPLE;
    cmap[6] = TFT_PINK;
    cmap[7] = TFT_LIGHTGREY;
    cmap[8] = TFT_YELLOW;
    cmap[9] = TFT_BLUE;
    cmap[10] = TFT_GREEN;
    cmap[11] = TFT_CYAN;
    cmap[12] = TFT_RED;
    cmap[13] = TFT_MAGENTA;
    cmap[14] = TFT_WHITE; // Keep as white for text
    cmap[15] = TFT_BLUE;  // Keep as blue for sprite border

    spr.createPalette(cmap);

    spr.fillSprite(0);
    spr.drawLine(0, height - 1, width, height - 1, 14);
    spr.drawLine(0, 0, 0, height, 14);

#define GRAPH_FONT (1)

    //spr.setTextDatum(MC_DATUM);
    spr.setTextColor(14); // White text

    if (maxValue != infinity())
    {
        spr.drawString(String(maxValue, 1), 2, 0, GRAPH_FONT);
    }

    if (minValue != infinity())
    {
        spr.drawString(String(minValue, 1), 2, height - 1 - spr.fontHeight(GRAPH_FONT), GRAPH_FONT);
    }
}

void Graph::Draw(TFT_eSprite &spr)
{
    float min = infinity();
    float max = infinity();

    uint16_t graphItemCnt = overflown ? MaxDotCount() : idx;

    for (uint16_t idx = 0; idx < graphItemCnt; ++idx)
    {
        auto curItem = dots[idx];

        if (curItem < min || min == infinity())
            min = curItem;
        if (curItem > max || max == infinity())
            max = curItem;
    }

    minValue = min;
    maxValue = max;

    if (min == max)
    {
        if (max < 100)
            max = 100;
        if (min > 0)
            min = 0;
    }

    if (max > 0)
    {
        max *= 1.1;
    }
    else
    {
        max *= .9;
    }

    if (min < 0)
    {
        min *= 1.1;
    }
    else
    {
        min *= .9;
    }

    resetSprite(spr);

#define DRAW_LINES_INSTEAD_OF_DOTS (true)

#if DRAW_LINES_INSTEAD_OF_DOTS == false
    for (uint16_t idx = 0; idx < graphItemCnt; ++idx)
    {
        float curItem = dots[idx];
        int scaled = scaleBetween(curItem, min, max, 1 /* 0 --> bottom-line */, height);

        spr.drawPixel(idx + 1, height - 1 - scaled, 12);
    }
#else
    for (uint16_t idx = 1; idx < graphItemCnt; ++idx)
    {
        float prevItem = dots[idx - 1];
        float curItem = dots[idx];
        int prevScaled = scaleBetween(prevItem, min, max, 1 /* 0 --> bottom-line */, height);
        int curScaled = scaleBetween(curItem, min, max, 1 /* 0 --> bottom-line */, height);

        spr.drawLine(idx, height - 1 - prevScaled, idx + 1, height - 1 - curScaled, 12);
    }
#endif
}
