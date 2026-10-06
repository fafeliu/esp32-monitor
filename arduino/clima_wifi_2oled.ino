#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// =====================================================
// CONFIGURACIÓN WIFI
// =====================================================

// IMPORTANTE:
// Utilizar la red Wi-Fi de 2.4 GHz

const char* WIFI_SSID     = "Nativa Admin";
const char* WIFI_PASSWORD = "75717571";


// =====================================================
// UBICACIÓN
// Córdoba Capital, Argentina
// =====================================================

const float LATITUD  = -31.4201;
const float LONGITUD = -64.1888;


// =====================================================
// OLED
// =====================================================

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

#define OLED_ADDR 0x3C


// OLED IZQUIERDA
#define SDA_IZQ 21
#define SCL_IZQ 22


// OLED DERECHA
#define SDA_DER 25
#define SCL_DER 26


// =====================================================
// BUSES I2C
// =====================================================

TwoWire I2C_IZQ = TwoWire(0);
TwoWire I2C_DER = TwoWire(1);


// =====================================================
// PANTALLAS
// =====================================================

Adafruit_SSD1306 oledIzq(
  OLED_WIDTH,
  OLED_HEIGHT,
  &I2C_IZQ,
  -1
);

Adafruit_SSD1306 oledDer(
  OLED_WIDTH,
  OLED_HEIGHT,
  &I2C_DER,
  -1
);


// =====================================================
// INTERVALOS
// =====================================================

const unsigned long INTERVALO_CLIMA =
  10UL * 60UL * 1000UL;       // 10 minutos

const unsigned long INTERVALO_WIFI =
  2000UL;                     // 2 segundos

const unsigned long INTERVALO_RECONEXION =
  10000UL;                    // 10 segundos


// =====================================================
// DATOS METEOROLÓGICOS
// =====================================================

float temperatura = 0.0;
float sensacion = 0.0;

float temperaturaMinima = 0.0;
float temperaturaMaxima = 0.0;

float humedad = 0.0;
float viento = 0.0;

int codigoClima = -1;

bool climaValido = false;


// =====================================================
// CONTROL DE ACTUALIZACIONES
// =====================================================

unsigned long ultimaConsultaClima = 0;
unsigned long ultimoIntentoWiFi = 0;
unsigned long ultimaActualizacionWiFi = 0;


// =====================================================
// HORA DE LA ÚLTIMA ACTUALIZACIÓN
// =====================================================

time_t horaUltimaActualizacion = 0;


// =====================================================
// TIPOS DE CLIMA
// =====================================================

enum TipoClima {

  DESPEJADO,

  PARCIALMENTE_DESPEJADO,

  NUBLADO,

  NIEBLA,

  LLOVIZNA,

  LLUVIA,

  CHAPARRONES,

  NIEVE,

  TORMENTA,

  DESCONOCIDO
};


// =====================================================
// CLASIFICAR CÓDIGO METEOROLÓGICO WMO
// =====================================================

TipoClima clasificarClima(int codigo) {

  if (codigo == 0)
    return DESPEJADO;


  if (codigo == 1 || codigo == 2)
    return PARCIALMENTE_DESPEJADO;


  if (codigo == 3)
    return NUBLADO;


  if (codigo == 45 || codigo == 48)
    return NIEBLA;


  if (codigo >= 51 && codigo <= 57)
    return LLOVIZNA;


  if (codigo >= 61 && codigo <= 67)
    return LLUVIA;


  if (codigo >= 71 && codigo <= 77)
    return NIEVE;


  if (codigo >= 80 && codigo <= 82)
    return CHAPARRONES;


  if (codigo >= 85 && codigo <= 86)
    return NIEVE;


  if (codigo >= 95 && codigo <= 99)
    return TORMENTA;


  return DESCONOCIDO;
}


// =====================================================
// HORA ACTUAL
// =====================================================

String obtenerHora() {

  struct tm tiempo;


  if (!getLocalTime(&tiempo)) {

    return "--:--";
  }


  char buffer[6];


  strftime(
    buffer,
    sizeof(buffer),
    "%H:%M",
    &tiempo
  );


  return String(buffer);
}


