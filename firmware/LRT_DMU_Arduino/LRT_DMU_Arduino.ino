//UDP-Listener
//
// for LRT and DMU
// INKA-RESPATI
//
//  by takur (9okt2020)
//  by Edes (5June2019)
//
// udp port = 1337
// send: ">ipconfig ?" to display ip setting
// send: ">ipconfig to=a.b.c.d ip=e.f.g.h gw=i.j.k.l nm=m.n.o.p;"
//       to ip-addr or broadcast-addr to change ip setting in device with ip a.b.c.d
//

#define USE_EEPROM				1
#if USE_EEPROM==1
#	define USE_EEPROM_IP		1
#	define USE_EEPROM_DEFAULT	0
#endif	//if USE_EEPROM==1

#include <EtherCard.h>
#include <IPAddress.h>
#include <avr/wdt.h>
#if USE_EEPROM==1
#	include <EEPROM.h>
#endif	//if USE_EEPROM==1

#define SS_ETH 				10
#define UDP_PORT 			1337
#if USE_EEPROM_IP==1
#	define EE_IP_ADDR		0
#endif	//if USE_EEPROM_IP==1

#if USE_EEPROM_DEFAULT==1
#	define EE_DEF_ADDR		16
#endif	//if USE_EEPROM_DEFAULT==1

static String super_default_msg = "PT KERETA API INDONESIA (PERSERO)";
static String default_header_msg = "$RTEXT,0,29,112,\"";
static String default_header_send_msg = "$RTEXT,0,9,112,\"";
String super_default_message = default_header_msg + super_default_msg + "\"*";
static byte default_IP[12] = { 192, 168, 1, 63, 192, 168, 1, 1, 255, 255, 255, 0 };
static byte MYdns[4] = { 8, 8, 8, 8 };

byte EEip[4], EEgw[4], EEnm[4];
byte MYip[4], MYgw[4], MYnm[4], MYmac[6];
byte NEW_value[12];  //new data received from server
#if USE_EEPROM_IP==1
byte EE_value[12];  //data stored in EEPROM
#endif	//if USE_EEPROM_IP==1

uint32_t millis_5s = 0;
uint32_t millis_30s = 0;
String last_default_message = default_header_send_msg + super_default_msg + "\"*";
String last_udp_message;

byte Ethernet::buffer[500];  // tcp/ip send and receive buffer

//callback that prints received packets to the serial port

String uartStr;

void setup()
{
	Serial.begin(38400);

	wdt_disable();
	wdt_enable(WDTO_1S);

#if USE_EEPROM_IP==1
	read_ip_from_eeprom();
	setting_ip(EE_value);
#endif	//if USE_EEPROM_IP==1

#if USE_EEPROM_DEFAULT==1
	if (!read_default_from_eeprom(&last_default_message)) {
		last_default_message = default_header_send_msg + super_default_msg + "\"*";
		save_default_to_eeprom(&last_default_message);
	}
#endif	//if USE_EEPROM_DEFAULT==1

	ether.udpServerListenOnPort(&udpSerialPrint, UDP_PORT);  //register udpSerialPrint() to port UDP_PORT
	delay_wdt(1000);
	Serial.println(F("$RTEXT,1,40,0,*"));
	delay_wdt(500);
	display_setting();
	delay_wdt(2000);
	Serial.println(F("$RTEXT,1,40,0,*"));
	delay_wdt(250);
	Serial.println(super_default_message);
	delay_wdt(250);
	Serial.println(last_default_message);
	delay_wdt(1000);
	millis_5s = millis() + 5000;
	millis_30s = millis() + 30000;

	uartStr.reserve(128);
	last_udp_message.reserve(500);

}

void loop()
{
	char c;
	bool uartCompleted = 0;
	ether.packetLoop(ether.packetReceive());

	wdt_reset();

	if (millis() >= millis_5s) {
		millis_5s = millis() + 5000;

		Serial.println(last_udp_message);
	}

	if (millis() >= millis_30s) {
		millis_30s = millis() + 30000;
		millis_5s = millis_30s + 10000;

		last_udp_message="";
		Serial.println(last_default_message);
	}

	if (Serial.available()) {
		c = Serial.read();

		if (c == '$')
			uartStr = "";
		else if (c == '*')
			uartCompleted = 1;

		uartStr += c;
	}

	if (uartCompleted)
		parsing_uart_command(&uartStr);
}

void parsing_uart_command(String *s)
{
	if (s->indexOf("$DEFAULT*") == 0) {
#if USE_EEPROM_IP==1
		save_ip_to_eeprom(default_IP);
#endif	//if USE_EEPROM_IP==1
#if USE_EEPROM_DEFAULT==1
		last_default_message = "$RTEXT,0,9,100,\"" + super_default_msg + "\"*";
		save_default_to_eeprom(&last_default_message);
#endif	//if USE_EEPROM_DEFAULT==1

		reset_wdt();
	}
}

