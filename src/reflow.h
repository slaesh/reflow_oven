#ifndef __REFLOW_H__
#define __REFLOW_H__

#include <Arduino.h>

typedef enum
{
    TEMPERATURE_RAMP_IS_COOLING = 0,
    TEMPERATURE_RAMP_IS_HEATING = 1
} t_e_temperatureRampState;

typedef struct
{
    float minTemp;
    float maxTemp;
    uint16_t minSeconds;
    uint16_t maxSeconds;
    t_e_temperatureRampState heatOrCool;
} t_s_tempertatureRamp;

typedef union {
    t_s_tempertatureRamp ramps[4];
    struct
    {
        t_s_tempertatureRamp preHeat;
        t_s_tempertatureRamp soak;
        t_s_tempertatureRamp reflow;
        t_s_tempertatureRamp cooling;
    } ramp;
} t_u_reflowProfile;

typedef enum
{
    REFLOW_STATE_PRE_HEAT = 0,
    REFLOW_STATE_SOAK,
    REFLOW_STATE_REFLOW,
    REFLOW_STATE_COOLING,
    REFLOW_STATE_IDLE,

    REFLOW_STATE_COUNT
} t_e_reflowState;

typedef struct
{
    /* hold's the current state of our reflow process */
    t_e_reflowState state = REFLOW_STATE_IDLE;

    /* pointer to the used reflow-profile */
    t_u_reflowProfile *profile = NULL;

    // TODO: add fields for temperatureReachedAt and separate started and reached .. !

    /* our starting time, hold in a union to easily access them via arr or named.. */
    union {
        unsigned long startedAtArr[REFLOW_STATE_COUNT - 1 /* remove IDLE .. */];
        struct
        {
            // we need to take care of our processor's endian !
#if true
            unsigned long preHeat;
            unsigned long soak;
            unsigned long reflow;
            unsigned long cooling;
#else
            unsigned long cooling;
            unsigned long reflow;
            unsigned long soak;
            unsigned long preHeat;
#endif
        } startedAt;
    };

    /* PWM duty-cycle to be outputed */
    uint8_t dutyCycle = 0;

    /* current temperature measured */
    float currentTemperature;

    /* temperature of last loop */
    float lastTemperature;
} t_s_reflowStatus;

extern t_s_reflowStatus reflow;

#endif /* __REFLOW_H__ */
