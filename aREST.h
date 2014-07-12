/* 
  aREST Library for Arduino
  See the README file for more details.
 
  Written in 2014 by Marco Schwartz under a GPL license. 

  Version 1.7

  Changelog:
  Version 1.7: Added compatibility with the Arduino Due & Teensy 3.x
  Version 1.6: Added compatibility with the Arduino Yun
  Version 1.5: Size reduction, and added compatibility with Adafruit BLE
  Version 1.4: Added authentification with API key
  Version 1.3: Added support for the Ethernet shield
  Version 1.2: Added support of Serial communications
  Version 1.1: Added variables & functions support
  Version 1.0: First working version of the library

*/

#include "Arduino.h"

#define NUMBER_VARIABLES 4
#define NUMBER_FUNCTIONS 6

class aREST {

public:
  aREST() {
  command = "";
  pin_selected = false;
  state_selected = false;
  command_selected = false;
  api_key_received = false;
  api_key_match = false;

  variables_index = 0;
  functions_index = 0;
  //name = "default_name";
  //id = "001";
  api_key = "";
}

template <typename T>
void send_http_headers(T& client){
  client.print(F("HTTP/1.1 200 OK"));
  client.print(F("\r\n"));
  client.print(F("Content-Type: application/json"));
  client.print(F("\r\n"));
  client.print(F("Connection: close"));
  client.print(F("\r\n"));
  client.print(F(""));
  client.print(F("\r\n"));  
}

// Reset variables after a request
void reset_status() {
  answer = "";
  command = "";
  command_selected = false;
  pin_selected = false;
  state_selected = false;
  command_sent = false;
  state = 'u';
  arguments = "";
  api_key_received = false;
  api_key_match = false;

}

// Handle request with the CC3000 WiFi chip
#ifdef ADAFRUIT_CC3000_H
void handle(Adafruit_CC3000_ClientRef& client) {
  
  if (client.available()) {

    // Handle request
    handle_proto(client,true);
    
    // Give the web browser time to receive the data
    delay(5);
    client.close();  
   
    // Reset variables for the next command
    reset_status();
  } 
}
#endif

// Handle request with the Arduino Yun
#ifdef _YUN_CLIENT_H_
void handle(YunClient& client) {
  
  if (client.available()) {

    // Handle request
    handle_proto(client,true);
    
    // Give the web browser time to receive the data
    delay(5);
    client.stop();  
   
    // Reset variables for the next command
    reset_status();
  } 
}
#endif

// Handle request with the Adafruit BLE board
#ifdef _ADAFRUIT_BLE_UART_H_
void handle(Adafruit_BLE_UART& serial) {
  
  if (serial.available()) {

    // Handle request
    handle_proto(serial,false);
    
    // Wait for data to be sent
    delay(5);

    // Reset variables for the next command
    reset_status();
  } 
}
#endif

// Handle request for the Arduino Ethernet shield
#ifdef ethernet_h
void handle(EthernetClient& client){

  if (client.available()) {

    // Handle request
    handle_proto(client,true);

    // Give the web browser time to receive the data and stop connection
    delay(5);
    client.stop();  
   
    // Reset variables for the next command
    reset_status();   
  }
}
#endif

#if defined(CORE_TEENSY)
// Handle request on the Serial port
void handle(usb_serial_class& serial){

  if (serial.available()) {

    // Handle request
    handle_proto(serial,false);

    // Wait for data to be sent
    delay(10);

    // Reset variables for the next command
    reset_status();     
  }
}
#endif

// Handle request on the Serial port
void handle(HardwareSerial& serial){

  if (serial.available()) {

    // Handle request
    handle_proto(serial,false);

    // Wait for data to be sent
    delay(10);

    // Reset variables for the next command
    reset_status();     
  }
}

template <typename T>
void handle_proto(T& serial, bool headers) 
{

  // API key needed ?
  if (api_key == "") {api_key_received = true;}

  // Check if there is data available to read
  while (serial.available()) {
       
    // Get the server answer
    char c = serial.read();
    delay(1);
    answer = answer + c;
    //Serial.print(c);

    // Check for API key or end of request
    if (c == '\n') {

      // API key ?
      if(!api_key_received){

        // Check for API key
        if(answer.startsWith("X-ApiKey")) {
          String received_key = answer.substring(10);
          received_key.trim();

          if (received_key == api_key) {
          api_key_match = true;
            //Serial.println("API key match");
          }

          //Serial.println("API key received");
          api_key_received = true;
        }

      }

      // Reset answer
      answer = "";

    }  

    // Check if we are receveing useful data and process it
    if ((c == '/' || c == '\r') && state_selected == false) {

      // Trim answer
      answer.trim();

      // Debug output
      //Serial.println(answer); 
      
      // If the command is mode, and the pin is already selected    
      if (command == "mode" && pin_selected == true && state_selected == false) {

        // Input command received ?     
        if (answer.startsWith("i")) {state = 'i';}
       
        // Output command received ?     
        if (answer.startsWith("o")) {state = 'o';}
       
        // Indicate that the state has been selected     
        state_selected = true;
            
     }
     
     // If a digital command has been received, process the data accordingly     
     if (command == "digital" && pin_selected == true && state_selected == false) {
                
       //Serial.println("Digital command, finding nature");

       // If it's a read command, read from the pin and send data back
       if (answer.startsWith("r")) {state = 'r';}
       
       // If not, get value we want to apply to the pin        
       else {value = answer.toInt();}
       
       // Declare that the state has been selected         
       state_selected = true;
     }
     
     // If analog command has been selected, process the data accordingly     
     if (command == "analog" && pin_selected == true && state_selected == false) {
                
       // If it's a read, read from the correct pin
       if (answer.startsWith("r")) {state = 'r';}
       
       // Else, write analog value        
       else {value = answer.toInt();}
       
       // Declare that the state has been selected        
       state_selected = true;
     }
     
     // If the command is already selected, get the pin     
     if (command_selected == true && pin_selected == false) {
       
       // Get pin
       pin = answer.toInt();
       // Serial.println("Pin " + String(pin) + " selected");
       pin_selected = true;

       // Nothing more ?
       if (answer.length() == String(pin).length() || answer[String(pin).length()] != '/') {

        // Nothing more & digital ?
        if (command == "digital") {

          // Save state & end there
          // Serial.println("Digital read");
          state = 'r';
          state_selected = true;
        }

       // Nothing more & analog ?
       if (command == "analog") {
        
         // Save state & end there
         state = 'r';
         state_selected = true;
       }
     }  

   }
     
     // Digital command received ?    
     if (answer.startsWith("digital")) {
       // Serial.println("Digital command received");
       command = "digital";
       command_selected = true;
     }
          
     // Mode command received ?
     if (answer.startsWith("mode")) {
       //Serial.println("Mode command received");
       command = "mode";
       command_selected = true;
     }
          
     // Analog command received ?
     if (answer.startsWith("analog")) {
       //Serial.println("Analog command received");
       command = "analog";
       command_selected = true;
     }

     // Variable or function request received ?
     if (command_selected == false) {
       
       // Check if variable name is in array
       for (int i = 0; i < variables_index; i++){
         if(answer.startsWith(int_variables_names[i])) {
           //Serial.println(F("Variable found")); 
           
           // End here
           command_selected = true;
           pin_selected = true;
           state_selected = true;

           // Set state
           command = "variable";
           value = i;
           
         }
       }

       // Check if function name is in array
       for (int i = 0; i < functions_index; i++){
         if(answer.startsWith(functions_names[i])) {
           //Serial.println(F("Function found"));
           
           // End here
           command_selected = true;
           pin_selected = true;
           state_selected = true;

           // Set state
           command = "function";
           value = i;

           // Get command
           int header_length = functions_names[i].length() + 8;
           arguments = answer.substring(header_length);

         }
       }

       // If the command is "id", return device id, name and status
       if (answer.startsWith("id")){
           //Serial.println(F("id command found"));

           // Set state
           command = "id";

           // End here
           command_selected = true;
           pin_selected = true;
           state_selected = true;
       }
     }
     
     //Serial.println("Answer reset");  
     //Serial.print("Command selected:");   
     //Serial.println(command_selected);
     //Serial.print("Pin selected:");   
     //Serial.println(pin_selected);
     //Serial.print("State selected:");   
     //Serial.println(state_selected);   
     answer = "";
     
     }
     
     // Send commands
     if (command_selected && pin_selected && state_selected && !command_sent && api_key_received) {
       //Serial.println("Sending command: " + command + String(pin) + state);

       // Is the API key needed ?
       if (api_key == "" || api_key_match){

       // Start of message
       if (headers) {send_http_headers(serial);}

       // Mode selected
       if (command == "mode"){

         // Send feedback to client 
         serial.print("{\"message\": \"Pin D");
         serial.print(pin);
         
         // Input
         if (state == 'i'){
          
          // Set pin to Input     
          pinMode(pin,INPUT);
              
          // Send feedback to client
          serial.print(" set to input\", ");
         }

         // Output
         if (state == 'o'){

           // Set to Output  
           pinMode(pin,OUTPUT);
              
           // Send feedback to client
           serial.print(" set to output\", ");
         }

       }

       // Digital selected
       if (command == "digital") {
         if (state == 'r'){

           // Read from pin
           value = digitalRead(pin);

           // Send answer
           serial.print(F("{\"return_value\": "));
           serial.print(value);
           serial.print(F(", "));
         }
         else {

           // Apply on the pin      
           digitalWrite(pin,value);

           // Send feedback to client
           serial.print(F("{\"message\": \"Pin D"));
           serial.print(pin);
           serial.print(F(" set to "));
           serial.print(value);
           serial.print(F("\", "));
         }
       }

       // Analog selected
       if (command == "analog") {
         if (state == 'r'){
           
           // Read analog value
           value = analogRead(pin);
          
           // Send feedback to client
           serial.print(F("{\"return_value\": "));
           serial.print(value);
           serial.print(F(", "));
       }
       else {

         // Write output value
         analogWrite(pin,value);
 
         // Send feedback to client
         serial.print(F("{\"message\": \"Pin D"));
         serial.print(pin);
         serial.print(F(" set to "));
         serial.print(value);
         serial.print(F("\", "));

       }
      }

      // Variable selected
      if (command == "variable") {          

           // Send feedback to client
           serial.print(F("{\""));
           serial.print(int_variables_names[value]);
           serial.print(F("\": "));
           serial.print(*int_variables[value]);
           serial.print(F(", "));
      }

      // Function selected
      if (command == "function") {

        // Execute function
        int result = functions[value](arguments);

        // Send feedback to client
        serial.print(F("{\"return_value\": "));
        serial.print(result);
        serial.print(F(", \"message\": \""));
        serial.print(functions_names[value]);
        serial.print(F(" executed\", "));
      }

      if (command == "id") {
        serial.print("{");
      }

       // End of message
       serial.print(F("\"id\": \""));
       serial.print(id);
       serial.print(F("\", \"name\": \""));
       serial.print(name);
       serial.print(F("\", ")); 
       serial.print(F("\"connected\": true}"));
       serial.print(F("\r\n"));

       // End here
       command_sent = true;
      }
      else {

        // Send message
        if (headers) {send_http_headers(serial);}
        serial.print(F("{\"message\": \"API key invalid.\"}"));
        serial.print(F("\r\n"));

        // End here
        command_sent = true;
      }

     } 
       
   }
}

void variable(String variable_name, int *variable){

  int_variables[variables_index] = variable;
  int_variables_names[variables_index] = variable_name;
  variables_index++;

}

void function(String function_name, int (*f)(String)){

  functions_names[functions_index] = function_name;
  functions[functions_index] = f;
  functions_index++;
}

void set_id(String device_id){

  id = device_id;
}

void set_name(String device_name){
  
  name = device_name;
}

void set_api_key(String the_api_key){
  
  api_key = the_api_key;
}
  
private:
  String answer;
  String command;
  uint8_t pin;
  char state;
  int value;
  boolean pin_selected;
  boolean state_selected;
  boolean command_selected;
  boolean command_sent;
  boolean api_key_received;
  boolean api_key_match;
  String name;
  String id;
  String api_key;
  String arguments;

  // Variables arrays
  uint8_t variables_index;
  int * int_variables[NUMBER_VARIABLES];
  String int_variables_names[NUMBER_VARIABLES];

  // Functions array
  uint8_t functions_index;
  int (*functions[NUMBER_FUNCTIONS])(String);
  String functions_names[NUMBER_FUNCTIONS];
};