// =====================================================
// ICONOS METEOROLÓGICOS
// =====================================================

void dibujarSol(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  display.fillCircle(
    x,
    y,
    7,
    SSD1306_WHITE
  );


  // Rayos

  display.drawLine(
    x,
    y - 12,
    x,
    y - 16,
    SSD1306_WHITE
  );

  display.drawLine(
    x,
    y + 12,
    x,
    y + 16,
    SSD1306_WHITE
  );

  display.drawLine(
    x - 12,
    y,
    x - 16,
    y,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 12,
    y,
    x + 16,
    y,
    SSD1306_WHITE
  );

  display.drawLine(
    x - 9,
    y - 9,
    x - 12,
    y - 12,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 9,
    y - 9,
    x + 12,
    y - 12,
    SSD1306_WHITE
  );

  display.drawLine(
    x - 9,
    y + 9,
    x - 12,
    y + 12,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 9,
    y + 9,
    x + 12,
    y + 12,
    SSD1306_WHITE
  );
}


// =====================================================
// NUBE
// =====================================================

void dibujarNube(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  display.fillCircle(
    x - 10,
    y,
    7,
    SSD1306_WHITE
  );

  display.fillCircle(
    x,
    y - 5,
    9,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 10,
    y,
    7,
    SSD1306_WHITE
  );

  display.fillRect(
    x - 15,
    y,
    30,
    9,
    SSD1306_WHITE
  );
}


// =====================================================
// SOL + NUBE
// =====================================================

void dibujarParcial(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  // Sol

  display.fillCircle(
    x - 8,
    y - 6,
    6,
    SSD1306_WHITE
  );


  // Nube

  display.fillCircle(
    x + 4,
    y,
    7,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 13,
    y + 2,
    6,
    SSD1306_WHITE
  );

  display.fillRect(
    x - 1,
    y,
    20,
    8,
    SSD1306_WHITE
  );
}


// =====================================================
// LLUVIA
// =====================================================

void dibujarLluvia(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  dibujarNube(
    display,
    x,
    y - 5
  );


  display.drawLine(
    x - 9,
    y + 10,
    x - 12,
    y + 16,
    SSD1306_WHITE
  );

  display.drawLine(
    x,
    y + 10,
    x - 3,
    y + 16,
    SSD1306_WHITE
  );

  display.drawLine(
    x + 9,
    y + 10,
    x + 6,
    y + 16,
    SSD1306_WHITE
  );
}


// =====================================================
// NIEVE
// =====================================================

void dibujarNieve(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  dibujarNube(
    display,
    x,
    y - 5
  );


  display.fillCircle(
    x - 9,
    y + 12,
    2,
    SSD1306_WHITE
  );

  display.fillCircle(
    x,
    y + 16,
    2,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 9,
    y + 12,
    2,
    SSD1306_WHITE
  );
}


// =====================================================
// NIEBLA
// =====================================================

void dibujarNiebla(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  display.drawLine(
    x - 16,
    y - 5,
    x + 16,
    y - 5,
    SSD1306_WHITE
  );

  display.drawLine(
    x - 12,
    y + 3,
    x + 12,
    y + 3,
    SSD1306_WHITE
  );

  display.drawLine(
    x - 16,
    y + 11,
    x + 16,
    y + 11,
    SSD1306_WHITE
  );
}


// =====================================================
// TORMENTA
// =====================================================

void dibujarTormenta(
  Adafruit_SSD1306 &display,
  int x,
  int y
) {

  dibujarNube(
    display,
    x,
    y - 5
  );


  // Relámpago

  display.fillTriangle(
    x + 2,
    y + 7,

    x - 4,
    y + 18,

    x + 2,
    y + 16,

    SSD1306_WHITE
  );


  display.fillTriangle(
    x + 2,
    y + 16,

    x + 8,
    y + 16,

    x + 1,
    y + 27,

    SSD1306_WHITE
  );
}


// =====================================================
// DIBUJAR ICONO SEGÚN CLIMA
// =====================================================

