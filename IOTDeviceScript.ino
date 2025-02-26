#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>
#include <DHT.h>

 
// Definiciones

// Pin del sensor de temperatura y humedad
#define DHTPIN 0
// Tipo de sensor de temperatura y humedad
#define DHTTYPE DHT11
// Intervalo en segundo de las mediciones
#define MEASURE_INTERVAL 2
// Duración aproximada en la pantalla de las alertas que se reciban
#define ALERT_DURATION 60

// Define las caracteristicas del LCD
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define I2C_ADDRESS 0x27
#define LED D0
#define BUZZER D4

// Declaraciones

// Sensor DHT
DHT dht(DHTPIN, DHTTYPE);
// Pantalla LCD
LiquidCrystal_I2C display(I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);  
// Cliente WiFi
WiFiClient net;
// Cliente MQTT
PubSubClient client(net);


// Variables a editar TODO

// WiFi
// Nombre de la red WiFi
const char ssid[] = "FAMILIA BATIOJA"; // TODO cambiar por el nombre de la red WiFi
// Contraseña de la red WiFi
const char pass[] = "1023884654"; // TODO cambiar por la contraseña de la red WiFi

//Conexión a Mosquitto
#define USER "user1" // TODO Reemplace UsuarioMQTT por un usuario (no administrador) que haya creado en la configuración del bróker de MQTT.
const char MQTT_HOST[] = "44.203.22.83"; // TODO Reemplace ip.maquina.mqtt por la IP del bróker MQTT que usted desplegó. Ej: 192.168.0.1
const int MQTT_PORT = 8082;
const char MQTT_USER[] = USER;
//Contraseña de MQTT
const char MQTT_PASS[] = "123456"; // TODO Reemplace ContrasenaMQTT por la contraseña correpondiente al usuario especificado.

//Tópico al que se recibirán los datos
// El tópico de publicación debe tener estructura: <país>/<estado>/<ciudad>/<usuario>/out
const char MQTT_TOPIC_PUB[] = "colombia/bogota/bogota/" USER "/out"; //TODO Reemplace el valor por el tópico de publicación que le corresponde.
// El tópico de suscripción debe tener estructura: <país>/<estado>/<ciudad>/<usuario>/in
const char MQTT_TOPIC_SUB[] = "colombia/bogota/bogota/" USER "/in"; //TODO Reemplace el valor por el tópico de suscripción que le corresponde.

// Declaración de variables globales

// Timestamp de la fecha actual.
time_t now;
// Tiempo de la última medición
long long int measureTime = millis();
// Tiempo en que inició la última alerta
long long int alertTime = millis();
// Mensaje para mostrar en la pantalla
String alert = "";
// Valor de la medición de temperatura
float temp;
// Valor de la medición de la humedad
float humi;
bool showMeasure = true;
int fakeSensor = 0;

/**
 * Conecta el dispositivo con el bróker MQTT usando
 * las credenciales establecidas.
 * Si ocurre un error lo imprime en la consola.
 */
void mqtt_connect()
{
  //Intenta realizar la conexión indefinidamente hasta que lo logre
  while (!client.connected()) {
    
    Serial.print("MQTT connecting ... ");
    
    if (client.connect(MQTT_USER, MQTT_USER, MQTT_PASS)) {
      
      Serial.println("connected.");
      client.subscribe(MQTT_TOPIC_SUB);
      
    } else {
      
      Serial.println("Problema con la conexión, revise los valores de las constantes MQTT");
      displayError("No conexion MQTT");
      int state = client.state();
      Serial.print("Código de error = ");
      alert = "MQTT error: " + String(state);
      Serial.println(state);
      
      if ( client.state() == MQTT_CONNECT_UNAUTHORIZED ) {
        ESP.deepSleep(0);
      }
      
      // Espera 5 segundos antes de volver a intentar
      delay(5000);
    }
  }
}

/**
 * Publica la temperatura y humedad dadas al tópico configurado usando el cliente MQTT.
 */
void sendSensorData(float temperatura, float humedad) {
  String data = "{";
  data += "\"temperatura\": "+ String(temperatura, 1) +", ";
  data += "\"humedad\": "+ String(humedad, 1);
  data += "}";
  char payload[data.length()+1];
  data.toCharArray(payload,data.length()+1);
  
  client.publish(MQTT_TOPIC_PUB, payload);
}


