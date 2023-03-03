/*
  Gm7CanProtocol.h - Library for supplying shared CAN message ID's and proper rules for parsing body data. 
                     This library needs to be imported by all GM7 devices that uses CAN to communicate with eachother.
  Date: 9 feb 2023
  Version: 0.0.1-A.1
  Created by Alexander Samson
  contact: alexander@gm7.nl
  Released into the public domain.
*/

#ifndef Gm7CanProtocol_h
#define Gm7CanProtocol_h

#include <Arduino.h>

class Gm7CanProtocol {
  //The message ID used in CAN 2B (extended) is a 29 bit long identifier.
  //Since the ID's need to be unique per node (no 2 nodes should use the same message ID) and priority is given to lower ID's, we need to combine uniqueness with a simple priority system
  //We can do this by splitting the 29 bits into a priority part and a unique-ID part.
  //The MSB side of these 29 bits can be used for the priorty part and the LSB side can be used for an unique ID per device.
  //The important thing here is to know in advance what the most optimal split is for pririty-id's and unique-id's. 
  //The more priority id's are assigned, the less unique-id's can be used; there are only 29 bits available to share between them both.
  //Let's say we want to have 8192 possible priority/message id's, that would be 13 bits (0b1111111111111), 
  //that leaves us with 16 bits, aka 65536 possibilities left for the unique-id (0b1111111111111111)
  //For the unique ID, we can just use the LSB's of the device's Serial number; which are unique anyhow.
  //There is a risk of rolling over after Serial numbers higher than 65535, but the change of making so many devices for this niche market is slim, to say at least.
  //The 13/16 split will leave us with plenty of room for both message priority ID's and unique ID's.

  //An example: 0b11111111111110000000000000001 which will translate to priority id of 65535 (very low priority) with unique-id of 1.
  //In this case another, very similar node/device can send the same message type/pririty over the same bus, because it's unique id will be different, '2' for example.
  //The only caveat about this way of structuring priority/uniqueness is that lower (older) id's are prioritized over higher (newer) id's when the both of them send the same message id the same time.
  //This will be very unlikely though, and the small wait delay would not be noticable. We just need to be carefull that nothing is actually spamming the bus non-stop; which is unwanted anyhow.
  
  //Concluding the following:
  //The Message/Priority ID will be 13 bits long and is called  PMID
  //The Unique ID will be 16 bits long and is called            UID
  //Together they fill up the 29-bit message id, called         MESSAGE ID

  //In representation (p = PMID,  u = UID,  | = CAN extended frame split):
  //  0  1  2  3  4  5  6  7  8  9  10 | 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28
  //  p  p  p  p  p  p  p  p  p  p  p  | p  p  u  u  u  u  u  u  u  u  u  u  u  u  u  u  u  u

  #define CAN_PAYLOAD_MESSAGE_BYTES 8 //CAN 2B allows for a payload of max 8 bytes (64 bits). Don't mess with this unless you know what you are doing.

    private:
      uint16_t uniqueIdSize = 16;
      uint16_t uniqueIdMask = 65535;
      uint32_t baudrate = 500000;
      uint8_t defaultMessageLength = CAN_PAYLOAD_MESSAGE_BYTES;
      bool useExtendedIds = true;
      uint32_t heartbeatIntervalMillis = 1000;
      uint32_t heartbeatTimeoutTresholdMillis = 1250;
      uint32_t deviceUpdateIntervalMillisBase = 30000;
      
      //About deviceUpdateIntervalRandomSpread = random(-250, 250):
      //There will be the possibility of multiple nodes on the same bus and each of them will send a multi-frame device update dataset.
      //When all devices power up simultanously, it could cause a traffic spike on the CAN bus, just because of the multi-frame bursts of multiple nodes at the same time.
      //These updates are not really timing-sensitive, so we can incorporate a random difference in timing (in milliseconds) for all nodes using this protocol file.
      //In this way the burst of device information can-frames will be spread out a bit, making the bus operate more smooth overall.
      int32_t deviceUpdateIntervalRandomSpread = random(-250, 250);