void dibujarIconoClima(
  Adafruit_SSD1306 &display,
  TipoClima clima
) {

  switch (clima) {

    case DESPEJADO:

      dibujarSol(
        display,
        23,
        29
      );

      break;


    case PARCIALMENTE_DESPEJADO:

      dibujarParcial(
        display,
        23,
        29
      );

      break;


    case NUBLADO:

      dibujarNube(
        display,
        23,
        29
      );

      break;


    case NIEBLA:

      dibujarNiebla(
        display,
        23,
        29
      );

      break;


    case LLOVIZNA:

    case LLUVIA:

    case CHAPARRONES:

      dibujarLluvia(
        display,
        23,
        29
      );

      break;


    case NIEVE:

      dibujarNieve(
        display,
        23,
        29
      );

      break;


    case TORMENTA:

      dibujarTormenta(
        display,
        23,
        29
      );

      break;


    default:
      break;
  }
}


// =====================================================
// CONECTAR WIFI
// =====================================================

void conectarWiFi() {

  Serial.println();
  Serial.println("==============================");
  Serial.println("       CONEXION WIFI");
  Serial.println("==============================");

  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);


  WiFi.mode(WIFI_STA);

  WiFi.disconnect(true);

  delay(1000);


  WiFi.setAutoReconnect(true);

  WiFi.persistent(false);


  Serial.println(
    "Iniciando conexion..."
  );


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  unsigned long inicio =
    millis();


  int intentos = 0;


  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - inicio < 20000
  ) {

    delay(500);

    Serial.print(".");

    intentos++;
  }


  Serial.println();


  if (
    WiFi.status() == WL_CONNECTED
  ) {

    Serial.println();
    Serial.println(
      "************************"
    );

    Serial.println(
      "     WIFI CONECTADO"
    );

    Serial.println(
      "************************"
    );


    Serial.print(
      "SSID: "
    );

    Serial.println(
      WiFi.SSID()
    );


    Serial.print(
      "IP: "
    );

    Serial.println(
      WiFi.localIP()
    );


    Serial.print(
      "Gateway: "
    );

    Serial.println(
      WiFi.gatewayIP()
    );


    Serial.print(
      "RSSI: "
    );

    Serial.print(
      WiFi.RSSI()
    );

    Serial.println(
      " dBm"
    );


    Serial.println();

  } else {

    Serial.println();
    Serial.println(
      "************************"
    );

    Serial.println(
      "    WIFI NO CONECTADO"
    );

    Serial.println(
      "************************"
    );


    Serial.print(
      "Estado: "
    );


    switch (
      WiFi.status()
    ) {

      case WL_NO_SSID_AVAIL:

        Serial.println(
          "No se encuentra la red"
        );

        break;


      case WL_CONNECT_FAILED:

        Serial.println(
          "Fallo de conexion / contraseña"
        );

        break;


      case WL_CONNECTION_LOST:

        Serial.println(
          "Conexion perdida"
        );

        break;


      case WL_DISCONNECTED:

        Serial.println(
          "Desconectado"
        );

        break;


      default:

        Serial.println(
          WiFi.status()
        );

        break;
    }


    Serial.println();
  }
}

// =====================================================
// ENVIAR PRIMER DATO A SUPABASE
// =====================================================

