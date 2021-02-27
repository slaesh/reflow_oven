#include "reflow.h"

t_e_reflowState &operator+=(t_e_reflowState &f, int value)
{
    int val = (int)f;
    val = (val + value) % REFLOW_STATE_COUNT;

    return f = (t_e_reflowState)val;
}

t_e_reflowState &operator++(t_e_reflowState &f)
{
    return f += 1;
}

String getString(t_e_reflowState state)
{
    switch (state)
    {
    case REFLOW_STATE_IDLE:
        return "IDLE";

    case REFLOW_STATE_PRE_HEAT:
        return "PRE_HEAT";

    case REFLOW_STATE_SOAK:
        return "SOAK";

    case REFLOW_STATE_REFLOW:
        return "REFLOW";

    case REFLOW_STATE_COOLING:
        return "COOLING";

    default:
        return "UNKNOWN";
    }
}

#include <max6675.h>
int thermoDO = 15;
int thermoCSfast = 13;
int thermoCLK = 12;
int thermoCSslow = 2;

MAX6675 thermocoupleFast;
MAX6675 thermocoupleSlow;

t_u_reflowProfile testProfile = {
    .ramp = {
        .preHeat = {
            .minTemp = 100,
            .maxTemp = 150,
            .minSeconds = 60,
            .maxSeconds = 90,
            .heatOrCool = TEMPERATURE_RAMP_IS_HEATING,
        },
        .soak = {150, 200, 60, 120, TEMPERATURE_RAMP_IS_HEATING},
        .reflow = {220, 240, /* 30 */ 40, 60, TEMPERATURE_RAMP_IS_HEATING},
        .cooling = {40, -1, 60, 120, TEMPERATURE_RAMP_IS_COOLING},
    },
};

t_s_reflowStatus reflow;

void ReflowSetup()
{
    ledcAttachPin(21, 0);
    ledcSetup(0, 4000, 8);
    ledcWrite(0, 0);

    thermocoupleFast.begin(thermoCLK, thermoCSfast, thermoDO);
    thermocoupleFast.setOffsetCelsius(-3);

    thermocoupleSlow.begin(thermoCLK, thermoCSslow, thermoDO);
}

double readAndRetry()
{
    double temp = thermocoupleFast.readCelsius();

    uint8_t retryCnt = 3;
    while (isnan(temp) && retryCnt-- > 0)
    {
        Serial.printf("retry! %04x\r\n", thermocoupleFast.getLastValue());
        temp = thermocoupleFast.readCelsius();
    }

    if (isnan(temp))
    {
        temp = -100;
    }

    return temp;
}

double readTemperature()
{
#if false
    double temp = NAN;

#define READ_X_TIMES (4)
    double values[READ_X_TIMES] = {0};
    for (uint8_t i = 0; i < READ_X_TIMES; ++i)
    {
        values[i] = readTemperature();
    }

    return temp;
#else
    return readAndRetry();
#endif
}

uint16_t getSecondsTakenAlready(unsigned long base)
{
    return (millis() - base) / 1000;
}

void resetReflowState()
{
    reflow.state = REFLOW_STATE_IDLE;

    /* reset times for all states .. */
    for (int idx = 0; idx < (REFLOW_STATE_COUNT - 1); ++idx)
    {
        reflow.startedAtArr[idx] = 0;
    }

    /* reflow.startedAt.preHeat = 0;
    reflow.startedAt.soak = 0;
    reflow.startedAt.reflow = 0;
    reflow.startedAt.cooling = 0; */

    reflow.dutyCycle = 0;

    reflow.currentTemperature = NAN;
    reflow.lastTemperature = NAN;
}

void StartReflow()
{
    resetReflowState();

    reflow.state = REFLOW_STATE_PRE_HEAT;
    reflow.startedAt.preHeat = millis();

    Serial.printf("Starting the reflow process!\r\n");
}

bool IsRunning()
{
    return reflow.startedAt.preHeat != 0 && reflow.state != REFLOW_STATE_IDLE;
}

void StopReflow()
{
    Serial.printf("STOPPING the reflow process!\r\n");

    resetReflowState();

    ledcWrite(0, 0);
}

#define REFLOW_LOOP_PERIOD_IN_MS (1000)