    public:
        static const uint8_t DEFAULT_CAN_MESSAGE_LENGTH_MAX = CAN_PAYLOAD_MESSAGE_BYTES;
        
        Gm7CanProtocol(){

        };

        enum CanDeviceType : uint8_t {
          CONTROLLER = 1,
          MODULE = 2,
          PERIPHERAL = 3,
          EXTERNAL_DEVICE = 4,
          READ_ONLY = 5
        };

        struct MessageId {      //MESSAGE ID of 29 bits, comprises:
          uint16_t pmid;        //PMID (Priority/Message ID) of 13 bits
          uint16_t uid;         //UID (Unique Id) of 16 bits
        };

        uint32_t getBaudrate(){
          return baudrate;
        };

        uint8_t getMessageLength(){
          return defaultMessageLength;
        };

        bool getUseExtendedIds(){
          return useExtendedIds;
        };

        uint32_t getHeartbeatIntervalRateInMillis(){
          return heartbeatIntervalMillis;
        };

        uint32_t getHeartbeatTimeoutTresholdInMillis(){
          return heartbeatTimeoutTresholdMillis;
        };

        uint32_t getDeviceUpdateIntervalRateInMillis(){
          return deviceUpdateIntervalMillisBase + deviceUpdateIntervalRandomSpread; 
        };

        void randomizeDeviceUpdateIntervalOffset(){
          deviceUpdateIntervalRandomSpread = random(-250, 250);
        };

        void clearBuffer(char * buffer, uint8_t bufferCount){
          for(int i = 0; i < bufferCount; i++){
            buffer[i] = 0;
          }
        };



        MessageId parseMessageId(uint32_t canMessageId){
          MessageId messageId;
          messageId.uid = (canMessageId & uniqueIdMask);
          messageId.pmid = (canMessageId >> uniqueIdSize);
          //messageId.priorityId = (messageId.priorityId & ((2^priorityIdSize)-1));
          return messageId;
        };

        uint32_t encodeMessageId(MessageId messageId){
            uint32_t msg = 0;
            msg += (messageId.pmid << uniqueIdSize);
            msg += messageId.uid;
            return msg;
        };

        //Param 1: priorityId, param 2: uniqueId
        uint32_t encodeMessageId(uint16_t priorityId, uint16_t uniqueId){
            uint32_t msg = 0;
            msg += (priorityId << uniqueIdSize);
            msg += uniqueId;
            return msg;
        };

        bool addUint64ToBuffer(char * buffer, uint8_t bufferCount, uint64_t value, uint8_t bufferStartPos = 0){
          if(bufferCount < bufferStartPos+8){
            return false;
          }
          buffer[bufferStartPos] =   value >> 56;
          buffer[bufferStartPos+1] = value >> 48;
          buffer[bufferStartPos+2] = value >> 40;
          buffer[bufferStartPos+3] = value >> 32;
          buffer[bufferStartPos+4] = value >> 24;
          buffer[bufferStartPos+5] = value >> 16;
          buffer[bufferStartPos+6] = value >>  8;
          buffer[bufferStartPos+7] = value;
          return true;
        };

        uint64_t extractUint64FromBuffer(char * buffer, uint8_t bufferCount, uint8_t bufferStartPos = 0){
          if(bufferCount < bufferStartPos+8){
            return 0;
          }
          uint64_t value = 0;
          value |= buffer[bufferStartPos]   << 56;
          value |= buffer[bufferStartPos+1] << 48;
          value |= buffer[bufferStartPos+2] << 40;
          value |= buffer[bufferStartPos+3] << 32;
          value |= buffer[bufferStartPos+4] << 24;
          value |= buffer[bufferStartPos+5] << 16;
          value |= buffer[bufferStartPos+6] << 8;
          value |= buffer[bufferStartPos+7];
          return value;
        };

