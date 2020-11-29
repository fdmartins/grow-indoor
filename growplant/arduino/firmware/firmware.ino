
#include <Adafruit_Sensor.h>                       // Biblioteca DHT Sensor Adafruit 
#include <DHT.h>
#include <DHT_U.h>
#include <OneWire.h>
#include <DallasTemperature.h> 
#include <AutoPID.h>  //autopid rayan
#include <SoftwareSerial.h> // para o dimmer.
#include "DS1307.h"

#define CONSOLE true
#define SERIAL_PLOTTER true

// Configuracao da Planta
#define temperature_air_target 25 //  temperatura objetivo do ambiente
#define temperature_soil_target 25 // temperatura objetivo do solo
#define light_level 2 // 1 = 1 lampada; 2 = 2 lampadas
#define light_time 12 // em horas / ver tempos ideias em cada ciclo da planta em: http://www.greenpower.net.br/blog/fotoperiodo/
#define air_recycle_seg 60*10  // tempo para troca de oxigenio da camera, repete a cada x segundos
#define water_seg 60       // tempo entre as regas, quando abaixo do nivel de umidade, em segundos.


// Sensor Suspenso Umidade e Temperatura
#define DHT_SENSOR_TEMP_HUMID_PIN 2               // Pino do Arduino conectado no Sensor(Data) 
#define CALIBRACAO_TEMP 0.4
#define DHTTYPE      DHT22                       // Sensor DHT21 ou (DHT22 e AM2302)
DHT_Unified dht(DHT_SENSOR_TEMP_HUMID_PIN, DHTTYPE);                  // configurando o Sensor DHT - pino e tipo

// Sensor de umidade do solo.
#define SOIL_HUMIDITY_PIN A0              

// Sensor temperatura interna.
#define SENSOR_DS18B20_PIN  3 //DEFINE O PINO DIGITAL UTILIZADO PELO SENSOR
OneWire ourWire(SENSOR_DS18B20_PIN); //CONFIGURA UMA INSTÂNCIA ONEWIRE PARA SE COMUNICAR COM O SENSOR
DallasTemperature sensors(&ourWire); //BIBLIOTECA DallasTemperature UTILIZA A OneWire

// Lampada Direita
#define LAMP1_PIN 7

// Lampada Esquerda
#define LAMP2_PIN 8

// Bomba Agua
#define WATER_PIN 5

// Ventilador
#define FAN_PIN 6


// configuracao serial dimmer
 /* Not all pins on the Mega and Mega 2560 support change interrupts,  so only the following can be used for RX:
 10, 11, 12, 13, 50, 51, 52, 53, 62, 63, 64, 65, 66, 67, 68, 69
 */
SoftwareSerial dimmerTemperatureSerial(10, 11); // RX=10, TX=11


// PID - controle temperatura.
#define OUTPUT_MIN -2.0 // limitamos a queda, pois usamos ventilador
#define OUTPUT_MAX +15 // maximo do dimmereh 22, mas por seguranca limitamos em 11, 50% da potencia da resistencia.
#define KP 0.5
#define KI 0.001
#define KD 0

double inputVal, setPoint, outputVal;
//input/output variables passed by reference, so they are updated automatically
AutoPID temperaturePID(&inputVal, &setPoint, &outputVal, OUTPUT_MIN, OUTPUT_MAX, KP, KI, KD);


int lastvaluedimmerok = -1;
uint32_t delayMS;                                  // variável para atraso no tempo
int32_t remain_air_recycle_seg; // variavel de tempo restante para troca de oxigenio da camera
int32_t remain_water_seg; // variavel de tempo restante para irrigacao

// outras variaveis
DS1307 clock;
 
void setup()
{
  delayMS = 800;  // tempo de ciclo loop principal - objetivo total eh 1 seg.
  
  Serial.begin(9600);                             // monitor serial 9600 bps
  dht.begin();                                    // inicializa a função

  dimmerTemperatureSerial.begin(9600);

  sensors.begin(); //INICIA O SENSOR

  clock.begin();

  pinMode(LAMP1_PIN, OUTPUT);
  pinMode(LAMP2_PIN, OUTPUT);
  pinMode(WATER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);

  digitalWrite(LAMP1_PIN, !LOW);
  digitalWrite(LAMP2_PIN, !LOW);
  digitalWrite(WATER_PIN, !LOW);
  digitalWrite(FAN_PIN, !LOW);

  //if temperature is more than 4 degrees below or above setpoint, 
  // OUTPUT will be set to min or max respectively
  //temperaturePID.setBangBang(4);
  //set PID update interval
  temperaturePID.setTimeStep(10*1000); // ms


  

   /*
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);           // imprime os detalhes do Sensor de Temperatura
  
  Serial.println("------------------------------------");
  Serial.println("Temperatura");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Valor max:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  Serial.print  ("Valor min:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  Serial.print  ("Resolucao:   "); Serial.print(sensor.resolution); Serial.println(" *C");
  Serial.println("------------------------------------");
 
  dht.humidity().getSensor(&sensor);            // imprime os detalhes do Sensor de Umidade
  Serial.println("------------------------------------");
  Serial.println("Umidade");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Valor max:    "); Serial.print(sensor.max_value); Serial.println("%");
  Serial.print  ("Valor min:    "); Serial.print(sensor.min_value); Serial.println("%");
  Serial.print  ("Resolucao:   "); Serial.print(sensor.resolution); Serial.println("%");
  Serial.println("------------------------------------");

  
  delayMS = sensor.min_delay / 5000;            // define o atraso baseado no tempo entre as leituras

   */
}