void ReflowLoop()
{
    static unsigned long nextLoop = 0;
    if (nextLoop != 0 && nextLoop > millis())
    {
        return;
    }
    nextLoop = millis() + REFLOW_LOOP_PERIOD_IN_MS;

    reflow.currentTemperature = readTemperature();
    float slowTemp = thermocoupleSlow.readCelsius();
    if (isnan(slowTemp))
    {
        slowTemp = reflow.currentTemperature;
    }
    float normalizedTemp = ((reflow.currentTemperature * 4) + slowTemp) / 5;

    float tempPerSec = NAN;
    if (!isnan(reflow.currentTemperature) && !isnan(reflow.lastTemperature))
    {
        tempPerSec = reflow.currentTemperature - reflow.lastTemperature;
    }

    Serial.printf("%s ; %03.1f ; %03.1f ; %03.1f >>> %+02.1f°/s\r\n", getString(reflow.state), reflow.currentTemperature, slowTemp, normalizedTemp, tempPerSec);

    t_s_tempertatureRamp *curRamp = &testProfile.ramps[reflow.state];

    float curStateAllowedRange = curRamp->maxTemp - curRamp->minTemp;  // size of our range to be within in this ramp..
    float curTempGoal = curRamp->minTemp + (curStateAllowedRange / 2); // lets aim to be right in the middle!
    float curThreshold = curStateAllowedRange / 4;
    float curDelta = curTempGoal - reflow.currentTemperature;

    if (reflow.state == REFLOW_STATE_IDLE)
    {
        if (reflow.currentTemperature < 60)
        {
            reflow.dutyCycle = 0;
            goto LEAVE; // will also set the last-temp and writes out our duty-cycle..
        }
        else
        {
            reflow.state = REFLOW_STATE_COOLING;
        }
    }

    switch (reflow.state)
    {
    case REFLOW_STATE_PRE_HEAT:
    case REFLOW_STATE_SOAK:
    case REFLOW_STATE_REFLOW:
    {
        if (!isnan(reflow.startedAtArr[reflow.state]) && reflow.startedAtArr[reflow.state] != 0 && getSecondsTakenAlready(reflow.startedAtArr[reflow.state]) >= curRamp->minSeconds)
        {
            Serial.println();
            Serial.printf(">> switch to next state..\r\n");
            Serial.printf("   %s => %s\r\n", getString(reflow.state), getString((t_e_reflowState)((int)reflow.state + 1)));
            Serial.println();

            reflow.state += 1;
            break;
        }

        if (reflow.currentTemperature < curRamp->minTemp)
        {
            if (!isnan(reflow.startedAtArr[reflow.state]) && reflow.startedAtArr[reflow.state] != 0)
            {
                Serial.println();
                Serial.printf("clear timer..\r\n");
                Serial.println();
            }

            reflow.startedAtArr[reflow.state] = 0; // reset our timer.. time should be counted if we are in the correct temp-range!
        }
        else if (isnan(reflow.startedAtArr[reflow.state]) || reflow.startedAtArr[reflow.state] == 0)
        {
            reflow.startedAtArr[reflow.state] = millis();

            Serial.println();
            Serial.printf("start timer..\r\n");
            Serial.println();
        }

        if (abs(curDelta) <= curThreshold)
        {
#define THRESHOLD_DUTY_CYCLE (30)

            if (reflow.dutyCycle != THRESHOLD_DUTY_CYCLE)
            {
                Serial.printf("%03.1f is within threshold of %03.1f (%03.1f)\r\n", reflow.currentTemperature, curThreshold, curTempGoal);
            }

            reflow.dutyCycle = THRESHOLD_DUTY_CYCLE; // seems like 30 is a good value to hold the actual temperature.. (in an empty oven-state)

            break; // nothing to be done yet ..
        }

        // current temperature is too high!
        if (curDelta < 0)
        {
            if (reflow.dutyCycle != 0)
            {
                Serial.printf("temp(%03.1f) too high(%03.1f) -> switch off!\r\n", reflow.currentTemperature, curDelta);
            }

            reflow.dutyCycle = 0;
        }
        // current temperature is too low!
        else
        {
            if (reflow.dutyCycle != 254)
            {
                Serial.printf("temp(%03.1f) too low(%03.1f) -> switch on!\r\n", reflow.currentTemperature, curDelta);
            }

            reflow.dutyCycle = 254;
        }

        break;
    }

    case REFLOW_STATE_COOLING:
    {
        reflow.dutyCycle = 0;

        if (reflow.currentTemperature < curRamp->minTemp)
        {
            reflow.state = REFLOW_STATE_IDLE;
            break;
        }

#define CALCULATE_COOLING_SPEED_WITHIN_THE_LAST_X_SECONDS (3)

        static uint8_t idx = 0;
        static float deltas[CALCULATE_COOLING_SPEED_WITHIN_THE_LAST_X_SECONDS] = {0};
        static uint8_t items = 0;
        static double coolingStartedAtTemperature = 0;

        if (isnan(reflow.startedAtArr[reflow.state]) || reflow.startedAtArr[reflow.state] == 0)
        {
            reflow.startedAtArr[reflow.state] = millis();
            coolingStartedAtTemperature = reflow.currentTemperature;

            Serial.printf("just waiting for now.. maybe power on the fan?!\r\n");

            idx = items = 0;
        }

        if (!isnan(reflow.lastTemperature) && reflow.lastTemperature != 0)
        {
            deltas[idx++] = reflow.lastTemperature - reflow.currentTemperature;
            items = idx;
            if (idx >= CALCULATE_COOLING_SPEED_WITHIN_THE_LAST_X_SECONDS)
            {
                idx = 0;
                items = CALCULATE_COOLING_SPEED_WITHIN_THE_LAST_X_SECONDS;
            }
        }

        double degreeCelsiusPerSec = 0;

        if (items != 0)
        {
            for (int i = 0; i < items; ++i)
            {
                degreeCelsiusPerSec += deltas[i];
            }

            degreeCelsiusPerSec /= items;
        }

        double tempDelta = coolingStartedAtTemperature - reflow.currentTemperature;
        double overallDegreeCelsiusPerSec = tempDelta / getSecondsTakenAlready(reflow.startedAtArr[reflow.state]);
        //Serial.printf("%02.1f (%02.1f) .. %03.1f - %03.1f .. %03.1f\r\n", degreeCelsiusPerSec, overallDegreeCelsiusPerSec, coolingStartedAtTemperature, tempDelta, reflow.lastTemperature);

        break;
    }

    default:
        break;
    }

LEAVE:

    reflow.lastTemperature = reflow.currentTemperature;
    ledcWrite(0, reflow.dutyCycle);
}