        bool addUint32ToBuffer(char * buffer, uint8_t bufferCount, uint32_t value, uint8_t bufferStartPos = 0){
          if(bufferCount < bufferStartPos+4){
            return false;
          }
          buffer[bufferStartPos] = value >> 24;
          buffer[bufferStartPos+1] = value >> 16;
          buffer[bufferStartPos+2] = value >>  8;
          buffer[bufferStartPos+3] = value;
          return true;
        };

        uint32_t extractUint32FromBuffer(char * buffer, uint8_t bufferCount, uint8_t bufferStartPos = 0){
          if(bufferCount < bufferStartPos+4){
            return 0;
          }
          uint32_t value = 0;
          value |= buffer[bufferStartPos] << 24;
          value |= buffer[bufferStartPos+1] << 16;
          value |= buffer[bufferStartPos+2] << 8;
          value |= buffer[bufferStartPos+3];
          return value;
        };

        bool addUint16ToBuffer(char * buffer, uint8_t bufferCount, uint16_t value, uint8_t bufferStartPos = 0){
          if(bufferCount < bufferStartPos+2){
            return false;
          }
          buffer[bufferStartPos] = value >> 8;
          buffer[bufferStartPos+1] = value;
          return true;
        };

        uint16_t extractUint16FromBuffer(char * buffer, uint8_t bufferCount, uint8_t bufferStartPos = 0){
          if(bufferCount < bufferStartPos+2){
            return 0;
          }
          uint16_t value = 0;
          value |= buffer[bufferStartPos] << 8;
          value |= buffer[bufferStartPos+1];
          return value;
        };

        bool addCharArrayToBuffer(char * buffer, uint8_t bufferCount, const char * value, uint8_t bufferStartPos = 0){
            if(bufferCount <= bufferStartPos){
              return false;
            }
            for(int i = bufferStartPos; i < bufferCount; i++){
              buffer[i] = value[i];
              if(value[i] == '\0'){
                break;
              }
            }
            return;
        };

        bool encodeHeartbeat(char * buffer, uint8_t bufferCount, uint32_t millisCurrent, uint32_t millisLast = 0){
          clearBuffer(buffer, bufferCount);
          if(bufferCount < 4){
            return false;
          }
          addUint32ToBuffer(buffer, bufferCount, millisCurrent, 0);
          if(bufferCount > 7){
            addUint32ToBuffer(buffer, bufferCount, millisLast, 4);
          }
          return true;
        };

        bool encodeSerialNumberToBuffer(char * buffer, uint8_t bufferCount, uint64_t serialNumber){
          clearBuffer(buffer, bufferCount);
          return addUint64ToBuffer(buffer, bufferCount, serialNumber, 0);
        };

        bool encodeTypeIdToBuffer(char * buffer, uint8_t bufferCount, uint16_t typeId){
          clearBuffer(buffer, bufferCount);
          return addUint16ToBuffer(buffer, bufferCount, typeId, 0);
        };

        bool encodeModelToBuffer(char * buffer, uint8_t bufferCount, char * model){
          clearBuffer(buffer, bufferCount);
          return addCharArrayToBuffer(buffer, bufferCount, model, 0);
        };

        bool encodeVendorToBuffer(char * buffer, uint8_t bufferCount, char * vendor){
          clearBuffer(buffer, bufferCount);
          return addCharArrayToBuffer(buffer, bufferCount, vendor, 0);
        };

        bool encodeShortNameToBuffer(char * buffer, uint8_t bufferCount, char * name){
          clearBuffer(buffer, bufferCount);
          return addCharArrayToBuffer(buffer, bufferCount, name, 0);
        };


        uint16_t extractDeviceTypeIdFromBuffer(char * buffer, uint8_t bufferCount){
          if(bufferCount < 2){
            return;
          }
          return extractUint16FromBuffer(buffer, bufferCount, 0);
        };



//MODULE STATUS
        struct StatusAndProgress{
          uint32_t status;
          uint16_t progress;
          uint16_t progressMax;
        };

