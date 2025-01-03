#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <HardwareSerial.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "apwifieeprommode.h"
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Pines de conexión del sensor TCS230
const int S0 = 32;
const int S1 = 33;
const int S2 = 26;
const int S3 = 25;
const int OUT = 27;


// Configuración de pines de botones (Pull-Down)
#define BOTON_SENSAR 15  // (boton rojo)
#define BOTON_MODO 2   // Cambiar modo (boton amarillo)
#define BOTON_JUGAR 4  // Confirmar selección (boton verde)
#define FIREBASE_HOST "appjuego-3a808-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyCZo0tb8khts6Yjp5d_BR69fdqPPbrygbI"
#define FIREBASE_URL "https://appjuego-3a808-default-rtdb.firebaseio.com"
#define USER_EMAIL "klebernunez2001@gmail.com"
#define USER_PASSWORD "kleber2001"


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
const long timeOffset = -5*(3600);

FirebaseData AppJuego;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;
FirebaseJson json;

bool estadoBotonModo = false;
bool estadoBotonSensar = false;
bool estadoBotonJugar = false; // Variables de estado de botones
bool botonModoPresionado = false;
bool botonSensarPresionado = false;
bool botonJugarPresionado = false; // Variables para verificar cambios de estado
volatile bool actualizarModo = false; // Indica si se debe actualizar el modo
volatile bool iniciarJuegoFlag = false; // Bandera para iniciar el juego


// Configuracion de Pines del LED RGB 
const int redPin = 5;
const int greenPin = 18;
const int bluePin = 19;


// Dirección del módulo I2C y configuración del LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Mensaje largo
const char* mensaje = "";


// Variables de juego
int rojo = 255, verde = 255, azul = 255;
int nivel = 1;
int puntaje = 0;
const int delayColor = 2000; // Tiempo de visualización de cada color en la secuencia
const int tiempoEspera = 5000; // Tiempo para que el usuario pase el objeto
String coloresNivel[3][9] = { {"Rojo", "Verde", "Azul"},
                              {"Rojo", "Verde", "Azul", "Amarillo", "Celeste", "Rosado"},
                              {"Rojo", "Verde", "Azul", "Amarillo", "Celeste", "Rosado", "Naranja", "Morado", "Blanco"}};
String secuenciaColores[10]; // Almacena la secuencia de colores en cada nivel
bool primerJuego = true;
int modoJuego = 0;   // Modo de juego actual
int modoConfirmado = 0;


void tareaActualizarLCD(void *pvParameters);
void tareaBotones(void *pvParameters);
void tareaJuego(void *pvParameters);
void TestHwm(char *taskName);


// Declaración e inialización de funciones
void mostrarSecuencia(int cantidadColores);
bool verificarSecuencia(int cantidadColores);
void color();
String detectarColor();
void setColor(int rojo, int verde, int azul);
void reproducirSonidoCorrecto();
void reproducirSonidoError();
void desplazarMensaje(const char* texto);
String substringLCD(const char* texto, int inicio, int longitudVisible);
void mensajeestatico(const char* msg1, const char* msg2);
void juegoDeMemoria();
void deteccionRapida();
void secuenciaInfinita();
void leer_botones();


void handleGetIP(){
  String ip = WiFi.localIP().toString();
  server.send(200, "text/plain", ip);
}

void handleGetMode(){
    String modo = String(modoJuego);
    server.send(200,"text/plain", modo);
}
void handleGetSelection(){
    String mensaje = String(modoConfirmado);
    server.send(200, "text/plain", mensaje);

}


void handlepuntos(){
    server.send(200,"text/plain", String(puntaje));
}

//ENVIAR DATOS A FIREBASE

void enviardatofirebase(){
  json.set("Modo de juego", modoConfirmado);
  json.set("Puntaje", puntaje);

  if(!timeClient.update()){
    timeClient.forceUpdate();
  }
  unsigned long currentEpoch = timeClient.getEpochTime();
  currentEpoch+=timeOffset;
  setTime(currentEpoch);

  String formatoFecha = String(year())+"-"+String(month())+"-"+String(day());
  String formatoHora = String(hour())+":"+String(minute())+":"+String(second()); 

  json.set("Tiempo actual", formatoFecha + MB_String(" ") + formatoHora );

  if(Firebase.pushJSON(AppJuego,"/resultado",json)){
    Serial.print("Datos enviados correctamente!");
  }else{
    Serial.print("Error al enviar datos");
    Serial.print(AppJuego.errorReason());
  }
}

