#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <Arduino.h>
#include <TFT_eSPI.h>

class Graph
{
public:
    Graph(uint16_t _height, uint16_t _width);
    ~Graph();

    void AddPoint(float val);
    void Reset();
    void Draw(TFT_eSprite &spr);

    uint16_t MaxDotCount() { return width - 1; }

private:
    float *dots;
    float minValue;
    float maxValue;
    uint16_t idx = 0;
    uint16_t height;
    uint16_t width;
    bool autoscale = true;
    bool autoShiftToLeft = true;
    bool overflown = false;

    void shiftGraphToLeft();
    void resetSprite(TFT_eSprite &spr);

    int scaleBetween(float value, float vMin, float vMax, int rMin, int rMax)
    {
        //if (sMax - sMin == 0) return this.map(num = > (tMin + tMax) / 2);
        if (vMax - vMin == 0)
            return (rMin + rMax) / 2;

        //return this.map(num = > (tMax - tMin) * (num - sMin) / (sMax - sMin) + tMin);
        return (rMax - rMin) * (value - vMin) / (vMax - vMin) + rMin;
    }
};

#endif /* __GRAPH_H__ */