#if false

void MatchTemp()
{
    float duty = 0;
    float wantedTemp = 0;
    float wantedDiff = 0;
    float tempDiff = 0;
    float perc = 0;

    // if we are still before the main flow cut-off time (last peak)
    if (timeX < CurrentGraph().offTime)
    {
        // We are looking XXX steps ahead of the ideal graph to compensate for slow movement of oven temp
        if (timeX < CurrentGraph().reflowGraphX[2])
            wantedTemp = CurrentGraph().wantedCurve[(int)timeX + set.lookAheadWarm];
        else
            wantedTemp = CurrentGraph().wantedCurve[(int)timeX + set.lookAhead];

        wantedDiff = (wantedTemp - lastWantedTemp);
        lastWantedTemp = wantedTemp;

        tempDiff = (currentTemp - lastTemp);
        lastTemp = currentTemp;

        perc = wantedDiff - tempDiff;

#ifdef DEBUG
        Serial.print("T: ");
        Serial.print(timeX);

        Serial.print("  Current: ");
        Serial.print(currentTemp);

        Serial.print("  Wanted: ");
        Serial.print(wantedTemp);

        Serial.print("  T Diff: ");
        Serial.print(tempDiff);

        Serial.print("  W Diff: ");
        Serial.print(wantedDiff);

        Serial.print("  Perc: ");
        Serial.print(perc);
#endif

        isCuttoff = false;

        // have to passed the fan turn on time?
        if (!isFanOn && timeX >= CurrentGraph().fanTime)
        {
#ifdef DEBUG
            Serial.print("COOLDOWN: ");
#endif
            // If we are usng the fan, turn it on
            if (set.useFan)
            {
                isFanOn = true;
                DrawHeading("COOLDOWN!", GREEN, BLACK);
                Buzzer(2000, 2000);

                StartFan(true);
            }
            else // otherwise YELL at the user to open the oven door
            {
                if (buzzerCount > 0)
                {
                    DrawHeading("OPEN OVEN", RED, BLACK);
                    Buzzer(2000, 2000);
                    buzzerCount--;
                }
            }
        }
    }
    else
    {
        // YELL at the user to open the oven door
        if (!isCuttoff && set.useFan)
        {
            DrawHeading("OPEN OVEN", GREEN, BLACK);
            Buzzer(2000, 2000);

#ifdef DEBUG
            Serial.print("CUTOFF: ");
#endif
        }

        //    // If we dont keep the fan on after reflow, turn it all off
        //    if ( keepFanOnTime == 0 )
        //    {
        //      StartFan ( false );
        //      isFanOn = false;
        //    }

        isCuttoff = true;
    }

    currentDetla = (wantedTemp - currentTemp);

#ifdef DEBUG
    Serial.print("  Delta: ");
    Serial.print(currentDetla);
#endif

    float base = 128;

    if (currentDetla >= 0)
    {
        base = 128 + (currentDetla * 5);
    }
    else if (currentDetla < 0)
    {
        base = 32 + (currentDetla * 15);
    }

    base = constrain(base, 0, 256);

#ifdef DEBUG
    Serial.print("  Base: ");
    Serial.print(base);
    Serial.print(" -> ");
#endif

    duty = base + (172 * perc);
    duty = constrain(duty, 0, 256);

    // override for full blast at start only if the current Temp is less than the wanted Temp, and it's in the ram before pre-soak starts.
    if (set.startFullBlast && timeX < CurrentGraph().reflowGraphX[1] && currentTemp < wantedTemp)
        duty = 256;

    currentPlotColor = GREEN;

    SetRelayFrequency(duty);
}

#endif