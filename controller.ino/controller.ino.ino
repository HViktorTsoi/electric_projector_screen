#include <ESP8266WiFi.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>


#define server_ip "bemfa.com"  //巴法云服务器地址默认即可
#define server_port "8344"     //服务器端口，tcp创客云端口8344

//********************需要修改的部分*******************//

// #define wifi_name "郊眠观"                        //WIFI名称，区分大小写，不要写错
// #define wifi_password "q7kpyfzuk9fm"              //WIFI密码
#define wifi_name "Space Robots"                        //WIFI名称，区分大小写，不要写错
#define wifi_password "xxxy5044"              //WIFI密码
String UID = "fdaa161508444738bbaf2dcfac76b463";  //用户私钥，
String TOPIC = "screen002";                       //主题名字，可在控制台新建
const int PIN_PWM_MOTOR = 13;                     // Motor PWM输出
const uint16_t PIN_IR = 14; // An IR detector/demodulator is connected to GPIO pin 14 Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
const int PIN_SWITCH = 12;                        // 微动开关
const int PIN_LED_BREAKOUT = 16;                  // 载板 LED
const int PIN_LED_SOM = 2;                        // SOM LED

//**************************************************//
//最大字节数
#define MAX_PACKETSIZE 512
//设置心跳值60s
#define KEEPALIVEATIME 60 * 1000
//tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";  //初始化字符串，用于接收服务器发来的数据
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;     //心跳
unsigned long preTCPStartTick = 0;  //连接
bool preTCPConnected = false;
//相关函数初始化
//连接WIFI
void doWiFiTick();
void startSTA();

//TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);

//led控制函数，具体函数内容见下方
void screen_up();
void screen_down();
void toggle_screen();


//********************IR Related*******************//
// ==================== start of TUNEABLE PARAMETERS ====================

// As this program is a special purpose capture/decoder, let us use a larger
// than normal buffer so we can handle Air Conditioner remote codes.
const uint16_t kCaptureBufferSize = 1024;

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
#if DECODE_AC
// Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
// A value this large may swallow repeats of some protocols
const uint8_t kTimeout = 50;
#else   // DECODE_AC
// Suits most messages, while not swallowing many repeats.
const uint8_t kTimeout = 15;
#endif  // DECODE_AC

// Set higher if you get lots of random short UNKNOWN messages when nothing
// should be sending a message. Set lower if you are sure your setup is working, but it doesn't see messages
// from your device. (e.g. Other IR remotes work.)
// NOTE: Set this value very high to effectively turn off UNKNOWN detection.
const uint16_t kMinUnknownSize = 12;

// How much percentage lee way do we give to incoming signals in order to match
// it?
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

// Legacy (No longer supported!)

// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(PIN_IR, kCaptureBufferSize, kTimeout, true);
decode_results results;  // Somewhere to store the results

/*
  *发送数据到TCP服务器
 */