/**
 * Lee la temperatura del sensor DHT, la imprime en consola y la devuelve.
 */
float readTemperatura() {
  
  // Se lee la temperatura en grados centígrados (por defecto)
  float t = dht.readTemperature();
  
  //Serial.print("Temperatura: ");
  //Serial.print(t);
  //Serial.println(" *C ");
  
  return t;
}

/**
 * Lee la humedad del sensor DHT, la imprime en consola y la devuelve.
 */
float readHumedad() {
  // Se lee la humedad relativa
  float h = dht.readHumidity();
  
  //Serial.print("Humedad: ");
  //Serial.print(h);
  //Serial.println(" %\t");

  return h;
}

/**
 * Verifica si las variables ingresadas son números válidos.
 * Si no son números válidos, se imprime un mensaje en consola.
 */
bool checkMeasures(float t, float h) {
  // Se comprueba si ha habido algún error en la lectura
    if (isnan(t) || isnan(h)) {
      Serial.println("Error obteniendo los datos del sensor DHT11");
      return false;
    }
    return true;
}

/**
 * Vincula la pantalla al dispositivo y asigna el color de texto blanco como predeterminado.
 * Si no es exitosa la vinculación, se muestra un mensaje en consola.
 */
void startDisplay() {
  display.init();
  display.backlight();
}

void printLcd(String message, int row) {
  if (row == 0) {
    display.setCursor(0,0);  
  } else {
    display.setCursor(0,1);
  }
  int size = message.length();
  for (int i = size; i < LCD_COLUMNS; i++) {
    message = message + ' ';
  }
  display.print(message);
}

/**
 * Imprime en la pantallaa un mensaje de "No hay señal".
 */
void displayNoSignal() {
  printLcd("No hay señal", 0);
}

void displayError(String error) {
  printLcd(error, 0);
}

/**
 * Agrega a la pantalla el header con mensaje "IOT Sensors" y en seguida la hora actual
 */
void displayHeader() {
  long long int milli = now + millis() / 1000;
  struct tm* tinfo;
  tinfo = localtime(&milli);
  String hour = String(asctime(tinfo)).substring(11, 19);
  
  String title = "IOT: " + hour;
  if (alert.equals("")) {
    printLcd(title, 0);
  }
}

/**
 * Agrega los valores medidos de temperatura y humedad a la pantalla.
 */
void displayMeasures() {
  String t = "T:";
  t.concat(temp);
  String h = "H:";
  h.concat(humi);
  printLcd(t + " " + h, 1);
}

/**
 * Agrega el mensaje indicado a la pantalla.
 * Si el mensaje es OK, se busca mostrarlo centrado.
 */
void displayMessage(String message) {
  if (!message.equals("OK")) {
    if (message.startsWith("ALERT2")) {
      digitalWrite(BUZZER, HIGH);
    }
    printLcd("Msg:" + message, 0);
    digitalWrite(LED, HIGH);
    
  } else {
    digitalWrite(LED, LOW);
    digitalWrite(BUZZER, LOW);
  }
}

/**
 * Muestra en la pantalla el mensaje de "Connecting to:" 
 * y luego el nombre de la red a la que se conecta.
 */
void displayConnecting(String ssid) {
  display.clear();
  printLcd("Connecting to:", 0);
  printLcd(ssid, 1);
}

/**
 * Verifica si ha llegdo alguna alerta al dispositivo.
 * Si no ha llegado devuelve OK, de lo contrario retorna la alerta.
 * También asigna el tiempo en el que se dispara la alerta.
 */
String checkAlert() {
  String message = "OK";
  
  if (alert.length() != 0) {
    message = alert;
    if ((millis() - alertTime) >= ALERT_DURATION * 1000 ) {
      alert = "";
      alertTime = millis();
     }
  }
  return message;
}

/**
 * Función que se ejecuta cuando llega un mensaje a la suscripción MQTT.
 * Construye el mensaje que llegó y si contiene ALERT lo asgina a la variable 
 * alert que es la que se lee para mostrar los mensajes.
 */
void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  String data = "";
  for (int i = 0; i < length; i++) {
    data += String((char)payload[i]);
  }
  Serial.print(data);
  if (data.indexOf("ALERT") >= 0) {
    alert = data;
  }
}

/**
 * Verifica si el dispositivo está conectado al WiFi.
 * Si no está conectado intenta reconectar a la red.
 * Si está conectado, intenta conectarse a MQTT si aún 
 * no se tiene conexión.
 */
