#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// ROVER-10 ESP32-CAM
// QVGA 320x240 LOW-LATENCY CAMERA
// GC2145 RGB565
// =====================================================

// Join the Rover ESP32 access point.
const char* WIFI_SSID = "MINE-ROVER";
const char* WIFI_PASSWORD = "mine12345";

WebServer server(80);

// =====================================================
// AI THINKER ESP32-CAM CAMERA PINS
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_LED          4

// =====================================================
// CAMERA INITIALIZATION
// =====================================================

bool initCamera()
{
    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;

    // =================================================
    // CAMERA FORMAT
    // =================================================

    // GC2145 successfully works with RGB565.
    // Do NOT change this to JPEG.
    config.pixel_format = PIXFORMAT_RGB565;

    // =================================================
    // RESOLUTION
    // =================================================

    // QVGA = 320 x 240
    config.frame_size = FRAMESIZE_QVGA;

    config.jpeg_quality = 12;

    // One buffer helps prevent old frames accumulating.
    config.fb_count = 1;

    // Store frame buffer in PSRAM.
    config.fb_location = CAMERA_FB_IN_PSRAM;

    // Always prefer newest frame.
    config.grab_mode = CAMERA_GRAB_LATEST;

    Serial.println("Initializing camera...");

    esp_err_t err =
        esp_camera_init(&config);

    if (err != ESP_OK)
    {
        Serial.print(
            "Camera initialization FAILED: 0x"
        );

        Serial.println(
            err,
            HEX
        );

        return false;
    }

    Serial.println(
        "Camera initialization SUCCESS!"
    );

    sensor_t *sensor =
        esp_camera_sensor_get();

    if (sensor != NULL)
    {
        Serial.print(
            "Camera PID = 0x"
        );

        Serial.println(
            sensor->id.PID,
            HEX
        );
    }

    Serial.println(
        "Format: RGB565"
    );

    Serial.println(
        "Resolution: 320 x 240"
    );

    Serial.println(
        "Frame size: 153600 bytes"
    );

    Serial.println(
        "Frame buffer: PSRAM"
    );

    return true;
}

// =====================================================
// WEB PAGE
// =====================================================

void handleRoot()
{
    String page = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>ROVER-10 Camera</title>

<style>

body {
    margin: 0;
    background: #111;
    color: white;
    font-family: Arial, sans-serif;
    text-align: center;
}

.header {
    padding: 18px;
    background: #222;
}

h1 {
    margin: 0;
    font-size: 32px;
}

.status {
    margin-top: 8px;
    color: #00ff88;
    font-size: 18px;
    font-weight: bold;
}

.camera {
    margin: 20px auto;
    width: 95%;
    max-width: 640px;
}

canvas {
    width: 100%;
    height: auto;
    display: block;
    border: 3px solid white;
    border-radius: 8px;
    background: black;
}

.info {
    margin-top: 12px;
    color: #bbb;
    line-height: 1.6;
}

.value {
    color: #00ff88;
    font-weight: bold;
}

</style>

</head>

<body>

<div class="header">

<h1>ROVER-10</h1>

<div class="status">
CAMERA ONLINE
</div>

</div>

<div class="camera">

<canvas
    id="camera"
    width="320"
    height="240">
</canvas>

</div>

<div class="info">

GC2145 RGB565 Camera<br>

Resolution:
<span class="value">320 × 240</span>
<br>

FPS:
<span
    id="fps"
    class="value">
0
</span>

&nbsp;&nbsp;&nbsp;

Frame Time:
<span
    id="latency"
    class="value">
0
</span>
ms

</div>

<script>

// =====================================================
// CAMERA PARAMETERS
// =====================================================

const WIDTH = 320;
const HEIGHT = 240;

const FRAME_SIZE =
    WIDTH * HEIGHT * 2;

// =====================================================
// CANVAS
// =====================================================

const canvas =
    document.getElementById("camera");

const ctx =
    canvas.getContext("2d");

const fpsElement =
    document.getElementById("fps");

const latencyElement =
    document.getElementById("latency");

// =====================================================
// FPS VARIABLES
// =====================================================

let frameCount = 0;

let lastFPS =
    performance.now();

// =====================================================
// DRAW RGB565 FRAME
// =====================================================

function drawFrame(buffer)
{
    const bytes =
        new Uint8Array(buffer);

    const image =
        ctx.createImageData(
            WIDTH,
            HEIGHT
        );

    const pixels =
        image.data;

    let p = 0;

    for (
        let i = 0;
        i < FRAME_SIZE;
        i += 2
    )
    {
        // RGB565 16-bit pixel

        const value =
            (bytes[i] << 8) |
            bytes[i + 1];

        // Red

        const r =
            ((value >> 11) & 0x1F)
            * 255 / 31;

        // Green

        const g =
            ((value >> 5) & 0x3F)
            * 255 / 63;

        // Blue

        const b =
            (value & 0x1F)
            * 255 / 31;

        pixels[p++] = r;
        pixels[p++] = g;
        pixels[p++] = b;
        pixels[p++] = 255;
    }

    ctx.putImageData(
        image,
        0,
        0
    );

    // FPS

    frameCount++;

    const now =
        performance.now();

    if (
        now - lastFPS >= 1000
    )
    {
        fpsElement.textContent =
            frameCount;

        frameCount = 0;

        lastFPS = now;
    }
}

// =====================================================
// REQUEST ONE FRAME
// =====================================================

async function getFrame()
{
    const start =
        performance.now();

    try
    {
        const response =
            await fetch(
                "/frame?t=" +
                Date.now(),
                {
                    cache: "no-store"
                }
            );

        if (!response.ok)
        {
            throw new Error(
                "HTTP " +
                response.status
            );
        }

        const buffer =
            await response.arrayBuffer();

        if (
            buffer.byteLength !== FRAME_SIZE
        )
        {
            console.log(
                "Unexpected frame size:",
                buffer.byteLength
            );

            return;
        }

        drawFrame(buffer);

        const elapsed =
            performance.now() -
            start;

        latencyElement.textContent =
            Math.round(elapsed);
    }

    catch(error)
    {
        console.log(
            "Frame error:",
            error
        );
    }
}

// =====================================================
// CAMERA LOOP
// =====================================================

async function cameraLoop()
{
    while (true)
    {
        await getFrame();
    }
}

cameraLoop();

</script>

</body>

</html>
)rawliteral";

    server.send(
        200,
        "text/html",
        page
    );
}

