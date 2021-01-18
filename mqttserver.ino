#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.

const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
const char *mqtt_server = "YOUR_MQTT_SERVER";
const char *mqtt_name = "YOUR_MQTT_SERVER_NAME";
const char *mqtt_password = "YOUR_MQTT_SERVER_PASSWORD";
const char *door_password = "DOOR_PASSWORD";

ESP8266WebServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

const String header = "<html><head><meta name=viewport content=\"width=device-width, initial-scale=1\"></head><body><div style=\"padding:10px\"><h3>DoorBoxy</h3>";
const String footer = "</div></body></html>";

void commonHeader()
{
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
}

void rootRouter()
{
    commonHeader();
    server.send(200, "text/html", header + "<form action=\"/door\" method=\"post\"><input type=\"password\" name=\"password\" placeholder=\"Password\" style=\"width:200px;\"/><input type=\"submit\" name=\"action\" value=\"Open\" style=\"margin-left:10px;width:50px\"/></form>" + footer);
}

void doorRouter()
{
    commonHeader();
    String password = server.arg("password");
    if (password != door_password)
    {
        server.send(200, "text/html", header + "Wrong Password! <a href=\"/\">Back</>" + footer);
        return;
    }

    String action = server.arg("action");
    if (action == "Open")
    {
        // 開門
        digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(5, HIGH);
        delay(100);
        digitalWrite(5, LOW);
        digitalWrite(LED_BUILTIN, HIGH);
        // 燈號
        server.send(200, "text/html", header + "Opened! <a href=\"/\">Back</>" + footer);
    }
    else
    {
        server.send(404, "text/plain", "Action NOT found!");
    }
}

void setup_wifi()
{

    delay(10);
    // We start by connecting to a WiFi network
    Serial.println("");
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.hostname("DoorBoxy");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/index.html", rootRouter);
    server.on("/", rootRouter);
    server.on("/door", doorRouter);

    server.onNotFound([]() {
        server.send(404, "text/plain", "Page NOT found!");
    });

    server.begin();
    Serial.println("HTTP server started.");
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1')
    {
        digitalWrite(BUILTIN_LED, LOW); // Turn the LED on (Note that LOW is the voltage level
        // but actually the LED is on; this is because
        // it is active low on the ESP-01)
        digitalWrite(5, HIGH);
        delay(100);
        digitalWrite(5, LOW);
        snprintf(msg, MSG_BUFFER_SIZE, "%ld", 1);
    }
    else
    {
        digitalWrite(BUILTIN_LED, HIGH); // Turn the LED off by making the voltage HIGH
        snprintf(msg, MSG_BUFFER_SIZE, "%ld", 0);
    }
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str(), mqtt_name, mqtt_password))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("home/rollingdoor/outTopic", "0");
            // ... and resubscribe
            client.subscribe("home/rollingdoor/inTopic");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup()
{
    pinMode(5, OUTPUT); // D1 GPIO5
    pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output
    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    server.handleClient();

    unsigned long now = millis();
    if (now - lastMsg > 2000)
    {
        lastMsg = now;
        ++value;
        client.publish("home/rollingdoor/outTopic", msg);
    }
}