void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Checking wifi");
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass);
      Serial.print(".");
      displayNoSignal();
      delay(10);
    }
    Serial.println("connected");
  }
  else
  {
    if (!client.connected())
    {
      mqtt_connect();
    }
    else
    {
      client.loop();
    }
  }
}

/**
 * Imprime en consola la cantidad de redes WiFi disponibles y
 * sus nombres.
 */
void listWiFiNetworks() {
  int numberOfNetworks = WiFi.scanNetworks();
  Serial.println("\nNumber of networks: ");
  Serial.print(numberOfNetworks);
  for(int i =0; i<numberOfNetworks; i++){
      Serial.println(WiFi.SSID(i));
 
  }
}

/**
 * Inicia el servicio de WiFi e intenta conectarse a la red WiFi específicada en las constantes.
 */
void startWiFi() {
  
  WiFi.hostname(USER);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  
  Serial.println("(\n\nAttempting to connect to SSID: ");
  Serial.print(ssid);
  //Intenta conectarse con los valores de las constantes ssid y pass a la red Wifi
  //Si la conexión falla el node se dormirá hasta que lo resetee
  while (WiFi.status() != WL_CONNECTED)
  {
    if ( WiFi.status() == WL_NO_SSID_AVAIL ) {
      Serial.println("\nNo se encuentra la red WiFi ");
      Serial.print(ssid);
      WiFi.begin(ssid, pass);
    } else if ( WiFi.status() == WL_WRONG_PASSWORD ) {
      Serial.println("\nLa contraseña de la red WiFi no es válida.");
    } else if ( WiFi.status() == WL_CONNECT_FAILED ) {
      Serial.println("\nNo se ha logrado conectar con la red, resetee el node y vuelva a intentar");
      WiFi.begin(ssid, pass);
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println("connected!");
}

/**
 * Consulta y guarda el tiempo actual con servidores SNTP.
 */
void setTime() {
  //Sincroniza la hora del dispositivo con el servidor SNTP (Simple Network Time Protocol)
  Serial.print("Setting time using SNTP");
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  //Una vez obtiene la hora, imprime en el monitor el tiempo actual
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

/**
 * Configura el servidor MQTT y asigna la función callback para mensajes recibidos por suscripción.
 * Intenta conectarse al servidor.
 */
void configureMQTT() {
  //Llama a funciones de la librería PubSubClient para configurar la conexión con Mosquitto
  client.setServer(MQTT_HOST, MQTT_PORT);
  
  // Se configura la función que se ejecutará cuando lleguen mensajes a la suscripción
  client.setCallback(receivedCallback);
  
  //Llama a la función de este programa que realiza la conexión con Mosquitto
  mqtt_connect();
}

/**
 * Verifica si ya es momento de hacer las mediciones de las variables.
 * si ya es tiempo, mide y envía las mediciones.
 */
void measure() {
  if ((millis() - measureTime) >= MEASURE_INTERVAL * 1000 ) {
    //Serial.println("\nMidiendo variables...");
    measureTime = millis();
    
    temp = readTemperatura();
    humi = readHumedad();

    if(fakeSensor == 1) {
      temp = 25.00;
    } else if(fakeSensor == 2) {
      temp = 30.10;
    }
    

    // Se chequea si los valores son correctos
    if (checkMeasures(temp, humi)) {
      // Se envían los datos
      sendSensorData(temp, humi); 
    }
  }
}

void resetMessage() {
  if ((millis() - alertTime) >= ALERT_DURATION * 1000 ) {
    
  }
}

/////////////////////////////////////
//         FUNCIONES ARDUINO       //
/////////////////////////////////////

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED, LOW);
  digitalWrite(BUZZER, LOW);
  Serial.begin(115200);

  listWiFiNetworks();

  startDisplay();

  displayConnecting(ssid);

  startWiFi();

  dht.begin();

  setTime();

  configureMQTT(); 
}

void loop() {
  checkWiFi();
  String message = checkAlert();
  measure();
  if (Serial.available()){
    int readSerial = Serial.parseInt();
    if (readSerial == 1 || readSerial == 2 || readSerial == 3) {
      fakeSensor = readSerial;
      Serial.println(fakeSensor);
    }
  }

  displayHeader();
  displayMeasures();
  displayMessage(message);
  resetMessage();
}