        bool encodeModuleStatusAndProgress(char * buffer, uint8_t bufferCount, uint32_t status, uint16_t progress, uint16_t progressMax){
            if(bufferCount < 8){
              return false; //We need all 64 bits for this method
            }
            if(addUint32ToBuffer(buffer, bufferCount, status, 0) == false){return false;} //Add the status in the first 4 bytes
            if(addUint16ToBuffer(buffer, bufferCount, progress, 4) == false){return false;} //Add the current progress in the next 2
            if(addUint16ToBuffer(buffer, bufferCount, progressMax, 6) == false){return false;} //Add the max progress in the last 2
            return true;
        };

        bool encodeModuleStatusAndProgress(char * buffer, uint8_t bufferCount, StatusAndProgress statusAndProgress){
            return encodeModuleStatusAndProgress(buffer, bufferCount, statusAndProgress.status, statusAndProgress.progress, statusAndProgress.progressMax);
        };

        StatusAndProgress decodeModuleStatusAndProgress(char * buffer, uint8_t bufferCount){
            if(bufferCount < 8){
              return; //We need all 64 bits for this method
            }
            StatusAndProgress statusAndProgress;
            statusAndProgress.status = extractUint32FromBuffer(buffer, bufferCount, 0); //Add the status in the first 4 bytes
            statusAndProgress.progress = extractUint16FromBuffer(buffer, bufferCount, 4); //Add the status in the first 4 bytes
            statusAndProgress.progressMax = extractUint16FromBuffer(buffer, bufferCount, 6); //Add the status in the first 4 bytes
            return statusAndProgress;
        };


///////////////////////////////////////////////////
/////
/////  PMID SECTION - All PMID's can be found here
/////
///////////////////////////////////////////////////

        //EMERGENCY CALLS
        //These calls are meant to deter unsafe situations that could arise.
        //Remember that anything connected to the CAN bus can send these commands, even Read-only devices. Make sure that there are no spurious or random calls in these sections, because it can cause a lot of grief.
        //Also remember that not all connected devices may have code implemented to listen to any of these calls; makse sure you test their functionality before trusting these calls.
        //Anything that uses mains power, relays through mains power, uses high current or anything that could cause harm to people if left in a faulty state, should have these calls properly implemented.
        const uint16_t EMERGENCY_SECTION_START = 1;
          const uint16_t EMERGENCY_SHUTDOWN = 2; //This call should order all devices to kill power it relays trough, it receives and uses; shutting down effectively.
          const uint16_t EMERGENCY_FAILSAFE = 3; //This call should order all connected devices on the bus to return to a safe state immediately. For example: shutting down (if possible), dropping enacted relays or stop using mains power.
          const uint16_t EMERGENCY_FIRE_ALARM = 4; //This alarm should signal to anything that can display it, that there is a possible fire going on. Smoke detectors, connected to the CAN, bus can use this call
        const uint16_t EMERGENCY_SECTION_END = 99;
        
        //HEARTBEATS
        //Heartbeats are mandatory to use according to the GM7 protocol. Every 1 second a heartbeat should be sent on the bus, using one of these PMID's.
        //Devices can update their online/offline status depening on whether a remote heartbeat was received in the last 1250 milliseconds (default).
        //Read-only devices cannot send heartbeats, since they are strict read-only and therefore cannot contribute to the online-check of other connected devices.
        //For modules and peripherals to register with a controller, these devices will wait for a controller heartbeat and upon receiving one the registration request should be sent.
        //However, registration requests are not really mandatory for generic devices, but it helps the controller to keep track of what's connected.
        //Game modules must be registered, because a game will use the registered devices list as a guide to what modules to activate when the game starts.
        //Also configuration files can be saved per module UID. By registering them, these files can be saved/loaded properly.
        const uint16_t HEARTBEATS_START = 200;
          const uint16_t HEARTBEAT_CONTROLLER = 201;
          const uint16_t HEARTBEAT_MODULE = 202;
          const uint16_t HEARTBEAT_PERIPHERAL = 203;
          const uint16_t HEARTBEAT_EXTERNAL_DEVICE = 204;
        const uint16_t HEARTBEATS_END = 299;

