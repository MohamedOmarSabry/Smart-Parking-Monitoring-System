#pragma once

//#define ROLE_SLOT
//#define ROLE_LANE
#define ROLE_MAIN

#if defined(ROLE_SLOT)

#define SLOT_ID         "SLOT_1"       // unique per slot

#define LANE_AP_SSID    "LANE1-AP"      
#define LANE_AP_PASS    "lane1pass"
#define LANE_SERVER_URL "http://192.168.4.1:3000/slot"

// GPIO pins
#define TRIG_PIN      GPIO_NUM_12
#define ECHO_PIN      GPIO_NUM_13
#define GREEN_PIN     GPIO_NUM_14
#define RED_PIN       GPIO_NUM_27
#define RGB_VCC_PIN   GPIO_NUM_26


#elif defined(ROLE_LANE)

#define LANE_ID         "LANE_1"       // unique per lane
#define LANE_AP_SSID    "LANE1-AP"     // this lane's AP (slots connect here)
#define LANE_AP_PASS    "lane1pass"
#define MAIN_AP_SSID    "MAIN-AP"      // main controller's AP (this lane connects to it)
#define MAIN_AP_PASS    "mainpass"
#define MAIN_SERVER_URL "http://192.168.5.1:3000/lane"

#elif defined(ROLE_MAIN)

#define MAIN_AP_SSID  "MAIN-AP"        // this controller's AP (lanes connect here)
#define MAIN_AP_PASS  "mainpass"
#define BACKEND_URL   "http://192.168.5.5:3000/sensor" // IP of the PC on MAIN-AP

#endif