int parsing_update(String *StringToParse)
{
	String command, NEW_ip, NEW_gw, NEW_nm, tem, TOip;
	int awal = 1, akhir;

	wdt_reset();
	akhir = StringToParse->indexOf(' ', awal);
	command = StringToParse->substring(awal, akhir);
	command.toLowerCase();
	if (command != "ipconfig")
		return (0);

	awal = akhir + 1;
	akhir = StringToParse->indexOf(' ', awal);
	TOip = StringToParse->substring(awal, akhir);
	if (TOip == "?")
		return (2);
	awal = akhir + 1;
	akhir = StringToParse->indexOf(' ', awal);
	NEW_ip = StringToParse->substring(awal, akhir);
	awal = akhir + 1;
	akhir = StringToParse->indexOf(' ', awal);
	NEW_gw = StringToParse->substring(awal, akhir);
	awal = akhir + 1;
	akhir = StringToParse->indexOf(';', awal);
	NEW_nm = StringToParse->substring(awal, akhir);

	awal = 0;
	int i;
	for ( i = 0; i < 4; ++i ) {
		if (i < 3) {
			akhir = TOip.indexOf('.', awal);
			tem = TOip.substring(awal, akhir);
		}
		else
			tem = TOip.substring(awal);
		if (tem.length() > 0)
			NEW_value[i] = byte(tem.toInt());
		awal = akhir + 1;
	}

	if ((NEW_value[0] == ether.myip[0]) && (NEW_value[1] == ether.myip[1])
			&& (NEW_value[2] == ether.myip[2]) && (NEW_value[3] == ether.myip[3])) {

		awal = 0;
		for ( i = 0; i < 4; ++i ) {
			if (i < 3) {
				akhir = NEW_ip.indexOf('.', awal);
				tem = NEW_ip.substring(awal, akhir);
			}
			else
				tem = NEW_ip.substring(awal);

			if (tem.length() > 0)
				NEW_value[i] = byte(tem.toInt());
			awal = akhir + 1;
		}

		awal = 0;
		for ( i = 4; i < 8; ++i ) {
			if (i < 7) {
				akhir = NEW_gw.indexOf('.', awal);
				tem = NEW_gw.substring(awal, akhir);
			}
			else
				tem = NEW_gw.substring(awal);
			if (tem.length() > 0)
				NEW_value[i] = byte(tem.toInt());
			awal = akhir + 1;
		}

		awal = 0;
		for ( i = 8; i < 12; ++i ) {
			if (i < 11) {
				akhir = NEW_nm.indexOf('.', awal);
				tem = NEW_nm.substring(awal, akhir);
			}
			else
				tem = NEW_nm.substring(awal);
			if (tem.length() > 0)
				NEW_value[i] = byte(tem.toInt());
			awal = akhir + 1;
		}

		return (1);
	}
	else
		return (0);
}

#if USE_EEPROM_DEFAULT==1
bool read_default_from_eeprom(String *s)
{
	char c;

	c = EEPROM.read(EE_DEF_ADDR);
	if (c == '$') {
		*s = "";
		for ( int i = 0; i < 500; i++ ) {
			c = EEPROM.read(EE_DEF_ADDR + i);
			*s += c;
			if (c == '*')
				break;
		}
		return 1;
	}
	Serial.println();

	return 0;
}

bool save_default_to_eeprom(String *s)
{
	char c;

	if ((s->indexOf('$') == 0) && (s->indexOf('*') > 0)) {
		for ( unsigned int i = 0; i < 500; i++ ) {
			c = s->charAt(i);
			EEPROM.update(EE_DEF_ADDR + i, c);
			if (c == '*')
				break;
		}
		return 1;
	}

	return 0;
}
#endif	//if USE_EEPROM_DEFAULT==1

#if USE_EEPROM_IP==1

void read_ip_from_eeprom()
{
	wdt_reset();  //reset wdt

	// read EEPROM value
	for ( int i = 0; i < 12; i++ )
		EE_value[i] = EEPROM.read(EE_IP_ADDR + i);

	// check if EEPROM value is empty (usually fresh burned)
	if (((EE_value[0] == 0xFF) && (EE_value[4] == 0xFF) && (EE_value[8] == 0xFF))
			|| ((EE_value[0] == 0) && (EE_value[4] == 0) && (EE_value[8] == 0))) {

		Serial.println(F("update eeprom!"));
		// if empty, update with default value
		for ( int i = 0; i < 12; i++ ) {
			EEPROM.update(EE_IP_ADDR + i, default_IP[i]);
			EE_value[i] = default_IP[i];  // so the content of EE-param is default
		}
	}

	Serial.print(F("IP: "));
	for ( int i = 0; i < 3; ++i ) {
		Serial.print(EE_value[i]);
		Serial.print('.');
	}
	Serial.println(EE_value[3]);

	Serial.print(F("GW: "));
	for ( int i = 4; i < 7; ++i ) {
		Serial.print(EE_value[i]);
		Serial.print('.');
	}
	Serial.println(EE_value[7]);

	Serial.print(F("NM: "));
	for ( int i = 8; i < 11; ++i ) {
		Serial.print(EE_value[i]);
		Serial.print('.');
	}
	Serial.println(EE_value[11]);

}

