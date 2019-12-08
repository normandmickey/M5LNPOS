#include <ezTime.h>
#include <M5ez.h>
#include <M5Stack.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <math.h>
#include "Physfau.c"

// For OpenNode
String ONApiKey = "OpenNode API Key";
String currency = "USD";

//For RaspiBlitz
String readmacaroon = "Raspiblitz LND Read Only Macaroon";
String invoicemacaroon = "Raspiblitz LND Invoice Macaroon";
const char* test_root_fingerprint = "SSL Fingerprint";
String blitzServer = "Raspiblitz Server";

//For Sats Hunter
String giftinvoice;
String giftid;
String giftlnurl;
String amount = "100";
bool spent = false;
String giftstatus = "unpaid";
unsigned long tickertime;
unsigned long tickertimeupdate;
unsigned long giftbreak = 2000;
unsigned long currenttime = 2000;
const char* gifthost = "api.lightning.gifts";
const int httpsPort = 443;

const char* lndhost = "Zap Hostname"; //in terminal run ssh -R SOME-NAME.serveo.net:3010:localhost:8180 serveo.net
String adminmacaroon = "Zap LND Admin Macaroon";


void setup() {
  ez.begin();
}

void loop() {
  ezMenu myMenu;
  myMenu.addItem("Bitcoin Price", mainmenu_one);
  myMenu.addItem("ON Lightning Invoice", mainmenu_two);
  myMenu.addItem("Raspiblitz Lightning Invoice", mainmenu_three);
  myMenu.addItem("Sats Hunter", mainmenu_four);
  myMenu.addItem("Settings", mainmenu_five);
  myMenu.run();
}

void mainmenu_one() {
  ONRates();
}

void mainmenu_two() {
  ONInvoice();
}

void mainmenu_three() {
  raspiBlitzInvoice();
}

void mainmenu_four() {
  SatsHunter();
}

void mainmenu_five() {
  ez.settings.menu();
}

void ONRates() {
  HTTPClient client;
  client.begin("https://api.opennode.co/v1/rates");
  int httpCode = client.GET();
  if (httpCode > 0) {
    const size_t capacity = 162 * JSON_OBJECT_SIZE(1) + 8 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(169) + 2110;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, client.getString());
    JsonObject data = doc["data"];

    String pair = "BTC" + currency;
    ez.msgBox("", data[pair][currency]);
  }
  client.end();
}

void ONInvoice() {
  double amount = ez.textInput(currency, "0.01").toDouble();
  //String amount = ez.textInput();

  HTTPClient client;
  client.begin("https://api.opennode.co/v1/charges");
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Authorization", ONApiKey);
  String jsonPost = String("{") + "\"amount\":" + "\"" + String(amount) + "\"" + "," + "\"currency\":" + "\"" + currency + "\"" + "}";
  int httpCode = client.POST(jsonPost);
  if (httpCode > 0) {
    const size_t capacity = 2 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(19) + 950;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, client.getString());
    JsonObject data = doc["data"];

    String lnInvoice = data["lightning_invoice"]["payreq"];
    String invoiceId = data["id"];

    ez.screen.clear();
    M5.begin();
    M5.Lcd.qrcode(lnInvoice);
    delay(5000);
    checkONInvoiceStatus(invoiceId);
  }
  client.end();
}

void checkONInvoiceStatus(String invoiceId) {
  delay (3000);
  for (int x = 0; x < 12; x++) {
    M5.update();
    if (M5.BtnC.isPressed()) {
        ez.msgBox("", "Invoice Cancelled");
        break;
      }
    HTTPClient client;
    String invoiceURL = "https://api.opennode.co/v1/charge/" + invoiceId;
    client.begin(invoiceURL);
    client.addHeader("Content-Type", "application/json");
    client.addHeader("Authorization", ONApiKey);
    int httpCode = client.GET();
    if (httpCode > 0) {
      const size_t capacity = JSON_ARRAY_SIZE(0) + JSON_OBJECT_SIZE(0) + 2 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(19) + 1060;
      DynamicJsonDocument doc(capacity);

      deserializeJson(doc, client.getString());
      JsonObject data = doc["data"];
      client.end();
      if (data["status"] == "paid") {
        ez.msgBox("", "Payment Complete");
        break;
      }
    }
    delay(5000);
  }
}

