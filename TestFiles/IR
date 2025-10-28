int led=2;                   //LED to digital pin 7  
int senRead=35;                 //Readings from sensor to analog pin 0  
int limit = 2000;            // play with system to find this limit (10K ohm resistor used)
void setup()    
 {  
  pinMode(led,OUTPUT);  
  digitalWrite(led, HIGH);
  delay(500);
  digitalWrite(led,LOW);      // LED initially off  
  Serial.begin(9600);         
 }  

int val = 0;
void loop()  
 {  
  val=analogRead(senRead);  //photodiode reading
  Serial.println(val);      
  if(val <= limit)             // NO OBSTICLE 
  {  
   digitalWrite(led,LOW);     // LED OFF
   delay(20);  
  }  
  else if(val > limit)        // OBSTICLE DETECTED  
  {  
   digitalWrite(led,HIGH);      //LED ON
   delay(20);  
  }  
 }  