        //Generic statusses.
        const uint16_t STATUS_CONTROLLER = 1001;
        const uint16_t STATUS_MODULE = 1002;
        const uint16_t STATUS_PERIPHERAL = 1003; 
        const uint16_t STATUS_EXTERNAL_DEVICE = 1004; 

        //Anything that sees itself as a controller needs to listen to these commands
        const uint16_t REQUEST_CONTROLLER_STATUS_CHANGE = 2001;
        const uint16_t REQUEST_CONTROLLER_GPIO = 2011; //First 32 bits for turning ON gpio pins, second 32 bits for turning OFF gpio pins
        
        //Single node/controller requests. THE FIRST 16 bits of these data packages will be parsed as message Id. 
        const uint16_t REQUEST_ADDRESSED_FILTER_START = 2100; //Use this and the -END variant to make a filter that reject or allows requests on node-level implementations
        const uint16_t REQUEST_STATUS_CHANGE = 2101;
        const uint16_t REQUEST_GPIO_ON = 2111; //First 16 bits for device ID, second 32 bits for turning ON gpio pins
        const uint16_t REQUEST_GPIO_OFF = 2112; //First 16 bits for device ID, second 32 bits for turning OFF gpio pins
        const uint16_t REQUEST_PROGRESS_SET = 2113;
        const uint16_t REQUEST_ADDRESSED_FILTER_END = 2199; //Use this and the -START variant to make a filter that reject or allows requests on node-level implementations

        //All connected nodes and peripherals should listen to these commands, controllers are exempt.
        const uint16_t REQUEST_ALL_NODES_STATUS_CHANGE = 2201;
        const uint16_t REQUEST_ALL_NODES_GPIO = 2211; //First 32 bits for turning ON gpio pins, second 32 bits for turning OFF gpio pins

        //All connected devices should listen to these commands, even controllers.
        const uint16_t REQUEST_ALL_STATUS_CHANGE = 2301;
        const uint16_t REQUEST_ALL_GPIO = 2311; //First 32 bits for turning ON gpio pins, second 32 bits for turning OFF gpio pins

        const uint16_t DEVICE_SECTION_START = 4000;
          const uint16_t DEVICE_SERIAL = 4001; //MAX 64 bits
          const uint16_t DEVICE_MODEL = 4002; //MAX 7 chars (+ 1 terminator) chars
          const uint16_t DEVICE_TYPE_ID = 4003;  //INT
          const uint16_t DEVICE_VENDOR = 4004;  //MAX 7 chars
          const uint16_t DEVICE_SHORT_NAME = 4005;  //MAX 7 chars
          const uint16_t DEVICE_VITALS_BATTERY = 4004;
          const uint16_t DEVICE_VITALS_CONNECTION = 4005;
          const uint16_t DEVICE_VITALS_DEBUGGING = 4006;
          const uint16_t DEVICE_STATUS = 4007; //General purpose status. To be implemented.
        const uint16_t DEVICE_SECTION_END = 4099;