void raspiBlitzInvoice() {
  double amount = ez.textInput(currency, "0.01").toDouble();
  float rate = getConversionRate();
  int amount2 = amount * rate * 100000000;
  String memo = "Memo " + String(random(1,1000)); //memo suffix, followed by the price then a random number
  
  HTTPClient client;
  String invoiceURL = "https://" + blitzServer + ":8080/v1/invoices";
  client.begin(invoiceURL, test_root_fingerprint);
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Grpc-Metadata-macaroon", invoicemacaroon);
  String jsonPost = "{\"value\":\"" + String(amount2) + "\",\"memo\":" + "\"" + memo + "\"" + ",\"expiry\":\"1000\"}";
  int httpCode = client.POST(jsonPost);
  if (httpCode > 0) {
    const size_t capacity = JSON_OBJECT_SIZE(3) + 320;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, client.getString());

    const char* payment_request = doc["payment_request"];
    String payreq = payment_request;
    //Serial.println(payreq);

    ez.screen.clear();
    M5.begin();
    M5.Lcd.qrcode(payreq, 45, 0, 240, 10);
    delay(5000);
    checkBlitzInvoiceStatus(payreq);
  }
  client.end();
}

void checkBlitzInvoiceStatus(String invoiceId) {
  String paymentHash = getHash(invoiceId);
  Serial.println(paymentHash);
  delay (3000);
      
  for (int x = 0; x < 12; x++) {
    M5.update();
    if (M5.BtnC.isPressed()) {
        ez.msgBox("", "Invoice Cancelled");
        break;
      }
      
    HTTPClient client;
    String invoiceURL = "https://" + blitzServer + ":8080/v1/invoice/" + paymentHash;
    client.begin(invoiceURL, test_root_fingerprint);
    client.addHeader("Content-Type", "application/json");
    client.addHeader("Grpc-Metadata-macaroon", readmacaroon);
    int httpCode = client.GET();
    if (httpCode > 0) {
      const size_t capacity = JSON_OBJECT_SIZE(16) + 580;
      DynamicJsonDocument doc(capacity);

      deserializeJson(doc, client.getString());
      client.end();
      if (doc["state"] == "SETTLED") {
        ez.msgBox("", "Payment Complete");
        break;
      }
    }
    delay(5000);
  }
}

String getHash(String invoiceId) {
  HTTPClient client;
  String invoiceURL = "https://" + blitzServer + ":8080/v1/payreq/" + invoiceId;
  client.begin(invoiceURL, test_root_fingerprint);
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Grpc-Metadata-macaroon", readmacaroon);
  const size_t capacity = JSON_OBJECT_SIZE(7) + 270;
  DynamicJsonDocument doc(capacity);
  int httpCode = client.GET();
  deserializeJson(doc, client.getString());
  String blitzPaymentHash = doc["payment_hash"];
  return blitzPaymentHash;
  client.end();
}

void SatsHunter() {
M5.Lcd.drawBitmap(0, 0, 320, 240, (uint8_t *)Physfau_map);
  bool checkah = false;
  while(checkah == false){
   M5.update();
   if (M5.BtnA.wasReleased()) {
    checkah = true;
   }
   else if (M5.BtnB.wasReleased()) {
    checkah = true;
   }
   else if (M5.BtnC.wasReleased()) {
    checkah = true;
   }
  }
  M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(80, 120);
     M5.Lcd.setTextSize(1);
     M5.Lcd.setTextColor(TFT_WHITE);
     M5.Lcd.println("PROCESSING");
  nodecheck();
  M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(10, 120);
     M5.Lcd.setTextSize(1);
     M5.Lcd.setTextColor(TFT_GREEN);
     M5.Lcd.println("GENERATING HUNT GIFT!");
  create_gift();
  makepayment();

  while(giftstatus == "unpaid"){
    checkgiftstatus();
    delay(500);
  }
  page_qrdisplay(giftlnurl);

  while(!spent){
    checkgift();
    delay(500);
  }

  for (int i = 5; i >= 1; i--){
     M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(55, 120);
     M5.Lcd.setTextSize(1);
     M5.Lcd.setTextColor(TFT_GREEN);
     M5.Lcd.println("More sats in "+ String(i) +" sec");
     delay(1000);
  }
spent = false;
}


