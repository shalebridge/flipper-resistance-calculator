#include "flipper.h"
#include "resistors_app.h"
#include "resistor_logic.h"
#include <math.h>

const uint8_t CHARS_NUMERIC = 3;
const uint8_t CHARS_MULTIPLIER = 7;
const uint8_t CHARS_TOLERANCE = 7;
const uint8_t CHARS_TEMP_COEFF = 3;
const uint8_t CHARS_CALCULATION = 12;

const char BLANK_CALCULATION[] = "        "; // "nnn Xohm"
const char BLANK_TOLERANCE[] = "       ";
const char BLANK_TEMP_COEFF[] = "   ";

const uint8_t INDEX_NUMERIC = 0;
const uint8_t INDEX_MULTIPLIER = 4;
const uint8_t INDEX_TOLERANCE = 0;

const int NUMERIC_BANDS_PER_RESISTOR[6] = {-1, -1, 2, 2, 3, 3};
const int MULTIPLIER_INDEX_PER_RESISTOR[6] = {-1, -1, 2, 2, 3, 3};
const int TOLERANCE_INDEX_PER_RESISTOR[6] = {-1, -1, -1, 3, 4, 4};
const int TEMP_COEFF_INDEX_PER_RESISTOR[6] = {-1, -1, -1, -1, -1, 5};

bool has_tolerance(ResistorType rtype) {
    return TOLERANCE_INDEX_PER_RESISTOR[rtype - 1] > -1;
}

bool has_temp_coeff(ResistorType rtype) {
    return TEMP_COEFF_INDEX_PER_RESISTOR[rtype - 1] > -1;
}

bool is_numeric_band(ResistorType rtype, int index) {
    return index < NUMERIC_BANDS_PER_RESISTOR[rtype - 1];
}

bool is_multiplier_band(ResistorType rtype, int index) {
    return index == MULTIPLIER_INDEX_PER_RESISTOR[rtype - 1];
}

bool is_tolerance_band(ResistorType rtype, int index) {
    return index == TOLERANCE_INDEX_PER_RESISTOR[rtype - 1];
}

bool is_temp_coefficient_band(ResistorType rtype, int index) {
    return index == TEMP_COEFF_INDEX_PER_RESISTOR[rtype - 1];
}

bool is_numeric_colour(BandColour colour) {
    return colour <= 9;
}

bool is_multiplier_colour(BandColour colour) {
    UNUSED(colour);
    return true;
}

bool is_tolerance_colour(BandColour colour) {
    return colour == BandBrown || colour == BandRed || colour == BandOrange ||
           colour == BandYellow || colour == BandGreen || colour == BandBlue ||
           colour == BandPurple || colour == BandGray || colour == BandGold ||
           colour == BandSilver;
}

bool is_temp_coeff_colour(BandColour colour) {
    return colour == BandBlack || colour == BandBrown || colour == BandRed ||
           colour == BandOrange || colour == BandYellow || colour == BandGreen ||
           colour == BandBlue || colour == BandPurple || colour == BandGray;
}

BandColour alter_resistor_band(
    ResistorType rtype,
    uint8_t band,
    BandColour current_colour,
    int8_t direction) {
    int8_t colour = current_colour;
    bool accepted = false;
    while(!accepted) {
        colour += direction;
        if(colour > 12) colour = 0;
        if(colour < 0) colour = 12;
        if(is_numeric_band(rtype, band) && is_numeric_colour(colour)) accepted = true;
        if(is_multiplier_band(rtype, band) && is_multiplier_colour(colour)) accepted = true;
        if(is_tolerance_band(rtype, band) && is_tolerance_colour(colour)) accepted = true;
        if(is_temp_coefficient_band(rtype, band) && is_temp_coeff_colour(colour)) accepted = true;
    }
    return colour;
}

double get_resistance_decimal(ResistorType rtype, BandColour colour) {
    switch(colour) {
    case BandBlue:
    case BandOrange:
    case BandBlack:
    case BandWhite:
        return 1;
    case BandBrown:
    case BandYellow:
    case BandPurple:
        return (rtype == R5 || rtype == R6) ? 0.01 : 10;
    case BandGray:
    case BandGreen:
    case BandRed:
    case BandGold:
        return 0.1;
    case BandSilver:
        return 0.01;
    case BandPink:
        return 0.001;
    default:
        return 0;
    }
}

double decode_resistance_number(ResistorType rtype, BandColour colours[]) {
    uint8_t bands = NUMERIC_BANDS_PER_RESISTOR[rtype - 1];
    uint8_t multiplier_index = MULTIPLIER_INDEX_PER_RESISTOR[rtype - 1];
    double decimal = get_resistance_decimal(rtype, colours[multiplier_index]);
    int value = 0;
    for(uint_fast8_t b = 0; b < bands; b++) {
        int pwr = bands - b - 1;
        int delta = ((int)pow(10.0, pwr)) * colours[b];
        value += delta;
    }

    return value * decimal;
}