void enviarTemperaturaSupabase() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println(
      "Supabase: WiFi no conectado"
    );

    return;
  }


  // ---------------------------------------------------
  // DATOS DE SUPABASE
  // ---------------------------------------------------

  const char* SUPABASE_URL =
    "https://xvztoywavhepjrnynuqf.supabase.co";

  const char* SUPABASE_KEY =
    "sb_publishable_pNAU538ou89EPlKLWYm5Gg_rgJy-uGN";


  // ---------------------------------------------------
  // URL DE LA TABLA
  // ---------------------------------------------------

  String url =
    String(SUPABASE_URL) +
    "/rest/v1/telemetria";


  // ---------------------------------------------------
  // CLIENTE HTTPS
  // ---------------------------------------------------

  NetworkClientSecure client;

  // TEMPORALMENTE para esta primera prueba.
  // Luego configuraremos el certificado correctamente.

  client.setInsecure();


  HTTPClient http;


  if (!http.begin(
        client,
        url
      )) {

    Serial.println(
      "Supabase: error iniciando HTTP"
    );

    return;
  }


  // ---------------------------------------------------
  // HEADERS
  // ---------------------------------------------------

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "apikey",
    SUPABASE_KEY
  );

  http.addHeader(
    "Prefer",
    "return=minimal"
  );


  // ---------------------------------------------------
  // JSON
  // ---------------------------------------------------

  String json =

    String("{\"temperatura\":") +

    String(
      temperatura,
      1
    ) +

    String("}");


  Serial.println();
  Serial.println(
    "Enviando datos a Supabase..."
  );

  Serial.print(
    "Temperatura: "
  );

  Serial.println(
    temperatura,
    1
  );


  // ---------------------------------------------------
  // POST
  // ---------------------------------------------------

  int codigoHTTP =
    http.POST(
      json
    );


  // ---------------------------------------------------
  // RESULTADO
  // ---------------------------------------------------

  Serial.print(
    "Codigo HTTP: "
  );

  Serial.println(
    codigoHTTP
  );


  if (
    codigoHTTP >= 200 &&
    codigoHTTP < 300
  ) {

    Serial.println(
      "OK - dato enviado"
    );

  } else {

    Serial.println(
      "ERROR - no se pudo enviar"
    );

    Serial.println(
      http.getString()
    );
  }


  http.end();
}

// =====================================================
// ACTUALIZAR CLIMA DESDE OPEN-METEO
// =====================================================

bool actualizarClima() {

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "Sin WiFi: no se puede actualizar clima"
    );

    return false;
  }


  // ---------------------------------------------------
  // URL
  // ---------------------------------------------------

  String url =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=" + String(LATITUD, 4) +
    "&longitude=" + String(LONGITUD, 4) +

    "&current="
    "temperature_2m,"
    "relative_humidity_2m,"
    "apparent_temperature,"
    "weather_code,"
    "wind_speed_10m"

    "&daily="
    "temperature_2m_min,"
    "temperature_2m_max"

    "&timezone=America%2FArgentina%2FCordoba";


  Serial.println();
  Serial.println(
    "Consultando Open-Meteo..."
  );

  Serial.println(url);


  // ---------------------------------------------------
  // HTTPS
  // ---------------------------------------------------

  NetworkClientSecure client;

  // Para simplificar el proyecto durante el desarrollo.
  // Más adelante podemos agregar certificado raíz.

  client.setInsecure();


  HTTPClient http;


  if (
    !http.begin(
      client,
      url
    )
  ) {

    Serial.println(
      "Error HTTP begin"
    );

    return false;
  }


  http.setTimeout(
    10000
  );


  // ---------------------------------------------------
  // PETICIÓN
  // ---------------------------------------------------

  int codigoHTTP =
    http.GET();


  if (
    codigoHTTP != HTTP_CODE_OK
  ) {

    Serial.print(
      "Error HTTP: "
    );

    Serial.println(
      codigoHTTP
    );


    http.end();

    return false;
  }


  // ---------------------------------------------------
  // RESPUESTA
  // ---------------------------------------------------

  String respuesta =
    http.getString();


  http.end();


  // ---------------------------------------------------
  // JSON
  // ---------------------------------------------------

  JsonDocument doc;


  DeserializationError error =
    deserializeJson(
      doc,
      respuesta
    );


  if (error) {

    Serial.print(
      "Error JSON: "
    );

    Serial.println(
      error.c_str()
    );

    return false;
  }


  // ---------------------------------------------------
  // CURRENT
  // ---------------------------------------------------

  JsonObject current =
    doc["current"];


  if (
    current.isNull()
  ) {

    Serial.println(
      "No existe current"
    );

    return false;
  }


  // ---------------------------------------------------
  // DAILY
  // ---------------------------------------------------

  JsonObject daily =
    doc["daily"];


  if (
    daily.isNull()
  ) {

    Serial.println(
      "No existe daily"
    );

    return false;
  }


  // ---------------------------------------------------
  // DATOS ACTUALES
  // ---------------------------------------------------

  float nuevaTemperatura =
    current["temperature_2m"] | 0.0;


  float nuevaSensacion =
    current["apparent_temperature"] | 0.0;


  float nuevaHumedad =
    current["relative_humidity_2m"] | 0.0;


  float nuevoViento =
    current["wind_speed_10m"] | 0.0;


  int nuevoCodigo =
    current["weather_code"] | -1;


  // ---------------------------------------------------
  // MINIMA / MAXIMA
  // ---------------------------------------------------

  float nuevaMinima =
    daily["temperature_2m_min"][0] | 0.0;


  float nuevaMaxima =
    daily["temperature_2m_max"][0] | 0.0;


  // ---------------------------------------------------
  // VALIDACIÓN BÁSICA
  // ---------------------------------------------------

  if (
    nuevoCodigo < 0
  ) {

    Serial.println(
      "Codigo meteorologico invalido"
    );

    return false;
  }


  // ---------------------------------------------------
  // GUARDAR DATOS
  // ---------------------------------------------------

  temperatura =
    nuevaTemperatura;


  sensacion =
    nuevaSensacion;


  humedad =
    nuevaHumedad;


  viento =
    nuevoViento;


  codigoClima =
    nuevoCodigo;


  temperaturaMinima =
    nuevaMinima;


  temperaturaMaxima =
    nuevaMaxima;


  // ---------------------------------------------------
  // HORA DE ACTUALIZACIÓN
  // ---------------------------------------------------

  time(
    &horaUltimaActualizacion
  );


  climaValido =
    true;


  // ---------------------------------------------------
  // MONITOR SERIE
  // ---------------------------------------------------

  Serial.println();
  Serial.println(
    "===== CLIMA ACTUALIZADO ====="
  );


  Serial.print(
    "Temperatura: "
  );

  Serial.print(
    temperatura,
    1
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Sensacion: "
  );

  Serial.print(
    sensacion,
    1
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Minima: "
  );

  Serial.print(
    temperaturaMinima,
    1
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Maxima: "
  );

  Serial.print(
    temperaturaMaxima,
    1
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Humedad: "
  );

  Serial.print(
    humedad,
    0
  );

  Serial.println(
    " %"
  );


  Serial.print(
    "Viento: "
  );

  Serial.print(
    viento,
    0
  );

  Serial.println(
    " km/h"
  );


  Serial.print(
    "Codigo WMO: "
  );

  Serial.println(
    codigoClima
  );


  Serial.print(
    "Actualizado: "
  );

  Serial.println(
    obtenerHora()
  );


  Serial.println(
    "=============================="
  );


  return true;
}