void create_gift(){

  WiFiClientSecure client;
  
  if (!client.connect(gifthost, httpsPort)) {
    return;
  }
  
  String topost = "{  \"amount\" : " + amount +"}";
  String url = "/create";
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                "Host: " + gifthost + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Content-Type: application/json\r\n" +
                "Connection: close\r\n" +
                "Content-Length: " + topost.length() + "\r\n" +
                "\r\n" + 
                topost + "\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  
  String line = client.readStringUntil('\n');
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(6) + 620;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, line);
  const char* order_id = doc["orderId"]; 
  const char* statuss = doc["status"]; // "unpaid"
  const char* lightning_invoice_payreq = doc["lightningInvoice"]["payreq"]; 
  const char* lnurl = doc["lnurl"];
  giftinvoice = lightning_invoice_payreq;
  giftstatus = statuss;
  giftid = order_id;
  giftlnurl = lnurl;
}


void nodecheck(){
  bool checker = false;
  while(!checker){
  WiFiClientSecure client;

  if (!client.connect(lndhost, 443)){

    M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(65, 80);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextColor(TFT_RED);
     M5.Lcd.println("NO NODE DETECTED");
     // checker = true;
     delay(1000);
  }
  else{
    checker = true;
  }
  }
  
}


void makepayment(){
  String memo = "Memo-";
  WiFiClientSecure client;
  if (!client.connect(lndhost, 443)){
    return;
  }
  String topost = "{\"payment_request\": \""+ giftinvoice +"\"}";
  client.print(String("POST ")+ "https://" + lndhost +"/v1/channels/transactions HTTP/1.1\r\n" +
               "Host: "  + lndhost +"\r\n" +
               "User-Agent: ESP322\r\n" +
               "Grpc-Metadata-macaroon:" + adminmacaroon + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n" +
               "Content-Length: " + topost.length() + "\r\n" +
               "\r\n" + 
               topost + "\n");

  String line = client.readStringUntil('\n');
  Serial.println(line);
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {  
      break;
    }
  }  
  String content = client.readStringUntil('\n');
  client.stop(); 
}


void checkgiftstatus(){
  WiFiClientSecure client;
  if (!client.connect(gifthost, httpsPort)) {
    return;
  }
  String url = "/status/" + giftid;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + gifthost + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String line = client.readStringUntil('\n');
  const size_t capacity = JSON_OBJECT_SIZE(1) + 40;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, line);
  const char* giftstatuss = doc["status"]; 
  giftstatus = giftstatuss;
  
}

void checkgift(){
  WiFiClientSecure client;
  if (!client.connect(gifthost, httpsPort)) {
    return;
  }
  String url = "/gift/" + giftid;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + gifthost + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.println(line);
  const size_t capacity = 3*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(9) + 1260;
  DynamicJsonDocument doc(capacity);  
  deserializeJson(doc, line);
  spent = doc["spent"]; 
}


void page_qrdisplay(String xxx)
{  

  M5.Lcd.fillScreen(BLACK); 
  M5.Lcd.qrcode(giftlnurl,45,0,240,10);
  delay(100);

}

float getConversionRate() {
  HTTPClient client;
  client.begin("https://api.opennode.co/v1/rates");
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Authorization", ONApiKey);
  const size_t capacity = 162*JSON_OBJECT_SIZE(1) + 8*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(169) + 2110;
  DynamicJsonDocument doc(capacity);  
  int httpCode = client.GET();
  deserializeJson(doc, client.getString());
  JsonObject data = doc["data"];

  String pair = "BTC" + currency;
  float n = 1.00;
  float d = data[pair][currency];
  float exchangeRate = 0.00;
  exchangeRate = n / d;
  //return data[pair]["BTC"];
  return exchangeRate;
  client.end();
}