void save_ip_to_eeprom(byte *data)
{
	wdt_reset();  //reset wdt

	for ( int i = 0; i < 12; i++ )
		EEPROM.update(EE_IP_ADDR + i, data[i]);
}

#endif	//if USE_EEPROM_IP==1

void setting_ip(byte *data)
{
	wdt_reset();
	byte MYip[4] = { data[0], data[1], data[2], data[3] };
	byte MYgw[4] = { data[4], data[5], data[6], data[7] };
	byte MYnm[4] = { data[8], data[9], data[10], data[11] };
	byte MYmac[6] = { 0xED, 0xE5, data[0], data[1], data[2], data[3] };  // ethernet mac address - must be unique on your network

	if (ether.begin(sizeof Ethernet::buffer, MYmac, SS_ETH) == 0)
		Serial.println(F("Failed to access Eth controller"));
	else
		Serial.println(F("Success access eth controller & setting MAC-addr"));

	ether.staticSetup(MYip, MYgw, MYdns, MYnm);
}

void reset_wdt()
{
	wdt_disable();
	wdt_enable(WDTO_250MS);
	while (1)
		;
}

void delay_wdt(uint32_t _delayTime)
{
	const uint32_t wdt_reset_time = 100;

	wdt_reset();
	if (_delayTime <= wdt_reset_time)
		delay(_delayTime);
	else {
		while (_delayTime > wdt_reset_time) {
			delay(wdt_reset_time);
			wdt_reset();
			_delayTime -= wdt_reset_time;
		}
		delay(_delayTime);
	}
	wdt_reset();
}

bool defaultText(String *s)
{
	/* default text or block */
	byte awal = 0, akhir;
	String tem;
	uint32_t defCheckVal;
	char buf[16];

	//	$RTEXT,x,[def],x,x*
	akhir = s->indexOf(',') + 1;
	awal = s->indexOf(',', akhir) + 1;
	akhir = s->indexOf(',', awal);
	tem = s->substring(awal, akhir);
	tem.toCharArray(buf, 16);
	defCheckVal = strtol(buf, NULL, 16);

	if (bitRead(defCheckVal, 5)) {
		String s_awal, s_akhir, defVal;

		s_awal = s->substring(0, awal);
		s_akhir = s->substring(akhir);
		bitClear(defCheckVal, 5);
		defVal = String(defCheckVal, 16);
		defVal.toUpperCase();
		last_default_message = s_awal + defVal + s_akhir;

#if USE_EEPROM_DEFAULT==1
		if (save_default_to_eeprom(&last_default_message))
			Serial.println("saved to eeprom!");
#endif	//if USE_EEPROM_DEFAULT==1

		return 1;
	}

	return 0;
}

bool blockCommand(String *s)
{
	byte awal, akhir;
	String tem;
	char c;

	awal = s->indexOf(',') + 1;
	akhir = s->indexOf(',', awal);

	/* only 1 char */
	if (akhir == awal + 1) {
		c = s->charAt(awal);
		if (c == '1')
			return 1;
	}

	return 0;
}

void udpSerialPrint(uint16_t dest_port, uint8_t src_ip[IP_LEN], uint16_t src_port, char *data,
		uint16_t len)
{
	IPAddress src(src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
	wdt_reset();
	Serial.println(data);

	String string_udp = data;
	int hasil = parsing_update(&string_udp);

	if (data[0] == '$') {

		if (!blockCommand(&string_udp) && !defaultText(&string_udp)) {
			last_udp_message = string_udp;
			millis_5s = millis() + 5000;
			millis_30s = millis() + 30000;
		}
		else
			millis_5s = millis() + 1000;
	}
	else if (data[0] == '>') {
		Serial.print(F("command:"));

		if (hasil == 1) {
#if USE_EEPROM_IP==1
			Serial.println(F("$Change IP & wait Reset*"));
			save_ip_to_eeprom(NEW_value);
			reset_wdt();
#endif	//if USE_EEPROM_IP==1
		}
		else if (hasil == 2) {
			display_setting();
		}
	}
}

void display_setting()
{
	String setting_string = "$RTEXT,0,1,1000,\"IP: ";
	for ( int i = 0; i < 4; i++ ) {
		setting_string += ether.myip[i];
		if (i < 3)
			setting_string += ".";
	}
	setting_string += "\r\n";
	setting_string += "GW: ";
	for ( int i = 0; i < 4; i++ ) {
		setting_string += ether.gwip[i];
		if (i < 3)
			setting_string += ".";
	}
	setting_string += "\"*";
	Serial.println(setting_string);

}