void setup() { 
  Serial.begin(115200);

  intentoconexion("grupo2", "@admin123");

  server.on("/", handleRoot);
  server.on("/getSelection", handleGetSelection);
  server.on("/getMode", handleGetMode);
  server.on("/puntaje", handlepuntos);
  server.on("/getIP", handleGetIP);
  
  server.begin();
  timeClient.begin();


  xTaskCreate(tareaActualizarLCD, "tareaActualizarLCD", 2048, NULL, 1, NULL);
  xTaskCreate(tareaBotones, "Tarea Botones", 8192, NULL, 1, NULL);
  xTaskCreate(tareaJuego, "Tarea Juego", 8192, NULL, 1, NULL);

  pinMode(S0, OUTPUT); pinMode(S1, OUTPUT); pinMode(S2, OUTPUT); pinMode(S3, OUTPUT); pinMode(OUT, INPUT);
  pinMode(redPin, OUTPUT); pinMode(greenPin, OUTPUT); pinMode(bluePin, OUTPUT);

  Wire.begin(21, 22); // Configura SDA y SCL
  lcd.init();        // Inicializa el LCD
  lcd.backlight();   // Activa la luz de fondo
  
  // Configuración de la frecuencia del sensor
  digitalWrite(S0, HIGH);
  digitalWrite(S1, HIGH);

  // Configuración PWM del LED RGB
  ledcAttachPin(redPin, 0); ledcAttachPin(greenPin, 1); ledcAttachPin(bluePin, 2);
  ledcSetup(0, 5000, 8); ledcSetup(1, 5000, 8); ledcSetup(2, 5000, 8);
  
  // Configuración de pines de botones como entrada (Pull-Down)
  pinMode(BOTON_MODO, INPUT_PULLDOWN);
  pinMode(BOTON_JUGAR, INPUT_PULLDOWN);
  pinMode(BOTON_SENSAR, INPUT);
  
  setColor(255,255,255);

  
  
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.api_key = FIREBASE_AUTH;
  firebaseConfig.database_url = FIREBASE_URL;

  firebaseAuth.user.email = USER_EMAIL;
  firebaseAuth.user.password = USER_PASSWORD;

  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) {
    Serial.println("Firebase inicializado correctamente");
  } else {
    Serial.println("Error al inicializar Firebase");
    Serial.println(AppJuego.errorReason());
  }

  

  Serial.println("Juego iniciado");
}


// ******* FUNCIONES DEL LED RGB ************
//Funcion para controlar el LED RGB utilizando PWM
void setColor(int rojo, int verde, int azul) {
  ledcWrite(0, rojo); ledcWrite(1, verde); ledcWrite(2, azul);
}


// Funcion para mostrar los colores por medio del led RGB
void mostrarSecuencia(int cantidadColores) {
  for (int i = 0; i < cantidadColores; i++) {
    if      (secuenciaColores[i] == "Rojo")     setColor(0, 255, 255);     // Máximo rojo, sin verde ni azul
    else if (secuenciaColores[i] == "Verde")    setColor(255, 0, 255);     // Máximo verde, sin rojo ni azul
    else if (secuenciaColores[i] == "Azul")     setColor(255, 255, 0);     // Máximo azul, sin rojo ni verde
    else if (secuenciaColores[i] == "Amarillo") setColor(0, 0, 255);     // Medio en rojo y verde, azul en 0
    else if (secuenciaColores[i] == "Celeste")  setColor(255, 0, 0);   // Máximo en verde y alto en azul, rojo en 0
    else if (secuenciaColores[i] == "Rosado")   setColor(0, 235, 155);  // Alto en rojo, azul moderado, verde bajo
    else if (secuenciaColores[i] == "Blanco")   setColor(0, 0, 0); // Todos los colores
    else if (secuenciaColores[i] == "Naranja")  setColor(35, 171, 255);    // Rojo alto, verde moderado, azul en 0
    else if (secuenciaColores[i] == "Morado")   setColor(127, 255, 75);   // Rojo y azul moderados, verde en 0
    delay(delayColor);
    setColor(255, 255, 255); // Apaga el LED antes de mostrar el siguiente color
    delay(2000);
  }
}