        //The ID of any of the devices below could be sent as payload (uint16, MSB) with the device DEVICE_REGISTRATION_REQUEST
        //Alternatively it is possible to use any of these register ID's as the PMID itself for device registration, 
        //You will need to implement either of two methods (or both) in your own code and methods to parse the registration correctly.
        //It does not really matter, since none of these ID's are used as PMID's for something else anyhow. 
        //Remember that these ID's are static members, so they can be used to easily 'hardcode' Device Type ID's based on this scheme (recommended).
        const uint16_t DEVICE_TYPE_SECTION_START =               4100;
          const uint16_t DEVICE_REGISTRATION_REQUEST =             4100; //A request from a device to register to any connected controller. Use a device type in the payload.
          const uint16_t DEVICE_TYPE_CONTROLLER_SECTION_START =    4100;
            static const uint16_t DEVICE_TYPE_CONTROLLER_SBC =       4101;  //A Single board computer, like a Raspberry Pi or something.
            static const uint16_t DEVICE_TYPE_CONTROLLER_MCU =       4102;  //A microcontroller of any sorts
            static const uint16_t DEVICE_TYPE_CONTROLLER_SERVER =    4103;  //Some kind of server
            static const uint16_t DEVICE_TYPE_CONTROLLER_USB =       4104;  //USB or CAN-USB interface dongle
            static const uint16_t DEVICE_TYPE_CONTROLLER_SERIAL =    4105;  //Serial/UART application
            static const uint16_t DEVICE_TYPE_CONTROLLER_WEBAPP =    4106;  //Webapplication
            static const uint16_t DEVICE_TYPE_CONTROLLER_WINPC =     4107;  //Windows app
            static const uint16_t DEVICE_TYPE_CONTROLLER_UNIX =      4108;  //Unix/linux app
            static const uint16_t DEVICE_TYPE_CONTROLLER_MACOS =     4109;  //MAC application
            static const uint16_t DEVICE_TYPE_CONTROLLER_MOBILE =    4110;  //registration via a generic mobile app
            static const uint16_t DEVICE_TYPE_CONTROLLER_IOS =       4111;  //registration via an IOS mobile app
            static const uint16_t DEVICE_TYPE_CONTROLLER_ANDROID =   4112;  //registration via an android app
            static const uint16_t DEVICE_TYPE_CONTROLLER_GENERIC =   4113;  //Generic controller registration
            static const uint16_t DEVICE_TYPE_CONTROLLER_GM7UTB =    4114;  //Universal Time Bomb
            static const uint16_t DEVICE_TYPE_CONTROLLER_GM7UCS =    4115;  //Universal Controller System
            static const uint16_t DEVICE_TYPE_CONTROLLER_GM7ACS =    4116;  //Advanced Controller System
            static const uint16_t DEVICE_TYPE_CONTROLLER_GM7AEM =    4117;  //Ambient Effects Module
            static const uint16_t DEVICE_TYPE_CONTROLLER_GM7GRC =    4118;  //Generic Room Controller
            static const uint16_t DEVICE_TYPE_CONTROLLER_OEM    =    4119;  //Some random OEM device
            static const uint16_t DEVICE_TYPE_CONTROLLER_DEV   =     4120;  //Some random dev device
            static const uint16_t DEVICE_TYPE_CONTROLLER_TEST   =    4121;  //Some random test device
            static const uint16_t DEVICE_TYPE_CONTROLLER_DEBUG   =   4122;  //Some random debug device
          const uint16_t DEVICE_TYPE_CONTROLLER_SECTION_END =      4199;
          const uint16_t DEVICE_TYPE_MODULE_SECTION_START =        4200;
            static const uint16_t DEVICE_TYPE_MODULE_TIMER       =   4201;  //An external timer module (counting down)
            static const uint16_t DEVICE_TYPE_MODULE_CLOCK       =   4202;  //An external clock module (counting up)  
            static const uint16_t DEVICE_TYPE_MODULE_TIMERCLOCK  =   4203;  //An external module consisting of clocks and timers 
            static const uint16_t DEVICE_TYPE_MODULE_DIAGNOSTICS =   4204;  //An external module receiving diagnostics from controllers/can bus
            static const uint16_t DEVICE_TYPE_MODULE_SENSOR =        4205;  //An external module sending generic sensor data to the can network
            static const uint16_t DEVICE_TYPE_MODULE_ACTUATOR =      4206;  //An external module using generic data from the network to drive an actuator
            static const uint16_t DEVICE_TYPE_MODULE_GENERIC_IO =    4207;  //An external module sending and receiving generic io data to/from the CAN network
            static const uint16_t DEVICE_TYPE_MODULE_GENERIC_RO =    4208;  //An external module reading (Read Only) generic data from the CAN network
            static const uint16_t DEVICE_TYPE_MODULE_GAME_MODULE =   4209;  //An external game module for use in room controllers and GM7UTB games. Uses advanced interaction.
            static const uint16_t DEVICE_TYPE_MODULE_TEST =          4210;  //An external module used for testing anything useful on the CAN bus
          const uint16_t DEVICE_TYPE_MODULE_SECTION_END =          4299;
          const uint16_t DEVICE_TYPE_PERIPHERAL_SECTION_START =    4300;
            static const uint16_t DEVICE_TYPE_PERIPHERAL_KEYBOARD =  4301;  //A peripheral, acting as a keyboard
          const uint16_t DEVICE_TYPE_PERIPHERAL_SECTION_END =      4399;
          const uint16_t DEVICE_TYPE_EXTERNAL_SECTION_START =      4400;
            static const uint16_t DEVICE_TYPE_EXTERNAL_GENERIC =     4301;  //An external generic device.
          const uint16_t DEVICE_TYPE_EXTERNAL_SECTION_END =        4499;
        const uint16_t DEVICE_TYPE_SECTION_END =                 4499;
        
