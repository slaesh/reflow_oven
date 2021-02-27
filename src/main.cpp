// Set delay after plotting the sprite
#define DELAY 1000

#include "reflow.h"
#include "graph.h"

// Width and height of sprite
#define WIDTH 200
#define HEIGHT 100

Graph graph(HEIGHT, WIDTH);

#include <TFT_eSPI.h> // Include the graphics library (this includes the sprite functions)

TFT_eSPI tft = TFT_eSPI();           // Declare object "tft"
TFT_eSprite spr = TFT_eSprite(&tft); // Declare Sprite object "spr" with pointer to "tft" object

#define BACKGROUND_COLOR (TFT_BLACK)

void drawGraph()
{
   graph.Draw(spr);
   spr.pushSprite(20, 20);
}

#include "InterpolationLib.h"

void testInterpolation()
{
   graph.Reset();

   const int numValues = 7;
   double xValues[] = {0, 30, 90, 150, 180, 210, 270};
   double yValues[] = {25, 100, 150, 180, 220, 220, 25};

   for (int idx = 0; idx < graph.MaxDotCount(); idx++)
   {
      //Serial.print(Interpolation::SmoothStep(xValues, yValues, numValues, xValue));
      //Serial.print(',');
      //Serial.print(Interpolation::CatmullSpline(xValues, yValues, numValues, xValue));
      //Serial.print(',');
      //Serial.println(Interpolation::ConstrainedSpline(xValues, yValues, numValues, xValue));

      //createNewDot(Interpolation::CatmullSpline(xValues, yValues, numValues, (int)(idx * 1.5)));
      //graphDots[curGraphIdx++] = Interpolation::CatmullSpline(xValues, yValues, numValues, (int)(idx * 1.5));
      graph.AddPoint(Interpolation::CatmullSpline(xValues, yValues, numValues, (int)(idx * 1.5)));
   }

   drawGraph();
}

void setup()
{
   Serial.begin(230400);
   Serial.println("STARTED!");

   randomSeed(analogRead(0));

   tft.init();
   tft.fillScreen(BACKGROUND_COLOR);
   tft.setRotation(1);

   delay(500);

   spr.setColorDepth(4);
   spr.createSprite(WIDTH, HEIGHT);

   drawGraph();

   testInterpolation();
   delay(8000);

   graph.Reset();

   extern void ReflowSetup();
   ReflowSetup();

   pinMode(0, INPUT_PULLUP);
   pinMode(35, INPUT_PULLUP);
}

void loop(void)
{
   extern void ReflowLoop();
   ReflowLoop();

   // set a dot !
   static unsigned long setNextDot = 0;
   if ((setNextDot == 0 || setNextDot < millis()) && !isnan(reflow.currentTemperature))
   {
      graph.AddPoint(reflow.currentTemperature);
      drawGraph();

#define DRAW_A_DOT_PERIOD_IN_MS (2500)
      setNextDot = millis() + DRAW_A_DOT_PERIOD_IN_MS;
   }

   extern bool IsRunning();
   if (!digitalRead(0) && IsRunning())
   {
      extern void StopReflow();
      StopReflow();
   }

   if (!digitalRead(35) && !IsRunning())
   {
      graph.Reset();

      extern void StartReflow();
      StartReflow();
   }

   delay(250);
}