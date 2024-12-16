#include <Arduino.h>
#include "apwifieeprommode.h"
#include <EEPROM.h>
    

// Configuración de pines de botones (Pull-Down)
#define BOTON_SENSAR 25 
#define BOTON_MODO 26   // Cambiar modo (boton amarillo)
#define BOTON_JUGAR 27  // Confirmar selección (boton verde)
bool estadoBotonModo = false;
bool estadoBotonSensar = false;
bool estadoBotonJugar = false; // Variables de estado de botones
bool botonModoPresionado = false;
bool botonSensarPresionado = false;
bool botonJugarPresionado = false; // Variables para verificar cambios de estado


// Configuracion de Pines del LED RGB 
const int redPin = 5;
const int greenPin = 18;
const int bluePin = 19;

// Variables de juego
int rojo = 255, verde = 255, azul = 255;  // RGB de ánodo común (valor más bajo, más brillante)
int nivel = 1;
int puntaje = 0;
int modoConfirmado = 0;
const int delayColor = 2000; // Tiempo de visualización de cada color en la secuencia
const int tiempoEspera = 5000; // Tiempo para que el usuario pase el objeto
String coloresNivel[3][9] = { {"Rojo", "Verde", "Azul"},
                              {"Rojo", "Verde", "Azul", "Amarillo", "Celeste", "Rosado"},
                              {"Rojo", "Verde", "Azul", "Amarillo", "Celeste", "Rosado", "Naranja", "Morado", "Blanco"}};
String secuenciaColores[10]; // Almacena la secuencia de colores en cada nivel
bool primerJuego = true;
int modoJuego = 0;   // Modo de juego actual

// funcion modo juego 

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
    Serial.println("Modo confirmado enviado al cliente: " + mensaje);

}
void handleGetSecuencia(){
  String mensaje = "";
    for (int i = 0; i < 10; i++) {
        if (secuenciaColores[i] != "") {
            mensaje += secuenciaColores[i] + ",";
        }
    }
    mensaje.remove(mensaje.length() - 1);  // Eliminar la última coma extra
    server.send(200, "text/plain", mensaje);
}
/*
void setmode(){
    String modo = String(modoJuego);
    server.send(200,"text/plain", modo);
    server.send(200, "text/plain", "Modo actualizado exitosamente");
}

*/


void setColor(int rojo, int verde, int azul) {
  ledcWrite(0, rojo); ledcWrite(1, verde); ledcWrite(2, azul);
}



// ******* FUNCIONES DEL LED RGB ************
//Funcion para controlar el LED RGB utilizando PWM



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

// ******* FUNCIONES DEL JUEGO ************
// Función para cambiar de modo
void cambiarModo() {
  modoJuego++;
  if (modoJuego > 3) modoJuego = 1;  // Volver al modo 1 después del 3
  // Mostrar el modo seleccionado
  Serial.print("Modo de juego: ");
  if (modoJuego == 1) {Serial.println("1 - Juego de Memoria");}
  else if (modoJuego == 2) {Serial.println("2 - Detección Rápida");}
  else if (modoJuego == 3) {Serial.println("3 - Secuencia Infinita");}
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
  delay(500);
  // Lógica del modo 1
  int cantidadColores = nivel + 2; // Aumenta la longitud de la secuencia con cada nivel
  Serial.print("Nivel "); Serial.println(nivel);
  Serial.print("Secuencia de colores: ");
  
  for (int i = 0; i < cantidadColores; i++) {
    int colorIndex = random(0, cantidadColores); // Selección aleatoria entre Rojo, Verde, Azul
    secuenciaColores[i] = coloresNivel[nivel-1][colorIndex];
    Serial.print(secuenciaColores[i] + " ");
  }
  Serial.println();
  mostrarSecuencia(cantidadColores);
}


void iniciarJuego() {
  switch (modoJuego) {
    case 1:
      juegoDeMemoria();  // Modo 1
      break;
    case 2:
      // deteccionRapida();  // Modo 2
      break;
    case 3:
      // secuenciaInfinita();  // Modo 3
      break;
  }
}


void btnaceptar(){
  if (modoConfirmado == 1) {
    juegoDeMemoria();
    server.send(200, "text/plain", "Modo 1 iniciado.");
  } else {
    server.send(200, "text/plain", "El modo seleccionado no es el 1.");
  }
}

void setup()  
{
    Serial.begin(115200);
    intentoconexion("group2", "@admin123");
    server.on("/", handleRoot);
    server.on("/getMode", handleGetMode);
    server.on("/getSelection", handleGetSelection);
    server.on("/getSecuencia",handleGetSecuencia);
    server.on("/aceptar", btnaceptar);
    server.on("/getIP", handleGetIP);

    server.begin();
    pinMode(redPin, OUTPUT); pinMode(greenPin, OUTPUT); pinMode(bluePin, OUTPUT);
    ledcAttachPin(redPin, 0); ledcAttachPin(greenPin, 1); ledcAttachPin(bluePin, 2);
    ledcSetup(0, 5000, 8); ledcSetup(1, 5000, 8); ledcSetup(2, 5000, 8);
    
    // Configuración de pines de botones como entrada (Pull-Down)
    pinMode(BOTON_MODO, INPUT_PULLDOWN);
    pinMode(BOTON_JUGAR, INPUT_PULLDOWN);
    pinMode(BOTON_SENSAR, INPUT);
    setColor(255,255,255);
    Serial.println("Juego iniciado");
}

void loop()
{
    loopAP();
    server.handleClient();  // Manejar las solicitudes HTTP
    bool lecturaBotonModo = digitalRead(BOTON_MODO);
    bool lecturaBotonJugar = digitalRead(BOTON_JUGAR);


    // Detectar transición de bajo a alto (presionar y soltar)
    if (lecturaBotonModo && !botonModoPresionado) {
        botonModoPresionado = true;
    } 
    if (!lecturaBotonModo && botonModoPresionado) {
        botonModoPresionado = false;
        cambiarModo();  // Cambiar modo al soltar el botón
    }

    if (lecturaBotonJugar && !botonJugarPresionado) {
        botonJugarPresionado = true;
    }
    if (!lecturaBotonJugar && botonJugarPresionado) {
        botonJugarPresionado = false;
        confirmarModo();
        iniciarJuego();  // Iniciar el juego seleccionado al soltar
    }
}   