        const uint16_t CONTROLLER_SECTION_START = 5100;
          const uint16_t CONTROLLER_STATUS_AND_PROGRESS = 5101; //32 bits status, 16 bits left for progress max, 16 bits for progress current
          const uint16_t CONTROLLER_MAIN_TIMER_STATUS = 5102; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t CONTROLLER_VALIDATION_TIMER_STATUS = 5103; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t CONTROLLER_INTERNAL_TIMER_STATUS = 5104; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t CONTROLLER_TRIES = 5105; //First 16 bits: tries current, next 16 bits: tries max, next 16 bits: total tries counter, next 16 bits: setting flags
        const uint16_t CONTROLLER_SECTION_END = 5299;

        const uint16_t MODULE_SECTION_START = 5300;
          const uint16_t MODULE_STATUS_AND_PROGRESS = 5301; //32 bits status, 16 bits left for progress max, 16 bits for progress current
          const uint16_t MODULE_MAIN_TIMER_STATUS = 5302; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t MODULE_VALIDATION_TIMER_STATUS = 5303; //32 bits for current validation timeleft, 32 for set validation timer
          const uint16_t MODULE_INTERNAL_TIMER_STATUS = 5304; //32 bits for current internal timeleft, 32 for set internal timer
          const uint16_t MODULE_TRIES = 5305; //First 16 bits: tries current, next 16 bits: tries max, next 16 bits: total tries counter, next 16 bits: setting flags
        const uint16_t MODULE_SECTION_END = 5499;

        const uint16_t PERIPHERAL_SECTION_START = 5500;
          const uint16_t PERIPHERAL_STATUS_AND_PROGRESS = 5501; //32 bits status, 16 bits left for progress max, 16 bits for progress current
          const uint16_t PERIPHERAL_MAIN_TIMER_STATUS = 5502; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t PERIPHERAL_VALIDATION_TIMER_STATUS = 5503; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t PERIPHERAL_INTERNAL_TIMER_STATUS = 5504; //32 bits for current internal timeleft, 32 for set internal timer
        const uint16_t PERIPHERAL_SECTION_END = 5699;

        const uint16_t EXTERNAL_DEVICE_SECTION_START = 5700;
          const uint16_t EXTERNAL_DEVICE_STATUS_AND_PROGRESS = 5701; //32 bits status, 16 bits left for progress max, 16 bits for progress current
          const uint16_t EXTERNAL_DEVICE_MAIN_TIMER_STATUS = 5702; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t EXTERNAL_DEVICE_VALIDATION_TIMER_STATUS = 5703; //32 bits for current main timer timeleft, 32 for set main timer
          const uint16_t EXTERNAL_DEVICE_INTERNAL_TIMER_STATUS = 5704; //32 bits for current internal timeleft, 32 for set internal timer
        const uint16_t EXTERNAL_DEVICE_SECTION_END = 5899;


