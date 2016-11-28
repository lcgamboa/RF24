/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example using Dynamic Payloads 
 *
 * This is an example of how to use payloads of a varying (dynamic) size. 
 */

#include "nRF24L01.h"
#include "RF24.h"

#include "serial.h"

char buff[10];//for itoa

//for serial printf
void 
putch(char c)
{
  Serial_tx(c);  
}

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 7 & 8

RF24 radio;

// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const char role_pin = 34;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.

//LSB first for xc8!
const uint8_t pipes[2][5] = { {0xE1, 0xF0, 0xF0, 0xF0, 0xF0},{0xD2, 0xF0, 0xF0, 0xF0, 0xF0} };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

// The various roles supported by this sketch
typedef enum { role_ping_out = 1, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role;

//
// Payload
//

const int min_payload_size = 4;
const int max_payload_size = 32;
const int payload_size_increments_by = 1;
int next_payload_size = 4;//min_payload_size;

char receive_payload[max_payload_size+1]; // +1 to allow room for a terminating NULL char

void setup(void)
{
  //
  // Role
  //

  RF24_init(&radio,36,35);
  // set up the role pin
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  // read the address pin, establish our role
  if ( digitalRead(role_pin) )
    role = role_ping_out;
  else
    role = role_pong_back;

  //
  // Print preamble
  //

  Serial_begin(115200);
  
  Serial_println(F("RF24/examples/pingpair_dyn/"));
  Serial_print(F("ROLE: "));
  Serial_println(role_friendly_name[role]);
  
 
  //
  // Setup and configure rf radio
  //

  RF24_begin(&radio);

  // enable dynamic payloads
  RF24_enableDynamicPayloads(&radio);

  // optionally, increase the delay between retries & # of retries
  RF24_setRetries(&radio,5,15);


  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  if ( role == role_ping_out )
  {
    RF24_openWritingPipe(&radio,pipes[0]);
    RF24_openReadingPipe(&radio,1,pipes[1]);
  }
  else
  {
    RF24_openWritingPipe(&radio,pipes[1]);
    RF24_openReadingPipe(&radio,1,pipes[0]);
  }

  //
  // Start listening
  //

  RF24_startListening(&radio);

  //
  // Dump the configuration of the rf unit for debugging
  //

  RF24_printDetails(&radio);
}

void loop(void)
{
  //
  // Ping out role.  Repeatedly send the current time
  //

  if (role == role_ping_out)
  {
    // The payload will always be the same, what will change is how much of it we send.
    static char send_payload[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ789012";

    // First, stop listening so we can talk.
    RF24_stopListening(&radio);

    // Take the time, and send it.  This will block until complete
    Serial_print(F("Now sending length "));
    Serial_println(itoa(buff,next_payload_size,10));
    RF24_write(&radio,send_payload, next_payload_size );

    // Now, continue listening
    RF24_startListening(&radio);

    // Wait here until we get a response, or timeout
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! RF24_available(&radio) && ! timeout )
      if (millis() - started_waiting_at > 500 )
        timeout = true;

    // Describe the results
    if ( timeout )
    {
      Serial_println(F("Failed, response timed out."));
    }
    else
    {
      // Grab the response, compare, and send to debugging spew
      uint8_t len = RF24_getDynamicPayloadSize(&radio);
      
      // If a corrupt dynamic payload is received, it will be flushed
      if(!len){
        return; 
      }
      
      RF24_read(&radio,receive_payload, len );

      // Put a zero at the end for easy printing
      receive_payload[len] = 0;

      // Spew it
      Serial_print(F("Got response size="));
      Serial_print(itoa(buff,len,10));
      Serial_print(F(" value="));
      Serial_println(itoa(buff,receive_payload[0],10));
    }
    
    // Update size for next time.
    next_payload_size += payload_size_increments_by;
    if ( next_payload_size > max_payload_size )
      next_payload_size = min_payload_size;

    // Try again 1s later
    delay(100);
  }

  //
  // Pong back role.  Receive each packet, dump it out, and send it back
  //

  if ( role == role_pong_back )
  {
    // if there is data ready
    while ( RF24_available(&radio) )
    {

      // Fetch the payload, and see if this was the last one.
      uint8_t len = RF24_getDynamicPayloadSize(&radio);
      
      // If a corrupt dynamic payload is received, it will be flushed
      if(!len){
        continue; 
      }
      
      RF24_read(&radio,receive_payload, len );

      // Put a zero at the end for easy printing
      receive_payload[len] = 0;

      // Spew it
      Serial_print(F("Got response size="));
      Serial_print(itoa(buff,len,10));
      Serial_print(F(" value="));
      Serial_println(receive_payload);

      // First, stop listening so we can talk
      RF24_stopListening(&radio);

      // Send the final one back.
      RF24_write(&radio,receive_payload, len );
      Serial_println(F("Sent response."));

      // Now, resume listening so we catch the next packets.
      RF24_startListening(&radio);
    }
  }
}
// vim:cin:ai:sts=2 sw=2 ft=cpp