// =====================================================
// SEND ONE FRAME
// =====================================================

void handleFrame()
{
    WiFiClient client =
        server.client();

    // Capture newest frame

    camera_fb_t *fb =
        esp_camera_fb_get();

    if (!fb)
    {
        Serial.println(
            "Camera capture FAILED"
        );

        server.send(
            500,
            "text/plain",
            "Camera capture failed"
        );

        return;
    }

    // Send HTTP header

    client.print(
        "HTTP/1.1 200 OK\r\n"
    );

    client.print(
        "Content-Type: "
        "application/octet-stream\r\n"
    );

    client.print(
        "Content-Length: "
    );

    client.print(
        fb->len
    );

    client.print(
        "\r\n"
    );

    client.print(
        "Cache-Control: no-store\r\n"
    );

    client.print(
        "Access-Control-Allow-Origin: *\r\n"
    );

    client.print(
        "Connection: close\r\n"
    );

    client.print(
        "\r\n"
    );

    // Send RGB565 frame

    client.write(
        fb->buf,
        fb->len
    );

    // Return frame buffer

    esp_camera_fb_return(
        fb
    );

    client.stop();
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(
        115200
    );

    delay(2000);

    pinMode(
        FLASH_LED,
        OUTPUT
    );

    digitalWrite(
        FLASH_LED,
        LOW
    );

    Serial.println();

    Serial.println(
        "================================"
    );

    Serial.println(
        "      ROVER-10 ESP32-CAM"
    );

    Serial.println(
        "     QVGA LOW LATENCY"
    );

    Serial.println(
        "================================"
    );

    // =================================================
    // CAMERA
    // =================================================

    if (!initCamera())
    {
        Serial.println(
            "CAMERA FAILED!"
        );

        while (true)
        {
            digitalWrite(
                FLASH_LED,
                HIGH
            );

            delay(300);

            digitalWrite(
                FLASH_LED,
                LOW
            );

            delay(300);
        }
    }

    // =================================================
    // WI-FI
    // =================================================

    WiFi.mode(
        WIFI_AP
    );

    // Disable Wi-Fi sleep for lower latency.

    WiFi.setSleep(
        false
    );

    Serial.println(
        "Wi-Fi sleep: DISABLED"
    );

    // =================================================
    // CONNECT TO ROVER WI-FI
    // =================================================

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    Serial.println();
    Serial.println("================================");
    Serial.println("     CONNECTING TO ROVER AP");
    Serial.println("================================");
    Serial.print("Wi-Fi SSID: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t wifiStart = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - wifiStart < 20000)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Wi-Fi connection FAILED!");
        Serial.println("Check Rover AP and credentials.");

        while (true)
        {
            digitalWrite(FLASH_LED, HIGH);
            delay(250);
            digitalWrite(FLASH_LED, LOW);
            delay(250);
        }
    }

    IPAddress IP = WiFi.localIP();

    Serial.println("================================");
    Serial.println("       CAMERA WIFI CONNECTED");
    Serial.println("================================");
    Serial.print("Camera IP: ");
    Serial.println(IP);
    Serial.print("Camera frame URL: http://");
    Serial.print(IP);
    Serial.println("/frame");
    Serial.print("Camera page: http://");
    Serial.print(IP);
    Serial.println("/");
    Serial.println("================================");

    // =================================================
    // WEB SERVER
    // =================================================

    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/frame",
        HTTP_GET,
        handleFrame
    );

    server.begin();

    Serial.println(
        "Web server started!"
    );

    Serial.println(
        "QVGA CAMERA READY!"
    );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    server.handleClient();

    delay(1);
}