char* calculate_decimal_places(double value) {
    static char formatter[] = "%.0f";
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.6g", value);
    const char* dec = strchr(buffer, '.');

    if(dec == NULL || value == 0) return formatter;

    // Count the characters after the decimal point
    const char* end = buffer + strlen(buffer);
    while(end > dec && *(end - 1) == '0') // Skip trailing zeros
        end--;

    formatter[2] = (uint8_t)(end - dec - 1) + '0';
    return formatter;
}

void update_resistance_number(ResistorType rtype, BandColour colours[], char string[]) {
    double value = decode_resistance_number(rtype, colours);
    char* formatter = calculate_decimal_places(value);
    uint8_t length = snprintf(NULL, 0, formatter, value);
    char* str = malloc(length + 1);
    snprintf(str, length + 1, formatter, value);
    char* target = string + INDEX_NUMERIC;
    strncpy(target, str, length);
    free(str);
}

char* decode_resistance_multiplier(ResistorType rtype, BandColour colour) {
    static char unit[] = " ohm";

    switch(colour) {
    case BandBrown:
        if(rtype == R5 || rtype == R6) {
            unit[0] = 'K';
            break;
        }
        /* fallthrough */
    case BandBlack:
    case BandGold:
    case BandSilver:
    case BandPink:
        unit[0] = ' ';
        break;
    case BandYellow:
        if(rtype == R5 || rtype == R6) {
            unit[0] = 'M';
            break;
        }
        /* fallthrough */
    case BandRed:
    case BandOrange:
        unit[0] = 'K';
        break;
    case BandPurple:
        if(rtype == R5 || rtype == R6) {
            unit[0] = 'G';
            break;
        }
        /* fallthrough */
    case BandGreen:
    case BandBlue:
        unit[0] = 'M';
        break;
    case BandGray:
    case BandWhite:
        unit[0] = 'G';
        break;
    }

    return unit;
}

void update_resistance_multiplier(ResistorType rtype, BandColour colours[], char string[]) {
    uint8_t multiplier_index = MULTIPLIER_INDEX_PER_RESISTOR[rtype - 1];
    char* unit = decode_resistance_multiplier(rtype, colours[multiplier_index]);
    char* target = string + INDEX_MULTIPLIER;
    strncpy(target, unit, CHARS_MULTIPLIER);
}

char* decode_resistance_tolerance(BandColour colour) {
    switch(colour) {
    case BandBrown:
        return "1%";
    case BandRed:
        return "2%";
    case BandOrange:
        return "3%";
    case BandYellow:
        return "4%";
    case BandGreen:
        return "0.5%";
    case BandBlue:
        return "0.25%";
    case BandPurple:
        return "0.1%";
    case BandGray:
        return "0.05%";
    case BandGold:
        return "5%";
    case BandSilver:
        return "10%";
    default:
        return "--";
    }
}

void update_resistance_calculation(ResistorType rtype, BandColour bands[], char* string) {
    strcpy(string, BLANK_CALCULATION);
    update_resistance_multiplier(rtype, bands, string);
    update_resistance_number(rtype, bands, string);
}

void update_resistance_tolerance(ResistorType rtype, BandColour colours[], char string[]) {
    strcpy(string, BLANK_TOLERANCE);
    int tolerance_index = TOLERANCE_INDEX_PER_RESISTOR[rtype - 1];
    char* unit = decode_resistance_tolerance(colours[tolerance_index]);
    char* target = string + INDEX_TOLERANCE;
    strncpy(target, unit, CHARS_TOLERANCE);
}

char* decode_resistance_temp_coeff(BandColour colour) {
    switch(colour) {
    case BandBlack:
        return "250";
    case BandBrown:
        return "100";
    case BandRed:
        return "50";
    case BandOrange:
        return "15";
    case BandYellow:
        return "25";
    case BandGreen:
        return "20";
    case BandBlue:
        return "10";
    case BandPurple:
        return "5";
    case BandGray:
        return "1";
    default:;
        return "--";
    }
}

void update_resistance_temp_coeff(ResistorType rtype, BandColour colours[], char string[]) {
    strcpy(string, BLANK_TEMP_COEFF);
    uint8_t temp_coeff_index = TEMP_COEFF_INDEX_PER_RESISTOR[rtype - 1];
    char* unit = decode_resistance_temp_coeff(colours[temp_coeff_index]);
    char* target = string + INDEX_TOLERANCE;
    strncpy(target, unit, CHARS_TEMP_COEFF);
}

char* get_colour_short_description(BandColour colour) {
    switch(colour) {
    case BandBlack:
        return "Bk";
    case BandBrown:
        return "Br";
    case BandRed:
        return "Re";
    case BandOrange:
        return "Or";
    case BandYellow:
        return "Ye";
    case BandGreen:
        return "Gr";
    case BandBlue:
        return "Bl";
    case BandPurple:
        return "Pu";
    case BandGray:
        return "Gy";
    case BandWhite:
        return "Wh";
    case BandGold:
        return "Go";
    case BandSilver:
        return "Si";
    case BandPink:
        return "Pi";
    default:
        return "--";
    }
}