  //You can use this method to sort of automatically assign a Can Device Type to any device, based on the supplied device type id.
  //For this to work, you will need to use the device type id's as listed in the PMID list above and select the proper one for your sepcific device
  uint8_t extractCanDeviceTypeFromDeviceTypeId(uint16_t deviceTypeId){
    if(deviceTypeId == DEVICE_TYPE_MODULE_GENERIC_RO){
      return CanDeviceType::READ_ONLY;
    }
    if((deviceTypeId > DEVICE_TYPE_CONTROLLER_SECTION_START) && (deviceTypeId < DEVICE_TYPE_CONTROLLER_SECTION_END)){
      return CanDeviceType::CONTROLLER;
    }
    if((deviceTypeId > DEVICE_TYPE_MODULE_SECTION_START) && (deviceTypeId < DEVICE_TYPE_MODULE_SECTION_END)){
      return CanDeviceType::MODULE;
    }
    if((deviceTypeId > DEVICE_TYPE_PERIPHERAL_SECTION_START) && (deviceTypeId < DEVICE_TYPE_PERIPHERAL_SECTION_END)){
      return CanDeviceType::PERIPHERAL;
    }
    if((deviceTypeId > DEVICE_TYPE_EXTERNAL_SECTION_START) && (deviceTypeId < DEVICE_TYPE_EXTERNAL_SECTION_END)){
      return CanDeviceType::EXTERNAL_DEVICE;
    }
    return CanDeviceType::READ_ONLY;
  };

  uint16_t getPmidHeartbeatForDeviceType(uint8_t canDeviceType){
    if(canDeviceType == CanDeviceType::CONTROLLER) { return HEARTBEAT_CONTROLLER; }
    if(canDeviceType == CanDeviceType::MODULE) { return HEARTBEAT_MODULE; }
    if(canDeviceType == CanDeviceType::PERIPHERAL) { return HEARTBEAT_PERIPHERAL; }
    if(canDeviceType == CanDeviceType::EXTERNAL_DEVICE) { return HEARTBEAT_EXTERNAL_DEVICE; }
    return 0;
  };

  uint16_t getPmidGameStatusForDeviceType(uint8_t canDeviceType){
    if(canDeviceType == CanDeviceType::CONTROLLER) { return CONTROLLER_STATUS_AND_PROGRESS; }
    if(canDeviceType == CanDeviceType::MODULE) { return MODULE_STATUS_AND_PROGRESS; }
    if(canDeviceType == CanDeviceType::PERIPHERAL) { return PERIPHERAL_STATUS_AND_PROGRESS; }
    if(canDeviceType == CanDeviceType::EXTERNAL_DEVICE) { return EXTERNAL_DEVICE_STATUS_AND_PROGRESS; }
    return 0;
  };

  uint16_t getPmidMainTimerForDeviceType(uint8_t canDeviceType){
    if(canDeviceType == CanDeviceType::CONTROLLER) { return CONTROLLER_MAIN_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::MODULE) { return MODULE_MAIN_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::PERIPHERAL) { return PERIPHERAL_MAIN_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::EXTERNAL_DEVICE) { return EXTERNAL_DEVICE_MAIN_TIMER_STATUS; }
    return 0;
  };

  uint16_t getPmidValidationTimerForDeviceType(uint8_t canDeviceType){
    if(canDeviceType == CanDeviceType::CONTROLLER) { return CONTROLLER_VALIDATION_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::MODULE) { return MODULE_VALIDATION_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::PERIPHERAL) { return PERIPHERAL_VALIDATION_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::EXTERNAL_DEVICE) { return EXTERNAL_DEVICE_VALIDATION_TIMER_STATUS; }
    return 0;
  };

  uint16_t getPmidInternalTimerForDeviceType(uint8_t canDeviceType){
    if(canDeviceType == CanDeviceType::CONTROLLER) { return CONTROLLER_INTERNAL_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::MODULE) { return MODULE_INTERNAL_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::PERIPHERAL) { return PERIPHERAL_INTERNAL_TIMER_STATUS; }
    if(canDeviceType == CanDeviceType::EXTERNAL_DEVICE) { return EXTERNAL_DEVICE_INTERNAL_TIMER_STATUS; }
    return 0;
  };

};

#endif