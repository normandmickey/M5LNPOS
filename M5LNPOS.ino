#include <ezTime.h>
#include <M5ez.h>
#include <M5Stack.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <math.h>

// For OpenNode
String ONApiKey = "OpenNode API Key";
String currency = "USD";

//For RaspiBlitz
String readmacaroon = "Raspiblitz Read-Only Macaroon";
String invoicemacaroon = "Raspiblitz Invoice Macaroon";
const char* test_root_fingerprint = "SSL Fingerprint";

void setup() {
  ez.begin();
}

void loop() {
  ezMenu myMenu;
  myMenu.addItem("Bitcoin Price", mainmenu_one);
  myMenu.addItem("ON Lightning Invoice", mainmenu_two);
  myMenu.addItem("Raspiblitz Lightning Invoice", mainmenu_three);
  myMenu.addItem("Settings", mainmenu_four);
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
  ez.settings.menu();
}

void ONRates() {
  HTTPClient client;
  client.begin("https://api.opennode.co/v1/rates");
  int httpCode = client.GET();
  if (httpCode >0) {
    const size_t capacity = 162*JSON_OBJECT_SIZE(1) + 8*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(169) + 2110;
  DynamicJsonDocument doc(capacity);  
  deserializeJson(doc, client.getString());
  JsonObject data = doc["data"];

  String pair = "BTC" + currency;
  ez.msgBox("", data[pair][currency]);
  }
  client.end();
}

void ONInvoice() {
  String amount = ez.textInput();
    
  HTTPClient client;
  client.begin("https://api.opennode.co/v1/charges");
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Authorization", ONApiKey);
  String jsonPost = String("{") + "\"amount\":" + "\"" + amount + "\"" + "," + "\"currency\":" + "\"" + currency + "\"" + "}";
  int httpCode = client.POST(jsonPost);
  if (httpCode >0) {  
    const size_t capacity = 2*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(19) + 950;
  DynamicJsonDocument doc(capacity);  
  deserializeJson(doc, client.getString());
  JsonObject data = doc["data"];
 
  String lnInvoice = data["lightning_invoice"]["payreq"];
  String invoiceId = data["id"];

  ez.screen.clear();
  M5.begin();
  M5.Lcd.qrcode(lnInvoice,45,0,240,10);
  delay(60000);
  checkONInvoiceStatus(invoiceId);
  }
  client.end();
}

void raspiBlitzInvoice() {
  String amount = ez.textInput();

  //BLITZ DETAILS
const char*  server = "liab1.mooo.com"; 
const int httpsPort = 443;
const int lndport = 8080;

String on_currency = "BTCUSD"; //currency can be changed here ie BTCUSD BTCGBP etc
String on_sub_currency = on_currency.substring(3);
String memo = "Memo "; //memo suffix, followed by the price then a random number
    
  HTTPClient client;
  client.begin("https://liab1.mooo.com:8080/v1/invoices", test_root_fingerprint);
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Grpc-Metadata-macaroon", invoicemacaroon);
  String jsonPost = "{\"value\":\"10\",\"memo\":\"memo2\",\"expiry\":\"1000\"}";
  int httpCode = client.POST(jsonPost);
  if (httpCode >0) {  
    const size_t capacity = JSON_OBJECT_SIZE(3) + 320;
  DynamicJsonDocument doc(capacity);  
  deserializeJson(doc, client.getString());
   
  const char* payment_request = doc["payment_request"];
  String payreq = payment_request;
  Serial.println(payreq);
  
  ez.screen.clear();
  M5.begin();
  M5.Lcd.qrcode(payreq,45,0,240,10);
  delay(20000);
  }
  client.end();
}


void checkONInvoiceStatus(String invoiceId) {
  delay (3000);
  for (int x = 0; x < 720; x++) {
  HTTPClient client;
  String invoiceURL = "https://api.opennode.co/v1/charge/" + invoiceId;
  client.begin(invoiceURL);
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Authorization", ONApiKey);
  int httpCode = client.GET();
  if (httpCode >0) {
    const size_t capacity = JSON_ARRAY_SIZE(0) + JSON_OBJECT_SIZE(0) + 2*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(19) + 1060;
  DynamicJsonDocument doc(capacity);
  
  deserializeJson(doc, client.getString());
  JsonObject data = doc["data"];
  client.end();
  //ez.msgBox("", data["status"]);
  if (data["status"] == "pao75432id") { ez.msgBox("", "Payment Complete");
  break; }
  }
  delay(5000);
 }
}