// ******* FUNCIONES DEL SENSOR DE COLOR ************
// Funcion para sensar los 9 colores el nivel de color (red, green, blue)
void color() {
  // Limpiar los valores antes de leerlos
  // Leer cada color una vez
  digitalWrite(S2, LOW); digitalWrite(S3, LOW);
  rojo = pulseIn(OUT, digitalRead(OUT) == HIGH ? LOW : HIGH);

  digitalWrite(S2, HIGH); digitalWrite(S3, HIGH);
  verde = pulseIn(OUT, digitalRead(OUT) == HIGH ? LOW : HIGH);

  digitalWrite(S2, LOW); digitalWrite(S3, HIGH);
  azul = pulseIn(OUT, digitalRead(OUT) == HIGH ? LOW : HIGH);

  // Mostrar los valores en el monitor serial para ajustar umbrales
  Serial.print("Lectura - Rojo: "); Serial.print(rojo);
  Serial.print(" Verde: "); Serial.print(verde);
  Serial.print(" Azul: "); Serial.println(azul);
}


// Funcion para validar que el color de la secuencia sea el mismo que el sensado
bool verificarSecuencia(int cantidadColores) {
  for (int i = 0; i < cantidadColores; i++) {
    Serial.println("Pasa el color correspondiente...");
    mensajeestatico("Pasa el color", "correspondiente...");

    String colorDetectado = "Desconocido";
    unsigned long tiempoInicio = millis();

    // Sensar el color durante 1 segundo
    while (millis() - tiempoInicio < 1000) {
      color();  
      String nuevoColor = detectarColor();
      if (nuevoColor != "Desconocido") {
        colorDetectado = nuevoColor; 
      }
    }

    // Mostrar resultado
    Serial.print("Color detectado: "); Serial.print(colorDetectado);
    Serial.print(" | Color esperado: "); Serial.println(secuenciaColores[i]);

    desplazarMensaje(("Color detectado: " + (MB_String(colorDetectado))).c_str());

    if (colorDetectado == secuenciaColores[i]) {
      int puntosGanados = (nivel == 1 ? 5 : (nivel == 2 ? 10 : 15));
      puntaje += puntosGanados;
      Serial.print("Correcto! Puntos ganados: "); Serial.println(puntosGanados);
      mensajeestatico("Correcto!", ("Puntos: " + MB_String(puntosGanados)).c_str());
    } else {
      mensajeestatico("Te equivocaste!", ("Puntaje: " + MB_String(puntaje)).c_str());
      desplazarMensaje(("Color esperado: " + (MB_String(secuenciaColores[i]))).c_str());
      Serial.print("¡Te equivocaste! Puntuación final: "); Serial.println(puntaje);
      enviardatofirebase();
      // Reiniciar el juego
      puntaje = 0; 
      nivel = 1;  
      primerJuego = true;
      return false;  // El jugador falló
    }
  }

  Serial.print("¡Nivel superado! Puntuación total: "); Serial.println(puntaje);
  mensajeestatico("Nivel superado!", ("Puntaje final: " + MB_String(puntaje)).c_str()); 
  handlepuntos();
  enviardatofirebase();
  return true;  // Nivel completado correctamente
}




// Funcion asignar un string dependiendo del color detectado por el sensor
String detectarColor() {
  color();
  if      ( (rojo >=6  && rojo <=8)  && (verde >=17 && verde <=22) && (azul >=13 && azul <=18))  return "Rojo"; 
  else if ( (rojo >=19 && rojo <=27) && (verde >=15 && verde <=20) && (azul >=8  && azul <=11))  return "Azul";
  else if ( (rojo >=12 && rojo <=15) && (verde >=9  && verde <=13) && (azul >=13 && azul <=18))  return "Verde";
  else if ( (rojo >=4  && rojo <=6)  && (verde >=6  && verde <=9)  && (azul >=11 && azul <=15))  return "Amarillo";
  else if ( (rojo >=19 && rojo <=27) && (verde >=10 && verde <=14) && (azul >=6  && azul <=8 ))  return "Celeste";
  else if ( (rojo >=6  && rojo <=10) && (verde >=15 && verde <=23) && (azul >=8  && azul <=13))  return "Rosado";
  else if ( (rojo >=5  && rojo <=7)  && (verde >=5  && verde <=7)  && (azul >=4  && azul <=5 ))  return "Blanco";
  else if ( (rojo >=8  && rojo <=10) && (verde >=18 && verde <=23) && (azul >=17 && azul <=21))  return "Naranja";
  else if ( (rojo >=13 && rojo <=18) && (verde >=19 && verde <=25) && (azul >=10 && azul <=14))  return "Púrpura";
  return "Desconocido";  // Retorna "Desconocido" si no detecta correctamente
}