void displayDateNow(){
    clock.getTime();
    Serial.print(clock.hour, DEC);
    Serial.print(":");
    Serial.print(clock.minute, DEC);
    Serial.print(":");
    Serial.print(clock.second, DEC);
    Serial.print("  ");
    Serial.print(clock.month, DEC);
    Serial.print("/");
    Serial.print(clock.dayOfMonth, DEC);
    Serial.print("/");
    Serial.print(clock.year + 2000, DEC);
    Serial.print("*");
    switch (clock.dayOfWeek) { // Friendly printout the weekday
        case MON:
            Serial.print("MON");
            break;
        case TUE:
            Serial.print("TUE");
            break;
        case WED:
            Serial.print("WED");
            break;
        case THU:
            Serial.print("THU");
            break;
        case FRI:
            Serial.print("FRI");
            break;
        case SAT:
            Serial.print("SAT");
            break;
        case SUN:
            Serial.print("SUN");
            break;
    }
    Serial.println(" ");
}
 
void loop()
{
  //displayDateNow();

  if (Serial.available() > 0) {
    // read the incoming byte:
    int hours = Serial.read(); 
    int minutes = Serial.read(); 

    // say what you got:
    Serial.print(">>>> atualizando horario.... ");
    //Serial.println(hours);
    //Serial.println(minutes);

    clock.fillByHMS(hours, minutes, 00); 
    clock.fillByYMD(2020, 9, 28);
    clock.fillDayOfWeek(WED);
    clock.setTime();

    displayDateNow();

  }
  

  clock.getTime();

  
  delay(delayMS);                               // atraso entre as medições

  //Serial.println("============");

  // leitura da temperatura e umidade ambiente...
  
  sensors_event_t event;                        // inicializa o evento da Temperatura

  float temperature_air = 0;
  
  dht.temperature().getEvent(&event);           // faz a leitura da Temperatura
  if (isnan(event.temperature))                 // se algum erro na leitura
  {
    Serial.println("\n\n!!!!!! Erro na leitura da Temperatura !!!!!!!\n\n");
  }
  else                                          // senão
  {
    temperature_air = event.temperature - CALIBRACAO_TEMP;
  }

  float humidity_air = 0;
  
  dht.humidity().getEvent(&event);              // faz a leitura de umidade
  if (isnan(event.relative_humidity))           // se algum erro na leitura
  {
    Serial.println("\n\n!!!!!! Erro na leitura da Umidade !!!!!!!\n\n");
  }
  else                                          // senão
  {
    humidity_air = event.relative_humidity;
  }

  

  // leitura da umidade do solo.

  int umidade_solo_value = analogRead(SOIL_HUMIDITY_PIN);
  // y = -0,1538 x + 146,15


  int umidade_solo_percent = -0.1538*umidade_solo_value + 146.15;
  
  // temperatura do solo

  float temperature_soil=0;

  if (sensors.isConnected(SENSOR_DS18B20_PIN)){
    Serial.println("\n\n!!!! Falha Sensor Temperatura de Solo !!!!\n\n");
  }else{
    sensors.requestTemperatures();//SOLICITA QUE A FUNÇÃO INFORME A TEMPERATURA DO SENSOR
    temperature_soil = sensors.getTempCByIndex(0);
  }

  // diferenca de temperatura solo e ar
  float diff_temp_ambiente_solo = temperature_air - temperature_soil;  


  if(SERIAL_PLOTTER){
    
    Serial.print("$TIME:");
    Serial.print(clock.hour); Serial.print("h");
    Serial.print(clock.minute); Serial.print("m");
    Serial.print(clock.second); 
    Serial.print(", ");
    
    Serial.print("TemperaturaAir:"); 
    Serial.print(temperature_air);
    Serial.print(", ");

    Serial.print("TemperaturaAirTarget:"); 
    Serial.print(temperature_air_target);
    Serial.print(", ");

    

    Serial.print("UmidadeAir:"); 
    Serial.print(humidity_air);
    Serial.print(", ");
    
    Serial.print("TemperaturaSolo:"); 
    Serial.print(temperature_soil);
    Serial.print(", ");

    Serial.print("UmidadeSolo:"); 
    Serial.print(umidade_solo_percent);
    Serial.print(", ");


    Serial.print("SaidaDimmerTemp:");
    Serial.print(lastvaluedimmerok);
    Serial.print(", "); 

    Serial.print("LAMP:");
    Serial.print((1-digitalRead(LAMP1_PIN))*100);
    Serial.print(", "); 

    Serial.print("VENT:");
    Serial.print((1-digitalRead(FAN_PIN))*100);
    Serial.print(", "); 
    
    Serial.println();
  
    }




  // controle de temperatura...(pid)
  inputVal = temperature_soil;
  setPoint = temperature_soil_target;
  temperaturePID.run(); //call every loop, updates automatically at certain time interval configured.

  //analogWrite(PIN_FAN, outputVal);
  

  // true when we're at setpoint in tolerance +-1 degree
  if (temperaturePID.atSetPoint(0.5)) 
  {
    // temperatura OK!!
    //temperaturePID.reset(); // reseta valores do integral e derivativo, para evitar lentidao na retomada.
    if(CONSOLE) Serial.println("Temperatura OK!");

    if(outputVal<0){
      temperaturePID.reset();
    }
  }

  if(remain_air_recycle_seg>0)
  {
    if (outputVal<-0.8)
    {
      // resfriamento, liga ventilador.
      if(CONSOLE){ Serial.print("Ligando Ventilador: "); Serial.println(outputVal); }
  
      digitalWrite(FAN_PIN, !HIGH);
    }else{
      digitalWrite(FAN_PIN, !LOW);
    }
  }

  

    
    
  // aguecimento, liga resistencia aquecedor.
  int potencia = round(outputVal);
  
  if(potencia<=0)
  {
    if(CONSOLE){ Serial.println("Desligando Resistencia! "); }
    potencia = 0;
  }

  if(potencia>10) // por seguranca nao ultrapassamos 10 (pode ir ate 22) LIMTIADO NO PID!!
  {
    //if(CONSOLE){ Serial.print("Ligando Resistencia: "); Serial.println(outputVal); }
    //potencia = 10;
  }

  if(clock.hour<6 or clock.hour>18){
    //if(CONSOLE){ Serial.println("Temperatura desabilitada na madrugada. "); }
    //potencia = 0;
  }


  if(potencia!=lastvaluedimmerok)
  {
    if(CONSOLE){ Serial.print("Enviando para Dimmer..."); Serial.println(potencia);}
    
    byte bytes_to_send[4] =  { 0x02, 0x01, potencia, 0x03 }; // stx, command, potencia 0d a 22d 0x00 ate 0x16 , etx
    dimmerTemperatureSerial.write(bytes_to_send,4);

    //dimmerTemperatureSerial.
    if (dimmerTemperatureSerial.available()){
      dimmerTemperatureSerial.read();
      dimmerTemperatureSerial.read();
      int status = dimmerTemperatureSerial.read();
      dimmerTemperatureSerial.read();

      if(status!=6)
      {
        if(CONSOLE) Serial.println("Dimmer nao reconheceu comando!");
      }else{
        if(CONSOLE) Serial.println("Dimmer OK!");
        lastvaluedimmerok = potencia;
      }
      
    }else{
      if(CONSOLE) Serial.println("Sem resposta Dimmer");
    }
  }else{
    if(CONSOLE){Serial.print("Mantendo potencia do dimmer..."); Serial.println(lastvaluedimmerok);}
  }

  //Troca de oxigenio da camera
  remain_air_recycle_seg = remain_air_recycle_seg-1;

  if(CONSOLE){ Serial.print("Restante para Troca de Oxigenio "); Serial.println(remain_air_recycle_seg); }
  
  if(remain_air_recycle_seg<0){
    // hora de reciclar oxigenio
    digitalWrite(FAN_PIN, !HIGH);
    
    if(CONSOLE){ Serial.print("Ligando Ventilador para Troca de Oxigenio "); Serial.println(remain_air_recycle_seg); }    
  }
  
  if(remain_air_recycle_seg<-10)
  {
    remain_air_recycle_seg = air_recycle_seg;
    digitalWrite(FAN_PIN, !LOW);
    
    if(CONSOLE){ Serial.print("DESLIGANDO Ventilador para Troca de Oxigenio "); Serial.println(remain_air_recycle_seg); }
    
  }
    

  // controle de umidade/irrigacao...(pid)
  if(umidade_solo_percent<60){
    if(CONSOLE){ Serial.print("Restante tempo para Irrigar Novamente "); Serial.println(remain_water_seg); }
    
    remain_water_seg = remain_water_seg-1;
    if(remain_water_seg<0)
    {
      if(CONSOLE){ Serial.println("Irrigando!!"); }
      
      digitalWrite(WATER_PIN, !HIGH);
      delay(3*1000); // segundos ligado...
      digitalWrite(WATER_PIN, !LOW);
      remain_water_seg = water_seg;
    }

  }else{
    digitalWrite(WATER_PIN, !LOW);
  }

  // controle de luminosidade (temporal)
  if(clock.hour>=6 and clock.hour<6+light_time){
    digitalWrite(LAMP1_PIN, !HIGH);
    if(light_level==2) digitalWrite(LAMP2_PIN, !HIGH);
  }else{
    digitalWrite(LAMP1_PIN, !LOW);
    digitalWrite(LAMP2_PIN, !LOW);
  }
  
  
}