void sendtoTCPServer(String p) {
  if (!TCPclient.connected()) {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
  preHeartTick = millis();  //心跳计时开始，需要每隔60秒发送一次数据
}


/*
  *初始化和服务器建立连接
*/
void startTCPClient() {
  if (TCPclient.connect(server_ip, atoi(server_port))) {
    digitalWrite(PIN_LED_BREAKOUT, HIGH);
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n", server_ip, atoi(server_port));

    String tcpTemp = "";                                        //初始化字符串
    tcpTemp = "cmd=1&uid=" + UID + "&topic=" + TOPIC + "\r\n";  //构建订阅指令
    sendtoTCPServer(tcpTemp);                                   //发送订阅指令
    tcpTemp = "";                                               //清空
    /*
     //如果需要订阅多个主题，可发送  cmd=1&uid=xxxxxxxxxxxxxxxxxxxxxxx&topic=xxx1,xxx2,xxx3,xxx4\r\n
    教程：https://bbs.bemfa.com/64
     */

    preTCPConnected = true;
    TCPclient.setNoDelay(true);
    digitalWrite(PIN_LED_BREAKOUT, LOW);
  } else {
    Serial.print("Failed connected to server:");
    Serial.println(server_ip);
    TCPclient.stopAll();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}


/*
  *检查数据，发送心跳
*/
void doTCPClientTick() {
  //检查是否断开，断开后重连
  if (WiFi.status() != WL_CONNECTED) return;
  if (!TCPclient.connected()) {  //断开重连
    if (preTCPConnected == true) {
      preTCPConnected = false;
      preTCPStartTick = millis();
      Serial.println();
      Serial.println("TCP Client disconnected.");
      TCPclient.stopAll();
    } else if (millis() - preTCPStartTick > 1 * 1000)  //重新连接
      TCPclient.stopAll();
    startTCPClient();
  } else {
    if (TCPclient.available()) {  //收数据
      char c = TCPclient.read();
      TcpClient_Buff += c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();

      if (TcpClient_BuffIndex >= MAX_PACKETSIZE - 1) {
        TcpClient_BuffIndex = MAX_PACKETSIZE - 2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
    }
    if (millis() - preHeartTick >= KEEPALIVEATIME) {  //保持心跳
      preHeartTick = millis();
      Serial.println("--Keep alive:");
      sendtoTCPServer("ping\r\n");  //发送心跳，指令需\r\n结尾，详见接入文档介绍
    }
  }
  if ((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick >= 200)) {
    TCPclient.flush();
    Serial.print("Rev string: ");
    TcpClient_Buff.trim();           //去掉首位空格
    Serial.println(TcpClient_Buff);  //打印接收到的消息
    String getTopic = "";
    String getMsg = "";
    if (TcpClient_Buff.length() > 15) {  //注意TcpClient_Buff只是个字符串，在上面开头做了初始化 String TcpClient_Buff = "";
      //此时会收到推送的指令，指令大概为 cmd=2&uid=xxx&topic=light002&msg=off
      int topicIndex = TcpClient_Buff.indexOf("&topic=") + 7;     //c语言字符串查找，查找&topic=位置，并移动7位，不懂的可百度c语言字符串查找
      int msgIndex = TcpClient_Buff.indexOf("&msg=");             //c语言字符串查找，查找&msg=位置
      getTopic = TcpClient_Buff.substring(topicIndex, msgIndex);  //c语言字符串截取，截取到topic,不懂的可百度c语言字符串截取
      getMsg = TcpClient_Buff.substring(msgIndex + 5);            //c语言字符串截取，截取到消息
      Serial.print("topic:------");
      Serial.println(getTopic);  //打印截取到的主题值
      Serial.print("msg:--------");
      Serial.println(getMsg);  //打印截取到的消息值
    }

    // 接收到消息 toggle screen
    if (getMsg == "on" || getMsg == "off") {
      toggle_screen();
    }
    // int intensity = -1;
    // int intensity_pos = getMsg.indexOf('#');
    // // 获取亮度
    // if (intensity_pos != -1) {
    //   intensity = getMsg.substring(intensity_pos + 1).toInt();
    //   Serial.println(intensity);
    // }
    // else if (intensity != -1) {  // 亮度消息
    //   screen_up();
    // } else if (getMsg == "off") {  //如果是消息==关闭
    //   screen_down();
    // }

    TcpClient_Buff = "";
    TcpClient_BuffIndex = 0;
  }
}
/*
  *初始化wifi连接
*/
void startSTA() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_name, wifi_password);
}



/**************************************************************************
                                 WIFI
***************************************************************************/
/*
  WiFiTick
  检查是否需要初始化WiFi
  检查WiFi是否连接上，若连接成功启动TCP Client
  控制指示灯
*/
void doWiFiTick() {
  static bool startSTAFlag = false;
  static bool taskStarted = false;
  static uint32_t lastWiFiCheckTick = 0;

  if (!startSTAFlag) {
    startSTAFlag = true;
    startSTA();
  }

  //未连接1s重连
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiCheckTick > 1000) {
      lastWiFiCheckTick = millis();
    }
    taskStarted = false;
  }
  //连接成功建立
  else {
    if (taskStarted == false) {
      taskStarted = true;
      Serial.print("\r\nGet IP Address: ");
      Serial.println(WiFi.localIP());
      digitalWrite(PIN_LED_BREAKOUT, LOW);
      startTCPClient();
    }
  }
}