// ******* FUNCIONES DEL LCD ************
//Funciones para mostrar un mensaje desplzandose en el LCD
void desplazarMensaje(const char* texto) {
  int longitud = strlen(texto); // Calcula la longitud del mensaje
  int espacios = 16;           // Espacios visibles en la pantalla

  for (int i = 0; i < longitud + espacios; i++) {
    lcd.clear();
    // Imprime la porción del texto visible
    lcd.setCursor(0, 0);
    lcd.print(substringLCD(texto, i, espacios));
    delay(500); // Velocidad del desplazamiento
  }
}


// Función para extraer una subcadena visible en el LCD
String substringLCD(const char* texto, int inicio, int longitudVisible) {
  String subcadena = "";
  // Añade espacios al principio si inicio es menor que 0
  for (int i = inicio; i < 0 && i < longitudVisible; i++) {
    subcadena += " ";
  }
  // Añade caracteres del texto si están dentro del rango válido
  for (int i = max(0, inicio); i < inicio + longitudVisible && i < (int)strlen(texto); i++) {
    subcadena += texto[i];
  }
  // Añade espacios al final si falta completar la longitudVisible
  for (int i = subcadena.length(); i < longitudVisible; i++) {
    subcadena += " ";
  }
  return subcadena;
}



// Tarea para actualizar el mensaje en el LCD
void tareaActualizarLCD(void *pvParameters) {
    for (;;) {
        if (actualizarModo) { // Verifica si hay un cambio en el modo
            // Actualizar el mensaje correspondiente al modo
            Serial.println("Actualizando LCD...");  // Depuración

            switch (modoJuego) {
                case 1:
                    mensajeestatico("Modo 1:", "Juego de Memoria");
                    desplazarMensaje("Juego de Memoria");
                    break;
                case 2:
                    mensajeestatico("Modo 2:", "Detección Rápida");
                    desplazarMensaje("Detección Rápida");
                    break;
                case 3:
                    mensajeestatico("Modo 3:", "Secuencia Infinita");
                    desplazarMensaje("Secuencia Infinita");
                    break;
            }
            actualizarModo = false; // Resetea la bandera después de procesar
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Retardo pequeño para ceder control
    }
}
  


// Función para mostrar un mensaje estático en toda la pantalla LCD
void mensajeestatico(const char* msg1, const char* msg2){
  Serial.println("==================");
  Serial.print("Mensaje estático: ");
  Serial.println(msg1);
  Serial.println(msg2);
  Serial.println("==================");
  lcd.init();        // Inicializa el LCD
  lcd.backlight();   // Activa la luz de fondo
  lcd.setCursor(0, 0); // Columna 3, Fila 0
  lcd.print(msg1);
  lcd.setCursor(0, 1); // Columna 2, Fila 1
  lcd.print(msg2);
  delay(500);
  lcd.clear(); // Limpia la pantalla
}






// ******* FUNCIONES DEL JUEGO ************
// Función para cambiar de modo
void cambiarModo() {
  modoJuego++;
  if (modoJuego > 3) modoJuego = 1;  // Volver al modo 1 después del 3
    modoConfirmado = 0;  // Esto reinicia el valor de selección
    puntaje = 0;
  // Mostrar el modo seleccionado
    Serial.print("Modo cambiado a: ");
    Serial.println(modoJuego);
    actualizarModo = true;            // Señal para que las tareas actúen

}

void confirmarModo() {
  modoConfirmado = modoJuego; // Actualizar el modo confirmado
  Serial.print("Modo seleccionado: ");
  if (modoJuego == 1) {
    Serial.println("Juego de Memoria");
    
  } 
  else if (modoJuego == 2) {
    Serial.println("Detección Rápida");

  }
  else if (modoJuego == 3) {
    Serial.println("Secuencia Infinita");
  }



}
//////////// Modo 1: Juego de Memoria ////////////
void juegoDeMemoria() {
  Serial.println("Modo 1: Juego de Memoria Iniciado.");
  mensajeestatico("Modo 1", "Iniciado...");

  // Bucle para avanzar entre niveles
  while (nivel <= 3) {
    int cantidadColores = (nivel == 1) ? 3 : (nivel == 2) ? 6 : 9;

    mensajeestatico("Nivel: ", String(nivel).c_str());
    Serial.print("Nivel "); Serial.println(nivel);

    mensajeestatico("Secuencia de", "colores...");
    Serial.print("Secuencia de colores: ");

    // Generar secuencia aleatoria
    for (int i = 0; i < cantidadColores; i++) {
      int colorIndex = random(0, cantidadColores);  
      secuenciaColores[i] = coloresNivel[nivel - 1][colorIndex];
      Serial.print(secuenciaColores[i]+" ");
    }
    Serial.println();

    // Mostrar y verificar la secuencia
    mostrarSecuencia(cantidadColores);
    if (!verificarSecuencia(cantidadColores)) {
      return;  // Salir si el jugador falla
    }

    // Si se supera el nivel, se avanza
    nivel++;
  }
  puntaje = 2;
  // Mostrar mensaje de victoria total
  mensajeestatico("¡Juego Terminado!", ("Puntaje: " + MB_String(puntaje)).c_str());
  Serial.println("¡Juego Terminado!");
  handlepuntos();
  enviardatofirebase();


}




//////////// Modo 2: Detección Rápida ////////////
void deteccionRapida() {
  Serial.println("Modo 2: Deteccion Rapida Iniciada.");
  mensajeestatico("Modo 2" , "Iniciado...");
  delay(500);
  // Lógica del modo 2
  int intentos = 5;  // Número de intentos permitidos
  for (int i = 0; i < intentos; i++) {
    String colorObjetivo = coloresNivel[0][random(0, 3)];
    mensajeestatico("Coloca: ", colorObjetivo.c_str());
    Serial.print("Coloca: "); Serial.println(colorObjetivo);

    // Mostrar el color en el LED RGB
    if (colorObjetivo == "Rojo") setColor(255, 0, 0);
    else if (colorObjetivo == "Verde") setColor(0, 255, 0);
    else if (colorObjetivo == "Azul") setColor(0, 0, 255);

    unsigned long tiempoInicio = millis();
    while (true) {
      color();  // Leer color del sensor
      String colorDetectado = detectarColor();
      if (colorDetectado != "Desconocido") {
        unsigned long tiempoRespuesta = millis() - tiempoInicio;

        if (colorDetectado == colorObjetivo) {
          int puntosGanados = max(15 - (int)(tiempoRespuesta / 10), 5);  // Puntaje según rapidez
          puntaje += puntosGanados;

          Serial.print("Correcto! Tiempo: ");
          Serial.print(tiempoRespuesta); Serial.print(" ms | Puntos: ");
          Serial.println(puntosGanados);
          mensajeestatico("Correcto! ", ("Puntos: " + MB_String(puntosGanados)).c_str());
        } else {
          Serial.println("Te equivocaste!");
          mensajeestatico("Te equivocaste!", " ");
        }
        break;
      }
    }
    delay(500);  // Pausa entre intentos
    setColor(0, 0, 0);  // Apagar LED
  }
  puntaje = 3;
  mensajeestatico("Juego Finalizado!", ("Puntaje Total: " + MB_String(puntaje)).c_str());
  Serial.println("Juego Finalizado!");
  handlepuntos();
  enviardatofirebase();

}


//////////// Modo 3: Secuencia Infinita ////////////
void secuenciaInfinita() {
  Serial.println("Modo 3: Secuencia Infinita Iniciada.");
  mensajeestatico("Modo 3" , "Iniciado...");
  delay(1000);
  // Lógica del modo 3
  bool jugando = true;
  while (jugando) {
    String colorObjetivo = coloresNivel[2][random(0, 9)];
    mensajeestatico("Coloca: ", colorObjetivo.c_str());
    Serial.print("Coloca: "); Serial.println(colorObjetivo);
    // Mostrar el color en el LED RGB
    if (colorObjetivo == "Rojo") setColor(255, 0, 0);
    else if (colorObjetivo == "Verde") setColor(0, 255, 0);
    else if (colorObjetivo == "Azul") setColor(0, 0, 255);
    else if (colorObjetivo == "Amarillo") setColor(255, 255, 0);
    else if (colorObjetivo == "Celeste") setColor(0, 255, 255);
    else if (colorObjetivo == "Rosado") setColor(255, 20, 100);
    else if (colorObjetivo == "Blanco") setColor(255, 255, 255);
    else if (colorObjetivo == "Naranja") setColor(220, 84, 0);
    else if (colorObjetivo == "Morado") setColor(128, 0, 180);
    delay(2000);  // Tiempo de visualización
    setColor(0, 0, 0);  // Apagar LED
    delay(2000);
    // Leer la respuesta del jugador
    color();  // Leer el sensor
    String colorDetectado = detectarColor();
    Serial.print("Color detectado: "); Serial.println(colorDetectado);
    delay(2000);
    if (colorDetectado == colorObjetivo) {
      puntaje += 10;
      mensajeestatico("Correcto!", ("Puntos: " + MB_String(puntaje)).c_str());
      Serial.println("Correcto!");
    } else {
      Serial.println("Te equivocaste!");
      mensajeestatico("Te equivocaste!", " ");
      jugando = false;  // Terminar el juego
    }
    delay(1000);  // Pausa antes de la siguiente secuencia
  }
  puntaje = 4;
  mensajeestatico("Juego Terminado!", ("Puntaje Total: " + MB_String(puntaje)).c_str());
  Serial.println("Juego Terminado!");
  handlepuntos();
  enviardatofirebase();

}


// Función para iniciar el juego según el modo
void iniciarJuego() {
  switch (modoJuego) {
    case 1:
      mensajeestatico("Modo 1" , "Iniciado...");
      delay(500);
      juegoDeMemoria();  // Modo 1
      break;
    case 2:
      deteccionRapida();  // Modo 2
      break;
    case 3:
      secuenciaInfinita();  // Modo 3
      break;
  }
}

void TestHwm(char *taskName){
  static int stack_hwm, stack_hwm_temp;

  stack_hwm_temp = uxTaskGetStackHighWaterMark(nullptr);
  if(!stack_hwm || (stack_hwm_temp < stack_hwm)){
    stack_hwm=stack_hwm_temp;
    Serial.printf("%s has stack hwm %u\n",taskName,stack_hwm);
  }
} 

void tareaBotones(void *pvParameters) {
  pinMode(BOTON_JUGAR, INPUT_PULLDOWN);
  pinMode(BOTON_MODO, INPUT_PULLDOWN);

  bool botonModoPresionado = false;
  bool botonJugarPresionado = false;

  for (;;) {
    bool lecturaBotonModo = digitalRead(BOTON_MODO);
    bool lecturaBotonJugar = digitalRead(BOTON_JUGAR);

    // Detectar transición de bajo a alto para cambiar modo
    if (lecturaBotonModo && !botonModoPresionado) {
      botonModoPresionado = true;
    }
    if (!lecturaBotonModo && botonModoPresionado) {
      botonModoPresionado = false;
      cambiarModo();
    }

    // Detectar transición de bajo a alto para confirmar modo
    if (lecturaBotonJugar && !botonJugarPresionado) {
      botonJugarPresionado = true;
    }
    if (!lecturaBotonJugar && botonJugarPresionado) {
      botonJugarPresionado = false;
      confirmarModo();
      iniciarJuegoFlag = true; // Indicamos que debe iniciarse el juego
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Evitar lectura constante
  }
}

// Tarea para manejar la lógica del juego
void tareaJuego(void *pvParameters) {
  for (;;) {
    if (iniciarJuegoFlag) {
      iniciarJuegoFlag = false; // Resetear bandera
      iniciarJuego();
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Esperar un poco antes de volver a verificar
  }
}


// ******* LOOP ************
  void loop() {    
    
    loopAP();
    server.handleClient();  // Manejar las solicitudes HTTP
  } 