// =====================================================
// MOSTRAR CLIMA
// =====================================================

void mostrarClima() {

  oledIzq.clearDisplay();

  oledIzq.setTextColor(
    SSD1306_WHITE
  );


  // ---------------------------------------------------
  // CABECERA
  // ---------------------------------------------------

  oledIzq.setTextSize(1);

  oledIzq.setCursor(
    29,
    0
  );

  oledIzq.print(
    "CORDOBA"
  );


  // Hora sin segundos

  oledIzq.setCursor(
    88,
    0
  );

  oledIzq.print(
    obtenerHora()
  );


  oledIzq.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );


  // ---------------------------------------------------
  // SIN DATOS
  // ---------------------------------------------------

  if (!climaValido) {

    oledIzq.setCursor(
      25,
      27
    );

    oledIzq.print(
      "SIN DATOS"
    );


    oledIzq.setCursor(
      18,
      40
    );

    oledIzq.print(
      "Esperando..."
    );


    oledIzq.display();

    return;
  }


  // ---------------------------------------------------
  // TIPO DE CLIMA
  // ---------------------------------------------------

  TipoClima clima =
    clasificarClima(
      codigoClima
    );


  // ---------------------------------------------------
  // ICONO
  // ---------------------------------------------------

  dibujarIconoClima(
    oledIzq,
    clima
  );


  // ---------------------------------------------------
  // TEMPERATURA ACTUAL
  // ---------------------------------------------------

  oledIzq.setTextSize(2);

  oledIzq.setCursor(
    51,
    15
  );


  oledIzq.print(
    temperatura,
    1
  );


  oledIzq.print(
    "C"
  );


  // ---------------------------------------------------
  // SENSACIÓN TÉRMICA
  // ---------------------------------------------------

  oledIzq.setTextSize(1);

  oledIzq.setCursor(
    55,
    35
  );


  oledIzq.print(
    "Sens "
  );


  oledIzq.print(
    sensacion,
    1
  );


  oledIzq.print(
    "C"
  );


  // ---------------------------------------------------
  // MÍNIMA
  // ---------------------------------------------------

  oledIzq.setCursor(
    2,
    47
  );


  oledIzq.print(
    "Min "
  );


  oledIzq.print(
    temperaturaMinima,
    1
  );


  oledIzq.print(
    "C"
  );


  // ---------------------------------------------------
  // MÁXIMA
  // ---------------------------------------------------

  oledIzq.setCursor(
    67,
    47
  );


  oledIzq.print(
    "Max "
  );


  oledIzq.print(
    temperaturaMaxima,
    1
  );


  oledIzq.print(
    "C"
  );


  // ---------------------------------------------------
  // HUMEDAD
  // ---------------------------------------------------

  oledIzq.setCursor(
    2,
    57
  );


  oledIzq.print(
    "H:"
  );


  oledIzq.print(
    humedad,
    0
  );


  oledIzq.print(
    "%"
  );


  // ---------------------------------------------------
  // VIENTO
  // ---------------------------------------------------

  oledIzq.setCursor(
    60,
    57
  );


  oledIzq.print(
    "V:"
  );


  oledIzq.print(
    viento,
    0
  );


  oledIzq.print(
    "km/h"
  );


  oledIzq.display();
}