/**************************************************************************
                                 IR
***************************************************************************/
void doIRTick() {
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Check if we got an IR message that was to big for our capture buffer.
    if (results.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
    // Display the tolerance percentage if it has been change from the default.
    if (kTolerancePercentage != kTolerance)
      Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);

    // 显示协议，以及包字符串
    Serial.printf("DECODE TYPE: %d\n", results.decode_type);
    String decode_msg = resultToHexidecimal(&results);
    if (decode_msg.equals("0xFF38C7")) {
      Serial.print("OK\n");
      toggle_screen();
    }
    // if (decode_msg.equals("0xFFB04F")) {
    //   Serial.print("#\n");
    //   screen_down();
    // }
    // if (decode_msg.equals("0xFF18E7")) {
    //   Serial.print("UP\n");
    // }
    // if (decode_msg.equals("0xFF4AB5")) {
    //   Serial.print("DOWN\n");
    // }
    // yield();  // Feed the WDT (again)
  }
}



/**************************************************************************
                                 GLOBAL STATE
***************************************************************************/
enum SCREEN_STATE { UP,
                    UP_ING,
                    DOWN,
                    DOWN_ING };

SCREEN_STATE screen_state;

int SECS_UP = 9;
int SECS_DOWN = 8;

void toggle_screen() {
  if (screen_state == UP) {
    // 降低屏幕
    screen_down();
    screen_state = DOWN;
  } else if (screen_state == DOWN) {
    // 升起屏幕
    screen_up();
    screen_state = UP;
  }
  Serial.printf("ST: %d\n", screen_state);
}

// 开启电机
void screen_up() {
  // Serial.println("Turn ON");
  digitalWrite(PIN_LED_SOM, LOW);
  // 缓启动
  analogWrite(PIN_PWM_MOTOR, 155);
  delay(1000);
  analogWrite(PIN_PWM_MOTOR, 165);
  delay(1000);
  // 等待一段时间电机卷上去
  analogWrite(PIN_PWM_MOTOR, 170);
  for (int i = 0; i < max(0, SECS_UP - 3); ++i) {
    delay(1000);
  }
  // 缓停止
  analogWrite(PIN_PWM_MOTOR, 160);
  delay(1000);
  // 锁住电机
  analogWrite(PIN_PWM_MOTOR, 150);

  digitalWrite(PIN_LED_SOM, HIGH);
}

//关闭灯泡
void screen_down() {
  // // Serial.println("Turn OFF");
  // // 释放电机 重力阻尼下降
  // analogWrite(PIN_PWM_MOTOR, 0);

  // // 等待一段时间
  // for (int i = 0; i < SECS_DOWN; ++i) {
  //   delay(500);
  //   digitalWrite(PIN_LED_SOM, LOW);
  //   delay(500);
  //   digitalWrite(PIN_LED_SOM, HIGH);
  // }
  // digitalWrite(PIN_LED_SOM, HIGH);

  // Serial.println("Turn ON");
  digitalWrite(PIN_LED_SOM, LOW);
  // 缓启动
  analogWrite(PIN_PWM_MOTOR, 145);
  delay(1000);
  analogWrite(PIN_PWM_MOTOR, 135);
  delay(1000);
  // 等待一段时间电机卷上去
  analogWrite(PIN_PWM_MOTOR, 130);
  for (int i = 0; i < max(0, SECS_DOWN - 3); ++i) {
    delay(1000);
  }
  // 缓停止
  analogWrite(PIN_PWM_MOTOR, 140);
  delay(1000);
  // 锁住电机
  analogWrite(PIN_PWM_MOTOR, 0);

  digitalWrite(PIN_LED_SOM, HIGH);
}


// 初始化，相当于main 函数
void setup() {
  Serial.begin(115200);
  pinMode(PIN_PWM_MOTOR, OUTPUT);
  pinMode(PIN_LED_BREAKOUT, OUTPUT);
  pinMode(PIN_LED_SOM, OUTPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);


  digitalWrite(PIN_LED_BREAKOUT, HIGH);
  digitalWrite(PIN_LED_SOM, HIGH);
  // TODO 开机时释放电机较长时间 保证screen在底部
  analogWriteFreq(50);
  analogWriteRange(2000);
  analogWrite(PIN_PWM_MOTOR, 0);

  // screen_down();

  // state 相关
  screen_state = DOWN;

  // IR setup
  assert(irutils::lowLevelSanityCheck() == 0);
  Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", PIN_IR);
#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                                        // DECODE_HASH
  irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
  irrecv.enableIRIn();                        // Start the receiver
}

//循环
void loop() {
  doWiFiTick();
  doTCPClientTick();
  doIRTick();
  // Serial.printf("GPIO12: %d\n", digitalRead(PIN_SWITCH));
  // delay(100);
}