// =====================================================
// MOSTRAR WIFI
// =====================================================

void mostrarWiFi() {

  oledDer.clearDisplay();

  oledDer.setTextColor(
    SSD1306_WHITE
  );


  // ---------------------------------------------------
  // TÍTULO
  // ---------------------------------------------------

  oledDer.setTextSize(1);

  oledDer.setCursor(
    45,
    0
  );

  oledDer.print(
    "WI-FI"
  );


  oledDer.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );


  // ---------------------------------------------------
  // CONECTADO
  // ---------------------------------------------------

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    // -------------------------------------------------
    // SSID
    // -------------------------------------------------

    oledDer.setCursor(
      2,
      15
    );


    String ssid =
      WiFi.SSID();


    if (
      ssid.length() > 20
    ) {

      ssid =
        ssid.substring(
          0,
          20
        );
    }


    oledDer.print(
      ssid
    );


    // -------------------------------------------------
    // IP
    // -------------------------------------------------

    oledDer.setCursor(
      2,
      28
    );


    oledDer.print(
      "IP:"
    );


    oledDer.print(
      WiFi.localIP()
    );


    // -------------------------------------------------
    // RSSI
    // -------------------------------------------------

    int rssi =
      WiFi.RSSI();


    oledDer.setCursor(
      2,
      40
    );


    oledDer.print(
      "RSSI:"
    );


    oledDer.print(
      rssi
    );


    oledDer.print(
      "dBm"
    );


    // -------------------------------------------------
    // BARRA DE SEÑAL
    // -------------------------------------------------

    int barras = 0;


    if (rssi > -50)
      barras = 4;

    else if (rssi > -60)
      barras = 3;

    else if (rssi > -70)
      barras = 2;

    else if (rssi > -80)
      barras = 1;


    for (
      int i = 0;
      i < 4;
      i++
    ) {

      int alto =
        (i + 1) * 3;


      if (
        i < barras
      ) {

        oledDer.fillRect(
          92 + i * 7,
          48 - alto,
          5,
          alto,
          SSD1306_WHITE
        );

      } else {

        oledDer.drawRect(
          92 + i * 7,
          48 - alto,
          5,
          alto,
          SSD1306_WHITE
        );
      }
    }


    // -------------------------------------------------
    // INTERNET
    // -------------------------------------------------

    oledDer.setCursor(
      2,
      56
    );


    oledDer.print(
      "INTERNET: OK"
    );


  } else {

    // -------------------------------------------------
    // DESCONECTADO
    // -------------------------------------------------

    oledDer.setCursor(
      21,
      25
    );


    oledDer.print(
      "DESCONECTADO"
    );


    oledDer.setCursor(
      20,
      40
    );


    oledDer.print(
      "Reconectando..."
    );
  }


  oledDer.display();
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );


  // ---------------------------------------------------
  // I2C IZQUIERDA
  // ---------------------------------------------------

  I2C_IZQ.begin(
    SDA_IZQ,
    SCL_IZQ,
    400000
  );


  // ---------------------------------------------------
  // I2C DERECHA
  // ---------------------------------------------------

  I2C_DER.begin(
    SDA_DER,
    SCL_DER,
    400000
  );


  // ---------------------------------------------------
  // OLED IZQUIERDA
  // ---------------------------------------------------

  if (
    !oledIzq.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  ) {

    Serial.println(
      "ERROR OLED IZQUIERDA"
    );

    while (true);
  }


  // ---------------------------------------------------
  // OLED DERECHA
  // ---------------------------------------------------

  if (
    !oledDer.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  ) {

    Serial.println(
      "ERROR OLED DERECHA"
    );

    while (true);
  }


  // ---------------------------------------------------
  // LIMPIAR OLED
  // ---------------------------------------------------

  oledIzq.clearDisplay();
  oledDer.clearDisplay();

  oledIzq.display();
  oledDer.display();


  // ---------------------------------------------------
  // WIFI
  // ---------------------------------------------------

  conectarWiFi();


  // ---------------------------------------------------
  // HORA ARGENTINA
  // ---------------------------------------------------

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    configTzTime(
      "ART3",
      "pool.ntp.org",
      "time.nist.gov"
    );


    Serial.println(
      "Sincronizando hora..."
    );


    struct tm tiempo;


    if (
      getLocalTime(
        &tiempo,
        10000
      )
    ) {

      Serial.print(
        "Hora: "
      );

      Serial.println(
        obtenerHora()
      );

    } else {

      Serial.println(
        "No se pudo sincronizar hora"
      );
    }
  }


  // ---------------------------------------------------
  // PRIMERA CONSULTA CLIMA
  // ---------------------------------------------------

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    if (
      actualizarClima()
    ) {

      ultimaConsultaClima =
        millis();
    }
  }
// ---------------------------------------------------
// PRUEBA SUPABASE
// ---------------------------------------------------

  enviarTemperaturaSupabase();

  // ---------------------------------------------------
  // PRIMER DIBUJO
  // ---------------------------------------------------

  mostrarClima();

  mostrarWiFi();
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long ahora =
    millis();


  // ===================================================
  // RECONEXIÓN WIFI
  // ===================================================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    if (
      ahora -
      ultimoIntentoWiFi >=
      INTERVALO_RECONEXION
    ) {

      ultimoIntentoWiFi =
        ahora;


      Serial.println(
        "Intentando reconectar..."
      );


      WiFi.disconnect();


      WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
      );
    }
  }


  // ===================================================
  // ACTUALIZAR CLIMA
  // ===================================================

  if (
    WiFi.status() == WL_CONNECTED &&
    (
      ultimaConsultaClima == 0 ||
      ahora -
      ultimaConsultaClima >=
      INTERVALO_CLIMA
    )
  ) {

    if (
      actualizarClima()
    ) {

      ultimaConsultaClima =
        ahora;


      mostrarClima();

    } else {

      Serial.println(
        "Fallo actualización del clima"
      );


      // No borrar los datos anteriores.
      // Se mantienen hasta conseguir
      // una actualización válida.

      ultimaConsultaClima =
        ahora;
    }
  }


  // ===================================================
  // ACTUALIZAR PANTALLA WIFI
  // ===================================================

  if (
    ahora -
    ultimaActualizacionWiFi >=
    INTERVALO_WIFI
  ) {

    ultimaActualizacionWiFi =
      ahora;


    mostrarWiFi();
  }


  // Pequeña pausa para evitar
  // un loop excesivamente rápido.

  delay(50);
}
